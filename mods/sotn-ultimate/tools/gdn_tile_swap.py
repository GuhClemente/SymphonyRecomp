#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: PS1 art for the Underground Garden's stairs and stonework
# (include/game.h MOD_GDN_PS1_TILES; off: nothing changes).
#
# The Garden's tiles come from the Saturn's art narrowed from 320 to 256 px
# (one column in five dropped), which makes its stair edges, steps and stone
# joints uneven ("wobbly"). The Garden's stairs and stone are the Castle
# Entrance's design, so the chosen cells take the PS1 Entrance's tiles instead:
# PS1 pixels for the stair / stone itself, the Garden's own pixels (its dark
# brick, the mushroom, transparency) everywhere else, and room 1's left stair
# corner as the mirror of its right one (already PS1 art). The approved art
# is a recipe over the player's own discs (builder/src/gdn_tile_swap_recipe.ts:
# no pixels), rebuilt into $MOD_WORK/tile_atlases (atlas_path: 16x16 RGBA
# tiles, colours as the converter's previews: 5-bit * 255 // 31), and
# gdn_ps1_tiles/cells.json (room, foreground row and column, atlas tile;
# Garden rooms 0-3).
#
# apply() runs between the converter and gdn_assets.py's repack: each changed
# cell gets a new tile (its old tile's collision; one per art + collision),
# its graphic appended to graphics.bin and its palette either an existing one
# holding all its colours, a new one (the Garden: packed 15 colours per new
# palette), or, where no palette slot is free (the Hell Garden), the nearest
# existing palette. The old tiles drop out of the stage when no cell uses them
# any more (repack places only the tiles in use). The Hell Garden is the
# Garden rotated 180 degrees: flip=True turns each cell and its art around.
# It is not an exact copy though (the Saturn's own Hell art, narrowed
# separately), so it also takes its own cells (review round 4, user
# 2026-10-26: room 1's left stair corner, the Garden's right corner turned
# around, which the Garden never needed): cells_rgdn.json and its atlas,
# in the Hell Garden's own orientation and palettes, applied as they are.
#
# The Cursed Prison and the Soul Prison use the same code with their own data
# (prs_assets.py: data=PRS_DATA, switch MOD_PRS_PS1_TILES, own="" for the
# Prison, "_rprs" for the Soul Prison, each in its own orientation): their
# staircases redrawn from the PS1 Underground Caverns' stair band, the
# Prison corridor's ceiling slopes from the Soul Prison's floor slopes
# (review rounds 5 / 5b, 2026-10-27).
# Every colour is in the cell's own palette (no new palette); cells beside a
# stair that were empty (tile 0) get art too (empty=True: air collision).
import json
import os
import re
import struct
import subprocess
import sys

import numpy as np

from mod_work import work_dir  # noqa: E402

from mod_work import read_src

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gdn_ps1_tiles")
SWITCH = "MOD_GDN_PS1_TILES"
PRS_DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "prs_ps1_tiles")
PRS_SWITCH = "MOD_PRS_PS1_TILES"


def enabled(switch=SWITCH):
    """the switch is defined (not commented out) in include/game.h"""
    src = read_src("include/game.h")
    return re.search(r"^#define %s\b" % switch, src, re.M) is not None


