#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: convert Saturn stage sprites to PSX sprite banks.
#
# A Saturn stage PRG's entity entries point to {images, palettes, 0xFFFF,
# 0xFFFF} sprite banks: images are {w/2, h/2, unused} records whose 4bpp
# pixels follow one another in the stage's .CHR file (32-byte aligned), in the
# order the banks are first referenced; palettes start with a u16 count, then
# 16-colour RGB555 banks (the same layout as PSX colours, bit 15 aside). A
# frame table entry points to a frame: u16 parts << 8, u16 flags, then parts
# {u16 attr, s16 x, s16 y, u16 image}; x/y are the image's centre, attr >> 8
# picks the palette, attr & 0x10 / 0x20 flip it.
#
# The PSX draws sprite parts from texture pages in VRAM: a part is 11 s16
# {flags, x, y, w, h, clut, tileset, u0, v0, u1, v1} with x/y its top-left
# corner, tileset = tpage << 2 | quadrant (u/v +128) and clut a g_ClutIds
# index (src/dra/4B758.c). Images are narrowed to PSX proportions (4 of every
# 5 columns, as the stage art: the Saturn draws 320 px where the PSX draws
# 256), placed with their centre scaled by 4/5, and packed into 128x128
# quadrants of the stage sprite pages (VRAM x 128-255, y 256-511: tpages 18
# and 19, where retail stages load theirs, e.g. NP3's enemies).
import struct


