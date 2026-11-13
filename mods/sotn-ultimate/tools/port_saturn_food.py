#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: add the Saturn's two new food sprites to weapon 21.
#
# PSX food is thrown by weapon 21 (src/weapon/w_021.c): an item's unk14 is its
# food id and the sprite is frame unk14 + 1 of w_021_1 (assets/weapon/
# w_021_1.animset.json, graphics in assets/weapon/f_021.png, 256x128 4bpp).
# PSX has foods 0..40 and 41 = Meal ticket. The Saturn added Smelly rice (41)
# and Delicious meal (42) and moved the Meal ticket to 43; PSX keeps the Meal
# ticket, so the new foods become 42 and 43 (frames 43 and 44; frame 42 stays
# empty, the Meal ticket never draws one).
#
# Saturn food sprites are shared sprite bank 10 (0.BIN tables at 0x06047070
# frames, 0x06047120 images, 0x060471D0 palettes); the pixels sit in every
# stage CHR. Saturn palette banks 16 and 17 have the colours of PSX food
# palettes 6 and 4, so the pixel indices are used as is. The Saturn drew its
# food 1.25x wider for its screen (its stretched PSX foods repeat every 5th
# column), so the new art is narrowed back by dropping every 5th column.
#
# Idempotent: rewrites the right half of f_021.png (empty in retail) and
# truncates the animset back to its 42 retail frames before appending.
import json
import os
import struct
import zlib

ROOT = os.path.join(os.path.dirname(__file__), "../../..")
SAT = os.path.join(ROOT, "disks/saturn")
PNG = os.path.join(ROOT, "assets/weapon/f_021.png")
ANIM = os.path.join(ROOT, "assets/weapon/w_021_1.animset.json")
RETAIL_FRAMES = 42

# (Saturn image, PSX clut, box left, food name)
FOODS = [(41, 6, 128, "Smelly rice"), (42, 4, 144, "Delicious meal")]
BOX = 16

zero = open(os.path.join(SAT, "0.BIN"), "rb").read()
prg = open(os.path.join(SAT, "STAGE_00.PRG"), "rb").read()
chr_ = open(os.path.join(SAT, "STAGE_00.CHR"), "rb").read()
ZB, LB = 0x06004080, 0x060DC000


def res(a):
    if LB <= a < LB + len(prg):
        return prg, a - LB
    if ZB <= a < ZB + len(zero):
        return zero, a - ZB
    return None


u32 = lambda d, o: struct.unpack_from(">I", d, o)[0]

# Walk the stage's sprite banks in order to find where bank 10's pixels start
# in the CHR (each image is padded to 0x20 bytes).
tab = u32(prg, 4) - LB
cursor, seen, food = 0, set(), None
for i in range(256):
    a = u32(prg, tab + i * 4)
    if res(a) is None:
        break
    d, o = res(a)
    ia, pa, al, fl = struct.unpack_from(">IIHH", d, o)
    if (al, fl) != (0xFFFF, 0xFFFF) or res(ia) is None or res(pa) is None or ia in seen:
        continue
    seen.add(ia)
    idd, io = res(ia)
    recs = []
    for k in range(512):
        w, h, _ = struct.unpack_from(">BBH", idd, io + k * 4)
        if w == 0 or h == 0:
            break
        recs.append((w * 2, h * 2, cursor))
        cursor += -(-(w * h * 2) // 0x20) * 0x20
    if ia == 0x06047120:
        food = recs
assert food and len(food) >= 43, "Saturn food bank not found"


def saturn_image(n):
    w, h, off = food[n]
    data = chr_[off:off + w * h // 2]
    px = [v for b in data for v in (b >> 4, b & 15)]
    return [px[y * w:(y + 1) * w] for y in range(h)]


def crop(rows):
    rows = [r for r in rows if any(r)]
    cols = [x for x in range(len(rows[0])) if any(r[x] for r in rows)]
    return [r[cols[0]:cols[-1] + 1] for r in rows]


def narrow(rows):
    # drop every 5th column, starting where the dropped columns look most
    # like their left neighbours (the Saturn's own stretches repeat them)
    w = len(rows[0])
    col = lambda x: [r[x] for r in rows]
    best = max(range(5), key=lambda p: sum(
        sum(a == b for a, b in zip(col(x), col(x - 1)))
        for x in range(max(p, 1), w, 5)))
    keep = [x for x in range(w) if not (x >= max(best, 1) and (x - best) % 5 == 0)]
    return [[r[x] for x in keep] for r in rows]


# --- graphics: paste into the empty right half of f_021.png -----------------
png = open(PNG, "rb").read()
# PNG chunks
chunks, pos = [], 8
while pos < len(png):
    n = struct.unpack(">I", png[pos:pos + 4])[0]
    chunks.append((png[pos + 4:pos + 8], png[pos + 8:pos + 8 + n]))
    pos += 12 + n
ihdr = dict(chunks)[b"IHDR"]
W, H, depth, ctype = struct.unpack(">IIBB", ihdr[:10])
assert (W, H, ctype) == (256, 128, 3) and depth in (4, 8), (W, H, depth, ctype)
raw = zlib.decompress(b"".join(d for t, d in chunks if t == b"IDAT"))
stride = (W * depth + 7) // 8
rows = []
for y in range(H):
    line = raw[y * (stride + 1):(y + 1) * (stride + 1)]
    assert line[0] == 0, "only filter 0 rows are supported"
    data = line[1:]
    if depth == 8:
        rows.append(list(data))
    else:
        rows.append([v for b in data for v in (b >> 4, b & 15)])

frames = []
for img, clut, left, name in FOODS:
    art = narrow(crop(saturn_image(img)))
    h, w = len(art), len(art[0])
    assert w <= BOX and h <= BOX, (name, w, h)
    ox, oy = (BOX - w) // 2, BOX - h  # centred, standing on the box bottom
    for y in range(BOX):
        for x in range(BOX):
            rows[y][left + x] = 0
    for y in range(h):
        for x in range(w):
            rows[oy + y][left + ox + x] = art[y][x]
    frames.append([{"flags": 0, "x": -BOX // 2, "y": -BOX // 2, "width": BOX,
                    "height": BOX, "clut": clut, "tileset": 0, "left": left,
                    "top": 0, "right": left + BOX, "bottom": BOX}])
    print(f"{name}: Saturn image {img} {len(saturn_image(img)[0])} wide -> {w}x{h}, clut {clut}")

out = b""
for r in rows:
    if depth == 8:
        out += b"\x00" + bytes(r)
    else:
        out += b"\x00" + bytes((r[i] << 4) | r[i + 1] for i in range(0, W, 2))


def chunk(t, d):
    return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)


# rewrite only IDAT: every other chunk stays byte-identical (see the note in
# mods/sotn-ultimate/tools/port_saturn_icons.py)
new = png[:8]
idat_done = False
for t, d in chunks:
    if t == b"IDAT":
        if not idat_done:
            new += chunk(b"IDAT", zlib.compress(out, 9))
            idat_done = True
        continue
    new += chunk(t, d)
open(PNG, "wb").write(new)

# --- frames -------------------------------------------------------------------
anim = json.load(open(ANIM))
assert len(anim) >= RETAIL_FRAMES
anim = anim[:RETAIL_FRAMES] + [None] + frames
json.dump(anim, open(ANIM, "w"), indent=2)
print(f"weapon 21: {len(anim)} frames (foods 42 and 43 added)")