def rgb(c):
    return ((c & 31) * 255 // 31, ((c >> 5) & 31) * 255 // 31, ((c >> 10) & 31) * 255 // 31)


FIVE = {c * 255 // 31: c for c in range(32)}


def prgb(v):
    """a palette entry as a colour; raw 0 is transparent on the PS1 (an
    unused entry), so it matches no colour"""
    return rgb(v) if v else None


def to555(col):
    r, g, b = (FIVE[v] for v in col)
    return r | g << 5 | b << 10


def reduce_colours(cols, counts, limit=15):
    """merge the closest colours (the rarer into the commoner) until at most
    `limit` remain; returns {colour: kept colour}"""
    keep = {c: c for c in cols}
    live = sorted(cols)
    while len(live) > limit:
        best = None
        for i, a in enumerate(live):
            for b in live[i + 1:]:
                d = sum((x - y) ** 2 for x, y in zip(a, b))
                if best is None or d < best[0]:
                    best = (d, a, b)
        _, a, b = best
        lose, win = (a, b) if counts[a] < counts[b] else (b, a)
        live.remove(lose)
        for k, v in keep.items():
            if v == lose:
                keep[k] = win
    return keep


def atlas_path(data, name):
    """the atlas of data set name (16x16 RGBA tiles, one after the other),
    rebuilt from the discs by mods/sotn-ultimate/tools/builder/node/tile_atlases.ts when
    missing or older than the recipe (src/gdn_tile_swap_recipe.ts)"""
    out = work_dir("tile_atlases")
    path = os.path.join(out, os.path.basename(data), "atlas%s.rgba" % name)
    builder = os.path.join(ROOT, "mods/sotn-ultimate/tools/builder")
    recipe = max(os.path.getmtime(os.path.join(builder, f)) for f in (
        "src/gdn_tile_swap_recipe.ts", "src/gdn_tile_swap.ts", "node/tile_atlases.ts"))
    if not os.path.exists(path) or os.path.getmtime(path) < recipe:
        subprocess.run([os.environ.get("NODE", "node"), "--no-warnings",
                        os.path.join(builder, "node/tile_atlases.ts"), out], check=True,
                       stdout=sys.stderr)
    return path


def apply(work, flip=False, free_palettes=None, data=DATA, switch=SWITCH, own=None,
          empty=False):
    """free_palettes: how many palettes may be added (None: any). data /
    switch: the data folder and its include/game.h switch; own: apply only
    that data set (cells<own>.json, its atlas) as it is, instead of the
    Garden's shared set (+ the Hell Garden's own when flip); empty: cells
    whose tile is 0 (nothing) may get art too"""
    if not enabled(switch):
        return None
    def load(name):
        spec = json.load(open(os.path.join(data, "cells%s.json" % name)))
        raw = np.frombuffer(open(atlas_path(data, name), "rb").read(), dtype=np.uint8)
        art = [t.copy() for t in raw.reshape(-1, 16, 16, 4)]
        return [(c, art[c["tile"]]) for c in spec["cells"]]

    if own is not None:
        jobs = [(c, a, False) for c, a in load(own)]
    else:
        # (cell, art, turned around): the shared list, then the Hell
        # Garden's own
        jobs = [(c, a, flip) for c, a in load("")]
        if flip:
            jobs += [(c, a, False) for c, a in load("_rgdn")]

    graphics = bytearray(open(os.path.join(work, "graphics.bin"), "rb").read())
    pal_raw = bytearray(open(os.path.join(work, "palettes.bin"), "rb").read())
    palettes = [list(struct.unpack_from("<16H", pal_raw, i)) for i in range(0, len(pal_raw), 32)]
    n_old_pals = len(palettes)
    td = {k: bytearray(open(os.path.join(work, f"tiledefs.{k}.bin"), "rb").read())
          for k in ("gfx_page", "gfx_index", "clut", "collision")}
    layers = json.load(open(os.path.join(work, "rooms.layers.json")))
    # raw values (STP bit included) the stage already uses per colour
    raw_of = {}
    for p in palettes:
        for c in p[1:]:
            if c:
                raw_of.setdefault(rgb(c), c)

    def raw(col):
        if col in raw_of:
            return raw_of[col]
        c = to555(col)
        return c if c else 0x8000  # opaque black needs the STP bit

    def slot_place(s):
        page = 0
        while s >= (240 if page < 4 else 256):
            s -= 240 if page < 4 else 256
            page += 1
        return page, (s // 16) << 4 | (s % 16)

    maps = {}
    new_graphic, new_tile = {}, {}
    stats = {"cells": 0, "tiles": 0, "graphics": 0, "palettes_new": 0,
             "palettes_reused": 0, "nearest": 0, "max_err": 0}
    for c, a, turn in jobs:
        room = c["room"]
        fg = layers[room]["fg"]
        w = (fg["right"] - fg["left"] + 1) * 16
        h = (fg["bottom"] - fg["top"] + 1) * 16
        y, x = c["y"], c["x"]
        if turn:
            y, x = h - 1 - y, w - 1 - x
            a = a[::-1, ::-1]
        path = os.path.join(work, fg["data"] + ".tilelayout.bin")
        if path not in maps:
            d = open(path, "rb").read()
            maps[path] = list(struct.unpack("<%dH" % (len(d) // 2), d))
        tm = maps[path]
        old = tm[y * w + x]
        assert old or empty, (work, room, y, x)
        coll = td["collision"][old]

        op = a[..., 3] > 0
        pix = [tuple(int(v) for v in p) for p in a[..., :3][op]]
        counts = {}
        for p in pix:
            counts[p] = counts.get(p, 0) + 1
        cols = set(counts)
        # an existing palette with every colour
        k = next((i for i, p in enumerate(palettes)
                  if cols <= {prgb(v) for v in p[1:]}), None)
        remap = {col: col for col in cols}
        if k is None and not flip:
            remap = reduce_colours(cols, counts)
            want = set(remap.values())
            k = next((i for i in range(n_old_pals, len(palettes))
                      if len(want | {rgb(v) for v in palettes[i][1:] if v}) <= 15), None)
            if k is None:
                assert free_palettes is None or len(palettes) - n_old_pals < free_palettes, \
                    "no palette slot left for the PS1 tiles"
                palettes.append([0] * 16)
                k = len(palettes) - 1
            have = [v for v in palettes[k][1:] if v]
            for col in sorted(want):
                if raw(col) not in have:
                    have.append(raw(col))
            palettes[k] = [0] + have + [0] * (15 - len(have))
            stats["max_err"] = max(stats["max_err"], max(
                max(abs(p - q) for p, q in zip(col, remap[col])) for col in cols))
        elif k is None:
            # no free palette slot: the existing palette nearest to the art
            def err(p):
                pc = [prgb(v) for v in p[1:] if v]
                if not pc:
                    return float("inf")
                return sum(counts[col] * min(sum((s - t) ** 2 for s, t in zip(col, q))
                                             for q in pc) for col in cols)
            k = min(range(len(palettes)), key=lambda i: err(palettes[i]))
            pc = [prgb(v) for v in palettes[k][1:] if v]
            for col in cols:
                remap[col] = min(pc, key=lambda q: sum((s - t) ** 2 for s, t in zip(col, q)))
            stats["nearest"] += 1
            stats["max_err"] = max(stats["max_err"], max(
                max(abs(p - q) for p, q in zip(col, remap[col])) for col in cols))
        else:
            stats["palettes_reused"] += 1
        pal_cols = [None] + [prgb(v) for v in palettes[k][1:]]
        idx = bytearray(256)
        for i, (yy, xx) in enumerate(zip(*np.nonzero(op))):
            idx[yy * 16 + xx] = pal_cols.index(remap[pix[i]], 1)
        gkey = (bytes(idx), k)
        if bytes(idx) not in new_graphic:
            new_graphic[bytes(idx)] = len(graphics) // 256
            graphics += idx
            stats["graphics"] += 1
        s = new_graphic[bytes(idx)]
        tkey = (gkey, coll)
        if tkey not in new_tile:
            assert len(td["clut"]) < 0x1000, "tileset full"
            page, index = slot_place(s)
            for kk, v in (("gfx_page", page), ("gfx_index", index), ("clut", k),
                          ("collision", coll)):
                td[kk].append(v)
            new_tile[tkey] = len(td["clut"]) - 1
            stats["tiles"] += 1
        tm[y * w + x] = new_tile[tkey]
        stats["cells"] += 1

    stats["palettes_new"] = len(palettes) - n_old_pals
    for path, tm in maps.items():
        open(path, "wb").write(struct.pack("<%dH" % len(tm), *tm))
    open(os.path.join(work, "graphics.bin"), "wb").write(graphics)
    open(os.path.join(work, "palettes.bin"), "wb").write(
        b"".join(struct.pack("<16H", *p) for p in palettes))
    for kk, v in td.items():
        open(os.path.join(work, f"tiledefs.{kk}.bin"), "wb").write(v)
    return stats
