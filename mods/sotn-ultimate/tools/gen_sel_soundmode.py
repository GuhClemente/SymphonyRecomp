#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: the main menu's "Sound mode" button (src/st/sel/2C048.c,
# MOD_MENU_SOUND), in the style of the retail File select / Name change /
# File copy / File delete buttons.
#
# The retail buttons are 128x32 4bpp images in F_SEL.BIN (VRAM tpage 0xC,
# x 768, CLUT 0x200; 0x203 under the cursor): File copy u 0 v 0x80, File
# delete u 0 v 0xA0, Name entry u 0 v 0xC0, Name change u 0 v 0xE0, File
# select u 0x80 v 0x90. They share one template (blue capital block at
# x 0-31, yellow carvings) with the word drawn over it.
#   - template: per pixel, the most common value among the buttons whose
#     word does not cover it (word = colours 3/4/5 and their black outline)
#   - capital block: the template's (F and N blocks merged) with a capital S
#     traced at 1.5x from the menu title's 16x16 gothic "S" (DrawString16x16's
#     sheet, u 0xA0 v 0xD0), shaded and outlined like the F / N (CAP_MASK)
#   - "ound mode": minuscules cut from the retail words (o: copy, n: change,
#     u: change's n turned 180 degrees, d: delete, m / e: Name), spaced like
#     the retail words
#
# Output: src/st/sel/mod_sel_soundmode_gfx.h (4bpp, 4 pixels per u16, 32
# rows of 32 u16) and, with PIL, previews in build/us (or argv[1]):
# sel_soundmode_button.png and sel_soundmode_menu.png (offline mockup of
# the main menu with the 2C048.c layout).
import os
import struct
import sys
from collections import Counter

os.chdir(os.path.join(os.path.dirname(__file__), "../../.."))
OUT_H = "src/st/sel/mod_sel_soundmode_gfx.h"
OUT_DIR = sys.argv[1] if len(sys.argv) > 1 else "build/us"

# F_SEL.BIN: 24 0x2000-byte chunks of 32x128 halfwords (dra/cd.c
# CdCallback_StageChr: x D_800AC998[n], y (n & 2) ? 0x80 : 0)
CHUNK_X = [0x200, 0x220, 0x200, 0x220, 0x240, 0x260, 0x240, 0x260, 0x280,
           0x2A0, 0x280, 0x2A0, 0x2C0, 0x2E0, 0x2C0, 0x2E0, 0x300, 0x320,
           0x300, 0x320, 0x340, 0x360, 0x340, 0x360]
data = open("disks/us/ST/SEL/F_SEL.BIN", "rb").read()
vram = [[0] * 1024 for _ in range(256)]
for n, x0 in enumerate(CHUNK_X):
    y0 = (n << 6) & 0x80
    for y in range(128):
        for x in range(32):
            vram[y0 + y][x0 + x] = struct.unpack_from(
                "<H", data, n * 0x2000 + (y * 32 + x) * 2)[0]


