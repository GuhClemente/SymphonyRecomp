#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: rebuild a Saturn room layer out of an existing PSX stage's
# tiles.
#
# For rooms the Saturn extended (e.g. the Castle Entrance shaft), the new
# screens are drawn in the same style as the PSX stage, so instead of adding
# tiles we match each 16x16 block of the Saturn art (narrowed from 320 to 256
# px wide, keeping 4 of every 5 columns) against the tiles the PSX stage
# already uses, and take the closest one. Collision comes with the PSX tile.
# For the foreground layer (0) the candidates are limited to PSX tiles whose
# collision equals the Saturn's for that cell: the Saturn room's collision
# stream (PRG +0x20 table, {MAP offset, size} per room, LZSS) is laid out on
# the PSX 16px grid and uses the same codes (3 solid, 0xE7 one-way platform,
# 0x80.. slopes), so platforms and slopes come out as on the Saturn. Tiles the
# stage already uses are preferred; any tile of the tileset is the fallback.
#
# --art-only matches the art alone and writes the Saturn collision next to the
# output (OUT.coll, one byte per cell): the caller pairs each tile's art with
# that collision through its own tileset copy, for when the PSX stage has no
# tile that both looks right and collides right (NP3's slope tiles are all
# wooden stairs, while the Saturn shaft's slopes are rock).
#
# --pool=A.bin,B.bin limits the art to the tiles of those tilemaps (e.g. the
# PSX room being extended), so edges keep that room's style instead of
# picking look-alikes from elsewhere in the stage.
#
# --set=N picks the PRG's Nth room descriptor set (tools/saturn/tile_dumper.py
# export_offset), for stages with several MAP files: STAGE_09's rooms in
# STAGE_9B.MAP are set 1.
#
# usage: saturn_tilematch.py SAT_PRG SAT_MAP ROOM LAYER  F_STAGE.BIN ASSET_DIR
#        TILEDEF_PREFIX OUT_TILEMAP.bin [PREVIEW.png]
import json
import os
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
sys.path.insert(0, os.path.join(ROOT, "tools/saturn"))
import tile_dumper  # noqa: E402

art_only = "--art-only" in sys.argv
pool_files = [a[len("--pool="):] for a in sys.argv if a.startswith("--pool=")]
pool_files = pool_files[0].split(",") if pool_files else []
dset = [int(a[len("--set="):]) for a in sys.argv if a.startswith("--set=")]
dset = dset[0] if dset else 0
argv = [a for a in sys.argv if a != "--art-only" and not a.startswith(("--pool=", "--set="))]
sat_prg, sat_map, room, layer, f_bin, adir, prefix, out = argv[1:9]
preview = argv[9] if len(argv) > 9 else None
room, layer = int(room), int(layer)

# --- Saturn layer, RGBA, narrowed ------------------------------------------
prg = open(sat_prg, "rb").read()
mp = open(sat_map, "rb").read()
chars, pal, tmap = tile_dumper.stage_streams(prg, mp, room, layer, descriptor_set=dset)
w, h, px, _ = tile_dumper.render_layer(chars, pal, tmap, transparent_zero=True)
W = w * 4 // 5


