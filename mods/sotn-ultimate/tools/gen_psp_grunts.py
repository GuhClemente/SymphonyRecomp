#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the PSP dub's player grunts (BIN/ALK_PSP.BIN, BIN/RIH_PSP.BIN).

Alucard's and Richter's grunts (attack yells, pain, death, Hydro Storm, Dark
Metamorphosis, Soul Steal, "What?") are not XA: they are samples in the
player's VAB (vab 1): VAB/SD_ALK (DRA's Alucard load) and VAB/SD_RIH (RIC.BIN's
load: Richter, the costume, the prologue). The PSP re-dubbed them in
res/ps/snd_e/SE/sd_alk.spk / sd_rih.spk (SEPK packs of VAGp, one entry per
VAB tone, both tones of each PS1 tone pair; the PS1 banks' silent placeholder
tones left out). Here each retail voiced tone pair gets the PSP clip of its
slot (8 kHz, silence trimmed, a global gain to the retail loudness); every
other tone and sample (the wolf bark, the whip, the slide, Hydro Storm's
rain, the silent slots) stays the retail one. Same programs/tones/volumes;
only the sample and the centre note (to play it at its own rate) change. The
VB keeps the retail length exactly (an unused padding sample), so the bank
takes the same SPU RAM and SsVabTransBodyPartly ends as before.

ALK_PSP.BIN = VH (3 sectors) + VB, laid out as VAB/SD_ALK.VH/VB on the disc;
RIH_PSP.BIN = VH (3) + VB (29) + SD_MAR.VH (2) + SD_MAR.VB (the prologue Maria,
vab 2, with her PSP clips: sd_mar.spk through PS.ELF's tone list,
gen_psp_sfx.build_bank), laid out as VAB/SD_RIH.VH..SD_MAR.VB (src/dra/cd.c moves those CdFiles by one offset,
as for MARIA_SD.BIN), when MOD_PSP_DUB_ON().

