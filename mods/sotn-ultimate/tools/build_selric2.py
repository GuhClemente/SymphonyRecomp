#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: BIN/SELRIC2.BIN, Richter in his Saturn coat in the endings.

SEL draws Richter in the endings (src/st/sel/34B30.c: func_801B54C8, the
Alucard endings where he was saved, frame 0x26; func_801B5548, the Richter
game's ending, frame 7; ANIMSET_OVL(2), palette 0x210) from its own three
Richter images (D_8018C718 / D_8018C92C / D_8018CB4C, byte-identical to RIC's
sprites 0, 6 and 37) and its copy of RIC's palettes 0x120-0x14F
(D_8018D5D4, PAL_BULK 0x2100 of the endings' clut list).

SELRIC2.BIN is the final build/us/SEL.BIN (same size, code and addresses,
PSP script copies included) with those three images replaced by RIC2.BIN's
(same sizes) and RIC2's palettes in the copy. src/dra/cd.c loads it instead
of SEL.BIN when Richter wears the coat: an Alucard game with the Saturn boss
Richter (MOD_BOSS_RIC_SATURN) or a Richter game in the Saturn costume
(MOD_RIC_COSTUME2). SEL is also the title; nothing there uses these images.
Run after the last change to SEL.BIN (build_disc.sh: after psp_script.py
--apply); prints "changed" when it rewrote the file.
"""
import os
import struct

import build_bo6ric2 as B

SEL_RAM = 0x80180000
SPRITES = {0: "D_8018C718", 6: "D_8018C92C", 37: "D_8018CB4C"}
PALS = "D_8018D5D4"  # RIC 0x120-0x14F


def main():
    ric = open(B.path("build/us/RIC.BIN"), "rb").read()
    ric2 = open(B.path("build/us/RIC2.BIN"), "rb").read()
    sel = open(B.path("build/us/SEL.BIN"), "rb").read()
    rs, ss = B.syms("build/us/ric.map"), B.syms("build/us/stsel.map")
    rp = B.sheet(ric, B.RIC_RAM, rs["richter_sprites"])
    r2p = B.sheet(ric2, B.RIC_RAM, rs["richter_sprites"])
    out = bytearray(sel)
    for i, sym in SPRITES.items():
        at = ss[sym] - SEL_RAM
        retail = B.sprite(ric, B.RIC_RAM, rp[i])
        assert sel[at:at + len(retail)] == retail, "SEL image %s" % sym
        coat = B.sprite(ric2, B.RIC_RAM, r2p[i])
        assert coat[:2] == retail[:2] and len(coat) == len(retail), sym
        out[at:at + len(coat)] = coat
    magic, extra = struct.unpack_from("<II", ric2, B.RIC2_HDR)
    assert magic == B.MAGIC
    o = extra - B.RIC_RAM
    n = struct.unpack_from("<I", ric2, o)[0]
    o += 4
    g2 = open(B.path("disks/us/BIN/F_GAME2.BIN"), "rb").read()
    base = ss[PALS] - SEL_RAM
    done = 0
    for _ in range(n):
        k = struct.unpack_from("<I", ric2, o)[0]
        assert 0x120 <= k < 0x150, hex(k)
        q = base + (k - 0x120) * 32
        retail = g2[0x40000 + (k - 0x100) * 32:0x40000 + (k - 0xFF) * 32]
        assert sel[q:q + 32] == retail, "SEL palette 0x%X" % k
        # only his own palette (0x210 = PAL_RICHTER), the one his ending
        # sprite uses: other slots of the copy may serve other art (TOP's
        # candelabras use its copy's 0x12B-0x12E slots)
        if k == 0x120:
            out[q:q + 32] = ric2[o + 4:o + 36]
            done += 1
        o += 36
    dst = B.path("build/us/SELRIC2.BIN")
    old = open(dst, "rb").read() if os.path.exists(dst) else None
    if old != bytes(out):
        open(dst, "wb").write(out)
    print("SELRIC2.BIN: 3 images, %d palettes%s" % (
        done, ", changed" if old != bytes(out) else ""))


if __name__ == "__main__":
    main()
