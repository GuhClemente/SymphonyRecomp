#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Maria boss fight's sounds in the Castle Center's bank.

On the Saturn the Castle Center's stage bank is SD/SD_TMARI.CRT ("enemy
Maria", stage record 8's 0xB32): her voice (attacks, hurt, death) and the
fight's sounds. The PS1's CEN bank (ST/CEN/SD_ZKCEN) is full (624 bytes free
of the 108,048 a stage bank may take), but CEN's code plays none of its stage
sounds (every sound it plays is a common-bank one: doors, clangs, pickups):
its programs 3, 5 and 6 hold leftover enemy voices of other stages (the
Hunting Girl's, Plate Lord's... slots). Those tones are blanked and their
samples dropped; Maria's sounds (rendered from the Saturn's banks by
mods/sotn-ultimate/tools/saturn_sfx.py) take their slots, reached through PS1 stage ids of
those slots (MOD_SFX_MB_* in the generated src/st/cen/mod_cen_sfx.h).
The other programs (0-2, 4, 7+) stay as they are.

The bank keeps the retail sizes (VH 6688, VB 107,424 padded with silence),
so no LBA table or stage table changes: build/us/SD_ZKCEN.VH/.VB, which
`make disk` takes over the retail files.

The PS1 dub's copy: build/us/CEN_SD_PS1.bin, the same bank laid out as
gen_cenp_sounds.py's (VH 4 sectors + VB), CENP.BIN's tail at 0x7C800
(mods/sotn-ultimate/tools/build_cenp.sh; src/dra/cd.c loads it for the Saturn boss with
the PS1 dub). Written by every run: the same sounds.
"""
import os
import struct
import subprocess
import sys
import wave

import yaml

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
sys.path.insert(0, os.path.dirname(__file__))
from vab import Vab  # noqa: E402
import gen_gdn_sounds as gs  # noqa: E402
from mod_work import work_dir  # noqa: E402

CEN = os.path.join(ROOT, "disks/us/ST/CEN/SD_ZKCEN")
OUT = os.path.join(ROOT, "build/us/SD_ZKCEN")
HEADER = os.path.join(ROOT, "src/st/cen/mod_cen_sfx.h")
WORK = work_dir("sfx")
FREE_PROGS = (3, 5, 6)
STAGE = "SD_TMARI.CRT"
RATE_CAPS = [32000, 22050] + list(range(20000, 7999, -1000))
OUT_PS1 = os.path.join(ROOT, "build/us/CEN_SD_PS1.bin")
VH_SECTORS = 4
SECTOR = 2048
# her voice at Richter's PS1 voice level (peak -1 dB), as gen_mar_sounds.py
# VOICE_PEAK does for the playable Maria: the Saturn renders peak at about
# half scale, which left the boss's voice 6-7 dB under hers. The flame
# sounds keep their own level (FLAME2's scream is near full scale already)
VOICE_PEAK = 29200
VOICES = {"MB_ATK1", "MB_ATK2", "MB_JUMP", "MB_DASH", "MB_HOLY", "MB_SUMMON",
          "MB_HURT1", "MB_HALF", "MB_HURT2", "MB_DEATH", "MB_THROW"}

# (name, Saturn id, PS1 id whose slot plays it)
SOUNDS = [
    ("MB_ATK1", 0x8B7, 0x725),
    ("MB_ATK2", 0x8B8, 0x728),
    ("MB_JUMP", 0x8B9, 0x723),
    ("MB_DASH", 0x8BD, 0x72D),
    ("MB_HOLY", 0x8BE, 0x72E),
    ("MB_SUMMON", 0x8BF, 0x72F),
    ("MB_HURT1", 0x8C5, 0x751),
    ("MB_HALF", 0x8C6, 0x752),
    ("MB_HURT2", 0x8C7, 0x753),
    ("MB_DEATH", 0x8CA, 0x743),
    ("MB_THROW", 0x8CC, 0x744),
    ("MB_FLAME1", 0x8D4, 0x745),
    ("MB_FLAME2", 0x8D5, 0x763),
]


def render(sid, cap):
    import saturn_sfx
    path = os.path.join(WORK, "cen_%X_%d.wav" % (sid, cap))
    return saturn_sfx.render_to_wav(sid, path=path, max_rate=cap, stage=STAGE)


def build(cap):
    cfg = yaml.safe_load(open(os.path.join(ROOT, "assets/dra/us/sfx_config.yaml")))
    vh_retail = open(CEN + ".VH", "rb").read()
    vb_retail = open(CEN + ".VB", "rb").read()
    vab = Vab(vh_retail, vb_retail)
    # the samples still used once the free programs are blanked
    keep = sorted({t["vag"] for p, tl in vab.tones.items() if p not in FREE_PROGS
                   for t in tl if t["vag"] > 0})
    old = vab.vags
    remap = {v: k + 1 for k, v in enumerate(keep)}
    vab.vags = [old[v - 1] for v in keep]
    # silence that ends (gen_gdn_sounds' blank): the free slots' sample
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
    os.makedirs(WORK, exist_ok=True)
    for name, sat, ps1 in SOUNDS:
        e = cfg[ps1 - 0x600]
        assert e["vabid"] == 3 and e["prog"] in FREE_PROGS, (name, hex(ps1))
        wav = render(sat, cap)
        with wave.open(wav) as w:
            rate, raw = w.getframerate(), w.readframes(w.getnframes())
        pcm = struct.unpack("<%dh" % (len(raw) // 2), raw)
        peak = max(1, max(abs(v) for v in pcm))
        gain = min(127.0 / e["volume"], 32000.0 / peak)
        if name in VOICES:
            gain = min(VOICE_PEAK / peak * 127.0 / e["volume"], 32000.0 / peak)
        if gain > 1.0:
            wav = os.path.join(WORK, "cen_%s_%d_vol.wav" % (name, cap))
            gs.write_wav(wav, [int(round(v * gain)) for v in pcm], rate)
        body = os.path.join(WORK, "cen_%s.vag" % name)
        subprocess.run([gs.SATXA, "vag", wav, body], check=True, capture_output=True)
        vab.vags.append(open(body, "rb").read())
        c, f = gs.centre(e["note"], gs.wav_rate(wav))
        slot = vab.tones[e["prog"]][e["tone"]]
        assert slot["vag"] == blank_vag, ("slot taken", name)
        vab.tones[e["prog"]][e["tone"]] = dict(
            template, prog=e["prog"], vag=len(vab.vags), vol=127, pan=64, center=c,
            shift=f, min=0, max=127, adsr1=0x80FF, adsr2=0x5FC0)
    vh, vb = vab.write()
    assert len(vh) == len(vh_retail), (len(vh), len(vh_retail))
    if len(vb) > len(vb_retail):
        print("cen sounds: rate cap %d gives %d bytes, over %d" % (cap, len(vb), len(vb_retail)))
        return None
    # the same size as the retail bank. The padding must be sample data: the
    # VH's sample sizes have to add up to the VB (SsVabTransBodyPartly waits
    # for exactly that many bytes; a VB padded past them never completes and
    # the stage load hangs). So the blank sample grows by silent blocks.
    pad = len(vb_retail) - len(vb)
    assert pad % 16 == 0, pad
    vab.vags[blank_vag - 1] = bytes(pad) + vab.vags[blank_vag - 1]
    vh, vb = vab.write()
    assert len(vb) == len(vb_retail), (len(vb), len(vb_retail))
    vh = bytearray(vh)
    struct.pack_into("<I", vh, 12, len(vh) + len(vb))  # VAB file size field
    assert len(vh) <= VH_SECTORS * SECTOR
    data = bytes(vh) + bytes(VH_SECTORS * SECTOR - len(vh)) + vb
    open(OUT_PS1, "wb").write(data + bytes(-len(data) % SECTOR))
    open(OUT + ".VH", "wb").write(bytes(vh))
    open(OUT + ".VB", "wb").write(vb)
    with open(HEADER, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gen_cen_sounds.py, do not edit\n")
        f.write("// Maria's Saturn sounds in CEN's stage bank (build/us/SD_ZKCEN)\n")
        for name, sat, ps1 in SOUNDS:
            f.write("#define MOD_SFX_%s 0x%X // Saturn 0x%X\n" % (name, ps1, sat))
    print("cen sounds: %d sounds, rate cap %d, bank %d of %d bytes used" % (
        len(SOUNDS), cap, len(vab.write()[1]), len(vb_retail)))
    return len(vb)


if __name__ == "__main__":
    for cap in RATE_CAPS:
        if build(cap) is not None:
            break
    else:
        raise SystemExit("Maria's sounds don't fit CEN's bank at any rate cap")
