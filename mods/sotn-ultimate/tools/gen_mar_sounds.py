#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Saturn Maria's own voice and sounds (BIN/MARIA_SD.BIN).

Where they are on the Saturn
----------------------------
  SD/SD_MARI.CRT, her player bank (Richter's is SD_LIH.CRT): 20 SEQs played
    as Saturn ids 0x71C-0x72F (0.BIN sound table type 1, index 0-19; the
    player table belongs to whichever player bank is loaded, so her code's
    0x71A / 0x71B are its SEQs 10 / 11 = 0x726 / 0x727 at priority 40).
    0x71C-0x721 are her voice (its tones 0-5, 8-bit 11025 Hz); 0x722-0x72F
    her attacks' and specials' sounds on common-bank (SD_JO10) instruments,
    plus 0x72C (Seiryuu) on her own voice tones 4/5 two octaves down.
  SD/SDE7.PCM, her voice clips (Japanese): Saturn ids 0x5CA-0x5D8 = sound
    commands 0x1701-0x170F (0.BIN 0x0604411A), group 0x17 -> the clip table
    at 0x0603A8B4 (offset, length, rate 11025, mono 8-bit) in SDE7.PCM,
    played on the PCM stream channel. 0x5CB and 0x5CD are never played.

Which plays where (MARIA.PRG, the Saturn Richter fork it was made from):
  0x71C-0x71E shot grunts; 0x71D also slide kick and high jump; 0x71F kick
  and blade dash; 0x721 death; 0x5CA dive kick; 0x5CC / 5D3 / 5D2 / 5D1 /
  5D0 the names of specials 0-4 (060A9F84), 0x5CE Four Beasts (the beasts
  entity 060AE3C0, second beast), 0x5CF the cue at a fixed frame of
  Genbu / Byakko / Seiryuu / Suzaku; hurt 0x5D4 / 5D6 / 5D7 (060A75AC), held
  by a boss 0x5D4 / 5D5 / 5D8 (060A7DF4).
  None of her SEQs is any common-bank SEQ (compared note by note), and her
  voice is her own: no PS1 sound stands for them (the user rule, keep the PS1
  sound when the Saturn's is the same recording, applies to the common-bank
  ids her code plays, 0x617, 0x644, 0x64B..., which stay PS1 ids, and to the
  hit sound below).

On the PS1
----------
In Richter mode DRA loads VAB/SD_RIH (vab 1) and VAB/SD_MAR (vab 2, the
prologue's Maria) after the player overlay. For Maria, src/dra/cd.c moves
those four CdFiles onto BIN/MARIA_SD.BIN, which holds her two banks laid out
as those four files are (VH 3 sectors, VB 29, VH 2, VB): one offset for all.
Her sounds take the slots of the PS1 ids that name vab 1 / vab 2 (Richter's
and Alucard's voice ids; the prologue Maria's and the familiars' ids, unused
in Maria mode), so Richter's code paths play her voice with no code change
(ATTACK_A-C = her grunts, DEATH = her death cry, SLIDE_SKID = her 0x71A...);
her own code uses the MAR_SFX_* ids of the generated
build/us/mod/mar/mar_sfx.h. Stage code plays SFX_RIC_WHIP_HIT when a non-
Alucard player hits an enemy: for her that slot holds the PS1
SFX_WEAPON_STAB_B, the common sound the Saturn plays for her shot's hits.

The PSP dub (gen_mar_sounds.py --psp -> BIN/MARIA_SD_PSP.BIN, same layout
and slots): her voice lines with the PSP Maria's English clips
(res/ps/snd_e/SE/sd_mar2.spk, 8 kHz) where the PSP has the same line, by
where the PSP plays each (src/maria): attack grunts MARY2A-C, attack yell
MARY2D (kick), dive kick MARY2E, the crash calls MARY2Q / P / O / N
(Turtle / Cat / Dragon / Cardinal = Genbu / Byakko / Seiryuu / Suzaku), the
summon cue MARY2M, pain MARY2R / T / V, death MARY2W. Guardian Knuckle's
and Four Beasts' names have no PSP line: they stay the Saturn's. The JP dub
keeps MARIA_SD.BIN (src/dra/cd.c).

Her select panel has no PS1 dub: BIN/MARIA_SD_PS1.BIN (which src/dra/cd.c
loads for the PS1 dub) is a copy of MARIA_SD_PSP.BIN (mods/sotn-ultimate/tools/build_disc.sh).

Limits: sound RAM 0x42CC0-0x50E50 for vab 1 (0xE190 = vab1Len) and
0x50E50-0x60A40 for vab 2 (0xFBF0 = vab2Len); DRA transfers exactly those
sizes, so each VB is padded to it (an unused VAG). The VHs keep SD_RIH's and
SD_MAR's sizes (3 and 2 programs). The banks don't hold everything at the
Saturn's rates, so the sound effects are rendered at lower rate caps until
both fit (STEPS); her voice stays at 11025 Hz unless noted.

Since 2026-10-23 (the user's picks): the sound effects start at their full
Saturn rate (STEPS). Four Saturn sounds left her banks for always-loaded
common-bank PS1 sounds her code plays instead (src/ric): the spark 0x724 ->
SFX_THUNDER_B, the beasts' 0x729 -> SFX_NOISE_SWEEP_DOWN_A, Suzaku's 0x72D ->
SFX_ANIME_SWORD_A, and Byakko's fire 0x72A (never played: the PS1
SFX_ELECTRICITY stands for it) is gone; Genbu moved to vab 2. Seiryuu's 11 s
(0x72C) is a loop ("loop": LOOP_CUT of its render, the part the user cut;
LOOP_START samples of intro, then a loop with LOOP_XFADE baked into its end
so the jump back is seamless), played on channels 22/23 (src/dra/cd.c) and
stopped by the dragon; the dragon roars ("roar": ROAR_CUT of the same render,
faded at both ends) 2-3 times on top. Both cuts were the user's Audacity
edits (10-23) and are now recipes: the builder makes them from the player's
Saturn disc.
"""
import math
import os
import struct
import subprocess
import sys
import wave

import numpy as np
import yaml

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vab import Vab  # noqa: E402
import saturn_sfx as S  # noqa: E402
import gen_gdn_sounds as G  # noqa: E402
from mod_work import work_dir  # noqa: E402

OUT = os.path.join(ROOT, "build/us/MARIA_SD.BIN")
OUT_PSP = os.path.join(ROOT, "build/us/MARIA_SD_PSP.BIN")
PSP_SPK = os.path.join(ROOT, "disks/psp/snd_e/SE/sd_mar2.spk")
HEADER = os.path.join(ROOT, "build/us/mod/mar/mar_sfx.h")
WORK = work_dir("sfx", "mar")
SATXA = os.path.join(ROOT, "bin/satxa")
TEMPLATE = {1: "disks/us/VAB/SD_RIH", 2: "disks/us/VAB/SD_MAR"}
VH_LEN = {1: 0x1020, 2: 0xE20}
VB_LEN = {1: 0xE190, 2: 0xFBF0}
# the retail files' sectors: SD_RIH.VH 3, SD_RIH.VB 29, SD_MAR.VH 2
SECTOR = 2048
LAYOUT_SECTORS = (3, 29, 2)

# her voice clips: SDE7.PCM through 0.BIN's clip table for group 0x17
ZERO_BASE = 0x06004080
VOX_TABLE = 0x0603A8B4 + 8
VOX_FIRST = 0x5CA
VOX_GAIN = 0.5  # as a full-scale tone through the synth (MIX_GAIN)
VOX_TRIM = 3  # silence threshold (of 127) for the clips' ends

# (name, bank, PS1 id whose slot it fills, kind, source, class)
#  kind "seq": a SD_MARI.CRT SEQ (Saturn id) rendered by saturn_sfx.py
#  kind "vox": a SDE7.PCM clip (Saturn id 0x5CA-0x5D8)
#  kind "ps1": (VAB, PS1 id) a PS1 tone copied (with its pair voice)
#  class: "voice" native rate, "spell" voice (STEPS' spell rate), "sfx" and
#  "seiryu" capped by STEPS
SOUNDS = [
    # vab 1: Richter's (0x6F9-0x708) and Alucard's (0x6F4-0x6F8) voice ids
    ("ATK_A", 1, 0x6F9, "seq", 0x71C, "voice"),  # SFX_VO_RIC_ATTACK_A
    ("ATK_B", 1, 0x6FA, "seq", 0x71D, "voice"),  # SFX_VO_RIC_ATTACK_B
    ("ATK_C", 1, 0x6FB, "seq", 0x71E, "voice"),  # SFX_VO_RIC_ATTACK_C
    ("KICK", 1, 0x6FC, "seq", 0x71F, "voice"),  # SFX_VO_RIC_ATTACK_YELL
    ("SHOT", 1, 0x6FD, "seq", 0x728, "sfx"),  # SFX_UNUSED_6FD
    ("FIRE", 1, 0x6FE, "seq", 0x722, "sfx"),  # SFX_UNUSED_6FE
    ("DEATH", 1, 0x6FF, "seq", 0x721, "voice"),  # SFX_VO_RIC_DEATH
    ("BALL", 1, 0x700, "seq", 0x723, "sfx"),  # SFX_VO_RIC_HYDRO_STORM
    ("HURT_A", 1, 0x702, "vox", 0x5D4, "voice"),  # SFX_VO_RIC_PAIN_B
    ("HURT_B", 1, 0x703, "vox", 0x5D6, "voice"),  # SFX_VO_RIC_PAIN_C
    ("HURT_C", 1, 0x704, "vox", 0x5D7, "voice"),  # SFX_VO_RIC_PAIN_D
    ("HIT", 1, 0x705, "ps1", ("disks/us/SD_J010", 0x62E), "sfx"),  # WHIP_HIT
    # Guardian Knuckle: an electric buzz (the common SFX_ELECTRICITY tone 9
    # semitones up, 41.6 kHz) and two thuds: its buzz needs a high rate
    ("LUNGE", 1, 0x6F4, "seq", 0x725, "buzz"),  # SFX_VO_ALU_SOUL_STEAL
    ("BEASTS_END", 1, 0x6F5, "seq", 0x727, "sfx"),  # SFX_UNUSED_6F5
    ("GRAB_C", 1, 0x6F6, "vox", 0x5D8, "voice"),  # SFX_VO_ALU_DEATH
    ("FLAP", 1, 0x6F7, "seq", 0x72E, "sfx"),  # SFX_ALU_WOLF_BARK
    ("SKID", 1, 0x707, "seq", 0x726, "sfx"),  # SFX_RIC_SLIDE_SKID (her 0x71A)
    # vab 2: the prologue Maria's ids and the familiars' (priority 90)
    ("DIVE", 2, 0x87D, "vox", 0x5CA, "voice"),  # SFX_MARIA_RICHTER
    ("FOUR", 2, 0x87E, "vox", 0x5CE, "spell"),  # ..._GRANT_HIM_YOUR_STRENGTH
    ("CUE", 2, 0x87F, "vox", 0x5CF, "voice"),  # ..._AIM_FOR_HIS_HEAD
    # the specials' names, consecutive ids (special n plays MAR_SFX_SPELL + n)
    ("SPELL", 2, 0x8A1, "vox", 0x5CC, "spell"),  # Guardian Knuckle
    ("SPELL_1", 2, 0x8A2, "vox", 0x5D3, "spell"),  # Genbu
    ("SPELL_2", 2, 0x8A3, "vox", 0x5D2, "spell"),  # Byakko
    ("SPELL_3", 2, 0x8A4, "vox", 0x5D1, "spell"),  # Seiryuu
    ("SPELL_4", 2, 0x8A5, "vox", 0x5D0, "spell"),  # Suzaku
    ("GRAB_B", 2, 0x880, "vox", 0x5D5, "voice"),  # ..._MONSTER_SILHOUETTE
    # (2.1 s; in Byakko's fire's old slot)
    ("GENBU", 2, 0x887, "seq", 0x72F, "long"),  # SFX_FAERIE_REGENERATION
    ("BYAKKO_FLAME", 2, 0x888, "seq", 0x72B, "sfx"),  # SFX_FAERIE_NO_MEDICINE
    ("SEIRYU", 2, 0x889, "loop", 0x72C, "seiryu"),  # SFX_FAERIE_HAMMER_A
    # the dragon's roar (not the Saturn's), in Suzaku's old slot
    ("ROAR", 2, 0x88A, "roar", 0x72C, "roar"),  # SFX_FAERIE_HAMMER_B
]
# Seiryuu's loop (see the top): 4912 Hz, the Saturn's top rate for 0x72C
# the 0x72C render at SEIRYU_RATE, cut: the loop's stored part (3.208 s, no
# roar in it) and the roar (3.079-5.137 s; it starts and ends mid-wave, so
# ROAR_FADE raised-cosine fades: a click otherwise at its start and end)
SEIRYU_RATE = 4912
LOOP_CUT = (0, 15756)
LOOP_START = 9156  # 1.864 s, a whole number of ADPCM blocks
LOOP_XFADE = 0.15  # s
ROAR_CUT = (15122, 15122 + 10111)
ROAR_FADE = (0.010, 0.080)  # s in, s out
# the PSP dub's English line for each of her voice sounds (see the top)
PSP_VOICE = {
    "ATK_A": "MARY2A", "ATK_B": "MARY2B", "ATK_C": "MARY2C",
    "KICK": "MARY2D", "DEATH": "MARY2W", "DIVE": "MARY2E", "CUE": "MARY2M",
    "HURT_A": "MARY2R", "HURT_B": "MARY2T", "HURT_C": "MARY2V",
    "GRAB_B": "MARY2T", "GRAB_C": "MARY2V",
    "SPELL_1": "MARY2Q", "SPELL_2": "MARY2P", "SPELL_3": "MARY2O",
    "SPELL_4": "MARY2N",
}
# set by --psp
PSP_CLIPS = None
# rate caps tried in order until each bank fits, by class: "sfx" her
# attacks' sounds, "buzz" the Knuckle buzz (it plays at 41625 Hz), "long"
# Genbu, "spell" the specials' names, "seiryu" 0x72C if rendered (its notes
# play at 819-4911 Hz), "roar" the dragon roar (98% of its energy is under
# 624 Hz: it goes down first)
FULL = {"sfx": 44100, "buzz": 44100, "long": 44100, "spell": 11025,
        "seiryu": 4912, "roar": 3500}
STEPS = [dict(FULL, roar=r) for r in (3500, 3200, 3000, 2756, 2500)]
# then the two names the PSP has no line for (the Saturn's; the PSP
# English voice around them is 8 kHz)
STEPS += [dict(FULL, roar=2500, spell=n) for n in (10000, 9000, 8000)]
STEPS += [dict(FULL, roar=2500, spell=8000, sfx=f, long=f) for f in (22050, 16000, 11025)]
STEPS += [dict(FULL, roar=2500, spell=8000, sfx=11025, long=11025, buzz=b)
          for b in (22050, 14000)]
STEPS += [dict(spell=n, seiryu=r, sfx=f, long=min(f, 5500), buzz=14000, roar=2205)
          for f, r, n in ((9000, 4000, 11025), (8000, 3300, 11025),
                          (7000, 3300, 11025), (6000, 3300, 11025),
                          (6000, 2756, 10000), (6000, 2756, 9000),
                          (5500, 2756, 8000))]
# her voice's peak (-1 dB, as Richter's PS1 voices)
VOICE_PEAK = 29200
# her attacks' sounds (class "sfx") as the voice: the synth's half-scale
# mix (saturn_sfx.MIX_GAIN) left them 6 dB under the Saturn's balance with
# her voice (the user's pick, 2026-11-07), as far as the headroom allows
SFX_GAIN = 2.0
# one-shot: full sustain, a short release (as gen_gdn_sounds.py)
ADSR = (0x80FF, 0x5FC0)


def u32(d, o):
    return struct.unpack_from(">I", d, o)[0]


def load_vab(path):
    return Vab(open(os.path.join(ROOT, path + ".VH"), "rb").read(),
               open(os.path.join(ROOT, path + ".VB"), "rb").read())


def vox_clip(sid):
    """a SDE7.PCM clip -> (int16 samples, rate)"""
    z = open(os.path.join(S.SAT, "0.BIN"), "rb").read()
    off, ln, rate, flags = struct.unpack_from(
        ">IIHH", z, VOX_TABLE - ZERO_BASE + 12 * (sid - VOX_FIRST))
    assert flags == 1 and rate == 11025, (hex(sid), flags, rate)
    raw = open(os.path.join(S.SD, "SDE7.PCM"), "rb").read()[off:off + ln]
    s = np.frombuffer(raw, dtype=np.int8).astype(np.float64)
    nz = np.nonzero(np.abs(s) > VOX_TRIM)[0]
    s = s[nz[0]:nz[-1] + 1]
    return [int(round(v * 256 * VOX_GAIN)) for v in s], rate


def wav_samples(path):
    with wave.open(path) as w:
        rate, raw = w.getframerate(), w.readframes(w.getnframes())
        ch = w.getnchannels()
    pcm = np.frombuffer(raw, dtype="<i2").astype(np.float64)
    if ch == 2:
        pcm = pcm.reshape(-1, 2).mean(axis=1)
    return [int(v) for v in pcm], rate


def psp_clip(name):
    """a sd_mar2.spk clip -> (int16 samples, rate), its silent ends cut"""
    import gen_psp_grunts as P
    rate, body = PSP_CLIPS[name]
    x = P.trim(G.decode_vag(body), rate)
    return [int(round(v)) for v in x], rate


_seiryu = {}


def seiryu_render(ctx):
    """the 0x72C render (int16 list) at SEIRYU_RATE, as the "seq" sounds"""
    if not _seiryu:
        x, rate, info = S.render(0x72C, ctx, max_rate=SEIRYU_RATE)
        assert not info.get("empty") and rate == SEIRYU_RATE, (rate, info.get("empty"))
        x = S.trim(x)
        if x.ndim == 2:
            x = x.mean(axis=1)
        pcm, _, _ = S.to_pcm16(x)
        _seiryu["pcm"] = [int(v) for v in pcm]
    return _seiryu["pcm"]


def roar_samples(ctx):
    """the dragon's roar: ROAR_CUT of the 0x72C render, faded in and out"""
    x = np.array(seiryu_render(ctx)[ROAR_CUT[0]:ROAR_CUT[1]], dtype=np.float64)
    assert len(x) == ROAR_CUT[1] - ROAR_CUT[0]
    fin, fout = int(ROAR_FADE[0] * SEIRYU_RATE), int(ROAR_FADE[1] * SEIRYU_RATE)
    x[:fin] *= 0.5 - 0.5 * np.cos(np.pi * np.arange(fin) / fin)
    x[-fout:] *= 0.5 + 0.5 * np.cos(np.pi * (np.arange(fout) + 1) / fout)
    return [int(round(v)) for v in x], SEIRYU_RATE


def loop_samples(ctx):
    """Seiryuu's stored part: the intro and whole blocks of loop, the loop's
    last LOOP_XFADE s faded into what comes just before its start"""
    pcm, rate = seiryu_render(ctx)[LOOP_CUT[0]:LOOP_CUT[1]], SEIRYU_RATE
    s, b = LOOP_START, 28
    end = s + (len(pcm) - s) // b * b
    x = np.array(pcm[:end], dtype=np.float64)
    c = int(LOOP_XFADE * rate) // b * b
    fade = np.linspace(0, 1, c)
    x[end - c:end] = x[end - c:end] * (1 - fade) + x[s - c:s] * fade
    return [int(round(v)) for v in x], rate


def set_loop(vag, start):
    """loop flags on a satxa VAG (a zero block, the data blocks, an end
    block): block flags 6 at the loop start, 2 inside, 3 at the last"""
    assert start % 28 == 0
    v = bytearray(vag)
    n = len(v) // 16 - 2  # the data blocks
    first = 1 + start // 28
    for k in range(1, n + 1):
        v[k * 16 + 1] = 0 if k < first else 6 if k == first else 2
    v[n * 16 + 1] = 3
    return bytes(v)


def source_wav(name, kind, src, cap, ctx):
    """the sound as a mono 16-bit WAV at <= cap Hz: its path"""
    os.makedirs(WORK, exist_ok=True)
    if kind == "psp":
        pcm, rate = psp_clip(src)
        if rate > cap:
            pcm, rate = G.resample(pcm, rate, cap)
        name = "psp_" + name
    elif kind == "loop":
        pcm, rate = loop_samples(ctx)
    elif kind == "roar":
        pcm, rate = roar_samples(ctx)
        if rate > cap:
            pcm, rate = G.resample(pcm, rate, cap)
    elif kind == "file":
        pcm, rate = wav_samples(src)
        if rate > cap:
            pcm, rate = G.resample(pcm, rate, cap)
    elif kind == "seq":
        x, rate, info = S.render(src, ctx, max_rate=cap)
        assert not info.get("empty"), hex(src)
        x = S.trim(x)
        if x.ndim == 2:
            x = x.mean(axis=1)
        pcm, _, _ = S.to_pcm16(x)
        pcm = [int(v) for v in pcm]
    else:
        pcm, rate = vox_clip(src)
        if rate > cap:
            pcm, rate = G.resample(pcm, rate, cap)
    path = os.path.join(WORK, "%s_%d.wav" % (name, cap))
    G.write_wav(path, pcm, rate)
    return path


def build_bank(bank, caps, cfg, ctx):
    """-> (vh, vb) of one bank, vb unpadded"""
    vab = load_vab(TEMPLATE[bank])
    template = dict(vab.tones[0][0])
    blank = dict(next(t for p in vab.tones for t in vab.tones[p] if t["vol"] == 0))
    for p in vab.tones:
        vab.tones[p] = [dict(blank, prog=p, vag=1) for _ in range(16)]
    # silence that ends (see gen_gdn_sounds.py: a looping blank keeps the
    # channel group taken)
    vab.vags = [bytes(16) + bytes([0, 1]) + bytes(14) + bytes([0, 7]) + bytes([0x77] * 14)]

    def take(prog, tone):
        assert vab.tones[prog][tone]["vag"] == 1, ("slot taken", bank, prog, tone)

    # one sample for slots that play the same clip at the same level (the
    # PSP dub's pain clips: hurt and boss grab); the tone sets each one's note
    shared = {}
    loops = set()  # the looping samples' vag numbers

    for name, b, ps1, kind, src, cls in SOUNDS:
        if b != bank:
            continue
        if PSP_CLIPS and name in PSP_VOICE:
            kind, src = "psp", PSP_VOICE[name]
        e = cfg[ps1 - 0x600]
        assert e["vabid"] == bank, (name, hex(ps1), e)
        prog, tone = e["prog"], e["tone"]
        take(prog, tone)
        if kind == "ps1":
            path, orig = src
            sv = load_vab(path)
            o = cfg[orig - 0x600]
            assert o["vabid"] == 0, (name, o)
            st = sv.tones[o["prog"]][o["tone"]]
            vab.vags.append(sv.vags[st["vag"] - 1])
            for k in (0, 1):  # the tone and its pair voice
                t = sv.tones[o["prog"]][o["tone"] + k]
                if k and (t["vag"] != st["vag"] or not t["vol"]):
                    break
                take(prog, tone + k)
                vab.tones[prog][tone + k] = dict(
                    t, prog=prog, vag=len(vab.vags),
                    center=t["center"] + e["note"] - o["note"],
                    vol=min(127, round(t["vol"] * o["volume"] / e["volume"])))
            continue
        cap = caps[cls]
        key = (kind, src, cls, cap, e["volume"])
        if kind == "psp" and key in shared:
            vag, rate = shared[key]
            c, f = G.centre(e["note"], rate)
            assert 0 <= c <= 127, (name, "centre note", c)
            vab.tones[prog][tone] = dict(
                template, prog=prog, vag=vag, vol=127, pan=64,
                center=c, shift=f, min=0, max=127, adsr1=ADSR[0], adsr2=ADSR[1])
            continue
        wav = source_wav(name, kind, src, cap, ctx)
        pcm, rate = wav_samples(wav)
        # the id's table volume scales it: undo that, as far as the
        # headroom allows (as gen_gdn_sounds.py)
        peak = max(1, max(abs(v) for v in pcm))
        gain = min(127.0 / e["volume"], 32000.0 / peak)
        if cls == "sfx":
            gain = min(SFX_GAIN * 127.0 / e["volume"], 32000.0 / peak)
        if cls in ("voice", "spell"):
            # her voice at Richter's PS1 voice level: his SD_RIH voices
            # peak at -1 dB (-13..-19 dB RMS), as does the prologue Maria's
            # SD_MAR. The synth's half-scale mix (and VOX_GAIN) left hers
            # 5-7 dB lower; the Saturn's clip table has no level of its own
            gain = min(VOICE_PEAK / peak * 127.0 / e["volume"], 32000.0 / peak)
        if gain > 1.0:
            wav = wav[:-4] + "_vol.wav"
            G.write_wav(wav, [int(round(v * gain)) for v in pcm], rate)
        body = os.path.join(WORK, "%s.vag" % name)
        subprocess.run([SATXA, "vag", wav, body], check=True, capture_output=True)
        vag = open(body, "rb").read()
        if kind == "loop":
            vag = set_loop(vag, LOOP_START)
            loops.add(len(vab.vags) + 1)
        vab.vags.append(vag)
        shared[key] = (len(vab.vags), rate)
        c, f = G.centre(e["note"], rate)
        assert 0 <= c <= 127, (name, "centre note", c)
        vab.tones[prog][tone] = dict(
            template, prog=prog, vag=len(vab.vags), vol=127, pan=64,
            center=c, shift=f, min=0, max=127, adsr1=ADSR[0], adsr2=ADSR[1])
    # the pair voice. The game plays a sound on its tone and the next one
    # (src/dra/sound.c func_80132A04) and frees its channel group only once both
    # voices stop. Left as the blank, whose centre note (the template's) is far
    # above her notes, that voice never stopped: her grunts and hurt sounds held
    # the groups at priority 90 and the stage's sounds stayed silent until the
    # next stage. So the pair voice is the sound itself at volume 0, ending with
    # it (the retail banks pair a tone with its copy). Not the loop's: Seiryuu
    # plays on channels 22/23, which her dragon releases
    for p, tl in vab.tones.items():
        real = [t["vag"] > 1 and t["vag"] not in loops for t in tl]
        for t in range(15):
            if real[t] and tl[t + 1]["vag"] == 1:
                tl[t + 1] = dict(tl[t], vol=0)
    return vab


def finish(vab, bank):
    """pad the VB to the size DRA transfers (an unused VAG); -> (vh, vb)"""
    vh, vb = vab.write()
    if len(vb) > VB_LEN[bank]:
        return None
    if len(vb) < VB_LEN[bank]:
        vab.vags.append(bytes(VB_LEN[bank] - len(vb)))
        vh, vb = vab.write()
    assert len(vh) == VH_LEN[bank] and len(vb) == VB_LEN[bank], (bank, len(vh), len(vb))
    return vh, vb


def pad(b, sectors=None):
    n = sectors * SECTOR if sectors else -(-len(b) // SECTOR) * SECTOR
    assert len(b) <= n
    return b + bytes(n - len(b))


def main():
    global PSP_CLIPS
    out = OUT
    if "--psp" in sys.argv[1:]:
        import gen_psp_grunts as P
        PSP_CLIPS = {n: (r, b) for n, r, b in P.spk_entries(PSP_SPK)}
        missing = set(PSP_VOICE.values()) - set(PSP_CLIPS)
        assert not missing, missing
        out = OUT_PSP
    cfg = yaml.safe_load(open(os.path.join(ROOT, "assets/dra/us/sfx_config.yaml")))
    ctx = S.Context(player="SD_MARI.CRT")
    banks = {}
    used_caps = {}
    for bank in (1, 2):
        classes = sorted({e[5] for e in SOUNDS if e[1] == bank} - {"voice"})
        tried = set()
        for step in STEPS:
            mine = tuple(step[c] for c in classes)
            if mine in tried:
                continue  # the same caps for this bank's sounds
            tried.add(mine)
            caps = dict(step, voice=44100)
            vab = build_bank(bank, caps, cfg, ctx)
            used = len(vab.write()[1])
            what = ", ".join("%s %d" % (c, step[c]) for c in classes)
            print("maria sounds: vab %d, caps %s: %d of %d bytes" % (
                bank, what, used, VB_LEN[bank]))
            banks[bank] = finish(vab, bank)
            if banks[bank]:
                used_caps[bank] = what
                break
        else:
            raise SystemExit("Maria's vab %d doesn't fit at any rate cap" % bank)
    (vh1, vb1), (vh2, vb2) = banks[1], banks[2]
    data = (pad(vh1, LAYOUT_SECTORS[0]) + pad(vb1, LAYOUT_SECTORS[1]) +
            pad(vh2, LAYOUT_SECTORS[2]) + pad(vb2))
    open(out, "wb").write(data)
    if PSP_CLIPS:
        # the same slots and ids as MARIA_SD.BIN: mar_sfx.h is its run's
        print("maria sounds: %s %d bytes" % (os.path.relpath(out, ROOT), len(data)))
        return
    os.makedirs(os.path.dirname(HEADER), exist_ok=True)
    with open(HEADER, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gen_mar_sounds.py, do not edit\n")
        f.write("// the Saturn Maria's sounds: PS1 ids whose vab 1 / vab 2 slots\n")
        f.write("// hold them in BIN/MARIA_SD.BIN\n")
        for bank in (1, 2):
            f.write("// vab %d rate caps (Hz): %s\n" % (bank, used_caps[bank]))
        for name, b, ps1, kind, src, cls in SOUNDS:
            if kind == "roar":
                what = "Saturn 0x%X %.3f-%.3f s, faded" % (
                    src, ROAR_CUT[0] / SEIRYU_RATE, ROAR_CUT[1] / SEIRYU_RATE)
            elif kind == "loop":
                what = "Saturn 0x%X, a loop" % src
                f.write("#define MAR_SFX_%s_LOOP 1\n" % name)
            else:
                what = ("Saturn 0x%X" % src) if kind != "ps1" else ("PS1 0x%X" % src[1])
            f.write("#define MAR_SFX_%s 0x%X // %s\n" % (name, ps1, what))
    print("maria sounds: %s %d bytes" % (os.path.relpath(OUT, ROOT), len(data)))


if __name__ == "__main__":
    main()
