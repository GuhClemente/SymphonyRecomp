#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: BIN/BO6RIC2.BIN, boss Richter in his Saturn coat.

On the Saturn the boss Richter (Castle Keep throne room) wears the long blue
coat of RICHTER2. On the PS1 the boss stage BO6 carries its own copy of
Richter's art: `richter_sprites` (the stage header's unk2C, drawn through
g_PlayerDraw[8] by DRA's RenderEntities for ANIMSET_OVL(1) frames with the
0x8000 bit), spritebank 1 (the 213 frames, the same as RIC's spritebank0) and
the palettes 0x220-0x24F (the same as PAL_RICHTER 0x120-0x14F, PAL_BULK at
0x2200). All three are byte-identical to the RIC overlay's, so the coat that
mods/sotn-ultimate/tools/build_ric2.py made for RIC2.BIN is transplanted as it is:

- every sprite of BO6's sheet gets RIC2.BIN's image (pointers relocated);
- the two frames RIC2 moved (pivots below 0) are moved the same way;
- the palettes RIC2 replaces (0x120.. -> 0x220..) are replaced in BO6's
  PAL_BULK data.

The whip-brandish arm (RIC2's F_GAME2 VRAM patch) is not needed: BO6 has no
spritebank 2, and the boss does not brandish.

BO6RIC2.BIN is a copy of build/us/BO6.BIN (same size, code and addresses)
with only those bytes rewritten; src/dra/cd.c loads it instead of BO6.BIN
when MOD_BOSS_RIC_SATURN() is true. Run after build_ric2.py.
"""
import os
import re
import struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
RIC_RAM = 0x8013C000
BO6_RAM = 0x80180000
RIC2_HDR = 0x170AC - 8  # {magic "RIC2", pointer to the extras}
MAGIC = 0x32434952


def path(p):
    return os.path.join(ROOT, p)


def syms(mapfile):
    out = {}
    for m in re.finditer(r"^\s+0x([0-9a-f]{8})\s+([A-Za-z_]\w*)\s*$",
                         open(path(mapfile)).read(), re.M):
        out.setdefault(m.group(2), int(m.group(1), 16))
    return out


def sheet(d, base, table):
    ptrs = []
    o = table - base
    while True:
        p = struct.unpack_from("<I", d, o)[0]
        if not base <= p < base + len(d) or (ptrs and o >= min(ptrs) - base):
            break
        ptrs.append(p)
        o += 4
    return ptrs


def sprite(d, base, p):
    q = p - base
    w, h = d[q], d[q + 1]
    return bytes(d[q:q + 4 + w * h // 2])


def frames(d, base, start, end):
    out = []
    o, lim = start - base, end - base
    while o < lim:
        p = struct.unpack_from("<I", d, o)[0]
        if p:
            lim = min(lim, p - base)
        out.append(p - base if p else None)
        o += 4
    return out


def main():
    ric = open(path("build/us/RIC.BIN"), "rb").read()
    ric2 = open(path("build/us/RIC2.BIN"), "rb").read()
    bo6 = open(path("build/us/BO6.BIN"), "rb").read()
    rs, bs = syms("build/us/ric.map"), syms("build/us/bobo6.map")
    assert len(ric) == len(ric2)

    # the sprite sheet: same images as RIC's, in the same order
    rp = sheet(ric, RIC_RAM, rs["richter_sprites"])
    r2p = sheet(ric2, RIC_RAM, rs["richter_sprites"])
    bp = sheet(bo6, BO6_RAM, bs["richter_sprites"])
    assert len(rp) == len(bp) == len(r2p), (len(rp), len(bp), len(r2p))
    for a, b in zip(rp, bp):
        assert sprite(ric, RIC_RAM, a) == sprite(bo6, BO6_RAM, b), "BO6 sheet"
    start = min(bp) - BO6_RAM
    end = max(b - BO6_RAM + len(sprite(bo6, BO6_RAM, b)) for b in bp)
    assert start == bs["richter_sprites"] - BO6_RAM + 4 * len(bp)

    out = bytearray(bo6)
    pos = start
    placed = {}
    for i, p in enumerate(r2p):
        if p not in placed:
            s = sprite(ric2, RIC_RAM, p)
            placed[p] = pos
            out[pos:pos + len(s)] = s
            pos = (pos + len(s) + 3) & ~3
        struct.pack_into("<I", out, bs["richter_sprites"] - BO6_RAM + 4 * i,
                         BO6_RAM + placed[p])
    assert pos <= ((end + 3) & ~3), "coat sheet too big: %d > %d" % (pos, end)
    out[pos:end] = bytes(max(0, end - pos))

    # frames: BO6 spritebank 1 == RIC spritebank0; copy RIC2's moved pivots
    rf = frames(ric, RIC_RAM, rs["spritebank0"], rs["spritebank1"])
    bf = frames(bo6, BO6_RAM, bs["D_us_801A39B0"], bs["D_us_801A43A4"])
    assert len(rf) == len(bf), (len(rf), len(bf))
    moved = 0
    done = set()
    for a, b in zip(rf, bf):
        if a is None:
            assert b is None
            continue
        assert ric[a:a + 8] == bo6[b:b + 8], "BO6 frame"
        if ric2[a:a + 8] != ric[a:a + 8] and b not in done:
            out[b:b + 8] = ric2[a:a + 8]
            done.add(b)
            moved += 1

    # palettes: RIC2's extras blob {n, {id, 16 colours} x n, ...}
    magic, extra = struct.unpack_from("<II", ric2, RIC2_HDR)
    assert magic == MAGIC
    o = extra - RIC_RAM
    n = struct.unpack_from("<I", ric2, o)[0]
    o += 4
    pal = bs["D_us_8019E9A0"] - BO6_RAM  # PAL_BULK(0x2200): 0x220-0x24F
    g2 = open(path("disks/us/BIN/F_GAME2.BIN"), "rb").read()
    done_pals = []
    for _ in range(n):
        k = struct.unpack_from("<I", ric2, o)[0]
        cols = ric2[o + 4:o + 36]
        o += 36
        assert 0x120 <= k < 0x150, hex(k)
        q = pal + (k - 0x120) * 32
        retail = g2[0x40000 + (k - 0x100) * 32:0x40000 + (k - 0xFF) * 32]
        assert bo6[q:q + 32] == retail, "BO6 palette 0x%X" % (k + 0x100)
        out[q:q + 32] = cols
        done_pals.append(k + 0x100)

    assert len(out) == len(bo6)
    open(path("build/us/BO6RIC2.BIN"), "wb").write(out)
    print("BO6RIC2.BIN: %d sprites (%d images), %d bytes free; %d frames moved;"
          " palettes %s" % (len(bp), len(placed), end - pos, moved,
                            " ".join("%03X" % k for k in done_pals)))


if __name__ == "__main__":
    main()
