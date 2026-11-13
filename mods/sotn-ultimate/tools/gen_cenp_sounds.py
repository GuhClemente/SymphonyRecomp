#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the PSP Maria boss's English voice (the PSP dub's CEN bank).

BIN/CENP.BIN (mods/sotn-ultimate/tools/build_cenp.sh) uses CEN's stage bank, whose free
programs (3, 5, 6) hold the Saturn Maria's (Japanese) clips
(gen_cen_sounds.py). With the PSP dub on, src/dra/cd.c loads this bank
instead: the retail SD_ZKCEN with programs 3, 5, 6 blanked (as
gen_cen_sounds.py) and her 16 PSP English clips (sd_mar2.spk MARY2*, the
8 kHz PS1 ADPCM of build/us/MARIA2_SD.BIN, gen_mar2_sounds.py) in 16 of
their slots. CENP's code plays none of the Saturn fight's sounds, so the
bank needs nothing else of them. Same VH / VB sizes as the retail bank (the
stage table's vhLen / vbLen; the blank sample grows by silent blocks so the
VH's sample sizes add up to the VB, as gen_cen_sounds.py).

Output: build/us/CENP_SD.bin = VH (4 sectors, as ST/CEN/SD_ZKCEN.VH) + VB,
appended to CENP.BIN at 0x60000 by build_cenp.sh (src/dra/cd.c:
g_LbaStageVh.loc = OFF_BIN_CENP + 0xC0); mods/sotn-ultimate/src/maria_psp/mod_cenp_sfx.h:
CENP_PSP_VOICE = the stage ids of clip i (her PSP id 0x8E6 + i).

--set jp: the JP dub's (include/game.h MOD_JP_DUB_ON) version, her Japanese
clips (tools/maria-sfx on the Japanese sd_mar2.spk, build/us/mod/mar2/
sfx_jp) in the same slots: build/us/CENP_SD_JP.bin (in BIN/JP_SD.BIN,
mods/sotn-ultimate/tools/gen_jp_sd.py); the header is left alone (same ids).

--ps1sat: the PS1 dub's version, build/us/CENP_SD_PS1.bin (CENP.BIN's tail
at 0x99000, mods/sotn-ultimate/tools/build_cenp.sh; src/dra/cd.c loads it for the PSP boss
with the PS1 dub): the Saturn boss's sounds in the same slots (SAT_PS1:
what she played with the PS1 dub before, when mods/sotn-ultimate/src/maria_psp/mod_cenp.c
mapped her ids onto the Saturn fight's slots), so every version of her
bank has one layout.

