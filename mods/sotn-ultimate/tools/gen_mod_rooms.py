#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: room and layer table copies for stages whose rooms change.
#
# A stage's rooms[] and rooms_layers[] live in its data section, which must
# stay byte-identical to retail (mods/sotn-ultimate/tools/check_stage_data.py). This writes
# <dir>/mod_rooms.h with const copies (rodata, included from the stage's
# e_red_door.c) carrying the mod's changes; the stage header points
# g_Overlay.rooms / .tileLayers at them under MOD_SATURN (a pointer into the
# overlay's own rodata, which check_stage_data allows).
#
# Castle Entrance (NP3), as on the Saturn (STAGE_01 room 17): the shaft
# (2,41)-(2,42) grows to (2,44), its art rebuilt from NP3's own tiles
# (mods/sotn-ultimate/tools/saturn_tilematch.py --art-only) and its collision taken from the
# Saturn. NP3 has no tile for the Saturn's rock slopes (its slope tiles are
# wooden stairs), so the foreground uses a tileset copy (rodata): each
# (art, collision) pair the shaft needs that NP3 lacks gets an index the
# shaft does not otherwise use, with that tile's art and the Saturn's
# collision. The indices the red door and the trapdoor write keep their
# retail entries. At the bottom right a
# loading room (3,44) and a transition cell (4,44) lead into the Underground
# Garden (MOD_TP_NP3_TO_GDN).
#
# The Saturn walls off the PSX save room door at (3,42) and moves the save
# room to (5,39). The mod keeps the PSX save room, one cell lower at (3,43),
# behind the Saturn shaft's dead-end alcove at rows 37-43 (open from local
# row 6 to 10, floor at 11): the alcove's right wall opens (col 15, rows
# 39-42) and the save room's left door (open rows 6-9, floor at 10) moves one
# tile down to match. Saves made in the old spot are moved by ApplySaveData
# (src/save_mgr.h).
import glob
import os
import re
import struct
import subprocess
import sys
from mod_work import work_dir  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
TP_NP3_TO_GDN = 0x83  # MOD_TP_NP3_TO_GDN, include/game.h
SAVE_ROOM = (3, 42)   # PSX save room, moves to (3, 43)
# Its entity layout (48) is an unused leftover: one E_ROOM_FOREGROUND with
# params 0, the loading corridor's angel statues, which garble a save room.
# It takes the layout of the stage's real save room (5,39) instead: the save
# room's top and bottom foreground pieces (params 1 at (128,48), 2 at
# (128,192)).
SAVE_ROOM_LAYOUT_FROM = (5, 39)
ALCOVE_ROWS = range(39, 43)  # shaft rows opened in the right wall (col 15)
# The shaft's rows 0-20 are the same on both consoles (same collision): they
# keep the retail PSX art, which rebuilding from the narrowed Saturn art only
# makes worse (dropping every 5th column breaks the thin edge lines).
RETAIL_ROWS = 21
# The Saturn's alcove ends in a wall, so its last columns are corner pieces;
# the mod continues it into the save room, so its ceiling (rows 36-38) and
# floor (rows 43-44) run straight on to the right edge.
ALCOVE_CEILING = (range(36, 39), 13, (14, 15))  # rows, source column, columns
ALCOVE_FLOOR = (range(43, 45), 12, (13, 14, 15))


TILEDEF_PARTS = ("tiles", "pages", "cluts", "cols")  # gfxPage, gfxIndex, clut, collision
TRAPDOOR_TILES = {0x129, 0x132, 0x6C8, 0x6C9}      # src/st/e_trapdoor.h


def asset_dir(stage):
    """a stage's asset folder (RNO3's are per version: assets/st/rno3/us)"""
    d = os.path.join(ROOT, f"assets/st/{stage}")
    return os.path.join(d, "us") if os.path.exists(os.path.join(d, "us", "layers.json")) else d


def gen_dir(stage):
    d = os.path.join(ROOT, f"src/st/{stage}/gen")
    return os.path.join(d, "us") if os.path.exists(os.path.join(d, "us", "layers.h")) else d


