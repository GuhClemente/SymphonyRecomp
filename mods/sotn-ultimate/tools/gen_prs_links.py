#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: the Cursed Prison's doors in the Underground Caverns (NO4)
# and the Marble Gallery (NO0), as on the Saturn.
#
# NO4 room 5 (37,33)-(37,34): its bottom corridor (rows 23-25) ends at a
# one-tile wall on the left, the doorway the Saturn opens with a red door
# (STAGE_09 list 58: (10,353) -> PSX (8,353)). Past it: a loading room (36,34)
# and the transition cell (35,34) into the Prison's tower (MOD_TP_NO4_TO_PRS).
# The Saturn also rebuilt the room around the door (STAGE_09 map 9B, i.e.
# PRG descriptor set 1, room 4): on the PSX the bottom corridor's left end is
# a pocket under a ledge (row 22), closed off by a solid staircase down to the
# floor, so the door can't be reached; the Saturn drops the ledge and moves
# the staircase up into the middle chamber (rows 18-22, whose floor opens),
# so the corridor runs four tiles high from the right edge to the door.
# NO0 room 0 (21,34)-(26,36): the Saturn opens its bottom-right wall with a
# red door (STAGE_03 list 7: (1910,609) -> PSX (1528,609), opening right),
# over two one-tile steps; then a loading room (27,36) and the transition cell
# (28,36) into the Prison's corridor (MOD_TP_NO0_TO_PRS).
#
# The cells the Saturn changed (SATURN below: a window of rows/columns) are
# grafted from the Saturn's room: its collision, and for the art the PSX
# (foreground, background) tile pair that shows the same Saturn art elsewhere
# in the room (the Saturn cell's art, both layers stacked, compared exactly,
# as the inverted castle's palettes differ from RGB-matching: see
# gen_mod_rooms.rno3), else the pair from around the window (plus
# mods/sotn-ultimate/tools/saturn_tilematch.py's picks) closest to it with the Saturn's
# collision. The Saturn's art has holes (both layers empty: its backdrop shows
# as black squares in the bricks); they get their row's nearest brick.
#
# The doors use each stage's door-tile row 0 (closed: a solid tile; open: the
# empty tile), as their rooms' other doors do. The doorway cells are edited in
# the rooms' tilemaps in place (same size; mods/sotn-ultimate/tools/check_stage_data.py
# allows those bytes); the room and layer-pair tables can't grow in the data
# section, so const copies (rodata, mod_rooms.h, included from the stage's
# e_red_door.c) carry the new rooms and the stage header points at them. The
# copied RoomDefs point into the stage's own LayerDef table, whose position
# relative to rooms_layers (the table right after it) is read from the
# compiled header.c and asserted. The red doors are entities (layout copies:
# mods/sotn-ultimate/tools/gen_mod_layouts.py).
#
# The inverted castle's Soul Prison (MOD_STAGE_RPRS) gets the same doors,
# turned around (every row r -> rows-1-r, column c -> cols-1-c; the inverted
# tilesets use the same tile ids for the turned-around art), as on the Saturn:
# RNO4 room 5 (26,29)-(26,30): its top corridor's right end (Saturn RSTAGE09
# map 9B room 4, list 58: red door (310,97) -> PSX (248,97), opening right),
# loading room (27,29), transition (28,29), with NO4's rebuilt room turned
# around: the corridor is four tiles high (rows 6-9, floor at row 10) and the
# door (rows 6-9) sits on its floor, as on the Saturn.
# RNO0 room 0 (37,27)-(42,29): its top-left wall (RSTAGE03 list 7: red door
# (10,97) -> PSX (8,97)) over a lowered ceiling, loading room (36,27),
# transition (35,27). The teleport ids are the normal castle's: the inverted
# castle turns them into the Soul Prison's (src/dra/5087C.c).
import json
import os
import re
import shutil
import struct
import subprocess
import sys
from collections import Counter
from mod_work import work_dir  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
PRISTINE = work_dir("retail-assets")
TP = {"MOD_TP_NO4_TO_PRS": 0x87, "MOD_TP_NO0_TO_PRS": 0x89}  # include/game.h