def sat(x, y):
    o = (y * w + (x * 5) // 4) * 4
    return px[o:o + 4]


# --- PSX tiles: render every tile the stage's tilemaps use ------------------
tex = open(f_bin, "rb").read()
vram = [[0] * 512 for _ in range(256)]
for i in range(32):
    bx, by = (i >> 2) * 0x40 + (i & 1) * 0x20, (i & 2) * 0x40
    blk = tex[i * 0x2000:(i + 1) * 0x2000]
    for y in range(128):
        row = struct.unpack_from("<32H", blk, y * 64)
        vram[by + y][bx:bx + 32] = row
pages = open(f"{adir}/{prefix}_tiles.bin", "rb").read()  # names are reversed
gfx = open(f"{adir}/{prefix}_pages.bin", "rb").read()
cluts = open(f"{adir}/{prefix}_cluts.bin", "rb").read()
used = set()
for L in json.load(open(f"{adir}/layers.json")):
    for part in L.values() if isinstance(L, dict) else ():
        if isinstance(part, dict) and part.get("tiledef") == prefix + ".json":
            d = open(f"{adir}/{part['data']}", "rb").read()
            used.update(struct.unpack("<%dH" % (len(d) // 2), d))
if pool_files:
    used = set()
    for f in pool_files:
        d = open(f, "rb").read()
        used.update(struct.unpack("<%dH" % (len(d) // 2), d))
used.discard(0)


cols = open(f"{adir}/{prefix}_cols.bin", "rb").read()
satcoll = None
if layer == 0:
    tab = tile_dumper.read_u32(prg, tile_dumper.export_offset(dset, 0x20)) - tile_dumper.STAGE_LOAD_ADDRESS
    c_off, c_size = struct.unpack_from(">II", prg, tab + room * 8)
    satcoll = tile_dumper.decompress_lzss(mp[c_off:c_off + c_size])


def rgb(c):
    return ((c & 31) * 255 // 31, ((c >> 5) & 31) * 255 // 31, ((c >> 10) & 31) * 255 // 31)


def render(t):
    page, ix, cl = pages[t], gfx[t], cluts[t]
    u, v = (ix & 15) * 16, ix & 0xF0
    k = cl
    pal16 = vram[0xF0 + k // 16][16 * (k % 16):16 * (k % 16) + 16]
    pix = []
    for y in range(16):
        for x in range(16):
            hw = vram[v + y][page * 64 + (u + x) // 4]
            i = (hw >> (((u + x) % 4) * 4)) & 15
            pix.append(None if i == 0 else rgb(pal16[i]))
    return pix


cat = {t: render(t) for t in used}


def sig(pix):
    s = []
    for by in range(4):
        for bx in range(4):
            acc = [0, 0, 0, 0]
            for y in range(4):
                for x in range(4):
                    p = pix[(by * 4 + y) * 16 + bx * 4 + x]
                    if p is not None:
                        acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2]; acc[3] += 1
            s.append(tuple(a // 16 for a in acc[:3]) + (acc[3] * 16,))
    return s


catsig = {t: sig(p) for t, p in cat.items()}
by_coll = {}
for t in sorted(used):
    by_coll.setdefault(cols[t], []).append(t)
all_by_coll = {}
for t in range(1, len(cols)):
    all_by_coll.setdefault(cols[t], []).append(t)


def candidates(c):
    # tiles with collision c: the stage's own first, else the whole tileset
    pool = by_coll.get(c) or all_by_coll.get(c, [])
    for t in pool:
        if t not in cat:
            cat[t] = render(t)
            catsig[t] = sig(cat[t])
    return pool


def dist(a, b):
    d = 0
    for p, q in zip(a, b):
        if p is None or q is None:
            if (p is None) != (q is None):
                d += 3 * 255 * 255
        else:
            d += (p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2
    return d


def sigdist(a, b):
    return sum((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2 + 4 * (p[3] - q[3]) ** 2
               for p, q in zip(a, b))


tw, th = W // 16, h // 16
if satcoll is not None:
    # (STAGE_03 room 6's stream has 3 bytes past its 96x48 cells)
    assert tw * th <= len(satcoll) < tw * th + 4, (len(satcoll), tw, th)
    satcoll = satcoll[:tw * th]
tiles = []
errs = []  # per cell, mean squared error per pixel (0 for empty cells)
err_total = 0
coll_missing = set()
blank = min(cat, key=lambda t: (sum(q is not None for q in cat[t]), t))
for ty in range(th):
    for tx in range(tw):
        blk = []
        for y in range(16):
            for x in range(16):
                p = sat(tx * 16 + x, ty * 16 + y)
                blk.append(None if p[3] == 0 else (p[0], p[1], p[2]))
        c = satcoll[ty * tw + tx] if satcoll is not None and not art_only else None
        pool = None
        if c is not None:
            pool = candidates(c) if c else None
            if c and not pool:
                coll_missing.add(c)
        if all(p is None for p in blk):
            if art_only and satcoll is not None and satcoll[ty * tw + tx]:
                # invisible but solid: the stage's least visible tile, which
                # the caller gives the Saturn's collision
                tiles.append(blank)
            elif pool:
                # no art but solid: the least visible tile with that collision
                tiles.append(min(pool, key=lambda t: sum(q is not None for q in cat[t])))
            else:
                tiles.append(0)
            errs.append(0)
            continue
        if c == 0:
            pool = [t for t in catsig if cols[t] == 0]
        s = sig(blk)
        cands = sorted(pool or catsig, key=lambda t: sigdist(s, catsig[t]))[:24]
        best = min(cands, key=lambda t: dist(blk, cat[t]))
        err_total += dist(blk, cat[best])
        errs.append(dist(blk, cat[best]) // 256)
        tiles.append(best)
open(out, "wb").write(struct.pack("<%dH" % len(tiles), *tiles))
open(out + ".err", "wb").write(struct.pack("<%dI" % len(errs), *errs))
# the full-width Saturn layer (u16 width, u16 height, RGBA rows), for callers
# that narrow a region themselves
with open(out + ".full", "wb") as f:
    f.write(struct.pack("<HH", w, h))
    f.write(bytes(px[:w * h * 4]))
# the narrowed Saturn art per cell (16x16 RGBA, alpha 0 = transparent), for
# callers that port the cells no PSX tile matches
with open(out + ".px", "wb") as f:
    for ty in range(th):
        for tx in range(tw):
            for y in range(16):
                for x in range(16):
                    f.write(bytes(sat(tx * 16 + x, ty * 16 + y)))
if satcoll is not None:
    open(out + ".coll", "wb").write(satcoll)
print(f"room {room} layer {layer}: {tw}x{th} tiles from {len(cat)} PSX tiles, "
      f"mean error {err_total / max(1, sum(1 for t in tiles if t)) / 256:.0f}")
if satcoll is not None and not art_only:
    bad = sum(cols[t] != satcoll[i] for i, t in enumerate(tiles))
    print(f"  collision: {bad} cells differ from the Saturn"
          + (f" (no PSX tile has {sorted(hex(c) for c in coll_missing)})" if coll_missing else ""))

if preview:
    from PIL import Image
    img = Image.new("RGB", (tw * 16 * 2 + 16, th * 16), (255, 0, 255))
    for ty in range(th):
        for tx in range(tw):
            for y in range(16):
                for x in range(16):
                    p = sat(tx * 16 + x, ty * 16 + y)
                    if p[3]:
                        img.putpixel((tx * 16 + x, ty * 16 + y), tuple(p[:3]))
                    t = tiles[ty * tw + tx]
                    if t and cat[t][y * 16 + x] is not None:
                        img.putpixel((tw * 16 + 16 + tx * 16 + x, ty * 16 + y), cat[t][y * 16 + x])
    img.save(preview)
