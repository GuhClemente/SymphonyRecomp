#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Bestiary picture of the cut Outer Wall skull (enemy record 0xB1, the
mod's "Ghost Skull", include/game.h MOD_OUTER_WALL_GHOST).

No original portrait exists, so it is drawn from its own sprite in the
retail disks/us/ST/NO1/NO1.BIN, the way the Saturn / PS1 pictures show an
enemy: the in-game sprite, unfiltered, on opaque black. NO1 entity 0x51
(func_us_801D4558): animset OVL(13), palette PAL_UNK_228, art in graphics
bank 3 (a compressed 128x128 4bpp block at VRAM 256,256 = tpage 0x50);
frame 1 (the skull in blue flames), scaled 2x (the 1x sprite is 22x28 px,
small in the 96x112 box), centred.

picture() -> 96x112 little-endian u16 pixels (21504 bytes), every pixel with
STP (0x8000) set like the retail MONSTER.BIN records; mods/sotn-ultimate/tools/
build_monster2.py appends the 1024-byte filler. No PIL needed.
"""
import os
import struct

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
BASE = 0x80180000
ANIMSET, UNK5A, PAL, BANK, FRAME, SCALE = 13, 0x50, 0x228, 3, 1, 2
W, H = 96, 112


def picture():
    d = open(os.path.join(ROOT, "disks/us/ST/NO1/NO1.BIN"), "rb").read()

    def rd32(a):
        return struct.unpack_from("<I", d, a - BASE)[0]

    def decompress(src):
        # DRA DecompressData (src/dra/4AEA4.c): 8 dictionary bytes, nibble ops
        dic = d[src - BASE:src - BASE + 8]
        rp = [(src - BASE + 8) * 2]
        out = []

        def rn():
            b = d[rp[0] >> 1]
            v = b & 15 if rp[0] & 1 else b >> 4
            rp[0] += 1
            return v
        while True:
            op = rn()
            if op == 0:
                a, b = rn(), rn()
                out += [0] * (a * 16 + b + 0x13)
            elif op == 1:
                out.append(rn())
            elif op == 2:
                out += [rn()] * 2
            elif op == 3:
                out += [rn(), rn()]
            elif op == 4:
                out += [rn(), rn(), rn()]
            elif op == 5:
                c, n = rn(), rn()
                out += [c] * (n + 3)
            elif op == 6:
                out += [0] * (rn() + 3)
            elif op <= 14:
                ch = dic[op - 7]
                if ch & 0xF0 == 0x20:
                    out += [ch & 15] * 2
                elif ch & 0xF0 == 0x10:
                    out.append(ch & 15)
                elif ch & 0xF0 == 0x60:
                    out += [0] * ((ch & 15) + 3)
            else:
                return out[:0x4000]  # 128x128 nibbles, low nibble first

    # graphics bank 3 (header +0x24): compressed 128x128 4bpp blocks
    bank = rd32(rd32(BASE + 0x24) + 4 * BANK)
    assert rd32(bank) == 4, "bank kind"
    e = bank + 4
    while rd32(e) != 0x01000100:  # {xy (x = hi), wh, data}: VRAM 256,256
        assert rd32(e) != 0xFFFFFFFF, "no block at 256,256"
        e += 12
    assert rd32(e + 4) == 0x00800080  # 128x128 4bpp
    blk = decompress(rd32(e + 8))  # nibbles of the block, row-major
    # palette PAL_UNK_228.. from the overlay's clut table (header +0x18)
    pals = {}
    p = rd32(rd32(BASE + 0x18))
    assert rd32(p) == 5
    q = p + 4
    while rd32(q) != 0xFFFFFFFF:
        dst, cnt, src = rd32(q), rd32(q + 4), rd32(q + 8)
        q += 12
        for k in range(cnt // 16):
            pals[dst // 16 + k] = struct.unpack_from("<16H", d, src - BASE + k * 32)
    # frame FRAME of animset 13 (header +0x14): {count, parts of 11 s16}
    fp = rd32(rd32(rd32(BASE + 0x14) + 4 * ANIMSET) + 4 * FRAME)
    cnt = struct.unpack_from("<H", d, fp - BASE)[0]
    spr = {}
    for k in range(cnt):
        fl, ox, oy, w, h, clut, ts, l, t, r, b = struct.unpack_from(
            "<11h", d, fp - BASE + 2 + 22 * k)
        tp = ts + UNK5A
        assert tp == 0x50, hex(tp)  # quarter 0 of page 20 = this bank's block
        pal = pals[PAL + clut]
        for yy in range(b - t):
            for xx in range(r - l):
                v = blk[(t + yy) * 128 + l + xx]
                if v:
                    sx = ox + ((r - l - 1 - xx) if fl & 2 else xx)
                    sy = oy + ((b - t - 1 - yy) if fl & 1 else yy)
                    spr[(sx, sy)] = pal[v]
    xs = [x for x, _ in spr]
    ys = [y for _, y in spr]
    sw, sh = (max(xs) - min(xs) + 1) * SCALE, (max(ys) - min(ys) + 1) * SCALE
    assert sw <= W and sh <= H, (sw, sh)
    x0, y0 = (W - sw) // 2, (H - sh) // 2
    px = [0x8000] * (W * H)
    for (x, y), c in spr.items():
        for dy in range(SCALE):
            for dx in range(SCALE):
                X = x0 + (x - min(xs)) * SCALE + dx
                Y = y0 + (y - min(ys)) * SCALE + dy
                px[Y * W + X] = c | 0x8000
    return struct.pack("<%dH" % (W * H), *px)


if __name__ == "__main__":
    print(len(picture()))
