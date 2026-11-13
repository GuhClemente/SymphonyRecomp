#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Underground Garden's stage sound bank.

The Saturn's Garden sounds (SD/SD_TEI.CRT, rendered to WAVs by
mods/sotn-ultimate/tools/saturn_sfx.py) in a PS1 VAB of their own, written as
build/us/SD_ZKTE1.VH/.VB (the unused TE1 stage's bank slot; `make disk` takes
build/us files over the retail ones; lba_stage.c's GDN row loads TE1's).

A stage bank (vab 3) is reached through g_SfxData: each sound id names a
(program, tone, note). Those slots belong to whichever stage bank is loaded,
so the Garden's bank fills the slots of the ids it plays:
  - the Saturn-only sounds (Saturn ids 0x8F0+) take ids whose slots are free
    here (MOD_SFX_* in the generated mods/sotn-ultimate/src/garden/gdn/mod_gdn_sfx.h);
  - the Garden's water sounds (Saturn 0x777/0x778) go in the slots the PS1
    water code plays (0x7C2/0x7C3);
  - the boss reward jingle (Saturn 0x766) is the PS1's SFX_BOSS_DEFEATED
    script (the same descending notes) on the Garden's own instrument: that
    instrument goes in the slot the script plays (INSTRUMENTS), instead of a
    4 s render.
The Saturn's Garden bank has no Corpseweed spit (0x7C4) or poison drip
(0x7CA): those stay silent here, as on the Saturn. Every other slot is empty.