Usage: gen_psp_grunts.py [--off] [--set psp|jp]
  --off: the retail banks, for MOD_PSP_DUB=0
  --set jp: the JP dub's (include/game.h MOD_JP_DUB_ON): the same slots
  from the Japanese packs (res/ps/sound/SE; their names differ in places,
  e.g. RIH LIHK1A/LIHK1a for the English LIHK1a/LIHK1B, so only the pairs
  are checked), build/us/ALK_JP.BIN / RIH_JP.BIN; RIH_JP's SD_MAR (the
  prologue Maria, vab 2) gets her Japanese clips too (sd_mar.spk through
  PS.ELF's tone list, gen_psp_sfx.build_bank). mods/sotn-ultimate/tools/gen_jp_sd.py puts
  them in BIN/JP_SD.BIN.
"""
import math
import os
import struct
import subprocess
import sys

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vab import Vab  # noqa: E402
import gen_gdn_sounds as G  # noqa: E402

SPK = os.path.join(ROOT, "disks/psp/snd_e/SE")
SPK_JP = os.path.join(ROOT, "disks/psp/sound/SE")
WORK = os.path.join(ROOT, "build/us/psp_grunts")
SECTOR = 2048
RATE = 8000  # the PSP clips' rate
# (prog, tone) of the retail voiced tone pair -> (spk entry, PSP name)
BANKS = {
    "ALK": ("disks/us/VAB/SD_ALK", "sd_alk.spk", {
        (0, 0): (0, "ai_dm1"),     # 6E7 SFX_VO_ALU_PAIN_A
        (0, 2): (2, "ai_dm2"),     # 6E8 PAIN_B
        (0, 4): (4, "ai_dm3"),     # 6E9 PAIN_C
        (0, 6): (6, "ai_dm4"),     # 6EA PAIN_D
        (0, 8): (8, "ai_dm5"),     # 6EB PAIN_E
        # (0, 10) 6EC SFX_VO_ALU_SILENCE: silent on the PS1, kept (PSP ai_dm6)
        (0, 12): (12, "ai_dm7"),   # 6ED YELL
        (0, 14): (14, "al_fun"),   # 6EE ATTACK_A
        (1, 0): (16, "al_ha"),     # 6EF ATTACK_B
        (1, 2): (18, "al_tei"),    # 6F0 ATTACK_C
        (1, 4): (20, "al_tou"),    # 6F1 ATTACK_D
        (1, 6): (22, "ALKO1V"),    # 6F2 WHAT
        (1, 8): (24, "ALKD1I"),    # 6F3 DARK_META
        (1, 10): (26, "ALKD1J"),   # 6F4 SOUL_STEAL
        # (1, 12) 6F5 unused, silent on the PS1, kept (PSP al_ha again)
        (1, 14): (30, "alk2a"),    # 6F6 DEATH
        # (2, 2) 6F7 wolf bark: same sample on both (wlof5), retail kept
        # (2, 6) 6F8 unused "whoa": retail kept (PSP ALKD1N), so all fit at 8 kHz
    }),
    "RIH": ("disks/us/VAB/SD_RIH", "sd_rih.spk", {
        (0, 0): (0, "LIHK1a"),     # 6F9 SFX_VO_RIC_ATTACK_A
        (0, 2): (2, "LIHK1"),      # 6FA ATTACK_B (LIHK1<fullwidth B>)
        (0, 4): (4, "LIHK1C"),     # 6FB ATTACK_C
        (0, 6): (6, "LIHK1D"),     # 6FC ATTACK_YELL
        # (0, 8)/(0, 10) 6FD/6FE unused, silent on the PS1 (PSP LIHK1D again)
        (0, 12): (10, "LIHK1M"),   # 6FF DEATH
        (0, 14): (12, "LIHK1H"),   # 700 HYDRO_STORM
        (1, 0): (14, "LIHK1I"),    # 701 PAIN_A
        (1, 2): (16, "LIHK1J"),    # 702 PAIN_B
        (1, 4): (18, "LIHK1K"),    # 703 PAIN_C
        (1, 6): (20, "LIHK1L"),    # 704 PAIN_D
        # (1, 8) whip hit, (2, 2) whip, (2, 4) slide, (2, 6) rain: retail
    }),
}
MAR = "disks/us/VAB/SD_MAR"
LAYOUT = {"ALK": (3,), "RIH": (3, 29, 2)}  # sectors of VH (, VB, SD_MAR.VH)
GAIN_MAX = 1.0  # never louder than the PSP mix


def jp_pitch(key, p, tone):
    """the JP PS1 tone's pitch for this voice (1/128 semitones, as pitch()):
    the PSP's JP takes are the JP PS1 samples, labelled 8000 Hz but played
    by the JP PS1 at ~7354 Hz (~7569 for SD_ALK program 0). Its VAB's tones
    have the centre 6 above the US ones, and SD_ALK program 0 a shift of 70
    (US 0)"""
    assert tone["shift"] == 0, (key, p, tone)
    return pitch(tone) + 6 * 128 - (70 if key == "ALK" and p == 0 else 0)


def spk_entries(path):
    d = open(path, "rb").read()
    assert d[:8] == b"SEPK1.20", path
    out = []
    for i in range(struct.unpack_from("<I", d, 8)[0]):
        off = struct.unpack_from("<I", d, 0x20 + i * 0x20)[0]
        v = d[off:]
        assert v[:4] == b"VAGp", (path, i)
        size, rate = struct.unpack_from(">II", v, 12)
        name = v[0x20:0x30].split(b"\0")[0].decode("latin1")
        out.append((name, rate, v[0x30:0x30 + size]))
    return out


def active_rms(x, rate):
    """RMS over the 10 ms frames within 30 dB of the loudest"""
    x = np.asarray(x, dtype=np.float64)
    n = max(1, rate // 100)
    fr = [x[i:i + n] for i in range(0, len(x) - n + 1, n)]
    e = np.array([math.sqrt(np.mean(f * f)) for f in fr]) if fr else np.array([0.0])
    act = e[e > e.max() * 10 ** (-30 / 20)]
    return float(math.sqrt(np.mean(act * act))) if len(act) else 0.0


def trim(x, rate, lead=0.01, tail=0.04):
    """cut silence (below -45 dB of the peak) at both ends"""
    x = np.asarray(x, dtype=np.float64)
    a = np.abs(x)
    thr = a.max() * 10 ** (-45 / 20)
    idx = np.nonzero(a > thr)[0]
    if not len(idx):
        return x
    s = max(0, idx[0] - int(lead * rate))
    e = min(len(x), idx[-1] + int(tail * rate))
    y = x[s:e].copy()
    fade = min(len(y), int(0.01 * rate))
    if fade:
        y[-fade:] *= np.linspace(1, 0, fade)
    return y


def pitch(t):
    """the tone's effective centre in 1/128 semitones (the shift raises the
    pitch, so it lowers the note that plays 44100 Hz)"""
    return t["center"] * 128 - t["shift"]


def set_pitch(t, total):
    total = int(total)
    t["center"] = -(-total // 128)
    t["shift"] = t["center"] * 128 - total
    assert 0 <= t["center"] <= 127, t


def encode(name, pcm, rate):
    wav = os.path.join(WORK, name + ".wav")
    body = os.path.join(WORK, name + ".vag")
    G.write_wav(wav, [int(max(-32768, min(32767, round(v)))) for v in pcm], rate)
    subprocess.run([G.SATXA, "vag", wav, body], check=True, capture_output=True)
    return open(body, "rb").read()


def build(key, off=False, jp=False):
    src, spk, table = BANKS[key]
    vh0 = open(os.path.join(ROOT, src + ".VH"), "rb").read()
    vb0 = open(os.path.join(ROOT, src + ".VB"), "rb").read()
    if off:
        print("psp grunts %s: off (retail bank)" % key)
        return vh0, vb0
    vab = Vab(vh0, vb0)
    assert vab.write() == (vh0, vb0), key  # round trip
    entries = spk_entries(os.path.join(SPK_JP if jp else SPK, spk))
    # the new centre plays the clip at its own rate at the note the game
    # plays (sfx_config.yaml): centre = note + 12 log2(44100 / rate)
    import yaml
    cfg = yaml.safe_load(open(os.path.join(ROOT, "assets/dra/us/sfx_config.yaml")))
    notes = {}
    for e in cfg:
        if e["vabid"] == 1:
            notes.setdefault((e["prog"], e["tone"]), set()).add(e["note"])
    clips, ratios = {}, []
    for (p, t), (i, name) in sorted(table.items()):
        n0, r0, b0 = entries[i]
        n1, r1, b1 = entries[i + 1]
        assert (jp or n0.startswith(name)) and n1 == n0 and b1 == b0, (key, p, t, n0, n1)
        assert r0 == RATE, (key, n0, r0)
        tone, pair = vab.tones[p][t], vab.tones[p][t + 1]
        assert tone["vag"] > 1 and tone["vol"] and pair["vag"] == tone["vag"], (key, p, t)
        users = [(q, u) for q in vab.tones for u in range(16)
                 if vab.tones[q][u]["vag"] == tone["vag"]]
        assert users == [(p, t), (p, t + 1)], (key, p, t, users)
        assert len(notes.get((p, t), ())) == 1, (key, p, t, notes.get((p, t)))
        psp = trim(G.decode_vag(b0)[28:], RATE)
        # the retail sample's rate from its tone: note plays at 44100 Hz *
        # 2^((note - centre + shift/128) / 12)
        note = next(iter(notes[(p, t)]))
        r_ps1 = 44100 * 2 ** ((note * 128 - pitch(tone)) / 128 / 12)
        ps1 = G.decode_vag(vab.vags[tone["vag"] - 1])[28:]
        a, b = active_rms(ps1, int(r_ps1)), active_rms(psp, RATE)
        if a and b:
            ratios.append(a / b)
        clips[(p, t)] = (n0, psp, note, jp_pitch(key, p, tone) if jp else None)
    gain = min(GAIN_MAX, float(np.median(ratios)))
    print("  %s loudness PS1/PSP: median %.2f (%s)" % (
        key, np.median(ratios), " ".join("%.2f" % r for r in ratios)))
    kept = sum(len(v) for i, v in enumerate(vab.vags)
               if not any(vab.tones[p][t]["vag"] == i + 1 for p, t in clips))
    budget = len(vb0) - kept
    rate = RATE
    os.makedirs(WORK, exist_ok=True)
    while True:
        bodies = {}
        for (p, t), (name, psp, note, _) in clips.items():
            x = psp * gain
            peak = np.abs(x).max()
            if peak > 32000:
                x *= 32000 / peak
            if rate != RATE:
                x = np.asarray(G.resample(x, RATE, rate)[0], dtype=np.float64)
            bodies[(p, t)] = encode("%s%s_p%dt%d_%d" % (key, "_jp" if jp else "", p, t, rate),
                                    x, rate)
        used = sum(map(len, bodies.values()))
        if used <= budget:
            break
        rate -= 250
        assert rate >= 6000, (key, used, budget)
    for (p, t), body in bodies.items():
        tone, pair = vab.tones[p][t], vab.tones[p][t + 1]
        vab.vags[tone["vag"] - 1] = body
        note, jpp = clips[(p, t)][2:]
        if jpp is None:
            # the PSP clip at its own rate
            c, f = G.centre(note, rate)
            total = c * 128 - f
        else:
            # a JP take with the JP PS1 tone's pitch (as libsnd plays it),
            # lowered as the clip was resampled from 8000 Hz
            total = jpp + int(round(1536 * math.log2(RATE / rate)))
        d = pitch(pair) - pitch(tone)
        if jpp is not None and (key, p, t) == ("ALK", 1, 14):
            d -= 2 * 128  # the JP PS1's death line doubles 2 semitones up
        set_pitch(tone, total)
        set_pitch(pair, total + d)
    vh, vb = vab.write()
    if len(vb) < len(vb0):
        vab.vags.append(bytes(len(vb0) - len(vb)))
        vh, vb = vab.write()
    assert len(vh) == len(vh0) and len(vb) == len(vb0), (key, len(vh), len(vb))
    print("%s grunts %s: %d clips, %d Hz, gain %.2f, %d/%d bytes" % (
        "jp" if jp else "psp", key, len(clips), rate, gain, used, budget))
    return vh, vb


def pad(b, sectors=None):
    n = sectors * SECTOR if sectors else -(-len(b) // SECTOR) * SECTOR
    assert len(b) <= n, (len(b), n)
    return b + bytes(n - len(b))


def dub_sd_mar(jp):
    """SD_MAR with the prologue Maria's PSP voice clips (MARY1*; the pack's
    other clips are sound effects): the English ("Richter!", her call to
    the spirits) or the Japanese ones"""
    import gen_psp_sfx as S
    r = S.build_bank("SD_MAR_jp" if jp else "SD_MAR_psp", MAR, MAR, "mar", 2, S.psp_tables(),
                     S.SPK_JP if jp else S.SPK_EN, "MARY")
    assert r, "SD_MAR: no PSP clip"
    vh, vb, report = r
    print("%s grunts SD_MAR (sd_mar): %s" % ("jp" if jp else "psp", "; ".join(report)))
    return vh, vb


def main():
    off = "--off" in sys.argv[1:]
    jp = "--set" in sys.argv[1:] and sys.argv[sys.argv.index("--set") + 1] == "jp"
    tag = "JP" if jp else "PSP"
    vh, vb = build("ALK", off, jp)
    out = pad(vh, LAYOUT["ALK"][0]) + pad(vb)
    open(os.path.join(ROOT, "build/us/ALK_%s.BIN" % tag), "wb").write(out)
    vh, vb = build("RIH", off, jp)
    mvh = open(os.path.join(ROOT, MAR + ".VH"), "rb").read()
    mvb = open(os.path.join(ROOT, MAR + ".VB"), "rb").read()
    if not off:
        mvh, mvb = dub_sd_mar(jp)
    out = (pad(vh, LAYOUT["RIH"][0]) + pad(vb, LAYOUT["RIH"][1]) +
           pad(mvh, LAYOUT["RIH"][2]) + pad(mvb))
    open(os.path.join(ROOT, "build/us/RIH_%s.BIN" % tag), "wb").write(out)


if __name__ == "__main__":
    main()