LINKS = {
    "no4": {
        "room": (37, 33, 37, 34),
        "load": (36, 34), "exit": (35, 34), "tp": "MOD_TP_NO4_TO_PRS",
        "saturn": ("STAGE_09", "STAGE_9B", 1, 4, (18, 27, 0, 16), {}),
        # the doorway: col 0 rows 22-25 hold the closed door (tile 1, solid)
        "edits": [(22 + i, 0, 0x001) for i in range(4)],
    },
    "no0": {
        "room": (21, 34, 26, 36),
        "load": (27, 36), "exit": (28, 36), "tp": "MOD_TP_NO0_TO_PRS",
        # rows 38-41 opened through the right wall over the Saturn's steps;
        # the corner stone under the top step: the wall's own tile
        "saturn": ("STAGE_03", "STAGE_03", 0, 6, (36, 45, 84, 96), {(43, 95): (0x002, 0)}),
        # col 95 the closed door, tile 0x597 as the room's left door
        "edits": [(38 + i, 95, 0x597) for i in range(4)],
    },
    "rno4": {
        "room": (26, 29, 26, 30),
        "load": (27, 29), "exit": (28, 29), "tp": "MOD_TP_NO4_TO_PRS",
        "saturn": ("RSTAGE09", "RSTAGE9B", 1, 4, (5, 14, 0, 16), {}),
        "edits": [(6 + i, 15, 0x001) for i in range(4)],
    },
    "rno0": {
        "room": (37, 27, 42, 29),
        "load": (36, 27), "exit": (35, 27), "tp": "MOD_TP_NO0_TO_PRS",
        "saturn": ("RSTAGE03", "RSTAGE03", 0, 6, (3, 12, 0, 12), {(4, 0): (0x002, 0)}),
        "edits": [(9 - i, 0, 0x597) for i in range(4)],
    },
}


def parse_layers(stage):
    s = open(os.path.join(ROOT, f"src/st/{stage}/gen/layers.h")).read()
    externs = re.findall(r"^extern .*;$", s, re.M)
    body = s[s.index("static LayerDef layers[] = {"):]
    layers = re.findall(r"^\s*\{ (\w+), (\w+), \{(\d+), (\d+), (\d+), (\d+), (0x[0-9A-F]+)\}, "
                        r"(0x[0-9A-F]+), (0x[0-9A-F]+)\},$", body[:body.index("};")], re.M)
    rd = s[s.index("RoomDef rooms_layers[] = {"):]
    roomdefs = re.findall(r"\{ &(\w+)(?:\[(\d+)\])?, &(\w+)(?:\[(\d+)\])? \}", rd[:rd.index("};")])
    return externs, layers, roomdefs


def parse_rooms(stage):
    s = open(os.path.join(ROOT, f"src/st/{stage}/gen/rooms.c")).read()
    nums = [int(x, 0) & 0xFF for x in re.findall(r"-?0x[0-9A-Fa-f]+|-?\d+", s[s.index("{"):])]
    rooms = []
    while nums[0] != 0x40:
        rooms.append(nums[:8])
        nums = nums[8:]
    return rooms


def table_offsets(stage, n_layers):
    """(layer_empty, layers) byte offsets before rooms_layers, from header.c.o"""
    obj = os.path.join(ROOT, f"build/us/src/st/{stage}/header.c.o")
    if os.path.exists(os.path.join(ROOT, f"src/st/{stage}/layers.c")):
        obj = os.path.join(ROOT, f"build/us/src/st/{stage}/layers.c.o")  # RNO0/RNO4
    if not os.path.exists(obj):
        # a clean tree: the layout asserted on earlier builds (layer_empty,
        # then layers[], then rooms_layers, as gen/layers.h defines them)
        print(f"gen_prs_links: {obj} not built yet, assuming the usual layout")
        return 16 * n_layers + 16, 16 * n_layers
    out = subprocess.run(["mipsel-linux-gnu-objdump", "-t", obj], capture_output=True,
                         text=True, check=True).stdout
    sym = {}
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 2 and p[-1] in ("layers", "layer_empty", "rooms_layers"):
            sym[p[-1]] = int(p[0], 16)
    rl = sym["rooms_layers"]
    assert sym["layers"] == rl - 16 * n_layers, (stage, sym, n_layers)
    assert sym["layer_empty"] == sym["layers"] - 16, (stage, sym)
    return 16 * n_layers + 16, 16 * n_layers


