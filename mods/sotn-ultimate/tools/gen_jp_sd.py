#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the JP dub's sound banks in one disc file (BIN/JP_SD.BIN).

The JP dub (include/game.h MOD_JP_DUB_ON: Japanese voices, the PSP English
script) swaps the banks whose voices differ between the PSP's English and
Japanese packs. Each part keeps the layout of the file it stands in for, so
src/dra/cd.c only moves a CdFile's sector (MOD_JP_SD_* there, checked here):
  ALK   Alucard's grunts, as ALK_PSP.BIN    (gen_psp_grunts.py --set jp)
  RIH   Richter's + the prologue Maria's, as RIH_PSP.BIN       (same)
  MAR2  the PSP Maria's voice, as MARIA2_SD.BIN (tools/maria-sfx on the
        Japanese sd_mar2.spk + gen_mar2_sounds.py --out)
  CENP  the PSP Maria boss's CEN bank, as CENP.BIN's tail
        (gen_cenp_sounds.py --set jp)
  SFX   the stage / familiar banks (gen_psp_sfx.py --set jp: SFX_JP.BIN;
        their sectors here + 1 are in src/st/sel/mod_jp_sfx.h)
Run after those, before `make disk`.
"""
import os
import re
import sys

from mod_work import read_src

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
SECTOR = 2048
# (name, file, sectors; the last one runs to the end)
PARTS = [("ALK", "build/us/ALK_JP.BIN", 32),
         ("RIH", "build/us/RIH_JP.BIN", 60),
         ("MAR2", "build/us/MARIA2_SD_JP.BIN", 66),
         ("CENP", "build/us/CENP_SD_JP.bin", 57),
         ("SFX", "build/us/SFX_JP.BIN", None)]
OUT = os.path.join(ROOT, "build/us/JP_SD.BIN")


def offset(name):
    o = 0
    for n, _, size in PARTS:
        if n == name:
            return o
        o += size
    raise KeyError(name)


SFX_BASE = offset("SFX")  # gen_psp_sfx.py --set jp


def main():
    src = read_src("src/dra/cd.c")
    data = bytearray()
    for name, path, size in PARTS:
        m = re.search(r"#define MOD_JP_SD_%s (\d+)" % name, src)
        if not m or int(m.group(1)) != len(data) // SECTOR:
            sys.exit("gen_jp_sd: src/dra/cd.c MOD_JP_SD_%s must be %d" % (
                name, len(data) // SECTOR))
        b = open(os.path.join(ROOT, path), "rb").read()
        n = size * SECTOR if size else -(-len(b) // SECTOR) * SECTOR
        if len(b) > n:
            sys.exit("gen_jp_sd: %s is %d bytes, over its %d sectors" % (path, len(b), size))
        data += b + bytes(n - len(b))
    open(OUT, "wb").write(bytes(data))
    print("jp sd: %s %d sectors (%s)" % (os.path.relpath(OUT, ROOT), len(data) // SECTOR,
                                        " ".join("%s@%d" % (n, offset(n)) for n, _, _ in PARTS)))


if __name__ == "__main__":
    main()
