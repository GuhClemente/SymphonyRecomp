#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Librarian's bestiary pictures from the Saturn.

The Saturn keeps its 164 pictures in disks/saturn/OYAJI.CHR: a table at file
offset 0x40000 of 164 {be32 offset from the table, be32 size}, each entry
LZSS (tools/saturn/character_viewer.py) -> 0x2C00 bytes = 256-colour palette
(BE RGB555) + 96x112 8bpp pixels, indexed by the Saturn list position
(disks/saturn/LIBRARY.PRG 0x1825C, 164 be16 enemy ids).

The PS1 picture (disks/us/BIN/MONSTER.BIN, a 0x5800-byte record per PS1 list
position) is the same image as 96x112 LE 16-bit pixels with STP (0x8000) set,
then 1024 bytes of retail "DRACULAX" filler. The conversion is lossless.

Writes:
- build/us/MONSTER2.BIN: the 18 Saturn-only pictures in bestiary-bit order
  146.. (src/st/lib/mod_bestiary.h g_ModLibEnemy), record bit - 146; loaded by
  src/dra/cd.c (OFF_BIN_MONSTER2). Then record 18 (index 164): the PSP
  port's own picture of Maria (disks/psp/hdbin/mo_139.bin, PSP list
  #139: 96x112 LE 16-bit without STP / filler), shown for Maria (bit 152)
  when the PSP boss is chosen (src/st/lib/e_shop.c MOD_LIB_PSP_MARIA_PIC).
  Then, only when include/game.h defines MOD_OUTER_WALL_GHOST, record 19
  (index 165): the cut Outer Wall skull "Ghost Skull" (Library No.165,
  bit 164, MOD_LIB_GHOST_PIC), drawn from its own NO1 sprite
  (mods/sotn-ultimate/tools/ghost_portrait.py).
- build/us/MONSTER.BIN: the retail file, same size, with the 3 pictures the
  Saturn touched up (enemy ids 0x043, 0x071, 0x182) replaced in place.
"""
import os
import re
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
sys.path.insert(0, os.path.join(ROOT, "tools/saturn"))
from character_viewer import decompress_lzss  # noqa: E402

from mod_work import read_src  # noqa: E402
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ghost_portrait  # noqa: E402

REC = 0x5800
PIX = 96 * 112
RETAIL = 146
PSP_MARIA = "disks/psp/hdbin/mo_139.bin"
TOUCHED_UP = (0x043, 0x071, 0x182)


def main():
    oy = open(os.path.join(ROOT, "disks/saturn/OYAJI.CHR"), "rb").read()
    lib = open(os.path.join(ROOT, "disks/saturn/LIBRARY.PRG"), "rb").read()
    mb = open(os.path.join(ROOT, "disks/us/BIN/MONSTER.BIN"), "rb").read()
    hdr = open(os.path.join(ROOT, "src/st/lib/mod_bestiary.h")).read()
    assert len(mb) == 147 * REC, len(mb)
    sat_ids = struct.unpack(">164H", lib[0x1825C:0x1825C + 328])
    body = hdr[hdr.index("g_ModLibEnemy[]"):]
    body = body[:body.index("};")]
    if "#ifdef" in body:  # the toggled Ghost Skull entry (not a Saturn one)
        body = body[:body.index("#ifdef")]
    bits = [int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]+)", body)]
    assert len(bits) == 164, len(bits)
    filler = mb[PIX * 2:REC]
    T = 0x40000

    def record(eid):
        pos = sat_ids.index(eid)
        o, s = struct.unpack(">2I", oy[T + 8 * pos:T + 8 * pos + 8])
        raw = decompress_lzss(oy[T + o:T + o + s])
        assert len(raw) == 0x2C00, (eid, len(raw))
        pal = struct.unpack(">256H", raw[:0x200])
        px = struct.pack("<%dH" % PIX,
                         *[(pal[p] & 0x7FFF) | 0x8000 for p in raw[0x200:]])
        return px + filler

    out = b"".join(record(eid) for eid in bits[RETAIL:])
    assert len(out) == 18 * REC
    # the PSP's Maria: same size, set STP on every pixel (black = 0x8000)
    psp = open(os.path.join(ROOT, PSP_MARIA), "rb").read()
    assert len(psp) == PIX * 2, len(psp)
    out += struct.pack("<%dH" % PIX, *[p | 0x8000 for p in
                                        struct.unpack("<%dH" % PIX, psp)])
    out += filler
    assert len(out) == 19 * REC
    ghost = re.search(r"^#define MOD_OUTER_WALL_GHOST *$", read_src("include/game.h"), re.M)
    if ghost:
        assert "#define MOD_LIB_GHOST_PIC 165" in hdr
        px = ghost_portrait.picture()
        assert len(px) == PIX * 2
        out += px + filler
        assert len(out) == 20 * REC
    open(os.path.join(ROOT, "build/us/MONSTER2.BIN"), "wb").write(out)

    mon = bytearray(mb)
    for eid in TOUCHED_UP:
        n = bits.index(eid)
        assert n < RETAIL
        old = mon[n * REC:(n + 1) * REC]
        new = record(eid)
        diff = sum(1 for i in range(PIX)
                   if old[2 * i:2 * i + 2] != new[2 * i:2 * i + 2])
        assert 0 < diff < 1000, (eid, diff)
        mon[n * REC:(n + 1) * REC] = new
    assert len(mon) == len(mb)
    open(os.path.join(ROOT, "build/us/MONSTER.BIN"), "wb").write(mon)
    print("bestiary pictures: MONSTER2.BIN %d records, MONSTER.BIN %d "
          "replaced" % (len(out) // REC, len(TOUCHED_UP)))


if __name__ == "__main__":
    main()
