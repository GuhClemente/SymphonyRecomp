#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Saturn's own HUD art for the playable Maria.

The Saturn draws Maria's HUD with its compact status code (GAME.PRG
func_0607672C / func_06076A04 UpdateCompactStatusHud) from MAR_W.CHR
package 5 (MARIA.PRG resource 5): the vine gauge holder (image 5, 48x96),
the MARIA plate (image 0, 80x24), the digits 0-9 (images 6-15, 8x8) and an
8x8 bar tile (image 17). DRA draws it on the PS1 (src/dra/5F60C.c,
MOD_SATURN_MARIA), so the art has to be in VRAM already: this writes it into
a part of F_GAME2.BIN's VRAM image (the Richter/Maria game graphics) that
the retail file leaves empty and nothing writes at run time,
tpage 0x18 u128-255 v136-231 (VRAM x 544-575, y 392-487):

  u128 v136  holder 38x96 (narrowed: Saturn columns x%5 == 4 dropped, at
             its Saturn x 2, like all the Saturn art)
  u168 v136  plate 64x24 (16 columns of plain background / padding dropped,
             so the lettering keeps its shapes)
  u168 v160  digits 0-9, 7x8 each (the empty eighth column dropped)
  u240 v136  bar fills, 96 rows: u240-243 HP (Saturn tile columns 3,4,6,7 =
             the PS1 x 6-9 of the narrowed tube), u245-247 MP

Colours stay in DRA (the plate's glow is computed every frame); the file's
palettes are unchanged. Richter never samples these pixels.
Output: build/us/F_GAME2.BIN (make disk takes it over disks/us/BIN's).
"""
import os
import struct
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SRC = os.path.join(ROOT, "disks/us/BIN/F_GAME2.BIN")
OUT = os.path.join(ROOT, "build/us/F_GAME2.BIN")
PRG = os.path.join(ROOT, "disks/saturn/MARIA.PRG")
CHR = os.path.join(ROOT, "disks/saturn/MAR_W.CHR")

PRG_BASE = 0x060A5000
RES_TABLE = 0x060C6D68  # {images, palettes, u16, u16} per resource
# MAR_W.CHR holds the packages in g_MariaSpriteBanks order
CHR_ORDER = (1, 2, 3, 8, 4, 6, 5, 7, 9, 10, 11, 12)

# F_GAME2: 32 chunks of 32x128 halfwords, then the CLUTs (cd.c D_800AC958)
CHUNK_X = [0x200, 0x220, 0x200, 0x220, 0x240, 0x260, 0x240, 0x260, 0x280,
           0x2A0, 0x280, 0x2A0, 0x2C0, 0x2E0, 0x2C0, 0x2E0, 0x300, 0x320,
           0x300, 0x320, 0x340, 0x360, 0x340, 0x360, 0x380, 0x3A0, 0x380,
           0x3A0, 0x3C0, 0x3E0, 0x3C0, 0x3E0]

TPAGE_X, TPAGE_Y = 0x200, 0x100  # tpage 0x18
PLATE_DROP = {12, 13, 14, 15, 16, 44, 48, 58, 59, 60, 61, 62, 63, 77, 78, 79}


def package_images(prg, chr_, res):
    """-> list of (w, h, pixel rows) for the resource's images"""
    base = 0
    for r in CHR_ORDER:
        im = struct.unpack_from(">I", prg, RES_TABLE - PRG_BASE + r * 12)[0]
        o = im - PRG_BASE
        recs = []
        while True:
            w, h, u = struct.unpack_from(">BBH", prg, o)
            o += 4
            if w == 0 and h == 0:
                size = u * 8
                break
            recs.append((w * 2, h * 2, u * 8))
        if r == res:
            out = []
            for w, h, off in recs:
                d = chr_[base + off:base + off + w * h // 2]
                px = []
                for b in d:
                    px += [b >> 4, b & 15]
                out.append((w, h, [px[y * w:(y + 1) * w] for y in range(h)]))
            return out
        base += size
    raise ValueError(res)


def cols(rows, keep):
    return [[row[c] for c in keep] for row in rows]


def build_art():
    prg = open(PRG, "rb").read()
    chr_ = open(CHR, "rb").read()
    img = package_images(prg, chr_, 5)
    art = []  # (u, v, rows)
    w, h, holder = img[5]
    art.append((128, 136, cols(holder, [c for c in range(w) if (2 + c) % 5 != 4])))
    w, h, plate = img[0]
    art.append((168, 136, cols(plate, [c for c in range(w) if c not in PLATE_DROP])))
    for n in range(10):
        w, h, dig = img[6 + n]
        art.append((168 + 7 * n, 160, cols(dig, range(7))))
    w, h, tile = img[17]
    row = [tile[0][c] for c in (3, 4, 6, 7)] + [0] + [tile[0][c] for c in (3, 4, 6)]
    art.append((240, 136, [row] * 96))
    for u, v, rows in art:
        assert u + len(rows[0]) <= 256 and v + len(rows) <= 232, (u, v)
    assert len(art[0][2][0]) == 38 and len(art[1][2][0]) == 64
    return art


def chunk_offset(hx, vy):
    """byte offset in F_GAME2 of VRAM halfword (hx, vy)"""
    for i, x in enumerate(CHUNK_X):
        y = ((i << 6) & 0x80) + 0x100
        if x <= hx < x + 32 and y <= vy < y + 128 and i != 0x1A:
            return i * 0x2000 + (vy - y) * 64 + (hx - x) * 2
    raise ValueError((hx, vy))


def main():
    data = bytearray(open(SRC, "rb").read())
    assert len(data) == 0x42000
    # every 4bpp pixel of the target block must be empty in the retail file
    for vy in range(136, 232):
        for hx in range(32, 64):
            off = chunk_offset(TPAGE_X + hx, TPAGE_Y + vy)
            assert data[off] == 0 and data[off + 1] == 0, (hx * 4, vy)
    px = {}
    for u, v, rows in build_art():
        for y, row in enumerate(rows):
            for x, c in enumerate(row):
                px[(u + x, v + y)] = c
    for vy in range(136, 232):
        for hx in range(32, 64):
            val = 0
            for k in range(4):
                val |= px.get((hx * 4 + k, vy), 0) << (4 * k)
            off = chunk_offset(TPAGE_X + hx, TPAGE_Y + vy)
            struct.pack_into("<H", data, off, val)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, "wb").write(data)
    if len(sys.argv) > 1:  # preview: the block, palette indices as grey
        from PIL import Image
        im = Image.new("L", (128, 96))
        for (x, y), c in px.items():
            im.putpixel((x - 128, y - 136), c * 17)
        im.resize((512, 384), Image.NEAREST).save(sys.argv[1])
    print("F_GAME2.BIN: Maria's HUD art at tpage 0x18 u128-255 v136-231")


if __name__ == "__main__":
    main()