def tilematch(prg, mapf, room, layer, stage, tiledef, out, art_only=False, pool=None):
    subprocess.run([sys.executable, os.path.join(ROOT, "mods/sotn-ultimate/tools/saturn_tilematch.py"),
                    prg, mapf, str(room), str(layer),
                    os.path.join(ROOT, f"disks/us/ST/{stage.upper()}/F_{stage.upper()}.BIN"),
                    asset_dir(stage), tiledef, out]
                   + (["--art-only"] if art_only else [])
                   + (["--pool=" + ",".join(pool)] if pool else []),
                   check=True, stdout=subprocess.DEVNULL)
    d = open(out, "rb").read()
    return list(struct.unpack("<%dH" % (len(d) // 2), d))


# Hand-picked cells (user's call in game): target (row, col) <- source
# (row, col) art, "H" mirrored left-right / "V" top-bottom, keeping the
# target's collision. The left chamber's floor corner (30,2) blends with the
# floor as the mirrored corner from the top right (23,8).
TILE_OVERRIDES = [((30, 2), (23, 8), "H")]

PORT_MIN_ERR = 30   # cells whose best PSX match is worse than this get the Saturn's art (worst first, while slots last)


def port_cells(stage, pool_tilemap, td, free, tiles, coll, err, px, overrides=None, pal_pairs=None):
    """Saturn art for the cells no PSX tile matches.

    Each distinct 16x16 Saturn block (narrowed) whose best match error exceeds
    PORT_MIN_ERR, worst first, goes into a free 16x16 slot of the stage's F_
    texture (blank and unreferenced by any of its tilesets; pages 4-6, then
    0-3 above the palette rows at v 0xF0; page 7 is left alone), quantized to
    the palette of the room's own tiles that fits it best. It gets a tileset
    entry with the cell's Saturn collision. Writes build/us/F_<STAGE>.BIN,
    which makedisk uses instead of the retail file.

    pal_pairs: (Saturn cell art, PSX tile) pairs of cells known to show the
    same art (the kept retail rows). When given, colours are translated
    through them instead of matched by RGB: the inverted castle's Saturn
    palettes are grey where RNO3's are pink, so a plain RGB match picks the
    wrong shades."""
    import glob
    from collections import Counter
    up = stage.upper()
    fbin = bytearray(open(os.path.join(ROOT, f"disks/us/ST/{up}/F_{up}.BIN"), "rb").read())

    def hw_off(X, Y):  # halfword (x 0..511 from VRAM 0x200, y 0..255) -> file offset
        blk = (X // 64) << 2 | (Y // 128) << 1 | (X % 64) // 32
        return blk * 0x2000 + ((Y % 128) * 32 + X % 32) * 2

    def hw(X, Y):
        o = hw_off(X, Y)
        return fbin[o] | fbin[o + 1] << 8

    refs = set()
    for f in glob.glob(os.path.join(asset_dir(stage), "*_tiles.bin")):
        pre = f[:-len("_tiles.bin")]
        refs.update(zip(open(f, "rb").read(), open(pre + "_pages.bin", "rb").read()))
    slots = []
    for page in (4, 5, 6, 0, 1, 2, 3):
        for i in range(256):
            u, v = (i & 15) * 16, i & 0xF0
            if (page, i) in refs or (page < 4 and v >= 0xF0):
                continue
            if all(hw(page * 64 + u // 4 + k, v + y) == 0 for y in range(16) for k in range(4)):
                slots.append((page, i))

    d = open(os.path.join(asset_dir(stage), f"{pool_tilemap}.bin"), "rb").read()
    pool = set(struct.unpack("<%dH" % (len(d) // 2), d)) - {0}
    cluts = sorted({td["cluts"][t] for t in pool})

    def rgb(c):
        return ((c & 31) * 255 // 31, ((c >> 5) & 31) * 255 // 31, ((c >> 10) & 31) * 255 // 31)

    pals = {k: [hw(16 * (k % 16) + j, 0xF0 + k // 16) for j in range(16)] for k in cluts}

    votes = {}
    for block, t in pal_pairs or ():
        page, i, k = td["tiles"][t], td["pages"][t], td["cluts"][t]
        u, v = (i & 15) * 16, i & 0xF0
        for y in range(16):
            for x in range(16):
                q = (y * 16 + x) * 4
                if not block[q + 3]:
                    continue
                j = (hw(page * 64 + (u + x) // 4, v + y) >> (((u + x) % 4) * 4)) & 15
                if j:
                    votes.setdefault(k, {}).setdefault(bytes(block[q:q + 3]), Counter())[j] += 1

    def quantize_learned(block):
        best = None
        for k, table in votes.items():
            keys = list(table)
            idx, tot = [], 0
            for q in range(256):
                if not block[q * 4 + 3]:
                    idx.append(0)
                    continue
                c = bytes(block[q * 4:q * 4 + 3])
                if c in table:
                    near, e = c, 0
                else:
                    near, e = min(((kk, sum((x - y) ** 2 for x, y in zip(c, kk))) for kk in keys),
                                  key=lambda z: z[1])
                idx.append(table[near].most_common(1)[0][0])
                tot += e
            if best is None or tot < best[0]:
                best = (tot, k, idx)
        return best[1], best[2]

    def quantize(block):
        if votes:
            return quantize_learned(block)
        best = None
        for k, pal in pals.items():
            opts = [(j, rgb(pal[j])) for j in range(1, 16) if pal[j]]
            idx, tot = [], 0
            for q in range(256):
                r, g, b, a = block[q * 4:q * 4 + 4]
                if not a:
                    idx.append(0)
                    continue
                j, e = min(((j, (r - c[0]) ** 2 + (g - c[1]) ** 2 + (b - c[2]) ** 2)
                            for j, c in opts), key=lambda z: z[1])
                idx.append(j)
                tot += e
            if best is None or tot < best[0]:
                best = (tot, k, idx)
        return best[1], best[2]

    order = sorted((n for n in range(len(tiles)) if err[n] > PORT_MIN_ERR),
                   key=lambda n: -err[n])
    ported, skipped = {}, 0
    for n in order:
        block = px[n * 1024:(n + 1) * 1024]
        if block not in ported:
            if not slots:
                skipped += 1
                continue
            page, i = slots.pop(0)
            k, idx = quantize(block)
            u, v = (i & 15) * 16, i & 0xF0
            for y in range(16):
                for x4 in range(4):
                    val = 0
                    for b in range(4):
                        val |= idx[y * 16 + x4 * 4 + b] << (b * 4)
                    o = hw_off(page * 64 + u // 4 + x4, v + y)
                    fbin[o], fbin[o + 1] = val & 0xFF, val >> 8
            t = free.pop(0)
            td["tiles"][t], td["pages"][t], td["cluts"][t] = page, i, k
            td["cols"][t] = coll[n]
            ported[block] = t
        tiles[n] = ported[block]
    overridden = 0
    for (ty, tx), (sy, sx), flip in (TILE_OVERRIDES if overrides is None else overrides):
        src = tiles[sy * 16 + sx]
        page, i = td["tiles"][src], td["pages"][src]
        u, v = (i & 15) * 16, i & 0xF0
        pix = [[(hw(page * 64 + (u + x) // 4, v + y) >> (((u + x) % 4) * 4)) & 15
                for x in range(16)] for y in range(16)]
        if "H" in flip:
            pix = [row[::-1] for row in pix]
        if "V" in flip:
            pix = pix[::-1]
        page2, i2 = slots.pop(0)
        u2, v2 = (i2 & 15) * 16, i2 & 0xF0
        for y in range(16):
            for x4 in range(4):
                val = 0
                for b in range(4):
                    val |= pix[y][x4 * 4 + b] << (b * 4)
                o = hw_off(page2 * 64 + u2 // 4 + x4, v2 + y)
                fbin[o], fbin[o + 1] = val & 0xFF, val >> 8
        t = free.pop(0)
        td["tiles"][t], td["pages"][t], td["cluts"][t] = page2, i2, td["cluts"][src]
        td["cols"][t] = coll[ty * 16 + tx]
        tiles[ty * 16 + tx] = t
        overridden += 1
    os.makedirs(os.path.join(ROOT, "build/us"), exist_ok=True)
    open(os.path.join(ROOT, f"build/us/F_{up}.BIN"), "wb").write(fbin)
    print(f"{stage}: {len(ported)} Saturn tiles ported into F_{up}.BIN "
          f"+ {overridden} hand-picked ({len(slots)} slots left, {skipped} cells left matched)")


def narrow_region(cell_px, full, cells):
    """Re-narrow the given cells' columns from the full-width Saturn art.

    The plain 4/5 narrowing drops every 5th column, which can erase a 1px
    detail (the Saturn bear head's right fang). For the cells' column span,
    this drops instead, one by one, the column most like its neighbour, so
    thin lines survive. Returns cell_px with those cells rewritten."""
    fw, fh, fpx = full
    cols = sorted({n % 16 for n in cells})
    rows = sorted({n // 16 for n in cells})
    assert cols == list(range(cols[0], cols[-1] + 1)), cols
    x0, x1 = cols[0] * 20, (cols[-1] + 1) * 20      # Saturn columns of the span
    y0, y1 = rows[0] * 16, (rows[-1] + 1) * 16

    def column(x):
        return [fpx[(y * fw + x) * 4:(y * fw + x) * 4 + 4] for y in range(y0, y1)]

    def diff(a, b):
        return sum((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2
                   + (0 if (p[3] > 0) == (q[3] > 0) else 3 * 255 * 255)
                   for p, q in zip(a, b))

    keep = list(range(x0, x1))
    cache = {x: column(x) for x in keep}
    while len(keep) > (x1 - x0) * 4 // 5:
        # never drop the span's edge columns, so it still meets its neighbours
        i = min(range(1, len(keep) - 1),
                key=lambda i: min(diff(cache[keep[i]], cache[keep[i - 1]]),
                                  diff(cache[keep[i]], cache[keep[i + 1]])))
        del keep[i]
    out = bytearray(cell_px)
    for n in cells:
        r, c = n // 16, n % 16
        for y in range(16):
            for x in range(16):
                sx, sy = keep[(c - cols[0]) * 16 + x], r * 16 + y
                o = (sy * fw + sx) * 4
                q = n * 1024 + (y * 16 + x) * 4
                out[q:q + 4] = fpx[o:o + 4]
    return bytes(out)


def parse_layers(stage):
    s = open(os.path.join(gen_dir(stage), "layers.h")).read()
    externs = re.findall(r"^extern .*;$", s, re.M)
    body = s[s.index("static LayerDef layers[] = {"):]
    layers = re.findall(r"^\s*\{ (\w+), (\w+), \{(\d+), (\d+), (\d+), (\d+), (0x[0-9A-F]+)\}, (0x[0-9A-F]+), (0x[0-9A-F]+)\},$",
                        body[:body.index("};")], re.M)
    rd = s[s.index("RoomDef rooms_layers[] = {"):]
    roomdefs = re.findall(r"\{ &(\w+)(?:\[(\d+)\])?, &(\w+)(?:\[(\d+)\])? \}", rd[:rd.index("};")])
    return externs, layers, roomdefs


def parse_rooms(stage):
    s = open(os.path.join(gen_dir(stage), "rooms.c")).read()
    nums = [int(x, 0) & 0xFF for x in re.findall(r"-?0x[0-9A-Fa-f]+|-?\d+", s[s.index("{"):])]
    rooms = []
    while nums[0] != 0x40:
        rooms.append(nums[:8]); nums = nums[8:]
    return rooms


def ref(name, idx):
    return f"&g_ModLayers[{idx}]" if name == "layers" else "&g_ModLayerEmpty"


def smooth_np3_step(fg, bg, coll):
    """The slope below the alcove, smoothed (user's call).

    In the Saturn's collision the 45-degree slope from the shaft up to the
    alcove floor, (46,8) -> (45,9), meets a solid block at (44,10) and goes on
    one row up at (43,10): a 16 px step Alucard has to hop. The slope runs on
    instead, (44,10) -> (43,11), and reaches the alcove floor (row 43's top)
    at col 12: (43,10) opens, (43,11) and (44,10) take the slope piece of
    (45,9), the cells under them (44,11), (45,10) the piece under (45,9), and
    the slope-to-floor corner (43,11) with its floor-only collision moves to
    (43,12). The background repeats every 4 cells: cells that gain some take
    the ones 4 rows up."""
    def at(y, x):
        return y * 16 + x
    assert [coll[at(y, x)] for y, x in ((46, 8), (45, 9), (44, 10), (43, 10), (43, 11), (43, 12))] \
        == [0x80, 0x80, 0x03, 0x80, 0x01, 0x03], "NP3 step cells"
    slope, under, corner = at(45, 9), at(46, 9), at(43, 11)
    cells = {at(43, 12): (fg[corner], bg[at(43, 12)], coll[corner]),
             at(43, 11): (fg[slope], bg[at(43, 11)], 0x80),
             at(44, 10): (fg[slope], bg[at(40, 10)], 0x80),
             at(43, 10): (0, bg[at(43, 10)], 0x00),
             at(44, 11): (fg[under], bg[at(40, 11)], 0x03),
             at(45, 10): (fg[under], bg[at(41, 10)], 0x03)}
    for n, (t, b, c) in cells.items():
        fg[n], bg[n], coll[n] = t, b, c


# The first visit's Castle Entrance (NO3) gets the same shaft: the Saturn's
# STAGE_0X reads STAGE_01.MAP and has STAGE_01's room table (its first 32
# 6-byte records are identical, room 17 the shaft to (2,44), 24 the loading
# room (3,44), 31 the transition (4,44)). To make room, the Saturn puts the
# opening forest run on row 46 (its rooms 18 (0,46)-(17,46) and 19 (18,46));
# NO3's rooms 18 and 19 move there likewise (the teleports into the forest
# name the room, not its cells). NO3's overlay has no room for a 16 KB
# tileset copy (it must end before 0x801E0000): the shaft's art/collision
# pairs go into tile slots no NO3 tilemap or tile-writing entity uses, which
# the stage writes into its own tileset when a room loads (g_ModTiledefPatch,
# mods/sotn-ultimate/src/castle/no3/no3_mod_shaft.h). Its link to the Garden
# is MOD_TP_NO3_TO_GDN, which reloads NO3 if the player turns back in the
# loading room.
NO3_FOREST = {18: (0, 44, 17, 44, 46), 19: (18, 44, 18, 44, 46)}  # room: old rect, new row
TP_NO3_TO_GDN = 0x8B  # MOD_TP_NO3_TO_GDN, mods/sotn-ultimate/include/game_mod.h


def np3(stage="np3"):
    tdef = f"{stage}_tiledef_{stage}"
    patch = stage == "no3"  # tile slots written at run time, no tileset copy
    tp = TP_NO3_TO_GDN if patch else TP_NP3_TO_GDN
    tp_name = "MOD_TP_NO3_TO_GDN" if patch else "MOD_TP_NP3_TO_GDN"
    sat = [os.path.join(ROOT, "disks/saturn/STAGE_01.PRG"), os.path.join(ROOT, "disks/saturn/STAGE_01.MAP")]
    externs, layers, roomdefs = parse_layers(stage)
    rooms = parse_rooms(stage)
    if patch:
        for i, (l, t, r, b, row) in NO3_FOREST.items():
            assert rooms[i][:4] == [l, t, r, b], (i, rooms[i])
    shaft = 1
    assert rooms[shaft][:4] == [2, 41, 2, 42], rooms[shaft]
    load = next(i for i, r in enumerate(rooms) if r[:4] == [15, 41, 15, 41])
    fg_name, fg_i, bg_name, bg_i = roomdefs[rooms[shaft][4]]
    fg, bg = layers[int(fg_i)], layers[int(bg_i)]

    def read_tm(name):
        d = open(os.path.join(asset_dir(stage), f"{name}.bin"), "rb").read()
        return list(struct.unpack("<%dH" % (len(d) // 2), d))

    work = work_dir(stage)
    os.makedirs(work, exist_ok=True)
    # foreground art from the PSX shaft's own rock tiles only (the stage-wide
    # pool picks stairs, bricks and grates for the Saturn's diagonal edges)
    new_fg = tilematch(*sat, 17, 0, stage, tdef, f"{work}/shaft_fg.bin", art_only=True,
                       pool=[os.path.join(asset_dir(stage), f"{fg[0]}.bin")])
    new_bg = tilematch(*sat, 17, 2, stage, tdef, f"{work}/shaft_bg.bin")
    fg_coll = list(open(f"{work}/shaft_fg.bin.coll", "rb").read())
    fg_err = struct.unpack("<1024I", open(f"{work}/shaft_fg.bin.err", "rb").read())
    fg_px = open(f"{work}/shaft_fg.bin.px", "rb").read()
    assert len(new_fg) == len(new_bg) == len(fg_coll) == 16 * 64

    # foreground tileset copy pairing each tile's art with the Saturn collision
    td = {k: bytearray(open(os.path.join(asset_dir(stage), f"{tdef}_{k}.bin"), "rb").read())
          for k in TILEDEF_PARTS}
    door_src = open(os.path.join(ROOT, f"src/st/{stage}/e_red_door_tiles.c")).read()
    door_tiles = {int(v, 16) for v in re.findall(r"0x[0-9A-Fa-f]+", door_src)}
    stage_used = set()
    for l in layers:
        if l[1] == tdef:
            stage_used.update(read_tm(l[0]))
    reserved = {0} | set(new_fg) | door_tiles | TRAPDOOR_TILES
    if patch:
        # NO3 changes its own tileset: only empty slots (all four entries 0),
        # which neither a tilemap nor a tile-writing entity can use (the rock
        # and door tiles those write, e.g. 0x6B2.., have their art set)
        free = [i for i in range(1, len(td["cols"])) if i not in reserved and i not in stage_used
                and not any(td[k][i] for k in TILEDEF_PARTS)]
    else:
        free = ([i for i in range(1, len(td["cols"])) if i not in reserved and i not in stage_used]
                + [i for i in range(1, len(td["cols"])) if i not in reserved and i in stage_used])
    retail_td = {k: bytes(v) for k, v in td.items()}

    retail_fg, retail_bg = read_tm(fg[0]), read_tm(bg[0])
    n = RETAIL_ROWS * 16
    assert [td["cols"][t] for t in retail_fg[:n]] == fg_coll[:n], "retail rows differ"
    new_fg[:n], new_bg[:n] = retail_fg[:n], retail_bg[:n]
    fg_err = [0] * n + list(fg_err[n:])

    port_cells(stage, fg[0], td, free, new_fg, fg_coll, fg_err, fg_px)

    for y in ALCOVE_ROWS:
        # extend the alcove through the wall with its own open tiles
        assert fg_coll[y * 16 + 15] == 3 and fg_coll[y * 16 + 14] == 0, y
        new_fg[y * 16 + 15] = new_fg[y * 16 + 14]
        new_bg[y * 16 + 15] = new_bg[y * 16 + 14]
        fg_coll[y * 16 + 15] = 0
    assert fg_coll[(ALCOVE_ROWS[0] - 1) * 16 + 15] == 3
    assert fg_coll[(ALCOVE_ROWS[-1] + 1) * 16 + 15] == 3
    for rows, src, dsts in (ALCOVE_CEILING, ALCOVE_FLOOR):
        for y in rows:
            for x in dsts:
                for layer in (new_fg, new_bg, fg_coll):
                    layer[y * 16 + x] = layer[y * 16 + src]
    smooth_np3_step(new_fg, new_bg, fg_coll)

    combos = {}
    for n, (t, c) in enumerate(zip(new_fg, fg_coll)):
        assert t or not c, f"cell {n}: solid without a tile"
        if not t or td["cols"][t] == c:
            continue
        if (t, c) not in combos:
            i = combos[(t, c)] = free.pop(0)
            for k in TILEDEF_PARTS[:3]:
                td[k][i] = td[k][t]
            td["cols"][i] = c
        new_fg[n] = combos[(t, c)]
    assert all(td["cols"][t] == c for t, c in zip(new_fg, fg_coll))

    save = next(i for i, r in enumerate(rooms) if r[:4] == [*SAVE_ROOM, *SAVE_ROOM])
    sf_name, sf_i, sb_name, sb_i = roomdefs[rooms[save][4]]
    sfg = layers[int(sf_i)]
    save_fg = read_tm(sfg[0])
    scols = open(os.path.join(asset_dir(stage), f"{sfg[1]}_cols.bin"), "rb").read()
    # left door: open rows 6-9 with the floor at 10 -> open rows 7-10, floor 11
    door = [[save_fg[y * 16 + x] for x in range(3)] for y in range(16)]
    assert all(scols[t] == 0 for y in range(6, 10) for t in door[y]), "door rows"
    assert all(scols[t] == 3 for t in door[10] + door[11] + door[5]), "door frame"
    for y in range(6, 12):
        for x in range(3):
            save_fg[y * 16 + x] = door[y - 1][x]
    # ... and as tall as the alcove (open from local row 6, its ceiling at 5):
    # the lower of the two ceiling blocks above it (row 6) goes, the opening's
    # rows shifting up one (its top-edge row to 6, the next to 7; 8-10 kept)
    assert all(fg_coll[(SAVE_ROOM[1] + 1 - 41) * 256 + y * 16 + 15] == 0 for y in range(6, 11)), "alcove rows"
    for y, src in ((6, 6), (7, 7)):
        for x in range(3):
            save_fg[y * 16 + x] = door[src][x]
    assert all(scols[save_fg[y * 16 + x]] == 0 for y in range(6, 11) for x in range(3)), "door open"

    out_layers = [list(l) for l in layers]
    nl = len(out_layers)
    out_layers.append(["g_ModShaftFg", tdef if patch else "(TileDefinition*)&g_ModShaftTiledef",
                       "2", "41", "2", "44", fg[6], fg[7], fg[8]])
    out_layers.append(["g_ModShaftBg", bg[1], bg[2], bg[3], bg[4], "44" if bg[5] != "0" else bg[5], bg[6], bg[7], bg[8]])
    lf_name, lf_i, lb_name, lb_i = roomdefs[rooms[load][4]]
    lfg = list(layers[int(lf_i)])
    lfg[2:6] = ["3", "44", "3", "44"]
    lfg[7] = "0x%02X" % tp  # preload the Garden's tiles
    out_layers.append(lfg)
    nsave = len(out_layers)
    sl = list(sfg)
    sl[0] = "g_ModSaveFg"
    sl[2:6] = [str(SAVE_ROOM[0]), str(SAVE_ROOM[1] + 1)] * 2
    out_layers.append(sl)
    out_roomdefs = [list(r) for r in roomdefs]
    out_roomdefs[rooms[shaft][4]] = ["layers", str(nl), "layers", str(nl + 1)]
    load_bg = (lb_name, lb_i) if lb_name == "layer_empty" else ("layers", lb_i)
    out_roomdefs.append(["layers", str(nl + 2), load_bg[0], load_bg[1] or ""])
    new_layer_id = len(out_roomdefs) - 1
    save_bg = (sb_name, sb_i) if sb_name == "layer_empty" else ("layers", sb_i)
    out_roomdefs.append(["layers", str(nsave), save_bg[0], save_bg[1] or ""])
    save_layer_id = len(out_roomdefs) - 1

    out_rooms = [list(r) for r in rooms]
    out_rooms[shaft][3] = 44
    # its wall sconces' art (mods/sotn-ultimate/tools/gen_np3_sconce.py: gfxBanks index 8)
    out_rooms[shaft][6] = 9
    lr = rooms[load]
    out_rooms.append([3, 44, 3, 44, new_layer_id, lr[5], lr[6], lr[7]])
    out_rooms.append([4, 44, 4, 44, tp, 0xFF, 0, 0])
    if patch:
        # the forest run to row 46, as on the Saturn, with its layers' rects
        for i, (l, t, r, b, row) in NO3_FOREST.items():
            out_rooms[i][1] = out_rooms[i][3] = row
            for name, idx in (roomdefs[rooms[i][4]][:2], roomdefs[rooms[i][4]][2:]):
                if name != "layers":
                    continue
                lay = out_layers[int(idx)]
                if lay[2:6] == ["0", "0", "0", "0"]:
                    continue  # a layer without a rect (the forest's backdrop)
                assert [lay[3], lay[5]] == [str(t), str(b)], (i, lay)
                assert sum(1 for r2 in rooms if idx in (roomdefs[r2[4]][1], roomdefs[r2[4]][3])) == 1, \
                    f"forest layer {idx} shared"
                lay[3] = lay[5] = str(row)
        for c in ((2, 43), (2, 44), (3, 44), (4, 44)):
            assert not any(r[0] <= c[0] <= r[2] and r[1] <= c[1] <= r[3]
                           for i, r in enumerate(out_rooms) if i != shaft and r[:4] != [c[0], c[1], c[0], c[1]]), c
    sx, sy = SAVE_ROOM
    out_rooms[save][:5] = [sx, sy + 1, sx, sy + 1, save_layer_id]
    real_save = next(r for r in rooms if r[:4] == [*SAVE_ROOM_LAYOUT_FROM, *SAVE_ROOM_LAYOUT_FROM])
    out_rooms[save][7] = real_save[7]
    assert not any(r[0] <= sx <= r[2] and r[1] <= sy + 1 <= r[3]
                   for i, r in enumerate(rooms) if i != save), "save room target taken"

    dst = os.path.join(ROOT, f"src/st/{stage}/mod_rooms.h")
    with open(dst, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gen_mod_rooms.py, do not edit\n// clang-format off\n")
        f.write("typedef char ModTeleportIdCheck[%s == 0x%02X ? 1 : -1];\n" % (tp_name, tp))
        f.write("\n".join(externs) + "\nextern TileDefinition %s[];\n" % tdef)
        for name, data in (("g_ModShaftFg", new_fg), ("g_ModShaftBg", new_bg),
                           ("g_ModSaveFg", save_fg)):
            f.write("static const u16 %s[] = {\n" % name)
            for i in range(0, len(data), 16):
                f.write("    " + ", ".join("0x%04X" % v for v in data[i:i + 16]) + ",\n")
            f.write("};\n")
        if patch:
            # {slot, gfxPage, gfxIndex, clut, collision} for the stage's tileset
            changed = [i for i in range(len(td["cols"]))
                       if any(td[k][i] != retail_td[k][i] for k in TILEDEF_PARTS)]
            assert changed and not set(changed) & (stage_used | reserved), "patched slots in use"
            assert all(not any(retail_td[k][i] for k in TILEDEF_PARTS) for i in changed), "patched slots not empty"
            f.write("#define MOD_TILEDEF_PATCH %s\n" % tdef)
            f.write("const u16 g_ModTiledefPatch[][5] = {\n")
            for i in changed:
                f.write("    {%d, %s},\n" % (i, ", ".join(str(td[k][i]) for k in TILEDEF_PARTS)))
            f.write("};\n")
        else:
            for k, name in zip(TILEDEF_PARTS, ("Page", "Index", "Clut", "Coll")):
                f.write("static const u8 g_ModShaftTd%s[] = {\n" % name)
                for i in range(0, len(td[k]), 32):
                    f.write("    " + ", ".join("%d" % v for v in td[k][i:i + 32]) + ",\n")
                f.write("};\n")
            f.write("static const TileDefinition g_ModShaftTiledef = {\n    (u8*)g_ModShaftTdPage, "
                    "(u8*)g_ModShaftTdIndex, (u8*)g_ModShaftTdClut, (u8*)g_ModShaftTdColl};\n")
        f.write("static const LayerDef g_ModLayerEmpty = { NULL, NULL, 0, 0, 0 };\n")
        f.write("static const LayerDef g_ModLayers[] = {\n")
        for l in out_layers:
            f.write("    { (u16*)%s, %s, {%s, %s, %s, %s, %s}, %s, %s},\n" % tuple(l))
        f.write("};\n")
        f.write("const RoomDef g_ModRoomsLayers[] = {\n")
        for a, ai, b, bi in out_roomdefs:
            f.write("    { (LayerDef*)%s, (LayerDef*)%s },\n" % (ref(a, ai), ref(b, bi)))
        f.write("};\n")
        f.write("const u8 g_ModRooms[] = {\n")
        for r in out_rooms:
            f.write("    " + ", ".join(str(v) for v in r) + ",\n")
        f.write("    0x40\n};\n// clang-format on\n")
    print(f"wrote src/st/{stage}/mod_rooms.h: shaft to (2,44), loading room (3,44) "
          f"layer {new_layer_id}, transition (4,44) -> 0x{tp:02X}, "
          f"save room to ({SAVE_ROOM[0]},{SAVE_ROOM[1] + 1}), "
          f"{len(combos)} art/collision tiles added")


# Reverse Entrance (RNO3), as on the Saturn (RSTAGE01 room 17): the inverted
# castle's mirror of np3() above, every row and column turned around. The
# shaft (61,21)-(61,22) grows UP to (61,19): its bottom 21 rows keep the
# retail art (the old room's rows 11-31), the rest is matched/ported from the
# Saturn's art (RNO3 tiles, F_RNO3 free slots) with the Saturn's collision. At
# the top left a loading room (60,19) and the transition cell (59,19) lead
# into the Hell Garden (MOD_TP_NP3_TO_GDN; the inverted castle turns it into
# the Hell Garden). The save room (60,21) moves one cell up to (60,20), behind
# the alcove (shaft rows 21-24, opened in the left wall), as NP3's (3,42) ->
# (3,43); its right door moves one tile up and it takes the entity layout of
# the real save room (58,24). RNO3's rooms 18 (46,19)-(63,19) and 19 (45,19),
# the mirrored NO3 forest (empty entity layouts, unreachable), would cover
# the Hell Garden's and the shaft's cells: they move to row 17 (the Saturn has
# no such rooms; their indices stay, later rooms' indices are used by the
# retail teleports).
RNO3_SHAFT = 1
RNO3_SAVE_ROOM = (60, 21)        # moves to (60, 20)
RNO3_SAVE_LAYOUT_FROM = (58, 24)
RNO3_LOAD_FROM = (48, 22)        # a loading room with its exit on the left
RNO3_ALCOVE_ROWS = range(21, 25)  # opened in the left wall (col 0)
RNO3_RETAIL_FROM = 64 - RETAIL_ROWS  # rows 43-63 keep the retail art
RNO3_ALCOVE_FLOOR = (range(25, 28), 2, (1, 0))   # NP3's ceiling, turned around
RNO3_ALCOVE_CEILING = (range(19, 21), 3, (2, 1, 0))
RNO3_TILE_OVERRIDES = [((33, 13), (40, 7), "H")]  # NP3's (30,2) <- (23,8), turned around
RNO3_FOREST = {18: (46, 17, 63, 17), 19: (45, 17, 45, 17)}
RNO3_SCONCE_GFX_ID = 9           # mods/sotn-ultimate/tools/gen_np3_sconce.py (rno3): gfxBanks index 8
# room 0 (row, col): closed -> open trapdoor tiles (NP3's 0x129/0x132 at
# (42,7)/(42,8) -> 0x6C8/0x6C9, turned around)
RNO3_TRAPDOOR = {(5, 24): (0x129, 0x6C8), (5, 23): (0x132, 0x6C9)}


def rno3():
    stage, tdef = "rno3", "rno3_tiledef_rno3"
    sat = [os.path.join(ROOT, "disks/saturn/RSTAGE01.PRG"), os.path.join(ROOT, "disks/saturn/RSTAGE01.MAP")]
    externs, layers, roomdefs = parse_layers(stage)
    rooms = parse_rooms(stage)
    shaft = RNO3_SHAFT
    assert rooms[shaft][:4] == [61, 21, 61, 22], rooms[shaft]
    load = next(i for i, r in enumerate(rooms) if r[:4] == [*RNO3_LOAD_FROM, *RNO3_LOAD_FROM])
    fg_name, fg_i, bg_name, bg_i = roomdefs[rooms[shaft][4]]
    fg, bg = layers[int(fg_i)], layers[int(bg_i)]
    for i, b in RNO3_FOREST.items():
        assert rooms[i][:2] in ([46, 19], [45, 19]), rooms[i]

    def read_tm(name):
        d = open(os.path.join(asset_dir(stage), f"{name}.bin"), "rb").read()
        return list(struct.unpack("<%dH" % (len(d) // 2), d))

    work = work_dir("rno3")
    os.makedirs(work, exist_ok=True)
    new_fg = tilematch(*sat, 17, 0, stage, tdef, f"{work}/shaft_fg.bin", art_only=True,
                       pool=[os.path.join(asset_dir(stage), f"{fg[0]}.bin")])
    new_bg = tilematch(*sat, 17, 2, stage, tdef, f"{work}/shaft_bg.bin")
    fg_coll = list(open(f"{work}/shaft_fg.bin.coll", "rb").read())
    fg_err = struct.unpack("<1024I", open(f"{work}/shaft_fg.bin.err", "rb").read())
    fg_px = open(f"{work}/shaft_fg.bin.px", "rb").read()
    assert len(new_fg) == len(new_bg) == len(fg_coll) == 16 * 64

    td = {k: bytearray(open(os.path.join(asset_dir(stage), f"{tdef}_{k}.bin"), "rb").read())
          for k in TILEDEF_PARTS}
    door_src = open(os.path.join(ROOT, f"src/st/{stage}/e_red_door_tiles.c")).read()
    door_tiles = {int(v, 16) for v in re.findall(r"0x[0-9A-Fa-f]+", door_src)}
    stage_used = set()
    for l in layers:
        if l[1] == tdef:
            stage_used.update(read_tm(l[0]))
    reserved = {0} | set(new_fg) | door_tiles
    free = ([i for i in range(1, len(td["cols"])) if i not in reserved and i not in stage_used]
            + [i for i in range(1, len(td["cols"])) if i not in reserved and i in stage_used])

    retail_fg, retail_bg = read_tm(fg[0]), read_tm(bg[0])
    assert len(retail_fg) == len(retail_bg) == 16 * 32
    a = RNO3_RETAIL_FROM * 16      # first kept cell of the new room
    b = (RNO3_RETAIL_FROM - 32) * 16  # the same cell in the old room
    # one cell differs: the Saturn's slope at row 51 col 7 is solid on the
    # PS1 (whose slope starts a row lower); the kept rows keep the PS1's
    retail_coll = [td["cols"][t] for t in retail_fg[b:]]
    differ = sum(x != y for x, y in zip(retail_coll, fg_coll[a:]))
    assert differ <= 1, f"retail rows differ in {differ} cells"
    fg_coll[a:] = retail_coll
    new_fg[a:], new_bg[a:] = retail_fg[b:], retail_bg[b:]
    fg_err = list(fg_err[:a]) + [0] * (len(fg_err) - a)

    # The Saturn shows the kept retail rows with the same art as RNO3, in
    # other (grey) palettes; RGB matching against RNO3's pink tiles picked
    # other rooms' bricks for the background and off-colour fill for the
    # foreground. Saturn cells that also occur in the kept rows take the
    # retail tile shown there; the background's other cells the retail tile
    # of the nearest such Saturn cell; the foreground's other visible cells
    # are ported with the colours translated through the kept rows.
    from collections import Counter
    bg_px = open(f"{work}/shaft_bg.bin.px", "rb").read()

    def cell(px, n):
        return px[n * 1024:(n + 1) * 1024]

    def learn(px, layer):
        m = {}
        for n in range(a, 16 * 64):
            m.setdefault(cell(px, n), Counter())[layer[n]] += 1
        return {k: c.most_common(1)[0][0] for k, c in m.items()}

    fg_map, bg_map = learn(fg_px, new_fg), learn(bg_px, new_bg)
    exact_fg = exact_bg = 0
    for n in range(a):
        c = cell(fg_px, n)
        if any(c[3::4]):
            if c in fg_map and fg_map[c]:
                new_fg[n], fg_err[n] = fg_map[c], 0
                exact_fg += 1
            else:
                fg_err[n] = max(fg_err[n], PORT_MIN_ERR + 1)
        c = cell(bg_px, n)
        if not any(c[3::4]):
            new_bg[n] = 0
        elif c in bg_map:
            new_bg[n] = bg_map[c]
            exact_bg += 1
        else:
            near = min((k for k in bg_map if any(k[3::4])),
                       key=lambda k: sum((x - y) ** 2 for x, y in zip(c, k)))
            new_bg[n] = bg_map[near]
    print(f"rno3: {exact_fg} fg / {exact_bg} bg cells from the kept rows' tiles")
    pal_pairs = [(cell(fg_px, n), new_fg[n]) for n in range(a, 16 * 64)
                 if new_fg[n] and any(cell(fg_px, n)[3::4])]

    port_cells(stage, fg[0], td, free, new_fg, fg_coll, fg_err, fg_px, overrides=RNO3_TILE_OVERRIDES,
               pal_pairs=pal_pairs)

    for y in RNO3_ALCOVE_ROWS:
        assert fg_coll[y * 16] == 3 and fg_coll[y * 16 + 1] == 0, y
        new_fg[y * 16] = new_fg[y * 16 + 1]
        new_bg[y * 16] = new_bg[y * 16 + 1]
        fg_coll[y * 16] = 0
    assert fg_coll[(RNO3_ALCOVE_ROWS[0] - 1) * 16] == 3
    assert fg_coll[(RNO3_ALCOVE_ROWS[-1] + 1) * 16] == 3
    for rows, src, dsts in (RNO3_ALCOVE_FLOOR, RNO3_ALCOVE_CEILING):
        for y in rows:
            for x in dsts:
                for layer in (new_fg, new_bg, fg_coll):
                    layer[y * 16 + x] = layer[y * 16 + src]

    combos = {}
    for n, (t, c) in enumerate(zip(new_fg, fg_coll)):
        assert t or not c, f"cell {n}: solid without a tile"
        if not t or td["cols"][t] == c:
            continue
        if (t, c) not in combos:
            i = combos[(t, c)] = free.pop(0)
            for k in TILEDEF_PARTS[:3]:
                td[k][i] = td[k][t]
            td["cols"][i] = c
        new_fg[n] = combos[(t, c)]
    assert all(td["cols"][t] == c for t, c in zip(new_fg, fg_coll))

    save = next(i for i, r in enumerate(rooms) if r[:4] == [*RNO3_SAVE_ROOM, *RNO3_SAVE_ROOM])
    sf_name, sf_i, sb_name, sb_i = roomdefs[rooms[save][4]]
    sfg = layers[int(sf_i)]
    save_fg = read_tm(sfg[0])
    scols = open(os.path.join(asset_dir(stage), f"{sfg[1]}_cols.bin"), "rb").read()
    # right door: open rows 6-9, floor at 10. Unlike NP3's (whose door moves
    # down to the alcove's floor) it already matches: the alcove (local rows
    # 5-9 of the shaft's cell row 20) has its floor at row 10 too; the turned
    # around rooms keep their floors at the bottom. (Moving it up as NP3's
    # mirror put its floor a tile above the alcove's.)
    door = [[save_fg[y * 16 + x] for x in range(13, 16)] for y in range(16)]
    assert all(scols[t] == 0 for y in range(6, 10) for t in door[y]), "door rows"
    assert all(scols[t] == 3 for t in door[10]), "door floor"
    for y in range(6, 10):
        assert fg_coll[(16 + y) * 16] == 0, ("alcove open", y)
    assert fg_coll[(16 + 10) * 16] == 3, "alcove floor"
    # The alcove is open from row 5 (ceiling at 4), the door from row 6: the
    # lower of the two ceiling blocks above the door (row 5) goes, so the
    # opening is as tall as the alcove's (its top-edge row moves to 5, the next
    # to 6; rows 7-9 kept).
    assert fg_coll[(16 + 5) * 16] == 0 and fg_coll[(16 + 4) * 16] == 3, "alcove ceiling"
    for y, src in ((5, 6), (6, 7)):
        for x in range(3):
            save_fg[y * 16 + 13 + x] = door[src][x]
    assert all(scols[save_fg[y * 16 + 13 + x]] == 0 for y in range(5, 10) for x in range(3)), "door open"

    out_layers = [list(l) for l in layers]
    # room 0 (60,23)-(61,25): the trapdoor at the shaft's foot open, as on the
    # Saturn (RSTAGE01 entity 0x53, 0x060E01A8: its condition 0x060E02F0
    # always returns 1, so it starts open with open collision). RNO3 has no
    # trapdoor entity: its closed door is tiles; they become NO3's open ones.
    r0_def = roomdefs[rooms[0][4]]
    assert rooms[0][:4] == [60, 23, 61, 25] and r0_def[0] == "layers", (rooms[0], r0_def)
    r0 = int(r0_def[1])
    room0_fg = read_tm(layers[r0][0])
    for (y, x), (old, new) in RNO3_TRAPDOOR.items():
        assert room0_fg[y * 32 + x] == old, (y, x, hex(room0_fg[y * 32 + x]))
        room0_fg[y * 32 + x] = new
    out_layers[r0][0] = "g_ModRoom0Fg"
    nl = len(out_layers)
    out_layers.append(["g_ModShaftFg", "(TileDefinition*)&g_ModShaftTiledef", "61", "19", "61", "22", fg[6], fg[7], fg[8]])
    out_layers.append(["g_ModShaftBg", bg[1], bg[2], "19" if bg[3] != "0" else bg[3], bg[4], bg[5], bg[6], bg[7], bg[8]])
    lf_name, lf_i, lb_name, lb_i = roomdefs[rooms[load][4]]
    lfg = list(layers[int(lf_i)])
    assert lfg[6] == "0x40", lfg   # exit on the left
    lfg[2:6] = ["60", "19", "60", "19"]
    lfg[7] = "0x%02X" % TP_NP3_TO_GDN  # preload the Hell Garden's tiles
    out_layers.append(lfg)
    nsave = len(out_layers)
    sl = list(sfg)
    sl[0] = "g_ModSaveFg"
    sl[2:6] = [str(RNO3_SAVE_ROOM[0]), str(RNO3_SAVE_ROOM[1] - 1)] * 2
    out_layers.append(sl)
    out_roomdefs = [list(r) for r in roomdefs]
    out_roomdefs[rooms[shaft][4]] = ["layers", str(nl), "layers", str(nl + 1)]
    load_bg = (lb_name, lb_i) if lb_name == "layer_empty" else ("layers", lb_i)
    out_roomdefs.append(["layers", str(nl + 2), load_bg[0], load_bg[1] or ""])
    new_layer_id = len(out_roomdefs) - 1
    save_bg = (sb_name, sb_i) if sb_name == "layer_empty" else ("layers", sb_i)
    out_roomdefs.append(["layers", str(nsave), save_bg[0], save_bg[1] or ""])
    save_layer_id = len(out_roomdefs) - 1

    out_rooms = [list(r) for r in rooms]
    out_rooms[shaft][1] = 19
    # its candelabras' art (mods/sotn-ultimate/tools/gen_np3_sconce.py rno3)
    out_rooms[shaft][6] = RNO3_SCONCE_GFX_ID
    for i, bounds in RNO3_FOREST.items():
        out_rooms[i][:4] = list(bounds)
    lr = rooms[load]
    out_rooms.append([60, 19, 60, 19, new_layer_id, lr[5], lr[6], lr[7]])
    out_rooms.append([59, 19, 59, 19, TP_NP3_TO_GDN, 0xFF, 0, 0])
    sx, sy = RNO3_SAVE_ROOM
    out_rooms[save][:5] = [sx, sy - 1, sx, sy - 1, save_layer_id]
    real_save = next(r for r in rooms if r[:4] == [*RNO3_SAVE_LAYOUT_FROM, *RNO3_SAVE_LAYOUT_FROM])
    out_rooms[save][7] = real_save[7]
    # no two rooms on one cell
    cells = {}
    for i, r in enumerate(out_rooms):
        for cx in range(r[0], r[2] + 1):
            for cy in range(r[1], r[3] + 1):
                assert (cx, cy) not in cells, ("cell taken", (cx, cy), i, cells[(cx, cy)])
                cells[(cx, cy)] = i

    dst = os.path.join(ROOT, f"src/st/{stage}/mod_rooms.h")
    with open(dst, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gen_mod_rooms.py, do not edit\n// clang-format off\n")
        f.write("typedef char ModTeleportIdCheck[MOD_TP_NP3_TO_GDN == 0x%02X ? 1 : -1];\n" % TP_NP3_TO_GDN)
        f.write("\n".join(externs) + "\n")
        for name, data in (("g_ModShaftFg", new_fg), ("g_ModShaftBg", new_bg),
                           ("g_ModSaveFg", save_fg), ("g_ModRoom0Fg", room0_fg)):
            f.write("static const u16 %s[] = {\n" % name)
            for i in range(0, len(data), 16):
                f.write("    " + ", ".join("0x%04X" % v for v in data[i:i + 16]) + ",\n")
            f.write("};\n")
        for k, name in zip(TILEDEF_PARTS, ("Page", "Index", "Clut", "Coll")):
            f.write("static const u8 g_ModShaftTd%s[] = {\n" % name)
            for i in range(0, len(td[k]), 32):
                f.write("    " + ", ".join("%d" % v for v in td[k][i:i + 32]) + ",\n")
            f.write("};\n")
        f.write("static const TileDefinition g_ModShaftTiledef = {\n    (u8*)g_ModShaftTdPage, "
                "(u8*)g_ModShaftTdIndex, (u8*)g_ModShaftTdClut, (u8*)g_ModShaftTdColl};\n")
        f.write("static const LayerDef g_ModLayerEmpty = { NULL, NULL, 0, 0, 0 };\n")
        f.write("static const LayerDef g_ModLayers[] = {\n")
        for l in out_layers:
            f.write("    { (u16*)%s, %s, {%s, %s, %s, %s, %s}, %s, %s},\n" % tuple(l))
        f.write("};\n")
        f.write("const RoomDef g_ModRoomsLayers[] = {\n")
        for a, ai, b, bi in out_roomdefs:
            f.write("    { (LayerDef*)%s, (LayerDef*)%s },\n" % (ref(a, ai), ref(b, bi)))
        f.write("};\n")
        f.write("const u8 g_ModRooms[] = {\n")
        for r in out_rooms:
            f.write("    " + ", ".join(str(v) for v in r) + ",\n")
        f.write("    0x40\n};\n// clang-format on\n")
    print(f"wrote src/st/{stage}/mod_rooms.h: shaft to (61,19), loading room (60,19) "
          f"layer {new_layer_id}, transition (59,19) -> 0x{TP_NP3_TO_GDN:02X}, "
          f"save room to ({sx},{sy - 1}), forest rooms to row 17, "
          f"{len(combos)} art/collision tiles added")


# Each warp room has its own statue head above the keyhole, so players know
# where they are; the Saturn's sixth has a bear. The PSX rooms differ only in
# the head cells (and their door sides); the Saturn draws statues and head on
# its background layer (2) under the keyhole frame (0). The sixth room's cells
# in the PSX head area that differ between the Saturn's Entrance room (0) and
# room 5 get the Saturn's art (both layers stacked, narrowed), ported into
# F_WRP like the Entrance shaft's (port_cells), through a tileset copy.
#
# Warp rooms (WRP), as on the Saturn (WARP.PRG room 5): a sixth warp room at
# (2,45), the Underground Garden's lower-left exit, with its loading room
# (3,45) and the transition cell (4,45) into the Garden (MOD_TP_WRP_TO_GDN).
# The room copies the Entrance warp room (15,38), which also has its door on
# the right, and the loading room copies (16,38), its neighbour. The warp
# room's entities (warp machine params 5, red door) are layout 6, which no
# WRP room uses (src/st/wrp/mod_layout.h, gen_mod_layouts.py).
TP_WRP_TO_GDN = 0x86   # MOD_TP_WRP_TO_GDN, include/game.h
WRP_ROOM6 = (2, 45)
WRP_ROOM6_INDEX = 15   # MOD_TP_GDN_TO_WRP's room byte offset is 15 * 8 (src/dra/5087C.c)
WRP_ROOM6_LAYOUT = 6   # MOD_LAYOUT_ID in src/st/wrp/wrp.h

# The inverted castle's warp rooms (RWRP), as on the Saturn (RWARP.PRG room
# 5): the same, rotated: the sixth warp room at (61,18), the Hell Garden's
# upper-right exit, its loading room (60,18) on the left and the transition
# cell (59,18) (MOD_TP_WRP_TO_GDN, which the inverted castle turns into the
# Hell Garden). It copies the inverted Entrance warp room (48,25), door on the
# left, and its loading room (47,25); layout 6 is unused in RWRP too. Same
# room index 15 (the teleport table is shared by both castles).
WARP6 = {
    "wrp": {"sat": "WARP", "cell": WRP_ROOM6, "src": (15, 38), "load": (16, 38), "dx": 1},
    "rwrp": {"sat": "RWARP", "cell": (61, 18), "src": (48, 25), "load": (47, 25), "dx": -1},
}


def wrp(stage="wrp"):
    spec = WARP6[stage]
    externs, layers, roomdefs = parse_layers(stage)
    rooms = parse_rooms(stage)
    assert len(rooms) == WRP_ROOM6_INDEX, len(rooms)
    assert not any(r[7] == WRP_ROOM6_LAYOUT for r in rooms if r[5] != 0xFF)
    src = next(i for i, r in enumerate(rooms) if r[:4] == [*spec["src"], *spec["src"]])
    load = next(i for i, r in enumerate(rooms) if r[:4] == [*spec["load"], *spec["load"]])
    x, y = spec["cell"]
    dx = spec["dx"]

    # the bear head (see above)
    tdef = f"{stage}_tiledef_{stage}"
    src_fg = layers[int(roomdefs[rooms[src][4]][1])]
    assert src_fg[1] == tdef

    def read_tm(name):
        d = open(os.path.join(asset_dir(stage), f"{name}.bin"), "rb").read()
        return list(struct.unpack("<%dH" % (len(d) // 2), d))

    psx = [read_tm(l[0]) for l in layers if l[1] == tdef]
    head = {n for n in range(256) if len({t[n] for t in psx}) > 1 and 3 <= n % 16 <= 12}
    work = work_dir(stage)
    os.makedirs(work, exist_ok=True)
    sat = [os.path.join(ROOT, f"disks/saturn/{spec['sat']}.PRG"),
           os.path.join(ROOT, f"disks/saturn/{spec['sat']}.MAP")]
    pool = [os.path.join(asset_dir(stage), f"{src_fg[0]}.bin")]

    def stacked(room):
        layer_px, full = [], []
        for layer in (0, 2):
            out = f"{work}/warp{room}_{layer}.bin"
            tilematch(*sat, room, layer, stage, tdef, out, art_only=True, pool=pool)
            layer_px.append(open(out + ".px", "rb").read())
            full.append(open(out + ".full", "rb").read())
        fgp, bgp = layer_px
        px = bytearray(bgp)
        for i in range(0, len(fgp), 4):
            if fgp[i + 3]:
                px[i:i + 4] = fgp[i:i + 4]
        fw, fh = struct.unpack_from("<HH", full[0])
        ff, fb = full[0][4:], full[1][4:]
        fpx = bytearray(fb)
        for i in range(0, len(ff), 4):
            if ff[i + 3]:
                fpx[i:i + 4] = ff[i:i + 4]
        return bytes(px), (fw, fh, bytes(fpx))

    (sat0, _), (sat5, full5) = stacked(0), stacked(5)
    cells = sorted(n for n in head if sat0[n * 1024:(n + 1) * 1024] != sat5[n * 1024:(n + 1) * 1024])
    sat5 = narrow_region(sat5, full5, cells)
    td = {k: bytearray(open(os.path.join(asset_dir(stage), f"{tdef}_{k}.bin"), "rb").read())
          for k in TILEDEF_PARTS}
    warp_fg = read_tm(src_fg[0])
    used = set().union(*psx)
    free = [i for i in range(1, len(td["cols"])) if i not in used]
    port_cells(stage, src_fg[0], td, free, warp_fg, [td["cols"][t] for t in warp_fg],
               [1 << 30 if n in cells else 0 for n in range(256)], sat5, overrides=[])

    out_layers = [list(l) for l in layers]
    out_roomdefs = [list(r) for r in roomdefs]
    for room, (cx, cy), zprio in ((src, (x, y), None), (load, (x + dx, y), TP_WRP_TO_GDN)):
        f_name, f_i, b_name, b_i = roomdefs[rooms[room][4]]
        l = list(layers[int(f_i)])
        l[2:6] = [str(cx), str(cy), str(cx), str(cy)]
        if zprio is None:
            l[0], l[1] = "g_ModWarp6Fg", "(TileDefinition*)&g_ModWarp6Tiledef"
        if zprio is not None:
            l[7] = "0x%02X" % zprio  # a loading room preloads its teleport's stage
        out_layers.append(l)
        bg = (b_name, b_i) if b_name == "layer_empty" else ("layers", b_i)
        out_roomdefs.append(["layers", str(len(out_layers) - 1), bg[0], bg[1] or ""])
    warp_def, load_def = len(out_roomdefs) - 2, len(out_roomdefs) - 1

    out_rooms = [list(r) for r in rooms]
    rs, rl = rooms[src], rooms[load]
    out_rooms.append([x, y, x, y, warp_def, rs[5], rs[6], WRP_ROOM6_LAYOUT])
    out_rooms.append([x + dx, y, x + dx, y, load_def, rl[5], rl[6], rl[7]])
    out_rooms.append([x + 2 * dx, y, x + 2 * dx, y, TP_WRP_TO_GDN, 0xFF, 0, 0])
    assert not any(r[0] <= cx <= r[2] and r[1] <= y <= r[3]
                   for r in rooms for cx in (x, x + dx, x + 2 * dx)), "warp room cells taken"
    assert out_rooms[WRP_ROOM6_INDEX][:2] == [x, y]

    dst = os.path.join(ROOT, f"src/st/{stage}/mod_rooms.h")
    with open(dst, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gen_mod_rooms.py, do not edit\n// clang-format off\n")
        f.write("typedef char ModTeleportIdCheck[MOD_TP_WRP_TO_GDN == 0x%02X ? 1 : -1];\n" % TP_WRP_TO_GDN)
        f.write("typedef char ModLayoutIdCheck2[MOD_LAYOUT_ID == %d ? 1 : -1];\n" % WRP_ROOM6_LAYOUT)
        f.write("\n".join(externs) + "\nextern TileDefinition %s[];\n" % tdef)
        f.write("static const u16 g_ModWarp6Fg[] = {\n")
        for i in range(0, 256, 16):
            f.write("    " + ", ".join("0x%04X" % v for v in warp_fg[i:i + 16]) + ",\n")
        f.write("};\n")
        for k, name in zip(TILEDEF_PARTS, ("Page", "Index", "Clut", "Coll")):
            f.write("static const u8 g_ModWarp6Td%s[] = {\n" % name)
            for i in range(0, len(td[k]), 32):
                f.write("    " + ", ".join("%d" % v for v in td[k][i:i + 32]) + ",\n")
            f.write("};\n")
        f.write("static const TileDefinition g_ModWarp6Tiledef = {\n    (u8*)g_ModWarp6TdPage, "
                "(u8*)g_ModWarp6TdIndex, (u8*)g_ModWarp6TdClut, (u8*)g_ModWarp6TdColl};\n")
        f.write("static const LayerDef g_ModLayerEmpty = { NULL, NULL, 0, 0, 0 };\n")
        f.write("static const LayerDef g_ModLayers[] = {\n")
        for l in out_layers:
            f.write("    { (u16*)%s, %s, {%s, %s, %s, %s, %s}, %s, %s},\n" % tuple(l))
        f.write("};\n")
        f.write("const RoomDef g_ModRoomsLayers[] = {\n")
        for a, ai, b, bi in out_roomdefs:
            f.write("    { (LayerDef*)%s, (LayerDef*)%s },\n" % (ref(a, ai), ref(b, bi)))
        f.write("};\n")
        f.write("const u8 g_ModRooms[] = {\n")
        for r in out_rooms:
            f.write("    " + ", ".join(str(v) for v in r) + ",\n")
        f.write("    0x40\n};\n// clang-format on\n")
    print(f"wrote src/st/{stage}/mod_rooms.h: warp room {(x, y)} (room {WRP_ROOM6_INDEX}), "
          f"loading room ({x + dx},{y}), transition ({x + 2 * dx},{y}) -> 0x{TP_WRP_TO_GDN:02X}, "
          f"bear head: {len(cells)} of {len(head)} head cells")


np3()
np3("no3")
wrp()
rno3()
wrp("rwrp")
