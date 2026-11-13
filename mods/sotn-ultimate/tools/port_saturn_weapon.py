#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: convert a Saturn weapon's art and data to the PSX.

The Saturn's WEAPON0.PRG / WEAPON1.PRG hold a 0x3000-byte slot per weapon id
(code and data linked at 0x060D6000 for hand 0, 0x060D9000 for hand 1; the
two hands are the same apart from addresses, so hand 0 is read). The header
is the PSX's 16 function pointers, then at +0x40 a pointer to a 0-terminated
list of sprite banks {images, palettes, 0xFFFF, 0xFFFF}: images are
{w/2, h/2, u16} records whose 4bpp pixels (high nibble first, 32-byte
aligned) follow one another in the weapon's 0x4000-byte WEAPON0.CHR slot;
palettes start with a u16 count. A bank's frame table is the pointer table
right before it; a frame is {u16 parts << 8, u16 flags} then parts
{u16 attr, s16 x, s16 y, u16 image} (x/y the image's centre, attr >> 8 the
palette, attr & 0x10 / 0x20 flips): the format mods/sotn-ultimate/tools/saturn_sprites.py
reads for stages. The attack data is the PSX's: WeaponAnimation (16 bytes,
big-endian), u16 frame lists (sprite frame | hitbox << 9, 0xFFFF ends) and
s8 {x, y, w, h} hitboxes.

The PSX weapon (mods/sotn-ultimate/src/items/weapon/w_0NN.c) gets:
  assets/weapon/f_0NN.png      its 4bpp 256x128 art page (VRAM tpage 25, the
                               upper half for hand 0, the lower for hand 1;
                               png2bin makes the 0x4000-byte slot art)
  mods/sotn-ultimate/src/items/weapon/mod/w_0NN_data.h  sprite bank(s) w_0NN_1.. (the gen/w_NNN_1.h
                               form), g_WeaponCluts, the frame lists,
                               hitboxes and WeaponAnimation tables
The Saturn drew its art 1.25x wider than the PSX: images are narrowed back to
4 of every 5 columns and their positions and the hitboxes' x / width scaled
by 4/5, as saturn_sprites.PsxSpriteSheet does for the stages (it packs the
images into the page's two 128x128 halves: part tileset 0 or 1, which the
entity's unk5A puts on tpage 25 / the hand's half).