def idx4(xb, u, v):
    return (vram[v][xb + u // 4] >> ((u & 3) * 4)) & 15


def crop(xb, u, v, w, h):
    return [[idx4(xb, u + x, v + y) for x in range(w)] for y in range(h)]


W, H = 128, 32
BUTTONS = {
    "filecopy": (0, 0x80), "filedelete": (0, 0xA0), "nameentry": (0, 0xC0),
    "namechange": (0, 0xE0), "fileselect": (0x80, 0x90),
}
btn = {k: crop(0x300, u, v, W, H) for k, (u, v) in BUTTONS.items()}
WORD = (3, 4, 5)


def word_mask(b):
    # the word's pixels and their outline (the block's frame never counts)
    m = [[False] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            if x < 8 or y < 7:
                continue
            for yy in range(max(0, y - 1), min(H, y + 2)):
                for xx in range(max(0, x - 1), min(W, x + 2)):
                    if b[yy][xx] in WORD:
                        m[y][x] = True
    return m


masks = {k: word_mask(b) for k, b in btn.items()}
tmpl = [[0] * W for _ in range(H)]
for y in range(H):
    for x in range(W):
        c = Counter(btn[k][y][x] for k in btn if not masks[k][y][x])
        if c:
            tmpl[y][x] = c.most_common(1)[0][0]
        elif x < 31:
            tmpl[y][x] = None  # under both capitals: filled below
for y in range(H):  # the block's texture, from 7 / 14 / .. pixels left
    for x in range(W):
        if tmpl[y][x] is None:
            for dx in (-7, 7, -14, 14, -21, 21):
                if 0 <= x + dx < W and tmpl[y][x + dx] not in (None,)                         and not any(masks[k][y][x + dx] for k in btn):
                    tmpl[y][x] = tmpl[y][x + dx]
                    break
            else:
                tmpl[y][x] = 6
img = [row[:] for row in tmpl]


def put(gl, ox, oy):
    # gl: {(x, y): value}; word colours win over outlines
    for (x, y), v in gl.items():
        x += ox
        y += oy
        if 0 <= x < W and 0 <= y < H:
            if v == 1 and img[y][x] in WORD and (x, y) in placed:
                continue
            img[y][x] = v
            placed.add((x, y))


placed = set()

# capital S: the main menu title's S ("Select your destiny" is drawn with
# the 16x16 gothic font of this sheet, S at u 0xA0 v 0xD0, 15x14 px),
# traced by hand at 1.5x over a nearest-neighbour 1.5x guide (a plain
# scale merges or drops its 1 px strokes and separators): same top curl,
# swash arm, double-banded spine, bowl and tail, with the 2-4 px strokes of
# the F / N capitals (two rows added to the spine and the bowl to reach
# their height). Shaded like the F / N: white body and left edges, light
# grey (4) on top / right / bottom edges and inside those of wide strokes,
# dark grey (3) on corners, black (1) outline; palette indices only.
CAP_MASK = """
.........######........
......####...#####.....
.....###.......###.....
.....###.......##...#..
.##...###.........###..
.###..####.......###...
..###..#####....##.....
...##############......
....###.##########.....
.....#############.....
.....###############...
........#######..####..
..........######..###..
...........######..###.
............###...####.
............##....###..
...........##.....###..
...........##.....###..
.....#########...###...
....###########.###....
..#####.....######.....
""".split()


def cap_shade(mask):
    h, w = len(mask), len(mask[0])

    def on(x, y):
        return 0 <= x < w and 0 <= y < h and mask[y][x] == "#"

    rows = []
    for y in range(h):
        row = ""
        for x in range(w):
            if not on(x, y):
                row += "."
                continue
            out = {d for d, (dx, dy) in
                   {"L": (-1, 0), "R": (1, 0), "T": (0, -1), "B": (0, 1)}.items()
                   if not on(x + dx, y + dy)}
            if not out or out == {"L"}:
                v = 5
                # wide strokes: a second grey row / column inside the
                # right / bottom edges, like the F / N stems (5 5 4 3)
                if (not out and (not on(x + 2, y) or not on(x, y + 2))
                        and on(x - 1, y) and on(x, y - 1)
                        and on(x - 1, y - 1)):
                    v = 4
            elif len(out - {"L"}) >= 2 or out >= {"L", "T"}:
                v = 3
            else:
                v = 4
            row += str(v)
        rows.append(row)
    lo = min(len(r) - len(r.lstrip(".")) for r in rows)
    hi = max(len(r.rstrip(".")) for r in rows)
    return [r[lo:hi] for r in rows]


CAP_ART = cap_shade(CAP_MASK)
cap = {(x, y): int(c) for y, row in enumerate(CAP_ART)
       for x, c in enumerate(row) if c != "."}
for (x, y) in list(cap):
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if (x + dx, y + dy) not in cap:
                cap[(x + dx, y + dy)] = 1
# right edge at x 30 and baseline at row 28 like the retail F / N
CAP_X, CAP_Y = 31 - len(CAP_ART[0]), 29 - len(CAP_ART)
put(cap, CAP_X, CAP_Y)


def glyph(name, x0, x1, rot=False, y0=12, y1=31):
    b = btn[name]
    g = {}
    for y in range(y0, y1):
        for x in range(x0, x1 + 1):
            v = b[y][x]
            if v in WORD or (v == 1 and any(
                    b[yy][xx] in WORD
                    for yy in range(y - 1, y + 2)
                    for xx in range(max(x0, x - 1), min(x1, x + 1) + 1))):
                g[(x - x0, y)] = v
    if rot:  # 180 degrees about the x-height band (rows 21-29)
        g = {(x1 - x0 - x, 50 - y): v for (x, y), v in g.items()}
    return g


GLYPHS = {
    "o": glyph("filecopy", 85, 95),
    "n": glyph("namechange", 96, 107),
    "d": glyph("filedelete", 71, 82),
    "m": glyph("nameentry", 41, 57),
    "e": glyph("nameentry", 57, 67),
}
GLYPHS["u"] = glyph("namechange", 96, 107, rot=True)
# copy's o reads as a u at this size: close its top and bottom joins
GLYPHS["o"].update({(5, 22): 4, (5, 28): 4, (6, 28): 4})


def extent(g):
    xs = [x for (x, y) in g]
    return min(xs), max(xs)


x = 31
for word in ("ound", "mode"):
    for ch in word:
        g = GLYPHS[ch]
        lo, hi = extent(g)
        put(g, x - lo, 0)
        x += hi - lo  # share the outline column
    x += 5
WORD_END = x - 5

# 4bpp header
lines = ["// generated by mods/sotn-ultimate/tools/gen_sel_soundmode.py, do not edit",
         "#define MOD_SOUND_BTN_W %d" % W, "#define MOD_SOUND_BTN_H %d" % H,
         "static const u16 g_ModSoundBtnPix[] MOD_RO __attribute__((aligned(4))) = {"]
for y in range(H):
    words = []
    for x in range(0, W, 4):
        words.append("0x%04X" % sum(img[y][x + i] << (i * 4) for i in range(4)))
    lines.append("    " + ", ".join(words) + ",")
lines.append("};")
text = "\n".join(lines) + "\n"
if not os.path.exists(OUT_H) or open(OUT_H).read() != text:
    open(OUT_H, "w").write(text)
print("text ends at x %d" % WORD_END)

try:
    from PIL import Image
except ImportError:
    sys.exit(0)


def png_pal(path, n):
    im = Image.open(path)
    if im.mode == "P":
        p = im.getpalette()
        return [tuple(p[i * 3:i * 3 + 3]) for i in range(n)]
    im = im.convert("RGB")
    return [im.getpixel((i % im.width, i // im.width)) for i in range(n)]


SEL = "assets/st/sel/"
CLUT = {0x200 + i: png_pal(SEL + "us/D_8018CD54/D_8018CD54_%d.png" % i, 16)
        for i in range(3)}
CLUT[0x203] = png_pal(SEL + "D_80180764/D_80180764.png", 16)


def render(pix, clut):
    pal = CLUT[clut]
    im = Image.new("RGBA", (len(pix[0]), len(pix)), (0, 0, 0, 0))
    for y, row in enumerate(pix):
        for x, v in enumerate(row):
            if v:
                im.putpixel((x, y), pal[v] + (255,))
    return im


os.makedirs(OUT_DIR, exist_ok=True)
pv = Image.new("RGBA", (W * 2 + 8, H * 3 + 8), (40, 40, 40, 255))
pv.alpha_composite(render(img, 0x200), (0, 0))
pv.alpha_composite(render(img, 0x203), (W + 8, 0))
pv.alpha_composite(render(btn["namechange"], 0x200), (0, H + 4))
pv.alpha_composite(render(btn["fileselect"], 0x200), (W + 8, H + 4))
pv.alpha_composite(render(btn["filecopy"], 0x200), (0, 2 * H + 8))
pv.alpha_composite(render(btn["filedelete"], 0x200), (W + 8, 2 * H + 8))
pv.resize((pv.width * 4, pv.height * 4), Image.NEAREST).save(
    os.path.join(OUT_DIR, "sel_soundmode_button.png"))

# mockup of the main menu (2C048.c InitMainMenuUI / func_801AD490)
scr = Image.new("RGBA", (384, 240), (0, 0, 0, 255))
bgpal = png_pal(SEL + "D_8018CDB4/D_8018CDB4.png", 256)
for i in range(3):  # 8bpp tpages 0x89-0x8B, CLUT 0x210
    xb = 0x240 + i * 64
    for y in range(240):
        for x in range(128):
            w = vram[y][xb + x // 2]
            scr.putpixel((i * 128 + x, y), bgpal[(w >> ((x & 1) * 8)) & 0xFF]
                         + (255,))
scr.alpha_composite(render(crop(0x340, 0, 0, 0x90, 0x48), 0x201), (0xD8, 0x10))


def gothic(s, x, y, clut=0x200):
    for ch in s:
        i = ord(ch) - ord("a")
        scr.alpha_composite(render(crop(0x300, 0x80 + (i & 7) * 16,
                                        0xB0 + (i >> 3) * 16, 16, 16), clut),
                            (x, y))
        x += 16


gothic("select", 0xF0, 0x20)
gothic("your", 0x100, 0x30)
gothic("destiny", 0xE8, 0x40)
for k, (u, v) in enumerate([(0xA0, 0x50), (0xA0, 0x70)]):
    scr.alpha_composite(render(crop(0x300, u, v, 16, 16), 0x200),
                        (0x20, 0xB8 + k * 16))
order = [btn["fileselect"], btn["namechange"], btn["filecopy"],
         btn["filedelete"], img]
sys.path.insert(0, "mods/sotn-ultimate/tools")
for i, b in enumerate(order, 1):
    x = 0x10 + (i - 1) * 0x34  # MOD_MENU_BTN_X
    y = 0x1C + (i - 1) * 0x20  # MOD_MENU_BTN_Y
    scr.alpha_composite(render(b, 0x203 if i == 5 else 0x200), (x, y))
scr.resize((768, 480), Image.NEAREST).save(
    os.path.join(OUT_DIR, "sel_soundmode_menu.png"))