Clip 6 (0x8EC, MARY2G "Guardian Knuckle!") is left out of every version (its
slot stays blank): the PSP Maria has no such attack and the boss never plays it.
"""
import os
import struct
import subprocess
import sys

import yaml

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vab import Vab  # noqa: E402
import gen_gdn_sounds as G  # noqa: E402
import gen_mar2_sounds as M2  # noqa: E402
from mod_work import work_dir  # noqa: E402

CEN = os.path.join(ROOT, "disks/us/ST/CEN/SD_ZKCEN")
MAR2 = os.path.join(ROOT, "build/us/MARIA2_SD.BIN")
OUT = os.path.join(ROOT, "build/us/CENP_SD.bin")
JP_CLIPS = os.path.join(ROOT, "build/us/mod/mar2/sfx_jp")
JP_OUT = os.path.join(ROOT, "build/us/CENP_SD_JP.bin")
PS1_OUT = os.path.join(ROOT, "build/us/CENP_SD_PS1.bin")
# --ps1sat: her clip i -> the Saturn boss's sound (gen_cen_sounds.py SOUNDS)
SAT_PS1 = ["MB_ATK1", "MB_ATK2", "MB_ATK1", "MB_THROW", "MB_SUMMON", "MB_HOLY", None,
           "MB_SUMMON", "MB_FLAME1", "MB_HOLY", "MB_SUMMON", "MB_JUMP", "MB_HURT1",
           "MB_HURT2", "MB_HURT1", "MB_DEATH"]
SAT_RATES = [22050, 16000, 14000, 12000, 11025, 10000, 9000, 8000]
NO_CLIP = {6}  # 0x8EC: Guardian Knuckle (see the top)
HEADER = os.path.join(ROOT, "mods/sotn-ultimate/src/maria_psp/mod_cenp_sfx.h")
FREE_PROGS = (3, 5, 6)
VH_SECTORS = 4
RATE = M2.RATE
# her clip i (PSP id 0x8E6 + i) -> the CEN stage id whose slot plays it
# (vab 3, programs 3 / 5, every slot distinct, the loudest ids of each)
SLOTS = [0x7F5, 0x7F6, 0x7F7, 0x7F8, 0x72D, 0x731, 0x733, 0x879,
         0x834, 0x835, 0x836, 0x837, 0x75E, 0x839, 0x83A, 0x83B]
NAMES = ["8E6 attack", "8E7 attack C", "8E8", "8E9 subweapon throw", "8EA",
         "8EB", "8EC", "8ED crash", "8EE", "8EF", "8F0", "8F1", "8F2 pain B",
         "8F3 pain C", "8F4 pain D", "8F5 death"]


def wav_vag(name, pcm, rate):
    wav = work_dir("sfx", name + ".wav")
    vag = work_dir("sfx", name + ".vag")
    G.write_wav(wav, pcm, rate)
    subprocess.run([G.SATXA, "vag", wav, vag], check=True, capture_output=True)
    return open(vag, "rb").read()


def main():
    if "--ps1sat" in sys.argv[1:]:
        for cap in SAT_RATES:
            if build(cap):
                return
        raise SystemExit("cenp sounds (ps1 sat): the Saturn sounds don't fit")
    build()


def build(cap=None):
    jp = "--set" in sys.argv[1:] and sys.argv[sys.argv.index("--set") + 1] == "jp"
    sat = cap is not None
    cfg = yaml.safe_load(open(os.path.join(ROOT, "assets/dra/us/sfx_config.yaml")))
    clips = []
    rates = [RATE] * 16
    if sat:
        import wave
        import gen_cen_sounds as C
        sid = {n: s for n, s, _ in C.SOUNDS}
        for i, (name, ps1) in enumerate(zip(SAT_PS1, SLOTS)):
            if name is None:
                clips.append((None, 0))
                continue
            with wave.open(C.render(sid[name], 22050)) as w:
                rate, raw = w.getframerate(), w.readframes(w.getnframes())
            pcm = list(struct.unpack("<%dh" % (len(raw) // 2), raw))
            if rate > cap:
                pcm, rate = G.resample(pcm, rate, cap)
            # as gen_cen_sounds.py: up to full scale against this slot's
            # volume, her voice at VOICE_PEAK
            e = cfg[ps1 - 0x600]
            peak = max(1, max(abs(v) for v in pcm))
            gain = min(127.0 / e["volume"], 32000.0 / peak)
            if name in C.VOICES:
                gain = min(C.VOICE_PEAK / peak * 127.0 / e["volume"], 32000.0 / peak)
            pcm = [int(round(v * gain)) for v in pcm]
            clips.append((wav_vag("cenp_sat_%02d_%s" % (i, name), pcm, rate), e["volume"]))
            rates[i] = rate
    elif jp:
        # maria-sfx's clips, in her id order (as gen_mar2_sounds.py places them)
        for (i, name, body), ps1 in zip(M2.psp_clips(JP_CLIPS), M2.SLOTS):
            clips.append((body, cfg[ps1 - 0x600]["volume"]))
    else:
        # her clips as MARIA2_SD.BIN's vab 1 holds them
        d = open(MAR2, "rb").read()
        s = M2.SECTOR * M2.LAYOUT_SECTORS[0]
        mar = Vab(d[:M2.VH_LEN[1]], d[s:s + M2.VB_LEN[1]])
        for ps1 in M2.SLOTS:
            e = cfg[ps1 - 0x600]
            t = mar.tones[e["prog"]][e["tone"]]
            assert t["vag"] > 1, hex(ps1)
            clips.append((mar.vags[t["vag"] - 1], e["volume"]))

    vh_retail = open(CEN + ".VH", "rb").read()
    vb_retail = open(CEN + ".VB", "rb").read()
    vab = Vab(vh_retail, vb_retail)
    keep = sorted({t["vag"] for p, tl in vab.tones.items() if p not in FREE_PROGS
                   for t in tl if t["vag"] > 0})
    old = vab.vags
    remap = {v: k + 1 for k, v in enumerate(keep)}
    vab.vags = [old[v - 1] for v in keep]
    vab.vags.append(bytes(16) + bytes([0, 1]) + bytes(14) + bytes([0, 7]) + bytes([0x77] * 14))
    blank_vag = len(vab.vags)
    template = None
    for p, tl in vab.tones.items():
        for t in tl:
            if p in FREE_PROGS:
                if template is None and t["vag"] > 0:
                    template = dict(t)
                t["vag"] = blank_vag
                t["vol"] = 0
            elif t["vag"] > 0:
                t["vag"] = remap[t["vag"]]
    assert len(set(SLOTS)) == 16
    for i, ((body, vol_mar), ps1, rate) in enumerate(zip(clips, SLOTS, rates)):
        e = cfg[ps1 - 0x600]
        if i in NO_CLIP or body is None:
            continue
        assert e["vabid"] == 3 and e["prog"] in FREE_PROGS, hex(ps1)
        slot = vab.tones[e["prog"]][e["tone"]]
        assert slot["vag"] == blank_vag, ("slot taken", hex(ps1))
        vab.vags.append(body)
        c, f = G.centre(e["note"], rate)
        assert 0 <= c <= 127, (hex(ps1), c)
        # the loudness MARIA2's slot gave it (her player id's volume)
        vol = min(127, int(round(127 * vol_mar / e["volume"])))
        vab.tones[e["prog"]][e["tone"]] = dict(
            template, prog=e["prog"], vag=len(vab.vags), vol=vol, pan=64, center=c,
            shift=f, min=0, max=127, adsr1=M2.ADSR[0], adsr2=M2.ADSR[1])
    vh, vb = vab.write()
    assert len(vh) == len(vh_retail), (len(vh), len(vh_retail))
    if sat and len(vb) > len(vb_retail):
        print("cenp sounds (ps1 sat): rate cap %d gives %d bytes, over %d" % (cap, len(vb), len(vb_retail)))
        return False
    assert len(vb) <= len(vb_retail), ("PSP clips don't fit", len(vb), len(vb_retail))
    used = len(vb)
    pad = len(vb_retail) - len(vb)
    assert pad % 16 == 0, pad
    vab.vags[blank_vag - 1] = bytes(pad) + vab.vags[blank_vag - 1]
    vh, vb = vab.write()
    assert len(vb) == len(vb_retail), (len(vb), len(vb_retail))
    vh = bytearray(vh)
    struct.pack_into("<I", vh, 12, len(vh) + len(vb))
    assert len(vh) <= VH_SECTORS * M2.SECTOR
    data = bytes(vh) + bytes(VH_SECTORS * M2.SECTOR - len(vh)) + vb
    if sat:
        open(PS1_OUT, "wb").write(M2.pad(data))
        print("cenp sounds (ps1 sat): 15 clips, rate cap %d, bank %d of %d bytes used" % (
            cap, used, len(vb_retail)))
        return True
    if jp:
        open(JP_OUT, "wb").write(M2.pad(data))
        print("cenp sounds (jp): 15 clips, bank %d of %d bytes used" % (used, len(vb_retail)))
        return True
    open(OUT, "wb").write(M2.pad(data))
    with open(HEADER, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gen_cenp_sounds.py, do not edit\n")
        f.write("// the PSP Maria boss's English clips in the PSP dub's CEN bank\n")
        f.write("// (build/us/CENP_SD.bin): her PSP id 0x8E6 + i -> stage id\n")
        f.write("#define CENP_PSP_VOICE {")
        f.write(", ".join("0x%X" % s for s in SLOTS))
        f.write("}\n")
        for s, n in zip(SLOTS, NAMES):
            f.write("// 0x%X: %s\n" % (s, n))
    print("cenp sounds: 15 PSP clips, bank %d of %d bytes used" % (used, len(vb_retail)))
    return True


if __name__ == "__main__":
    main()