Usage: port_saturn_weapon.py ID...   (ids in WEAPONS below)
"""
import json
import os
import struct
import sys

import png

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from saturn_sprites import PsxSpriteSheet  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
SAT = os.path.join(ROOT, "disks/saturn")
PRG_SLOT, CHR_SLOT = 0x3000, 0x4000
BASE = 0x060D6000  # hand 0
N_WEAPON_PAL = 24

# Saturn Alucard animation ids (WeaponAnimation.frameStart) -> PSX ones: the
# Saturn's player animation table is laid out differently
FRAME_START = {0x40: 0x48, 0x60: 0x68, 0x77: 0x7D, 0x7E: 0x84, 0xE0: 247}

# Per weapon:
#  anims      [(Saturn address, entries)] of each WeaponAnimation table, in the
#             order the C code wants them (w_0NN_anims_0, _1, ...); found in
#             the slot's code (dis/sat_w0_NN.s). None: scan for them.
#  front_ref  a PSX weapon animset (assets/weapon/*.animset.json) with the same
#             frame layout; its frames drawn in front of Alucard (part flag
#             0x100: zPriority + 4) get the flag here too. The Saturn data has
#             no such flag.
#  semi_pals  Saturn palette numbers drawn additive (STP set on every colour,
#             e.g. the swing trails; the entity's blendMode is TRANSP | ADD,
#             which only blends the texels with STP)
#  cluts      g_WeaponCluts entries (an item's palette field picks one)
WEAPONS = {
    # Double Blade Axe: the Claymore family's frames (w_051), Morningstar code
    63: dict(anims=[(0x060D6580, 1)], front_ref="w_051_1", semi_pals=[1], cluts=1),
    # Astral Dagger: the Combat knife's frames (w_032) + 14 for the special's
    # raised dagger. Its homing dagger is a primitive textured with image 3
    # (prim_images: Saturn image numbers of the first bank whose page
    # position is emitted as W_0NN_IMG<n>_U/V/W/H)
    62: dict(anims=[(0x060D6A80, 2)], front_ref="w_032_1", semi_pals=[], cluts=1,
             prim_images=[3]),
    # Woodland Shield: the Leather shield's frames (w_008); a shield has no
    # WeaponAnimation (its AnimationFrames/hitboxes are w_008's, in the C).
    # The Shield rod spell's primitives: thorn burst (image 3) and the vine's
    # six leaf segments (10-15)
    61: dict(anims=[], front_ref="w_008_1", semi_pals=[], cluts=1,
             prim_images=[3, 10, 11, 12, 13, 14, 15]),
    # Alucard Spear: the Claymore family's frames (w_051) for the swing, then
    # 65-68 the special's sparks and 69-79 the dash's after-images. One
    # table, three anims: 0 the swing (frameStart 0x60), 1 the back-forward
    # special (0xE0, the new player anim 247), 2 the QCF special (0x60).
    # Palette 1 (swing trail, sparks) and 2 (red after-images) are glow
    # ramps from black: additive
    60: dict(anims=[(0x060D6A50, 3)], front_ref="w_051_1", semi_pals=[1, 2], cluts=1),
}


class SaturnWeapon:
    """One Saturn weapon slot; image/palettes/frame match saturn_sprites'
    SaturnStage so PsxSpriteSheet can convert its sprites."""

    def __init__(self, wid, hand=0):
        self.base = BASE + hand * 0x3000
        prg = open(os.path.join(SAT, "WEAPON%d.PRG" % hand), "rb").read()
        chr_ = open(os.path.join(SAT, "WEAPON%d.CHR" % hand), "rb").read()
        self.prg = prg[wid * PRG_SLOT:(wid + 1) * PRG_SLOT]
        self.chr = chr_[wid * CHR_SLOT:(wid + 1) * CHR_SLOT]
        self.banks = {}     # bank -> ([(w, h, chr offset)], palettes address)
        self.bank_list = []  # [(bank, frame table, frames)]
        cursor = 0
        tab = self.u32(self.base + 0x40)
        for i in range(16):
            bank = self.u32(tab + 4 * i)
            if not self.ok(bank):
                break
            ia, pa, al, fl = struct.unpack_from(">IIHH", self.prg, bank - self.base)
            assert (al, fl) == (0xFFFF, 0xFFFF), "bad sprite bank %08X" % bank
            recs = []
            for k in range(512):
                w, h, _ = struct.unpack_from(">BBH", self.prg, ia - self.base + 4 * k)
                if w == 0 or h == 0:
                    break
                recs.append((w * 2, h * 2, cursor))
                cursor += -(-(w * h * 2) // 0x20) * 0x20
            assert cursor <= CHR_SLOT
            self.banks[bank] = (recs, pa)
            # the frame table: the pointers to frames right before the bank
            a = bank
            while self.ok(self.u32(a - 4)) and self._is_frame(self.u32(a - 4)):
                a -= 4
            n = (bank - a) // 4
            assert n > 0, "no frame table before bank %08X" % bank
            self.bank_list.append((bank, a, n))

    def ok(self, a):
        return self.base <= a < self.base + PRG_SLOT

    def u32(self, a):
        return struct.unpack_from(">I", self.prg, a - self.base)[0]

    def u16(self, a):
        return struct.unpack_from(">H", self.prg, a - self.base)[0]

    def _is_frame(self, p):
        n, fl = struct.unpack_from(">HH", self.prg, p - self.base)
        # (the spear's dash after-images have up to 18 parts)
        return n & 0xFF == 0 and (n >> 8) <= 24

    # --- SaturnStage interface
    def palettes(self, bank):
        pa = self.banks[bank][1]
        n = self.u16(pa) & 0xFF
        return [struct.unpack_from(">16H", self.prg, pa - self.base + 2 + 32 * k)
                for k in range(max(n, 1))]

    def image(self, bank, img):
        w, h, off = self.banks[bank][0][img]
        data = self.chr[off:off + w * h // 2]
        px = [v for b in data for v in (b >> 4, b & 15)]
        return w, h, [px[y * w:(y + 1) * w] for y in range(h)]

    def frame(self, frames, i):
        p = self.u32(frames + 4 * i)
        n = self.u16(p) >> 8
        return [struct.unpack_from(">Hhhh", self.prg, p - self.base + 4 + 8 * k)
                for k in range(n)]

    # --- attack data
    def scan_anims(self):
        """WeaponAnimation-looking records: two in-slot pointers, a palette,
        a sound id 0x600-0x7FF, a start, a sound frame, 0."""
        found = []
        for o in range(0, PRG_SLOT - 16, 4):
            fr, fp, pal, snd, st, sf, un = struct.unpack_from(">IIHHBBh", self.prg, o)
            if self.ok(fr) and self.ok(fp) and pal < N_WEAPON_PAL and 0x600 <= snd < 0x800 \
                    and un == 0 and self.ok(self.u32(fr)):
                found.append(self.base + o)
        return found

    def anim(self, a):
        return struct.unpack_from(">IIHHBBh", self.prg, a - self.base)

    def frame_lists(self, table, stop):
        """The u16 lists of a frames table (pointers until one isn't, or an
        address in stop)."""
        lists = []
        a = table
        while self.ok(self.u32(a)) and a not in stop:
            p = self.u32(a)
            seq = []
            while True:
                v = self.u16(p)
                seq.append(v)
                p += 2
                if v == 0xFFFF:
                    break
                assert len(seq) < 256
            lists.append(seq)
            a += 4
        return lists


def trunc45(v):
    # C-style (toward zero), which matches the PSX's own hitboxes of the
    # weapons both versions have (Saturn weapon 3 vs w_003)
    return int(v * 4 / 5)


def psx_colour(c, semi):
    c &= 0x7FFF
    if semi:
        return c | 0x8000 if c else 0
    return c or 0x8000


def convert(wid):
    cfg = WEAPONS[wid]
    sw = SaturnWeapon(wid)
    name = "w_%03d" % wid

    front = set()
    if cfg.get("front_ref"):
        ref = json.load(open(os.path.join(ROOT, "assets/weapon/%s.animset.json" % cfg["front_ref"])))
        front = {i for i, f in enumerate(ref) if f and any(p["flags"] & 0x100 for p in f)}

    def fill(sheet):
        banks = []
        for bank, table, n in sw.bank_list:
            sheet.palette_block(sw, bank)
            frames = [sheet.frame(sw, bank, table, i) or None for i in range(n)]
            banks.append(frames)
        return banks

    # the first pass learns each image's widest narrowed copy (NW_MAX) and
    # which images are asked for; the second packs them tallest first
    probe = PsxSpriteSheet([(0, 0), (0, 1)] * 8, 0)
    fill(probe)
    sheet = PsxSpriteSheet([(0, 0), (0, 1)], 0)
    sheet.prepack(sw, probe.requests)
    banks = fill(sheet)
    # images the C draws as primitives (cfg prim_images): their unflipped copy
    # (the widest if frames asked for several), placed now if no frame uses it
    prim_images = []
    for img in cfg.get("prim_images", []):
        bank = sw.bank_list[0][0]
        keys = [k for k in sheet.placed if k[:3] == (bank, img, 0)]
        if keys:
            q, u, v, iw, ih = sheet.placed[max(keys, key=lambda k: sheet.placed[k][3])]
        else:
            q, u, v, iw, ih = sheet._place(sw, bank, img)
        quad = sheet.quadrants[q][1]
        prim_images.append((img, u + 128 * (quad & 1), v + 64 * (quad & 2), iw, ih))
    assert sheet.used_quadrants() <= 2, "art doesn't fit a 256x128 page"
    for frames in banks:
        for i, parts in enumerate(frames):
            for p in parts or []:
                if i in front:
                    p["flags"] |= 0x100

    # art: quadrant 0 = columns 0-127, quadrant 1 = 128-255
    rows = [sheet.pages[0][y] + sheet.pages[1][y] for y in range(128)]
    grey = [(17 * i, 17 * i, 17 * i) for i in range(16)]
    with open(os.path.join(ROOT, "assets/weapon/f_%03d.png" % wid), "wb") as f:
        png.Writer(256, 128, palette=grey, bitdepth=8).write(f, rows)

    # animset json too, for the asset viewers (the header is what's built)
    for bi, frames in enumerate(banks):
        with open(os.path.join(ROOT, "assets/weapon/%s_%d.animset.json" % (name, bi + 1)), "w") as f:
            json.dump(frames, f, indent=2)

    # palettes: the bank palettes from the anims' palette offset on (the
    # PSX's clut is 0x110 + anim palette + part clut, part clut being the
    # Saturn palette number here)
    anims = cfg.get("anims")
    if anims is None:
        anims = [(a, 1) for a in sw.scan_anims()]
        print("%s: scanned anim records %s" % (name, [hex(a) for a, _ in anims]))
    records = [[sw.anim(a + 16 * k) for k in range(n)] for a, n in anims]
    pal_offsets = {r[2] for t in records for r in t}
    pal0 = min(pal_offsets) if pal_offsets else 0  # (a shield: no anims)
    if len(pal_offsets) > 1:
        print("%s: WARNING anims use palette offsets %s; bank palettes placed at %d"
              % (name, sorted(pal_offsets), pal0))
    semi = set(cfg.get("semi_pals", []))
    cluts = [[0] * 16 for _ in range(N_WEAPON_PAL)]
    # (palette_block gave the banks' palettes consecutive cluts from 0, in
    # bank order; k is that clut)
    k = 0
    for bank, _, _ in sw.bank_list:
        for colours in sw.palettes(bank):
            assert pal0 + k < N_WEAPON_PAL, "too many palettes"
            cluts[pal0 + k] = [0 if j == 0 else psx_colour(c, k in semi)
                               for j, c in enumerate(colours)]
            k += 1
    assert k == len(sheet.cluts)

    out = []
    w = out.append
    w("// Generated by mods/sotn-ultimate/tools/port_saturn_weapon.py from the Saturn's weapon %d\n" % wid)
    w("// (disks/saturn/WEAPON0.PRG/.CHR slot %d); do not edit\n" % wid)
    w("// clang-format off\n")
    # sprite banks, as tools/sotn-assets writes src/weapon/gen/w_NNN_1.h
    for bi, frames in enumerate(banks):
        sym = "%s_%d" % (name, bi + 1)
        sizes = {}
        for i, parts in enumerate(frames):
            if parts:
                n = len(parts)
                sizes[i] = n * 11 + 1 + (2 if n & 1 else 1)
                w("static s16 %s_%d[%d];\n" % (sym, i, sizes[i]))
        w("static s16* %s[] = {\n" % sym)
        for i, parts in enumerate(frames):
            w("    %s,\n" % ("%s_%d" % (sym, i) if parts else "0"))
        w("};\n")
        for i, parts in enumerate(frames):
            if not parts:
                continue
            vals = [len(parts)]
            for p in parts:
                vals += [p[k] for k in ("flags", "x", "y", "width", "height", "clut",
                                        "tileset", "left", "top", "right", "bottom")]
            vals += [0] * (sizes[i] - len(vals))
            w("static s16 %s_%d[] = {\n    %d,\n" % (sym, i, vals[0]))
            for k in range(len(parts)):
                w("    %s,\n" % ", ".join(str(v) for v in vals[1 + 11 * k:12 + 11 * k]))
            if sizes[i] > len(parts) * 11 + 1:
                w("    %s\n" % ", ".join(str(v) for v in vals[1 + 11 * len(parts):]))
            w("};\n")
    # palettes
    w("\nstatic u16 %s_cluts[N_WEAPON_PAL][COLORS_PER_PAL] = {\n" % name)
    for c in cluts:
        w("    {%s},\n" % ", ".join("0x%04X" % v for v in c))
    w("};\n")
    w("static u16* g_WeaponCluts[] = {%s};\n" % ", ".join(["%s_cluts" % name] * cfg.get("cluts", 1)))
    # primitive images: u/v in tpage 0x19 for hand 0 (hand 1: v + 0x80), size
    for img, u, v, iw, ih in prim_images:
        for k, val in zip("UVWH", (u, v, iw, ih)):
            w("#define %s_IMG%d_%s %d\n" % (name.upper(), img, k, val))
    # frame lists, hitboxes, WeaponAnimation tables
    tables, props = {}, {}
    stop = {a for a, n in anims for a in range(a, a + 16 * n, 16)}
    stop |= {r[1] for t in records for r in t}
    for t in records:
        for fr, fp, *_ in t:
            if fr not in tables:
                tables[fr] = sw.frame_lists(fr, stop)
            props.setdefault(fp, 0)
            for seq in tables[fr]:
                for v in seq[:-1]:
                    props[fp] = max(props[fp], ((v >> 9) & 0x7F) + 1)
    for ti, (fr, lists) in enumerate(tables.items()):
        uniq = []
        for seq in lists:
            if seq not in uniq:
                uniq.append(seq)
        w("\n")
        for li, seq in enumerate(uniq):
            w("static u16 %s_frames_%d_%d[] = {\n    %s};\n" % (
                name, ti, li, ", ".join("0x%04X" % v for v in seq)))
        w("static u16* %s_frames_%d[] = {\n    %s};\n" % (
            name, ti, ", ".join("%s_frames_%d_%d" % (name, ti, uniq.index(s)) for s in lists)))
    for pi, (fp, n) in enumerate(props.items()):
        w("\nstatic s8 %s_hitboxes_%d[][4] = {\n" % (name, pi))
        for k in range(n):
            x, y, bw, bh = struct.unpack_from("4b", sw.prg, fp - sw.base + 4 * k)
            w("    {%d, %d, %d, %d},\n" % (trunc45(x), y, trunc45(bw), bh))
        w("};\n")
    for ai, t in enumerate(records):
        w("\nstatic WeaponAnimation %s_anims_%d[] = {\n" % (name, ai))
        for fr, fp, pal, snd, st, sf, _ in t:
            assert st in FRAME_START, "%s: no PSX animation for Saturn frameStart 0x%X" % (name, st)
            w("    {%s_frames_%d, %s_hitboxes_%d, %d, 0x%03X, 0x%02X, %d},\n" % (
                name, list(tables).index(fr), name, list(props).index(fp), pal, snd,
                FRAME_START[st], sf))
        w("};\n")
    os.makedirs(os.path.join(ROOT, "mods/sotn-ultimate/src/items/weapon/mod"), exist_ok=True)
    path = os.path.join(ROOT, "mods/sotn-ultimate/src/items/weapon/mod/%s_data.h" % name)
    text = "".join(out)
    if not os.path.exists(path) or open(path).read() != text:
        open(path, "w").write(text)
    print("%s: %d frames, %d images in %d quadrant(s), %d palettes, %d anim table(s)" % (
        name, sum(len(f) for f in banks), len(sheet.placed), sheet.used_quadrants(),
        len(sheet.cluts), len(records)))


def main():
    ids = [int(a) for a in sys.argv[1:]] or sorted(WEAPONS)
    for wid in ids:
        convert(wid)


if __name__ == "__main__":
    main()
