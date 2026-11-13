#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod. Run by mods/sotn-ultimate/tools/build_disc.sh: make disk takes build/us/F_MAP.BIN
# over the retail one (the map blueprint is loaded once at boot).
"""Paint Saturn-only castle-map cells into the PS1 blueprint F_MAP.BIN.

PS1 F_MAP.BIN: 256x256, 4bpp, LOW nibble = left pixel, 128 bytes per row,
loaded once by main.c to CASTLE_MAP_PTR (0x801E0000, 0x8000 bytes).
Cell (x,y) = 5x5 window at pixel (4x, 4y); rows/cols 0 and 4 are shared with
the neighbours (walls/doors), 1..3 are the interior.
Blueprint colours: 0 wall/none (drawn as 14 once visited), 1 room revealed by
the bought map, 2 room NOT on the bought map (drawn as 1 once visited),
4 save room, 5 warp room.

Saturn SUB_DISP.MAP (first 0xA000 bytes): 320 wide, 4bpp, HIGH nibble = left,
160 bytes per row, same grid (0,0 origin) but 5 px per cell: window 6x5 at
(5x, 4y). Colours: 1 room, 4 save, 6 warp.

usage: paint_fmap.py IN_F_MAP.BIN SUB_DISP.MAP OUT_F_MAP.BIN [preview.png]
"""
import sys
import numpy as np

# Saturn -> PS1 colour. Rooms use 2 (not on the bought map) unless listed in
# BOUGHT, which must match D_800A2C0C (src/dra/5087C.c).
SAT2PS = {1: 2, 4: 4, 6: 5}
BOUGHT = set()  # e.g. {(2, 41), (2, 42), (2, 43), (2, 44)} if D_800A2C0C gets them

# Cells taken from the Saturn blueprint (Garden + Entrance shaft + 6th warp)
CELLS = (
    [(2, 41), (2, 42), (2, 43), (2, 44), (3, 44), (2, 45), (3, 45)]
    + [(x, 44) for x in range(4, 22)]
    + [(x, 45) for x in range(4, 14)]
)
# Cursed Prison (STAGE_16), for later:
PRISON = ([(x, 34) for x in range(32, 37)] + [(x, 35) for x in range(33, 36)]
          + [(x, 36) for x in range(27, 36)])

# PS1-only: the mod's moved save room (gen_mod_rooms.py SAVE_ROOM -> (3,43)),
# door on its left, into the shaft cell (2,43). (3,42) is kept (the inverted
# castle's map mirrors it), but its door to the shaft is wall: both Entrance
# stages (NO3 and NP3) and the inverted one moved their save room behind the
# shaft's alcove.
MOD_SAVE = (3, 43)
OLD_SAVE_DOOR = (4 * 3, 4 * 42 + 2)  # (X, Y): (3,42)'s left edge, middle row


def load_ps(path):
    a = np.frombuffer(open(path, "rb").read(), np.uint8).reshape(256, 128)
    px = np.zeros((256, 256), np.uint8)
    px[:, 0::2] = a & 0xF
    px[:, 1::2] = a >> 4
    return px


def save_ps(px, path):
    a = (px[:, 0::2] & 0xF) | (px[:, 1::2] << 4)
    data = a.astype(np.uint8).tobytes()
    assert len(data) == 0x8000
    open(path, "wb").write(data)


def load_sat(path):
    b = open(path, "rb").read()[: 160 * 256]
    s = np.frombuffer(b, np.uint8).reshape(256, 160)
    px = np.zeros((256, 320), np.uint8)
    px[:, 0::2] = s >> 4
    px[:, 1::2] = s & 0xF
    return px


COLS = {0: (0,), 1: (1,), 2: (2, 3), 3: (4,), 4: (5,)}


