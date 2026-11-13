#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: the Saturn's New Game character select portraits.
#
# The Saturn draws its character select (TITLE.PRG states 28/29) with three
# VDP1 sprites, not VDP2 cells: TITLE.PRG registers sprite banks with
# func_0600AE30 (0.BIN) in the order below, and their pixels follow one
# another in TITLE.CHR (loaded at VDP1 VRAM 0x13980). A bank is {w/2, h/2,
# u16 offset in 8-byte units} records, a {0, 0, end} terminator, then
# (usually) a palette: u16 header (0x4010 = 16 banks of 16 colours), RGB555.
# The portraits are 80x88 8bpp banks 0x060B1666 / 0x060B1870 / 0x060B1A7A
# (Alucard, Richter, Maria; bank handles 0x060B3060[i]); the draw routine
# 0x060AA2FC picks the palette by handle 0x060B3074[i], which are the
# palettes registered first (func_0600AEE4): Alucard's is the one after bank
# 0x060B1438 (0x060B1464), Richter's after Alucard's bank (0x060B166E),
# Maria's after Richter's bank (0x060B1878). Only indices 0..127 are used;
# 0 is transparent. Positions (0x060A8986..): Richter (72,124), Alucard
# (168,124), Maria (264,124), sprite centres on the Saturn's screen.
#
# Output: src/st/sel/mod_charsel_gfx.h (rodata: 8bpp pixels, two per u16,
# and 128-colour PS1 CLUTs) + a preview PNG in build/us.
import struct
import sys
import os

os.chdir(os.path.join(os.path.dirname(__file__), "../../.."))
SAT = "disks/saturn/"
BASE = 0x060A5000
prg = open(SAT + "TITLE.PRG", "rb").read()
chr_ = open(SAT + "TITLE.CHR", "rb").read()

# banks in registration order (0x060A9598), with their bit depth
BANKS = [(0x60B1116, 4), (0x60B1328, 8), (0x60B1438, 4), (0x60B1666, 8),
         (0x60B1870, 8), (0x60B1A7A, 8)]
PORTRAITS = [  # name, bank, palette header
    ("alucard", 0x60B1666, 0x60B1464),
    ("richter", 0x60B1870, 0x60B166E),
    ("maria", 0x60B1A7A, 0x60B1878),
]
W, H, NCOL = 80, 88, 128


def walk():
    """CHR byte offset of each bank's first image."""
    cur, out = 0, {}
    for addr, _ in BANKS:
        o = addr - BASE
        out[addr] = cur
        k = 0
        while True:
            w, h, off = struct.unpack_from(">BBH", prg, o + 4 * k)
            if w == 0:
                cur += off * 8
                break
            k += 1
    return out


def ps1_colour(c, index):
    if index == 0:
        return 0  # transparent
    v = c & 0x7FFF  # same RGB555 bit order; Saturn bit 15 is not PS1 STP
    return v if v else 0x8000  # opaque black needs STP on the PS1


# The rest of the Player select screen (frames, "Player select" header, the
# gothic names) is a VDP2 4bpp cell layer, not sprites. TITLE.MAP is loaded
# at LWRAM 0x256000 (0.BIN 0x06005488), but TITLE.PRG addresses it from
# ASCII.FON's 0x252000, so its offsets are MAP offset + 0x4000. The menu
# layer's characters and palette are the LZSS streams at table 0x060AC870
# ({0x664CC, 0x4D46}, {0x6B212, 0x112}); func_060A8A58 copies the screens'
# raw 40x64-cell name tables from table 0x060AC880 (0x40FC0 ... 0x44BC0, one
# per menu: the Player select screen is the first 32 rows of 0x40FC0). Cell
# rows/columns below are in that screen; the layer is drawn 8 px right of
# the sprites (the portraits' centres 72/168/264 sit in the frames then).
# Frames + names use banks 7/13/14 (identical colours; the Saturn rewrites a
# bank per item to show the cursor, colour 0 = black for the chosen one),
# the header bank 6.
MAP_DELTA = 0x4000
LAYER_CHARS = (0x664CC, 0x4D46)
LAYER_PAL = (0x6B212, 0x112)
LAYER_MAP = 0x40FC0
LAYER_ELEMS = [  # name, col, row, w, h (cells), guard
    ("header", 2, 4, 18, 4, None),
    ("frame", 15, 8, 11, 15, None),  # the middle (Alucard) frame
    ("name_alucard", 15, 23, 11, 3, None),
    ("name_richter", 3, 23, 11, 3, None),
    ("name_maria", 27, 23, 11, 3, "MOD_SATURN_MARIA"),
]
LAYER_BANKS = {"header": 6}  # others 13