The bank must fit the sound RAM after the stage bank's address (0x60A40,
g_VabAddrs[3]) and before the reverb work area (Studio B, 0x4840 bytes at
the end): VB_LIMIT is the largest retail stage bank. Sample rates are capped
lower until it fits (RATE_CAPS).
"""
import math
import os
import struct
import subprocess
import sys
import wave

import yaml

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
sys.path.insert(0, os.path.dirname(__file__))
from vab import Vab  # noqa: E402
from mod_work import work_dir  # noqa: E402

NP3 = os.path.join(ROOT, "disks/us/ST/NP3/SD_ZKNP3")  # the layout template
OUT = os.path.join(ROOT, "build/us/SD_ZKTE1")
HEADER = os.path.join(ROOT, "mods/sotn-ultimate/src/garden/gdn/mod_gdn_sfx.h")
WORK = work_dir("sfx")
SATXA = os.path.join(ROOT, "bin/satxa")
VB_LIMIT = 0x1A610  # the largest retail stage bank (DRE)
# for the messages and the header (mods/sotn-ultimate/tools/gen_prs_sounds.py reuses this
# module for the Cursed Prison's bank)
LABEL = "gdn"
DESCRIPTION = "the Garden's own sounds (its stage bank, build/us/SD_ZKTE1)"
# sample rate caps tried, highest first, until the bank fits
RATE_CAPS = [44100, 32000, 22050] + list(range(20000, 10999, -500))
# short sounds whose highs matter, kept at the Saturn's rate (the shears
# click is a 29 kHz render, 2.7 KB)
UNCAPPED = {"GARDENER_SHEARS"}

# (name, Saturn id, PS1 id played; the bank fills that id's slot). Sound
# effects share 4 channel pairs by priority (g_SfxData unk6; src/dra/sound.c
# func_8013572C), the same scale as the Saturn's (its sound table byte 4).
# The ids keep the Saturn's priorities: 50 for its Garden sounds, 30 for
# the water.
SOUNDS = [
    ("BOSS_BOMB", 0x8F7, 0x70B),
    ("BOSS_DEATH", 0x8F8, 0x72D),
    ("TREE_FACE_FALL", 0x8FA, 0x709),
    ("TREE_FACE_BURST", 0x8FB, 0x70A),
    ("TREE_HURT", 0x8FC, 0x713),
    ("TREE_DEATH", 0x8FD, 0x731),
    ("GARGOYLE_CLAW", 0x8FE, 0x725),
    ("GARGOYLE_DEBRIS", 0x8FF, 0x728),
    ("MANEATER_BURST", 0x90B, 0x734),
    ("MANEATER_OPEN", 0x90C, 0x726),
    ("MANEATER_SNAP", 0x90D, 0x733),
    ("GARDENER_DEATH", 0x90E, 0x735),
    ("GARDENER_SHEARS", 0x90F, 0x73B),
    ("LEAPER_DEATH", 0x911, 0x751),
    ("LEAPER_LEAP", 0x912, 0x74B),
    ("LEAPER_FALL", 0x913, 0x74C),
    ("WATER_SPLASH", 0x777, 0x7C2),
    ("WATER_SURFACE", 0x778, 0x7C3),
]
SATURN_PRIORITY = {"WATER_SPLASH": 30, "WATER_SURFACE": 30}  # else 50

# PS1 common-bank sounds copied in at another id: (name, VAB, program, tone,
# original id, PS1 id played). None: the Saturn's priorities are kept.
COPIES = []

# (name, Saturn bank file, voice, (program, tone) the PS1 plays it from)
INSTRUMENTS = []

# PS1 stage-bank tones copied as they are, (VAB, program, tone) into the same
# slot: the Saturn sound is the PS1's one (same recording; user rule: keep the
# PS1 sound then)
PS1_TONES = [
    # SFX_BOSS_DEFEATED's notes 69..64 (vab 3, program 1, tone 14): the
    # Saturn's jingle instrument (SD_TEI v00) is this sample, 6 semitones up
    # (correlation 0.88); the PS1 jingle sounds through it in every stage bank
    # that has it (WRP, RWRP, RNZ1...)
    # stored at half rate (the centre note an octave up): 3.8 KB, not 7.7
    ("disks/us/ST/WRP/SD_ZKWRP", 1, 14, 2),
]
# also copy each PS1 tone's pair voice (tone + 1: the same sample, detuned
# and quieter, which the game starts with it) when the source has one. Off
# for the Garden and the Prison (their banks predate it); the inverted
# stages' (gen_rgdn_sounds.py, gen_rprs_sounds.py) turn it on
PS1_PAIRS = False

K0 = [0, 60, 115, 98, 122]
K1 = [0, 0, -52, -55, -60]


def decode_vag(b):
    """a VAB sample body -> int16 samples (to the end flag)"""
    out, o, oo = [], 0, 0
    for k in range(0, len(b) - 15, 16):
        f, r = min(b[k] >> 4 & 7, 4), b[k] & 15
        for i in range(28):
            t = b[k + 2 + i // 2] >> ((i & 1) * 4) & 15
            t = t - 16 if t >= 8 else t
            s = max(-32768, min(32767, ((t << 12) >> r) + ((o * K0[f] + oo * K1[f] + 32) >> 6)))
            oo, o = o, s
            out.append(s)
        if b[k + 1] & 1 and not b[k + 1] & 2:
            break
    return out


def wav_rate(path):
    with wave.open(path) as w:
        return w.getframerate()


def write_wav(path, samples, rate):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(struct.pack("<%dh" % len(samples), *samples))


def centre(note, rate, fine=0):
    """the VAB centre note and shift at which `note` plays the sample at its
    own rate. libsnd (note2pitch2) plays 44100 Hz at the centre note and the
    shift RAISES the pitch, in 1/128 semitones read in 1/16 steps: so the
    centre rounds up and the shift brings it back down to the rate"""
    semis = note + 12 * math.log2(44100 / rate) - fine / 128.0
    q = int(round(semis * 16))
    c = -(-q // 16)
    return c, (c * 16 - q) * 8


def resample(samples, rate, new_rate):
    import numpy as np
    x = np.asarray(samples, dtype=np.float64)
    n = int(len(x) * new_rate / rate)
    cutoff = 0.45 * new_rate / rate
    k = np.arange(-32, 33)
    h = 2 * cutoff * np.sinc(2 * cutoff * k) * np.hanning(len(k))
    y = np.convolve(x, h / h.sum(), "same")
    out = np.interp(np.arange(n) * rate / new_rate, np.arange(len(y)), y)
    return [int(max(-32768, min(32767, round(v)))) for v in out], new_rate


def build(render, instrument, cap):
    """render(saturn_id, max_rate) -> a 16-bit WAV path of that sound;
    instrument(file, voice) -> (int16 samples, rate, base note, fine).
    Returns the bank size, or None when it doesn't fit."""
    cfg = yaml.safe_load(open(os.path.join(ROOT, "assets/dra/us/sfx_config.yaml")))
    vab = Vab(open(NP3 + ".VH", "rb").read(), open(NP3 + ".VB", "rb").read())
    template = dict(vab.tones[0][4])  # a used tone (attack, sustain)
    blank = dict(vab.tones[0][8])  # an empty slot: a dummy sample at volume 0
    for p in vab.tones:
        vab.tones[p] = [dict(blank, prog=p, vag=1) for _ in range(16)]
    # silence that ends, as the retail samples do (an "end, mute" block, then
    # the 0x07 footer). The game plays a sound on a voice pair (its tone and
    # the next one) and frees the channel group only once both voices stop:
    # a looping blank kept groups taken forever (the menu's confirm sound,
    # priority 30, then had nowhere to play)
    vab.vags = [bytes(16) + bytes([0, 1]) + bytes(14) + bytes([0, 7]) + bytes([0x77] * 14)]
    os.makedirs(WORK, exist_ok=True)

    def add(name, wav):
        body = os.path.join(WORK, "%s.vag" % name)
        subprocess.run([SATXA, "vag", wav, body], check=True, capture_output=True)
        vab.vags.append(open(body, "rb").read())
        return len(vab.vags)

    def place(slot, vag, c, f, adsr=None):
        t = dict(template, prog=slot[0], vag=vag, vol=127, pan=64, center=c,
                 shift=f, min=0, max=127)
        if adsr:
            t["adsr1"], t["adsr2"] = adsr
        assert vab.tones[slot[0]][slot[1]]["vag"] == 1, ("slot taken", slot)
        vab.tones[slot[0]][slot[1]] = t

    for name, crt, voice, slot in INSTRUMENTS:
        samples, rate, base, fine = instrument(crt, voice)
        if rate > cap:
            samples, rate = resample(samples, rate, cap)
        wav = os.path.join(WORK, "%s_%d.wav" % (name, cap))
        write_wav(wav, samples, rate)
        c, f = centre(base, rate, fine)
        place(slot, add(name, wav), c, f)
    for src, prog, tone, div in PS1_TONES:
        sv = Vab(open(os.path.join(ROOT, src + ".VH"), "rb").read(),
                 open(os.path.join(ROOT, src + ".VB"), "rb").read())
        st = dict(sv.tones[prog][tone])
        body = sv.vags[st["vag"] - 1]
        if div > 1:
            pcm = decode_vag(body)[28:]  # past the leading zero block
            pcm, _ = resample(pcm, 44100, 44100 // div)
            wav = os.path.join(WORK, "ps1_p%d_t%d.wav" % (prog, tone))
            write_wav(wav, pcm, 44100 // div)
            body = os.path.join(WORK, "ps1_p%d_t%d.vag" % (prog, tone))
            subprocess.run([SATXA, "vag", wav, body], check=True, capture_output=True)
            body = open(body, "rb").read()
            st["center"] += round(12 * math.log2(div))
        vab.vags.append(body)
        assert vab.tones[prog][tone]["vag"] == 1, ("slot taken", src, prog, tone)
        vab.tones[prog][tone] = dict(st, prog=prog, vag=len(vab.vags))
        pair = dict(sv.tones[prog][tone + 1]) if tone < 15 else None
        if PS1_PAIRS and pair and pair["vag"] == sv.tones[prog][tone]["vag"] and pair["vol"]:
            if div > 1:
                pair["center"] += round(12 * math.log2(div))
            assert vab.tones[prog][tone + 1]["vag"] == 1, ("slot taken", src, prog, tone + 1)
            vab.tones[prog][tone + 1] = dict(pair, prog=prog, vag=len(vab.vags))
    for name, src, prog, tone, orig, ps1 in COPIES:
        sv = Vab(open(os.path.join(ROOT, src + ".VH"), "rb").read(),
                 open(os.path.join(ROOT, src + ".VB"), "rb").read())
        st = sv.tones[prog][tone]
        o, e = cfg[orig - 0x600], cfg[ps1 - 0x600]
        assert e["vabid"] == 3 and o["vabid"] != 3, (name, hex(ps1))
        vab.vags.append(sv.vags[st["vag"] - 1])
        # same pitch at the new id's note; same loudness at its volume
        t = dict(st, prog=e["prog"], vag=len(vab.vags),
                 center=st["center"] + e["note"] - o["note"],
                 vol=min(127, round(st["vol"] * o["volume"] / e["volume"])))
        assert vab.tones[e["prog"]][e["tone"]]["vag"] == 1, ("slot taken", name)
        vab.tones[e["prog"]][e["tone"]] = t
    for name, sat, ps1 in SOUNDS:
        e = cfg[ps1 - 0x600]
        assert e["vabid"] == 3, (name, hex(ps1))
        assert e["unk6"] == SATURN_PRIORITY.get(name, 50), (name, "priority")
        wav = render(sat, 44100 if name in UNCAPPED else cap)
        # the id's table volume scales it: undo that, as far as the headroom
        # allows, so the Saturn's levels stay as rendered
        with wave.open(wav) as w:
            rate, raw = w.getframerate(), w.readframes(w.getnframes())
        pcm = struct.unpack("<%dh" % (len(raw) // 2), raw)
        peak = max(1, max(abs(v) for v in pcm))
        gain = min(127.0 / e["volume"], 32000.0 / peak)
        if gain > 1.0:
            wav = os.path.join(WORK, "%s_%d_vol.wav" % (name, cap))
            write_wav(wav, [int(round(v * gain)) for v in pcm], rate)
        # the id plays its note: the centre note makes that the WAV's rate
        c, f = centre(e["note"], wav_rate(wav))
        # one-shot: full sustain, a short release
        place((e["prog"], e["tone"]), add(name, wav), c, f, (0x80FF, 0x5FC0))
    vh, vb = vab.write()
    if len(vb) > VB_LIMIT:
        print("%s sounds: rate cap %d gives %d bytes, over %d" % (LABEL, cap, len(vb), VB_LIMIT))
        return None
    open(OUT + ".VH", "wb").write(vh)
    open(OUT + ".VB", "wb").write(vb)
    with open(HEADER, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gen_%s_sounds.py, do not edit\n" % LABEL)
        f.write("// %s\n" % DESCRIPTION)
        for name, sat, ps1 in SOUNDS:
            f.write("#define MOD_SFX_%s 0x%X // Saturn 0x%X\n" % (name, ps1, sat))
        for name, src, prog, tone, orig, ps1 in COPIES:
            f.write("#define MOD_SFX_%s 0x%X // 0x%X at a higher priority\n" % (name, ps1, orig))
    print("%s sounds: %d + %d instruments, rate cap %d, bank %d bytes (limit %d)" % (
        LABEL, len(SOUNDS), len(INSTRUMENTS), cap, len(vb), VB_LIMIT))
    return len(vb)


def instrument(crt, voice):
    import saturn_sfx  # mods/sotn-ultimate/tools/saturn_sfx.py
    layer = saturn_sfx.crt(crt).tones.voices[voice][1][0]
    samples = [int(max(-32768, min(32767, round(v * 32767)))) for v in layer.samples]
    return samples, 44100, layer.base, layer.fine


def render(sid, cap):
    import saturn_sfx
    path = os.path.join(WORK, "gdn_%X_%d.wav" % (sid, cap))
    return saturn_sfx.render_to_wav(sid, path=path, max_rate=cap)


if __name__ == "__main__":
    for cap in RATE_CAPS:
        if build(render, instrument, cap) is not None:
            break
    else:
        raise SystemExit("the Garden's sounds don't fit its bank at any rate cap")