class SaturnStage:
    def __init__(self, prg_path, chr_path, zero_path, base=0x060DC000):
        self.prg = open(prg_path, "rb").read()
        self.chr = open(chr_path, "rb").read()
        self.zero = open(zero_path, "rb").read()
        self.base = base
        self.zbase = 0x06004080
        self.banks = self._walk_banks()

    def res(self, a):
        if self.base <= a < self.base + len(self.prg):
            return self.prg, a - self.base
        if self.zbase <= a < self.zbase + len(self.zero):
            return self.zero, a - self.zbase
        return None

    def u32(self, a):
        d, o = self.res(a)
        return struct.unpack_from(">I", d, o)[0]

    def _walk_banks(self):
        tab = self.u32(self.base + 4)
        cursor, seen, banks = 0, set(), {}
        for i in range(256):
            a = self.u32(tab + i * 4)
            if self.res(a) is None:
                break
            d, o = self.res(a)
            ia, pa, al, fl = struct.unpack_from(">IIHH", d, o)
            if (al, fl) != (0xFFFF, 0xFFFF) or self.res(ia) is None or self.res(pa) is None \
                    or ia in seen:
                continue
            seen.add(ia)
            idd, io = self.res(ia)
            recs = []
            for k in range(512):
                w, h, _ = struct.unpack_from(">BBH", idd, io + k * 4)
                if w == 0 or h == 0:
                    break
                recs.append((w * 2, h * 2, cursor))
                cursor += -(-(w * h * 2) // 0x20) * 0x20
            banks[a] = (recs, pa)
        return banks

    def palettes(self, bank):
        recs, pa = self.banks[bank]
        d, o = self.res(pa)
        n = struct.unpack_from(">H", d, o)[0] & 0xFF
        return [struct.unpack_from(">16H", d, o + 2 + 32 * k) for k in range(max(n, 1))]

    def image(self, bank, img):
        w, h, off = self.banks[bank][0][img]
        data = self.chr[off:off + w * h // 2]
        px = [v for b in data for v in (b >> 4, b & 15)]
        return w, h, [px[y * w:(y + 1) * w] for y in range(h)]

    def frame(self, frames, i):
        p = self.u32(frames + 4 * i)
        d, o = self.res(p)
        n = struct.unpack_from(">H", d, o)[0] >> 8
        return [struct.unpack_from(">Hhhh", d, o + 4 + 8 * k) for k in range(n)]


QUAD_EDGE = 127  # usable columns/rows of a 128x128 quadrant (see _alloc)
# Banks of rotating entities (ENTITY_ROTATE): the PSX rotates each part's
# corners on their own and rounds them, which can open a one-pixel seam where
# two parts meet. Their images repeat an edge column into the part's border
# where another part meets that edge in any frame, and their parts get a
# border row above and below too, repeated the same way (BLEED: image key ->
# bits, 1 left, 2 right, 4 top, 8 bottom; gathered by frame(), shared by
# every sheet).
BLEED_BANKS = set()
BLEED = {}  # (bank, image, flip) -> bits
# One PSX copy per Saturn image (and flip): its narrowed width is the widest
# any frame asks for (frame() narrows each part from its edges, which can ask
# for an image a column narrower or wider by where it sits; a copy per width
# took the Man Eater's sprites from 4 to 5.4 quadrants). A part given the
# wider copy overlaps its neighbour by a column instead of touching it.
NW_MAX = {}  # (bank, image, flip) -> width
BLEED_ROWS = False
# banks whose palette blocks set the STP bit on every colour (palette_block):
# no change when drawn opaque, but their sprites can be drawn translucent (the
# PS1 only blends STP texels)
STP_BANKS = set()
# banks whose images are packed trimmed to their content (their transparent
# margins dropped, the parts moved to match): bank -> images kept whole (e.g.
# one a primitive stretches whole). The Ifrit's bank only fits room 0's
# quadrants this way.
TRIM_BANKS = {}
TRIM = {}  # placed key -> (dx, dy) of the stored content in the full image


def _trim(rows):
    """rows -> (rows cut to their content's bounding box, dx, dy)"""
    ys = [y for y, r in enumerate(rows) if any(r)]
    if not ys:
        return [rows[0][:1]], 0, 0
    xs = [x for x in range(len(rows[0])) if any(r[x] for r in rows)]
    x0, x1, y0, y1 = xs[0], xs[-1] + 1, ys[0], ys[-1] + 1
    return [r[x0:x1] for r in rows[y0:y1]], x0, y0


class PsxSpriteSheet:
    """Packs converted sprites into 128x128 quadrants and builds PSX frames."""

    def __init__(self, quadrants, clut_base, cluts=None):
        self.quadrants = list(quadrants)        # [(tpage, q)], filled in order
        self.pages = [[[0] * 128 for _ in range(128)] for _ in self.quadrants]
        self.skylines = {}                     # quadrant -> filled height per column
        self.requests = []                     # placed keys, in request order
        self.clut_base = clut_base
        # converted palettes, in clut order (may be shared by several sheets)
        self.cluts = [] if cluts is None else cluts
        self.placed = {}                       # (bank, img) -> (quad, u, v, w, h)
        self.blocks = {}                       # bank -> first clut of its palettes

    def _clut(self, colours, semi=False):
        # bit 15 (STP) marks the texels a semi-transparent primitive blends;
        # opaque sprites only need it on black (0 would be transparent)
        pal = tuple(0 if j == 0 or not c else
                    (c | 0x8000) if semi else (c & 0x7FFF) or 0x8000
                    for j, c in enumerate(colours))
        if pal not in self.cluts:
            self.cluts.append(pal)
        return self.clut_base + self.cluts.index(pal)

    @staticmethod
    def _pixels(stage, bank, img, flip, nw=None):
        # narrowed to PSX proportions (to nw columns, sampled at their
        # centres); flipped images are stored flipped: the PSX's own part flip
        # reads u - 1 (src/dra/4B758.c), which leaves the quadrant at u = 0
        w, h, rows = stage.image(bank, img)
        if nw is None:
            nw = max(1, w * 4 // 5)
        # a column sampled transparent takes the content of the source columns
        # it covers if there is any: shapes keep their extent, so parts that
        # touch on the Saturn still touch (the Water Leaper's body ends one
        # column short of its image; sampling dropped its last content column
        # and left a gap before the tail)
        spans = [(x * w // nw, max(x * w // nw + 1, -(-(x + 1) * w // nw)),
                  (2 * x + 1) * w // (2 * nw)) for x in range(nw)]
        rows = [[r[c] or next((v for v in r[a:b] if v), 0) for a, b, c in spans]
                for r in rows]
        if flip & 2:
            rows = [r[::-1] for r in rows]
        if flip & 1:
            rows = rows[::-1]
        return rows

    def _limits(self, q):
        """(first usable column, columns, rows) of a quadrant. A part's u/v are
        u8s that the PSX moves by 0x80 in a tpage's right/lower quadrant
        (src/dra/4B758.c), so an edge at 128 there becomes 256 = 0: those
        quadrants keep their last column/row free (QUAD_EDGE). A part drawn
        facing right reads from u - 1, so a part at u = 0 of a tpage's left
        quadrant reads from 0xFF and is drawn garbled: column 0 of those
        quadrants stays free."""
        quad = self.quadrants[q][1]
        return (0 if quad & 1 else 1), (QUAD_EDGE if quad & 1 else 128), \
            (QUAD_EDGE if quad & 2 else 128)

    def _alloc(self, w, h):
        # skyline packing: each quadrant keeps the filled height of each of
        # its columns; the image goes where its top would be lowest, in the
        # first quadrant it fits (images of different heights nest, where
        # shelves would leave dead bands)
        for q in range(len(self.pages)):
            x0, lw, lh = self._limits(q)
            sky = self.skylines.setdefault(q, [0] * 128)
            best = None
            for x in range(x0, lw - w + 1):
                y = max(sky[x:x + w])
                if y + h <= lh and (best is None or y < best[0]):
                    best = (y, x)
            if best is not None:
                y, x = best
                for i in range(x, x + w):
                    sky[i] = y + h
                return q, x, y
        raise AssertionError("sprite quadrants full")

    def _place(self, stage, bank, img, flip=0, nw=None):
        if nw is not None:
            nw = max(nw, NW_MAX.get((bank, img, flip), nw))
        key = (bank, img, flip, nw)
        bleed = BLEED.get((bank, img, flip), 0)
        if key in self.placed:
            return self.placed[key]
        rows = self._pixels(stage, bank, img, flip, nw)
        if bank in TRIM_BANKS and img not in TRIM_BANKS[bank]:
            rows, dx, dy = _trim(rows)
            TRIM[key] = (dx, dy)
        w, h = len(rows[0]), len(rows)
        # rows above and below (BLEED_ROWS; their bleed costs a quadrant of the
        # Leaper's and wasn't needed once narrowing kept the shapes' extent)
        vb = 2 if BLEED_ROWS and bank in BLEED_BANKS else 0
        # the rotating banks' images get a border column on each side, which
        # their parts span (frame(): BLEED repeats the edge there); the others
        # are packed tight (the PSX maps a part's texture exactly either way)
        hb = 1 if bank in BLEED_BANKS else 0
        q, x, y = self._alloc(w + 2 * hb, h + 2 * vb)
        x += hb
        y += vb
        if vb and bleed & 4:
            self.pages[q][y - 1][x:x + w] = rows[0]
        if vb and bleed & 8:
            self.pages[q][y + h][x:x + w] = rows[-1]
        for yy, r in enumerate(rows):
            self.pages[q][y + yy][x:x + w] = r
            # the edge column copied into the part's border (BLEED)
            if bleed & 1:
                self.pages[q][y + yy][x - 1] = r[0]
            if bleed & 2:
                self.pages[q][y + yy][x + w] = r[-1]
        self.placed[key] = (q, x, y, w, h)
        self.requests.append(key)
        return self.placed[key]

    def prepack(self, stage, keys):
        """Places the given images (keys from another sheet's requests) tallest
        first, which packs far tighter than placing them as frames ask."""
        sized = []
        for key in keys:
            rows = self._pixels(stage, *key)
            if key[0] in TRIM_BANKS and key[1] not in TRIM_BANKS[key[0]]:
                rows = _trim(rows)[0]
            sized.append((len(rows), len(rows[0]), key))
        for _, _, key in sorted(sized, key=lambda t: (-t[0], -t[1])):
            self._place(stage, *key)

    def palette_block(self, stage, bank):
        """Reserves all of a bank's palettes, in the Saturn's order, as
        consecutive cluts: its parts then use base + the Saturn palette index,
        so an entity's palette offset picks a colour variant as the Saturn's
        clutBase offset does. Returns the base."""
        if bank not in self.blocks:
            self.blocks[bank] = self.clut_base + len(self.cluts)
            for colours in stage.palettes(bank):
                stp = 0x8000 if bank in STP_BANKS else 0
                self.cluts.append(tuple(0 if j == 0 or not c else (c & 0x7FFF | stp) or 0x8000
                                        for j, c in enumerate(colours)))
        return self.blocks[bank]

    def frame(self, stage, bank, frames, index, pal_shift=0):
        """pal_shift is added to the parts' Saturn palette numbers (for frames
        the Saturn draws with a negative clutBase offset, which the PSX can't
        take: see the Human Face Tree's leaves)."""
        pals = stage.palettes(bank)
        parts = []
        # narrowed from their edges, not centre and width: parts that touch on
        # the Saturn (a whip, a skeleton) share a PSX column edge too, where
        # rounding each part alone opened gaps between them
        spans = []
        for attr, cx, cy, img in stage.frame(frames, index):
            sw, sh = stage.image(bank, img)[:2]
            spans.append((round((cx - sw / 2) * 4 / 5), round((cx + sw / 2) * 4 / 5),
                          cy - sh // 2, cy - sh // 2 + sh))
        for i, (attr, cx, cy, img) in enumerate(stage.frame(frames, index)):
            flip = (2 if attr & 0x10 else 0) | (1 if attr & 0x20 else 0)
            left, right, top, bottom = spans[i]
            nw = max(1, right - left)
            NW_MAX[(bank, img, flip)] = max(nw, NW_MAX.get((bank, img, flip), 0))
            if bank in BLEED_BANKS:
                # the edges another part of the frame meets (see BLEED)
                bleed = 0
                for j, (l2, r2, t2, b2) in enumerate(spans):
                    if j != i and t2 < bottom and top < b2:
                        if abs(r2 - left) <= 1 and l2 < left:
                            bleed |= 1
                        if abs(l2 - right) <= 1 and r2 > right:
                            bleed |= 2
                    if j != i and l2 < right and left < r2:
                        if abs(b2 - top) <= 1 and t2 < top:
                            bleed |= 4
                        if abs(t2 - bottom) <= 1 and b2 > bottom:
                            bleed |= 8
                BLEED[(bank, img, flip)] = BLEED.get((bank, img, flip), 0) | bleed
            q, u, v, w, h = self._place(stage, bank, img, flip, nw)
            tpage, quad = self.quadrants[q]
            # a trimmed image (TRIM_BANKS): its content's place in the image
            nw_key = max(nw, NW_MAX.get((bank, img, flip), nw))
            tdx, tdy = TRIM.get((bank, img, flip, nw_key), (0, 0))
            sh = stage.image(bank, img)[1]
            # the rotating banks' parts span a border column on each side (and
            # a row above and below with BLEED_ROWS; see _place)
            hb = 1 if bank in BLEED_BANKS else 0
            vb = 1 if BLEED_ROWS and bank in BLEED_BANKS else 0
            parts.append({
                "flags": 0,
                "x": left - hb + tdx, "y": (cy - sh // 2 + tdy if (tdx, tdy) != (0, 0) or
                                           bank in TRIM_BANKS else cy - h // 2) - vb,
                "width": w + 2 * hb, "height": h + 2 * vb,
                "clut": (self.blocks[bank] + ((attr >> 8) + pal_shift) % len(pals)
                         if bank in self.blocks
                         else self._clut(pals[((attr >> 8) + pal_shift) % len(pals)])),
                "tileset": tpage << 2 | quad,
                "left": u - hb, "top": v - vb, "right": u + w + hb, "bottom": v + h + vb,
            })
        # the Saturn draws a frame's parts in order (the last on top); the PSX
        # links each into the ordering table's head (src/dra/4B758.c addPrim),
        # so its first part ends up on top: store them the other way round
        return parts[::-1]

    def image_part(self, stage, bank, img, dx, dy, colours, flip=0):
        """A part for one image whose top-left corner sits at (dx, dy), with
        the given 16 colours (e.g. a colour lookup table); flip as frame()'s
        (2 left-right, 1 top-bottom: stored flipped)."""
        q, u, v, w, h = self._place(stage, bank, img, flip)
        sw = stage.image(bank, img)[0]
        tpage, quad = self.quadrants[q]
        return {
            "flags": 0, "x": round((dx + sw / 2) * 4 / 5 - w / 2), "y": dy,
            "width": w, "height": h, "clut": self._clut(colours, semi=True),
            "tileset": tpage << 2 | quad,
            "left": u, "top": v, "right": u + w, "bottom": v + h,
        }

    def quadrant_pixels(self, q):
        """4bpp, 128x128, as LoadTPage takes it (low nibble first)."""
        out = bytearray()
        for row in self.pages[q]:
            for x in range(0, 128, 2):
                out.append(row[x] | row[x + 1] << 4)
        return bytes(out)

    def used_quadrants(self):
        return max(p[0] for p in self.placed.values()) + 1 if self.placed else 0
