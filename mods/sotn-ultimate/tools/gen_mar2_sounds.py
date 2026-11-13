#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the PSP Maria's voice (BIN/MARIA2_SD.BIN).

The PSP keeps her player voice in res/ps/snd_e/SE/sd_mar2.spk (English; 16
MARY2* clips, played as PSP ids 0x8E6-0x8F5). tools/maria-sfx (upstream,
Rust) converts them to PS1 ADPCM at 8 kHz, trimmed, into a VAB the size of
SD_RIH (its VB is exactly vab1Len, 0xE190) with the clips on programs 1/2;
upstream then patches DRA's g_SfxData for Richter's 16 voice ids. Here
nothing in DRA is patched: the clips go into the vab 1 slots that those 16
ids already name (retail prog / tone / note, so the centre note plays each
at its own rate), and mods/sotn-ultimate/src/maria_psp/mar2.c MarPlaySfx maps her ids to them
(SLOTS below = its g_Mar2SfxSlot).

The file has MARIA_SD.BIN's layout (gen_mar_sounds.py; src/dra/cd.c moves
the four CdFiles SD_RIH.VH/VB, SD_MAR.VH/VB onto it by one offset): vab 1 =
her voice, vab 2 = the retail SD_MAR (the prologue Maria's bank: 0x881
SFX_MARIA_ANIMALS_APPEAR, which her crashes play) padded to vab2Len with an
unused VAG, because DRA loads Maria's vab 2 with that size.

Usage: gen_mar2_sounds.py <maria-sfx out dir> [out file]
(out file: build/us/MARIA2_SD_JP.BIN for the JP dub, from maria-sfx on the
Japanese sd_mar2.spk; mods/sotn-ultimate/tools/gen_jp_sd.py)

Her PS1-dub bank (BIN/MARIA_SD_PS1.BIN's second part, which src/dra/cd.c
loads for the PSP Maria with the PS1 dub) is a copy of MARIA2_SD.BIN
(mods/sotn-ultimate/tools/build_disc.sh).
"""
import os
import sys

import yaml

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vab import Vab  # noqa: E402
import gen_gdn_sounds as G  # noqa: E402

OUT = os.path.join(ROOT, "build/us/MARIA2_SD.BIN")
TEMPLATE = {1: "disks/us/VAB/SD_RIH", 2: "disks/us/VAB/SD_MAR"}
VH_LEN = {1: 0x1020, 2: 0xE20}
VB_LEN = {1: 0xE190, 2: 0xFBF0}
SECTOR = 2048
LAYOUT_SECTORS = (3, 29, 2)  # SD_RIH.VH, SD_RIH.VB, SD_MAR.VH
RATE = 8000  # maria-sfx's target rate
# her ids 0x8E6 + i -> the PS1 id whose slot holds MARY2 clip i
SLOTS = [0x6F9, 0x6FB, 0x6FA, 0x6FC, 0x6FD, 0x6FE, 0x700, 0x701,
         0x705, 0x706, 0x707, 0x708, 0x702, 0x703, 0x704, 0x6FF]
ADSR = (0x80FF, 0x5FC0)


def load_vab(path):
    return Vab(open(os.path.join(ROOT, path + ".VH"), "rb").read(),
               open(os.path.join(ROOT, path + ".VB"), "rb").read())


def finish(vab, bank):
    vh, vb = vab.write()
    assert len(vb) <= VB_LEN[bank], (bank, len(vb))
    if len(vb) < VB_LEN[bank]:
        vab.vags.append(bytes(VB_LEN[bank] - len(vb)))
        vh, vb = vab.write()
    assert len(vh) == VH_LEN[bank] and len(vb) == VB_LEN[bank], (bank, len(vh), len(vb))
    return vh, vb


def pad(b, sectors=None):
    n = sectors * SECTOR if sectors else -(-len(b) // SECTOR) * SECTOR
    assert len(b) <= n
    return b + bytes(n - len(b))


def psp_clips(sfx_dir):
    """maria-sfx's VB split into its 16 VAGs, in clip order"""
    rows = [l.rstrip("\n").split("\t") for l in open(os.path.join(sfx_dir, "maria-sfx.tsv"))
            if l.strip() and not l.startswith("#")]
    head, rows = rows[0], rows[1:]
    col = {k: i for i, k in enumerate(head)}
    vb = open(os.path.join(sfx_dir, "SD_RIH.VB"), "rb").read()
    rows.sort(key=lambda r: int(r[col["vag"]]))
    clips, pos = [], 0
    for r in rows:
        assert int(r[col["rate"]]) == RATE, r
        n = int(r[col["psx_bytes"]])
        clips.append((int(r[col["index"]]), r[col["psp_name"]], vb[pos:pos + n]))
        pos += n
    clips.sort()
    assert [c[0] for c in clips] == list(range(16)), clips
    return clips


PSP_SFX = os.path.join(ROOT, "build/us/mod/mar2/sfx")  # build_mar2.sh's maria-sfx out


def main():
    out = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else OUT
    build([(i, n, b, RATE) for i, n, b in psp_clips(sys.argv[1])], out)


def build(clips, out, what=None):
    cfg = yaml.safe_load(open(os.path.join(ROOT, "assets/dra/us/sfx_config.yaml")))
    # vab 1: Richter's bank emptied, her clips in his voice ids' slots
    vab = load_vab(TEMPLATE[1])
    template = dict(vab.tones[0][0])
    blank = dict(next(t for p in vab.tones for t in vab.tones[p] if t["vol"] == 0))
    for p in vab.tones:
        vab.tones[p] = [dict(blank, prog=p, vag=1) for _ in range(16)]
    vab.vags = [bytes(16) + bytes([0, 1]) + bytes(14) + bytes([0, 7]) + bytes([0x77] * 14)]
    for (i, name, body, rate), ps1 in zip(clips, SLOTS):
        e = cfg[ps1 - 0x600]
        assert e["vabid"] == 1, (name, hex(ps1), e)
        prog, tone = e["prog"], e["tone"]
        assert vab.tones[prog][tone]["vag"] == 1, ("slot taken", name, hex(ps1))
        vab.vags.append(body)
        c, f = G.centre(e["note"], rate)
        assert 0 <= c <= 127, (name, c)
        vab.tones[prog][tone] = dict(
            template, prog=prog, vag=len(vab.vags), vol=127, pan=64,
            center=c, shift=f, min=0, max=127, adsr1=ADSR[0], adsr2=ADSR[1])
    vh1, vb1 = finish(vab, 1)
    # vab 2: the retail prologue Maria bank, padded to vab2Len
    vh2, vb2 = finish(load_vab(TEMPLATE[2]), 2)
    data = (pad(vh1, LAYOUT_SECTORS[0]) + pad(vb1, LAYOUT_SECTORS[1]) +
            pad(vh2, LAYOUT_SECTORS[2]) + pad(vb2))
    open(out, "wb").write(data)
    print("psp maria sounds%s: %s %d bytes" % (
        " (%s)" % what if what else "", os.path.relpath(out, ROOT), len(data)))


if __name__ == "__main__":
    main()