def lzss(data):
    sys.path.insert(0, "tools/saturn")
    import tile_dumper
    return tile_dumper.decompress_lzss(data)


def layer_art():
    mp = open(SAT + "TITLE.MAP", "rb").read()

    def stream(off, size):
        return lzss(mp[off - MAP_DELTA:off - MAP_DELTA + size])

    chars = stream(*LAYER_CHARS)
    pal = struct.unpack(">256H", stream(*LAYER_PAL))
    cells = struct.unpack_from(">1280H", mp, LAYER_MAP - MAP_DELTA)

    def tile(v):
        t, fx, fy = v & 0x3FF, v & 0x400, v & 0x800
        out = []
        for y in range(8):
            sy = 7 - y if fy else y
            for x in range(8):
                sx = 7 - x if fx else x
                c = chars[t * 32 + sy * 4 + sx // 2]
                out.append(c >> 4 if sx % 2 == 0 else c & 15)
        return tuple(out)

    pool, maps, banks = [], {}, {}
    for name, col, row, w, h, guard in LAYER_ELEMS:
        m = []
        for y in range(row, row + h):
            for x in range(col, col + w):
                v = cells[y * 40 + x]
                t = tile(v)
                if not any(t):
                    m.append(0)
                    continue
                banks.setdefault(name, set()).add(v >> 12)
                if t not in [p for p, _ in pool]:
                    pool.append((t, guard))
                m.append([p for p, _ in pool].index(t) + 1)
        maps[name] = m
    assert banks["header"] == {6}, banks
    assert all(b <= {2, 7, 13, 14} for k, b in banks.items() if k != "header")
    # guarded (Maria-only) tiles must come last to be cut out of the array
    first = [g for _, g in pool].index("MOD_SATURN_MARIA")
    assert all(g for _, g in pool[first:]), "shared tile after Maria's"
    assert len(pool) < 256
    cluts = []
    for b in (6, 13):
        cluts.append([ps1_colour(pal[b * 16 + i], i) for i in range(16)])
    # the cursor: TITLE.PRG 0x060A8478 (mode 6) writes 16 colours per item
    # into CRAM 0x25F00400 + 0x1A0/0xE0/0x1C0 (banks 13/7/14) when the cursor
    # moves: row 6 of the 20-row brightness ramp at 0x060ACB10 for the chosen
    # one, row 0 (the normal colours) for the others
    prg = open(SAT + "TITLE.PRG", "rb").read()
    ramp = struct.unpack_from(">16H", prg, 0x60ACB10 + 6 * 32 - BASE)
    cluts.append([ps1_colour(ramp[i], i) for i in range(16)])
    for b in (2, 7, 14):  # the other frames' banks: same colours
        assert pal[b * 16 + 1:b * 16 + 16] == pal[13 * 16 + 1:13 * 16 + 16]
    # the frame's inner window (the Saturn fills it with the navy colour 0):
    # flood fill of the transparent pixels from the frame's centre
    w, h = 11 * 8, 15 * 8
    px = [0] * (w * h)
    m = maps["frame"]
    for k, v in enumerate(m):
        if v:
            t = pool[v - 1][0]
            for j in range(64):
                px[(k // 11 * 8 + j // 8) * w + k % 11 * 8 + j % 8] = t[j]
    seen, todo = set(), [(w // 2 - 4, h // 2)]
    while todo:
        x, y = todo.pop()
        if (x, y) in seen or not (0 <= x < w and 0 <= y < h) or px[y * w + x]:
            continue
        seen.add((x, y))
        todo += [(x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)]
    xs = [x for x, _ in seen]
    ys = [y for _, y in seen]
    win = (min(xs), min(ys), max(xs) + 1 - min(xs), max(ys) + 1 - min(ys))
    assert len(seen) * 100 >= 90 * win[2] * win[3], (win, len(seen))
    return pool, maps, cluts, win


def layer_lines(pool, maps, cluts, win):
    lines = [
        "// The Player select screen's VDP2 layer (TITLE.MAP): 8x8 4bpp tiles",
        "// (PS1 nibble order, one u32 per row) and per-element cell maps",
        "// (0 = empty, n = tile n - 1); CLUT 0 = header, 1 = frame/names,",
        "// 2 = frame/names of the chosen character. WIN = the frame's inner",
        "// window (Saturn pixels from the frame's top left).",
        "#define MOD_CHARSEL_WIN_X %d" % win[0],
        "#define MOD_CHARSEL_WIN_Y %d" % win[1],
        "#define MOD_CHARSEL_WIN_W %d" % win[2],
        "#define MOD_CHARSEL_WIN_H %d" % win[3],
    ]
    for name, col, row, w, h, guard in LAYER_ELEMS:
        lines.append("#define MOD_CHARSEL_%s_W %d" % (name.upper(), w))
        lines.append("#define MOD_CHARSEL_%s_H %d" % (name.upper(), h))
    lines.append("const u32 g_ModCharselTiles[] MOD_RO = {")
    open_guard = None
    for t, guard in pool:
        if guard != open_guard:
            lines.append("#if defined(%s)" % guard)
            open_guard = guard
        words = []
        for y in range(8):
            v = 0
            for x in range(8):
                v |= t[y * 8 + x] << (4 * x)
            words.append("0x%08X" % v)
        lines.append("    " + ", ".join(words) + ",")
    if open_guard:
        lines.append("#endif")
    lines.append("};")
    for name, col, row, w, h, guard in LAYER_ELEMS:
        if guard:
            lines.append("#if defined(%s)" % guard)
        lines.append("const u8 g_ModCharselMap_%s[%d] MOD_RO = {" % (name, w * h))
        m = maps[name]
        for j in range(0, len(m), w):
            lines.append("    " + ", ".join("%d" % v for v in m[j:j + w]) + ",")
        lines.append("};")
        if guard:
            lines.append("#endif")
    lines.append("const u16 g_ModCharselClut4[%d][16] MOD_RO = {" % len(cluts))
    for c in cluts:
        lines.append("    {" + ", ".join("0x%04X" % v for v in c) + "},")
    lines.append("};")
    return lines


def main():
    offs = walk()
    images, cluts = [], []
    for name, bank, pal in PORTRAITS:
        o = bank - BASE
        w, h, off = struct.unpack_from(">BBH", prg, o)
        assert (w * 2, h * 2, off) == (W, H, 0), (name, w, h, off)
        start = offs[bank]
        px = chr_[start:start + W * H]
        assert len(px) == W * H and max(px) < NCOL, name
        hdr = struct.unpack_from(">H", prg, pal - BASE)[0]
        assert hdr == 0x4010, (name, hex(hdr))
        cols = struct.unpack_from(">%dH" % NCOL, prg, pal - BASE + 2)
        images.append(px)
        cluts.append([ps1_colour(c, i) for i, c in enumerate(cols)])

    lines = [
        "// Generated by mods/sotn-ultimate/tools/gen_title_portraits.py from the Saturn's",
        "// TITLE.PRG/TITLE.CHR: the New Game character select portraits, 80x88",
        "// 8bpp (two pixels per u16, left pixel in the low byte) and their",
        "// 128-colour CLUTs. Do not edit.",
        "#define MOD_CHARSEL_TEX_W %d" % W,
        "#define MOD_CHARSEL_TEX_H %d" % H,
        "#define MOD_CHARSEL_CLUT_LEN %d" % NCOL,
    ]
    for i, (name, _, _) in enumerate(PORTRAITS):
        guard = name == "maria"
        if guard:
            lines.append("#if defined(MOD_SATURN_MARIA)")
        words = [images[i][j] | images[i][j + 1] << 8 for j in range(0, W * H, 2)]
        lines.append("const u16 g_ModCharselPix_%s[%d] MOD_RO = {" % (name, len(words)))
        for j in range(0, len(words), 12):
            lines.append("    " + ", ".join("0x%04X" % v for v in words[j:j + 12]) + ",")
        lines.append("};")
        lines.append("const u16 g_ModCharselClut_%s[%d] MOD_RO = {" % (name, NCOL))
        for j in range(0, NCOL, 12):
            lines.append("    " + ", ".join("0x%04X" % v for v in cluts[i][j:j + 12]) + ",")
        lines.append("};")
        if guard:
            lines.append("#endif")
    pool, maps, cluts4, win = layer_art()
    lines += layer_lines(pool, maps, cluts4, win)
    text = "\n".join(lines) + "\n"
    path = "src/st/sel/mod_charsel_gfx.h"
    if not os.path.exists(path) or open(path).read() != text:
        open(path, "w").write(text)

    try:
        from PIL import Image
    except ImportError:
        return
    # preview of the layout on the PS1 menu screen (see 2C048.c)
    def rgb(c):
        return ((c & 31) << 3, (c >> 5 & 31) << 3, (c >> 10 & 31) << 3, 255)

    def elem(name, clut):
        w = [e for e in LAYER_ELEMS if e[0] == name][0][3]
        m = maps[name]
        img = Image.new("RGBA", (w * 8, len(m) // w * 8))
        for k, v in enumerate(m):
            if True:
                t = pool[v - 1][0] if v else (0,) * 64
                for j, ci in enumerate(t):
                    if clut[ci]:
                        img.putpixel((k % w * 8 + j % 8, k // w * 8 + j // 8), rgb(clut[ci]))
        return img.resize((img.width * 91 // 80, img.height), Image.NEAREST)

    im = Image.new("RGB", (384, 240), (16, 16, 32))
    hdr = elem("header", cluts4[0])
    im.paste(hdr, (192 + (24 - 168) * 91 // 80, 36), hdr)
    names = {0: "name_alucard", 1: "name_richter", 2: "name_maria"}
    for i, cx in ((1, 82), (0, 192), (2, 302)):
        x0 = cx - 46
        wx, wy = x0 + win[0] * 91 // 80, 68 + win[1]
        ww = win[2] * 91 // 80
        im.paste((0, 0, 66), (wx, wy, wx + ww, wy + win[3]))
        src = Image.new("RGBA", (W, H))
        for j, v in enumerate(images[i]):
            if v:
                src.putpixel((j % W, j // W), rgb(cluts[i][v]))
        cx0, cy0 = (W - win[2]) // 2, (H - win[3]) // 2
        src = src.crop((cx0, cy0, cx0 + win[2], cy0 + win[3])).resize((ww, win[3]), Image.NEAREST)
        im.paste(src, (wx, wy), src)
        fr = elem("frame", cluts4[2 if i == 0 else 1])
        im.paste(fr, (x0, 68), fr)
        nm = elem(names[i], cluts4[2 if i == 0 else 1])
        im.paste(nm, (x0, 188), nm)
    os.makedirs("build/us", exist_ok=True)
    im.save("build/us/mod_charsel_preview.png")


if __name__ == "__main__":
    sys.exit(main())
