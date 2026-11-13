#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the boss Richter's coat on TOP's kneeling Richter.

After Shaft's orb, TOP (Castle Keep) shows Richter kneeling
(src/st/top/post_richter_cutscene.c func_us_801AD320) from its own two
Richter images (richter_sprites[16] and [17], byte-identical to RIC's sprites
16 and 17) and its copy of PAL_RICHTER (PAL_BULK 0x230-0x23F = RIC
0x120-0x12F). With the Saturn boss (MOD_BOSS_RIC_SATURN) he should keep the
coat BO6RIC2.BIN gives him in the fight.

Fills g_ModTopRic2 (mods/sotn-ultimate/src/richter_saturn/mod_ric2.h) in build/us/TOP.BIN with RIC2.BIN's
sprites 16 and 17 (same sizes, written over the PS1's at run time) and the
RIC2 palette 0x120 (his own) as TOP clut 0x230 {count, {id, pad, 16 colours}};
ModTopRic2Apply copies them when the actor starts. Run after build_ric2.py.
"""
import os
import struct

import build_bo6ric2 as B

TOP_RAM = 0x80180000
WORDS = 231  # mod_ric2.h MOD_TOP_RIC2_WORDS


def main():
    ric = open(B.path("build/us/RIC.BIN"), "rb").read()
    ric2 = open(B.path("build/us/RIC2.BIN"), "rb").read()
    top = bytearray(open(B.path("build/us/TOP.BIN"), "rb").read())
    rs, ts = B.syms("build/us/ric.map"), B.syms("build/us/sttop.map")
    rp = B.sheet(ric, B.RIC_RAM, rs["richter_sprites"])
    r2p = B.sheet(ric2, B.RIC_RAM, rs["richter_sprites"])
    tab = ts["richter_sprites"] - TOP_RAM
    out = b""
    for i in (16, 17):
        tp = struct.unpack_from("<I", top, tab + 4 * i)[0]
        retail = B.sprite(top, TOP_RAM, tp)
        assert retail == B.sprite(ric, B.RIC_RAM, rp[i]), "TOP sprite %d" % i
        coat = B.sprite(ric2, B.RIC_RAM, r2p[i])
        assert coat[:2] == retail[:2], "RIC2 sprite %d size" % i
        out += coat
    assert len(out) % 4 == 0
    magic, extra = struct.unpack_from("<II", ric2, B.RIC2_HDR)
    assert magic == B.MAGIC
    o = extra - B.RIC_RAM
    n = struct.unpack_from("<I", ric2, o)[0]
    o += 4
    pals = []
    for _ in range(n):
        k = struct.unpack_from("<I", ric2, o)[0]
        # only his own palette (PAL_RICHTER): TOP's candelabras (palette
        # 0x238 + part offsets) draw with 0x23B-0x23E, so RIC2's variants
        # of 0x12B-0x12E turned them blue
        if k == 0x120:
            pals.append(struct.pack("<HH", 0x230 + k - 0x120, 0) + ric2[o + 4:o + 36])
        o += 36
    out += struct.pack("<I", len(pals)) + b"".join(pals)
    assert len(out) <= WORDS * 4, "g_ModTopRic2 too small: %d" % len(out)
    at = ts["g_ModTopRic2"] - TOP_RAM
    assert not any(top[at:at + WORDS * 4]), "g_ModTopRic2 not empty"
    top[at:at + len(out)] = out
    open(B.path("build/us/TOP.BIN"), "wb").write(top)
    print("TOP coat: 2 sprites, %d palettes, %d of %d bytes" % (
        len(pals), len(out), WORDS * 4))


if __name__ == "__main__":
    main()