def read_tm(path):
    d = open(path, "rb").read()
    return list(struct.unpack("<%dH" % (len(d) // 2), d))


def pristine_tm(stage, name):
    pristine = os.path.join(PRISTINE, stage, name + ".bin")
    if not os.path.exists(pristine):
        os.makedirs(os.path.dirname(pristine), exist_ok=True)
        shutil.copy(os.path.join(ROOT, f"assets/st/{stage}/{name}.bin"), pristine)
    return read_tm(pristine)


def saturn_graft(stage, sat, fg_name, bg_name, tdef, width):
    """(row, col, fg tile, bg tile) for the Saturn's cells in the window"""
    prg, mapf, dset, sroom, (r0, r1, c0, c1), overrides = sat
    up = stage.upper()
    work = work_dir("prslinks", stage)
    os.makedirs(work, exist_ok=True)
    adir = os.path.join(ROOT, f"assets/st/{stage}")
    fbin = os.path.join(ROOT, f"disks/us/ST/{up}/F_{up}.BIN")
    match = {}
    for layer in (0, 2):
        out = f"{work}/layer{layer}.bin"
        subprocess.run([sys.executable, os.path.join(ROOT, "mods/sotn-ultimate/tools/saturn_tilematch.py"),
                        os.path.join(ROOT, f"disks/saturn/{prg}.PRG"),
                        os.path.join(ROOT, f"disks/saturn/{mapf}.MAP"), str(sroom), str(layer),
                        fbin, adir, tdef, out, f"--set={dset}"], check=True, stdout=subprocess.DEVNULL)
        match[layer] = read_tm(out)
    coll = open(f"{work}/layer0.bin.coll", "rb").read()
    fpx = open(f"{work}/layer0.bin.px", "rb").read()
    bpx = open(f"{work}/layer2.bin.px", "rb").read()
    fg, bg = pristine_tm(stage, fg_name), pristine_tm(stage, bg_name)
    n_cells = len(fg)
    assert len(coll) == len(bg) == n_cells == len(fpx) // 1024, (len(coll), len(bg), n_cells)
    td = [open(f"{adir}/{tdef}_{k}.bin", "rb").read() for k in ("tiles", "pages", "cluts", "cols")]
    cols = td[3]
    tex = open(fbin, "rb").read()

    def hw(X, Y):
        o = ((X // 64) << 2 | (Y // 128) << 1 | (X % 64) // 32) * 0x2000 + ((Y % 128) * 32 + X % 32) * 2
        return tex[o] | tex[o + 1] << 8

    tiles = {}

    def render(t):
        if t not in tiles:
            pix = [None] * 256
            if t:
                page, ix, k = td[0][t], td[1][t], td[2][t]
                u, v = (ix & 15) * 16, ix & 0xF0
                for y in range(16):
                    for x in range(16):
                        j = (hw(page * 64 + (u + x) // 4, v + y) >> (((u + x) % 4) * 4)) & 15
                        if j:
                            c = hw(16 * (k % 16) + j, 0xF0 + k // 16)
                            pix[y * 16 + x] = ((c & 31) * 255 // 31, ((c >> 5) & 31) * 255 // 31,
                                               ((c >> 10) & 31) * 255 // 31)
            tiles[t] = pix
        return tiles[t]

    def pair_px(f, b):
        return [p if p is not None else q for p, q in zip(render(f), render(b))]

    def sat_cell(n):  # the Saturn's art, foreground over background (16x16 RGBA)
        f, b = bytearray(fpx[n * 1024:(n + 1) * 1024]), bpx[n * 1024:(n + 1) * 1024]
        for q in range(0, 1024, 4):
            if not f[q + 3]:
                f[q:q + 4] = b[q:q + 4]
        return bytes(f)

    def dist(cell, pix):
        d = 0
        for i, q in enumerate(pix):
            a = cell[i * 4 + 3]
            if not a or q is None:
                d += 0 if (not a) == (q is None) else 3 * 255 * 255
            else:
                d += sum((cell[i * 4 + j] - q[j]) ** 2 for j in range(3))
        return d // 256

    inside = lambda n: r0 <= n // width < r1 and c0 <= n % width < c1
    learned = {}
    for n in range(n_cells):
        if not inside(n):
            learned.setdefault(sat_cell(n), Counter())[(fg[n], bg[n])] += 1
    rows = n_cells // width
    near = {(fg[r * width + c], bg[r * width + c])
            for r in range(max(0, r0 - 4), min(rows, r1 + 4)) for c in range(max(0, c0 - 4), min(width, c1 + 4))}
    out = {}
    for r in range(r0, r1):
        for c in range(c0, c1):
            n = r * width + c
            cell = sat_cell(n)
            if (r, c) in overrides:
                out[n] = overrides[(r, c)]
                continue
            if sum(not a for a in cell[3::4]) > 64:
                # (mostly) a hole: its row's bricks, below
                assert coll[n] == 0, ("solid hole", stage, r, c)
                out[n] = None
                continue
            if cell in learned:
                f, b = learned[cell].most_common(1)[0][0]
                if cols[f] == coll[n]:
                    out[n] = (f, b)
                    continue
            cands = [p for p in near | {(match[0][n], match[2][n])} if cols[p[0]] == coll[n]]
            best = min(cands, key=lambda p: (dist(cell, pair_px(*p)), p))
            if cols[fg[n]] == coll[n] and dist(cell, pair_px(fg[n], bg[n])) <= dist(cell, pair_px(*best)):
                best = (fg[n], bg[n])
            out[n] = best
    for n in sorted(k for k, v in out.items() if v is None):
        r, c = divmod(n, width)
        for d in (1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6):
            m = n + d
            if not 0 <= c + d < width:
                continue
            v = out.get(m) if inside(m) else (fg[m], bg[m])
            if v and v[0] == 0 and v[1] and None not in render(v[1]):  # an opaque brick
                out[n] = (0, v[1])
                break
        assert out[n], ("hole", stage, r, c)
        print(f"gen_prs_links: {stage}: hole ({r},{c}) <- bg 0x{out[n][1]:03X}")
    for n, (f, b) in out.items():
        assert cols[f] == coll[n] or divmod(n, width) in overrides, (stage, divmod(n, width))
    changed = sum((fg[n], bg[n]) != v for n, v in out.items())
    print(f"gen_prs_links: {stage}: {changed} of {len(out)} cells grafted from {prg} room {sroom}")
    return [(n // width, n % width, f, b) for n, (f, b) in sorted(out.items())]


def edit_tilemap(stage, name, width, edits):
    pristine = os.path.join(PRISTINE, stage, name + ".bin")
    path = os.path.join(ROOT, f"assets/st/{stage}/{name}.bin")
    if not os.path.exists(pristine):
        os.makedirs(os.path.dirname(pristine), exist_ok=True)
        shutil.copy(path, pristine)
    d = open(pristine, "rb").read()
    tm = list(struct.unpack("<%dH" % (len(d) // 2), d))
    for row, col, tile in edits:
        tm[row * width + col] = tile
    open(path, "wb").write(struct.pack("<%dH" % len(tm), *tm))
    return len(d)


def build(stage, spec):
    externs, layers, roomdefs = parse_layers(stage)
    rooms = parse_rooms(stage)
    room = next(i for i, r in enumerate(rooms) if tuple(r[:4]) == spec["room"])
    fg_i = int(roomdefs[rooms[room][4]][1])
    fg = layers[fg_i]
    bg = layers[int(roomdefs[rooms[room][4]][3])]
    assert fg[1] == bg[1] and roomdefs[rooms[room][4]][2] == "layers", (fg, bg)
    width = (int(fg[4]) - int(fg[2]) + 1) * 16
    graft = saturn_graft(stage, spec["saturn"], fg[0], bg[0], fg[1], width)
    size = edit_tilemap(stage, fg[0], width, [(r, c, f) for r, c, f, b in graft] + spec["edits"])
    EDITED.setdefault("st" + stage, []).append([fg[0], size])
    size = edit_tilemap(stage, bg[0], width, [(r, c, b) for r, c, f, b in graft])
    EDITED.setdefault("st" + stage, []).append([bg[0], size])

    # the loading room: a copy of the stage's own, with its exit on the same
    # side (flags byte 0x40: exit left, 0x41: right)
    (lx, ly), (ex, ey) = spec["load"], spec["exit"]
    side = 0x41 if ex > lx else 0x40
    tps = {(r[0], r[1]) for r in rooms if r[5] == 0xFF}
    src = next(i for i, r in enumerate(rooms)
               if r[5] != 0xFF and r[0] == r[2] and r[1] == r[3]
               and "load" in layers[int(roomdefs[r[4]][1])][1]
               and int(layers[int(roomdefs[r[4]][1])][6], 16) == side)
    sfg = list(layers[int(roomdefs[rooms[src][4]][1])])
    sfg[2:7] = [str(lx), str(ly), str(lx), str(ly), "0x%02X" % side]
    sfg[7] = "0x%02X" % TP[spec["tp"]]   # the loading room preloads its teleport's stage
    assert all(not (r[0] <= x <= r[2] and r[1] <= y <= r[3])
               for r in rooms if r[5] != 0xFF for (x, y) in ((lx, ly), (ex, ey))), "cells taken"
    assert (ex, ey) not in tps

    empty_off, layers_off = table_offsets(stage, len(layers))

    def ref(name, idx):
        if name == "layer_empty":
            return "(LayerDef*)((u8*)rooms_layers - %d)" % empty_off
        return "(LayerDef*)((u8*)rooms_layers - %d)" % (layers_off - 16 * int(idx))

    out_roomdefs = [(ref(a, ai), ref(b, bi)) for a, ai, b, bi in roomdefs]
    sb = roomdefs[rooms[src][4]]
    out_roomdefs.append(("(LayerDef*)&g_ModLoadLayer", ref(sb[2], sb[3])))
    out_rooms = [list(r) for r in rooms]
    rs = rooms[src]
    out_rooms.append([lx, ly, lx, ly, len(out_roomdefs) - 1, rs[5], rs[6], rs[7]])
    out_rooms.append([ex, ey, ex, ey, TP[spec["tp"]], 0xFF, 0, 0])

    dst = os.path.join(ROOT, f"src/st/{stage}/mod_rooms.h")
    with open(dst, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gen_prs_links.py, do not edit\n// clang-format off\n")
        f.write("typedef char ModTeleportIdCheck[%s == 0x%02X ? 1 : -1];\n" % (spec["tp"], TP[spec["tp"]]))
        f.write("extern RoomDef rooms_layers[];\n")
        f.write("\n".join(e for e in externs if sfg[0] in e or sfg[1] in e) + "\n")
        f.write("static const LayerDef g_ModLoadLayer = { (u16*)%s, %s, {%s, %s, %s, %s, %s}, %s, %s};\n"
                % tuple(sfg))
        f.write("const RoomDef g_ModRoomsLayers[] = {\n")
        for a, b in out_roomdefs:
            f.write("    { %s, %s },\n" % (a, b))
        f.write("};\n")
        f.write("const u8 g_ModRooms[] = {\n")
        for r in out_rooms:
            f.write("    " + ", ".join(str(v) for v in r) + ",\n")
        f.write("    0x40\n};\n// clang-format on\n")
    print(f"wrote src/st/{stage}/mod_rooms.h: doorway in room {room} ({len(spec['edits'])} cells), "
          f"loading room {spec['load']} (from room {src}), transition {spec['exit']} -> "
          f"{spec['tp']}")


EDITED = {}  # overlay -> [[tilemap symbol, bytes]] edited in place

if __name__ == "__main__":
    for stage, spec in LINKS.items():
        build(stage, spec)
    # for mods/sotn-ultimate/tools/check_stage_data.py: data bytes allowed to differ
    os.makedirs(os.path.join(ROOT, "build/us"), exist_ok=True)
    json.dump(EDITED, open(os.path.join(ROOT, "build/us/mod_data_edits.json"), "w"), indent=2)