def sat_window(sat, x, y):
    w = np.zeros((5, 5), np.uint8)
    for r in range(5):
        for c, scs in COLS.items():
            v = 0
            for sc in scs:
                v = v or int(sat[4 * y + r, 5 * x + sc])
            if v:
                v = SAT2PS[v]
                if v == 2 and (x, y) in BOUGHT:
                    v = 1
            w[r, c] = v
    return w


def paint(ps, sat, cells):
    targets = set(cells)
    orig = ps.copy()

    def cell_nonempty(cx, cy):
        return 0 <= cx < 64 and 0 <= cy < 64 and orig[4 * cy + 2, 4 * cx + 2] != 0

    for (x, y) in cells:
        w = sat_window(sat, x, y)
        for r in range(5):
            for c in range(5):
                X, Y = 4 * x + c, 4 * y + r
                v = int(w[r, c])
                interior = 1 <= r <= 3 and 1 <= c <= 3
                if interior or v:
                    ps[Y, X] = v
                    continue
                # border pixel with no Saturn opening: keep the PS1 pixel
                # only when it is shared with a kept (non-target) PS1 room
                nb = []
                if c == 0: nb.append((x - 1, y))
                if c == 4: nb.append((x + 1, y))
                if r == 0: nb.append((x, y - 1))
                if r == 4: nb.append((x, y + 1))
                keep = any(n not in targets and cell_nonempty(*n) for n in nb)
                ps[Y, X] = orig[Y, X] if keep else 0


def paint_mod_save(ps, shaft_colour):
    x, y = MOD_SAVE
    for r in range(5):
        for c in range(5):
            X, Y = 4 * x + c, 4 * y + r
            if 1 <= r <= 3 and 1 <= c <= 3:
                ps[Y, X] = 4
            elif c == 0 and r == 2:
                ps[Y, X] = shaft_colour  # door: corridor colour, as (2,42)|(3,42)
            elif c == 0:
                ps[Y, X] = 0  # rest of the shaft's right wall
            elif not (c == 4 or r == 0 or r == 4):
                pass
            else:
                # other walls of the save room; (3,44) below and (4,43) right
                # are not connected
                ps[Y, X] = 0 if (c == 4 or r == 4 or r == 0) and ps[Y, X] not in (1, 2) else ps[Y, X]


def preview(ps, path, cells):
    from PIL import Image
    # display colours as func_800F1B08 would draw visited cells
    pal = {0: (0, 0, 0), 1: (40, 90, 220), 4: (220, 40, 40), 5: (230, 200, 40), 14: (235, 235, 235)}
    img = np.zeros((256, 256, 3), np.uint8)
    for y in range(64):
        for x in range(64):
            if not ps[4 * y + 2, 4 * x + 2] and (x, y) not in cells:
                continue
            for r in range(5):
                for c in range(5):
                    v = int(ps[4 * y + r, 4 * x + c])
                    v = 1 if v == 2 else (14 if v == 0 else v)
                    if img[4 * y + r, 4 * x + c].any() and v == 14:
                        continue
                    img[4 * y + r, 4 * x + c] = pal.get(v, (255, 0, 255))
    crop = img[34 * 4:48 * 4 + 1, 0:26 * 4 + 1]
    Image.fromarray(crop).resize((crop.shape[1] * 10, crop.shape[0] * 10), Image.NEAREST).save(path)


def main():
    ps = load_ps(sys.argv[1])
    sat = load_sat(sys.argv[2])
    paint(ps, sat, CELLS + PRISON)
    paint_mod_save(ps, 1 if (2, 43) in BOUGHT else 2)
    ps[OLD_SAVE_DOOR[1], OLD_SAVE_DOOR[0]] = 0
    save_ps(ps, sys.argv[3])
    if len(sys.argv) > 4:
        preview(ps, sys.argv[4], set(CELLS + PRISON) | {MOD_SAVE})
    for Y in range(40 * 4, 46 * 4 + 1):
        print("%3d %s" % (Y, "".join("%x" % v if v else "." for v in ps[Y, 0:23 * 4 + 1])))


if __name__ == "__main__":
    main()
