#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Saturn Maria's graphics for MAR.BIN (build_mar.sh).

Writes build/us/mod/mar/:
- mar_sprites.h   her 346 images, narrowed to PS1 proportions (one column in
                  five dropped at a fixed absolute phase, so every frame of an
                  animation narrows alike), cropped, 4bpp, each LZSS-packed
                  behind a raw {w, h, px, py} header. They don't fit raw (about
                  198 KB against Richter's 94 KB); like the Saturn (MARIA.CHR is
                  LZSS too) the overlay unpacks the images on screen into
                  per-entity buffers every frame (pl_blueprints.c).
- mar_spritebank0.h  her frame table (Saturn frame_table) in the PS1
                  {sprite | 0x8000, x, y, clut} format; Saturn x is the image
                  centre at 1.25x, so x maps through the same column drop.
- mar_anims.h     pl_anims.c's animations with the Saturn Maria's data. The
                  Richter -> Maria mapping comes from the Saturn code: the
                  Saturn Richter animations are byte-identical to the PS1 ones,
                  and every Maria function that is a fork of a Richter one
                  loads her animation where his loads his. Animations with no Maria counterpart
                  keep Richter's timing and endings (the step code waits on
                  them) with her standing frame. Also her hitbox table, x and
                  width at 4/5 (the Saturn's Richter table is the PS1's at
                  1.25x).
- mar_pal.h       her palette (PAL_RICHTER), uploaded by RicInit.
"""
import os
import struct
import sys

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "mods", "sotn-ultimate", "tools"))
import build_ric2 as b  # noqa: E402

OUT = os.path.join(ROOT, "build", "us", "mod", "mar")
SAT = 0x060A5000
PHASE = 4  # absolute Saturn column x with x % 5 == PHASE is dropped
PROF = dict(b.cv.CHARACTER_PROFILES["maria"])
HITBOX_TABLE = 0x060C37CC  # right after her animations, up to the palettes
# the Saturn frame table's y is 1 below the PS1's: the Saturn Richter frames
# (same art as the PS1's) draw 1 px lower than the PS1 Richter ones when put
# through "centre - h // 2", so her frames move up 1 (she stood 1 px into the
# floor)
SAT_Y = -1
STAND = 0x060C28E8  # her idle (Saturn 060A9210 off a ledge)

# PS1 pl_anims.c symbol -> Saturn Maria animation (see the module doc)
ANIM_MAP = {
    # Saturn 060A9210: 060C28E8 idle (hands at the waist), 060C292C at a
    # ledge (IN_AIR_OR_EDGE: mar_anim_stand_edge)
    "ric_anim_stand": 0x060C28E8,
    "ric_anim_stand_relax": 0x060C28E8,
    "ric_anim_walk": 0x060C29B0,
    "ric_anim_crouch_from_stand2": 0x060C2AA8,
    "ric_anim_crouch": 0x060C2AB0,
    "D_801554E0": 0x060C2BEC,
    "D_80155544": 0x060C2DB8,
    "D_8015555C": 0x060C3704,
    "D_80155588": 0x060C3760,
    "D_801555A8": 0x060C30B0,
    "D_801555C8": 0x060C30EC,
    "D_801555E8": 0x060C2ED8,
    "D_80155638": 0x060C2F00,
    "ric_anim_run": 0x060C2A18,
    "ric_anim_stun": 0x060C2C28,
    "D_8015569C": 0x060C2D84,
    "D_801556C4": 0x060C2C28,
    "D_80155704": 0x060C2D84,
    "ric_anim_brandish_whip": 0x060C28D8,
    "D_80155740": 0x060C2B38,
    "D_80155788": 0x060C3720,
    "ric_anim_high_jump": 0x060C2BB8,
    "ric_anim_blade_dash": 0x060C2ADC,
    # her RicSetJump / fall / landing forks (Saturn 060A9474, 060A8F2C,
    # 060A939C, 060A8248), the hit / item steps (060A9D10, 060A9DA4) and the
    # stage-start one (060A914C)
    "D_801554F0": 0x060C2B18,
    "D_8015550C": 0x060C2ADC,
    "D_80155528": 0x060C2B38,
    "D_80155534": 0x060C2B38,
    "D_80155750": 0x060C2E4C,
    "D_8015577C": 0x060C2F8C,
    "D_8015591C": 0x060C3788,
    "D_80155950": 0x060C37B8,
}

# her attack animations with no Richter counterpart (Saturn 060A9990 attack,
# 060A9BC4 kick, 060A6B90 dive kick, 060A66C8 / 060A714C attack endings)
EXTRA_ANIMS = {
    "mar_anim_atk_air_down": 0x060C3114,  # attack in the air holding Down
    "mar_anim_kick": 0x060C2FA4,  # Triangle standing
    "mar_anim_kick_crouch": 0x060C3080,
    "mar_anim_kick_air_fall": 0x060C2F18,
    "mar_anim_kick_air_rise": 0x060C3734,
    "mar_anim_atk_end": 0x060C3164,  # stand after the attack
    "mar_anim_atk_end_b": 0x060C3184,  # same, vram_flag & 0x20
    "mar_anim_kick_end": 0x060C2FC8,
    "mar_anim_kick_end_b": 0x060C2FF0,
    "mar_anim_kick_crouch_end": 0x060C309C,
    "mar_anim_dive": 0x060C2F48,  # dive kick moving
    "mar_anim_dive_down": 0x060C2F54,  # dive kick straight down
    "mar_anim_uppercut_diag": 0x060C2B04,  # 060A9608, a direction held
    "mar_anim_roll": 0x060C2E64,  # the roll ending her slide (060A8860)
    "mar_anim_stand_edge": 0x060C292C,  # standing at a ledge
    "mar_anim_backdash": 0x060C2E04,  # R: her backdash (060A9E40)
    "mar_anim_backdash_end": 0x060C2E34,  # standing after it (060A66C8)
    "mar_anim_backdash_end_b": 0x060C2E40,  # same, vram_flag & 0x20
    "mar_anim_throw": 0x060C2E78,  # sub-weapons but the dagger (060A973C)
    "mar_anim_throw_b": 0x060C2EA8,  # same, vram_flag & 0x20
    # her command specials (060A9F54 table; 060A9064 runs steps 0x30-0x35).
    # Their POSE_JUMPs (into the Saturn's own group) end the animation here;
    # MarStepSpecial sets the next one (mar_sp_next)
    "mar_anim_sp_knuckle": 0x060C3518,  # Guardian Knuckle
    "mar_anim_sp_genbu": 0x060C31AC,  # Genbu
    "mar_anim_sp_genbu2": 0x060C31E8,
    "mar_anim_sp_byakko": 0x060C33D8,  # Byakko
    "mar_anim_sp_byakko2": 0x060C3438,
    "mar_anim_sp_byakko3": 0x060C34D4,
    "mar_anim_sp_byakko4": 0x060C34F8,
    "mar_anim_sp_seiryu": 0x060C32BC,  # Seiryuu
    "mar_anim_sp_seiryu2": 0x060C32F8,
    "mar_anim_sp_seiryu3": 0x060C3390,
    "mar_anim_sp_seiryu4": 0x060C33B0,
    "mar_anim_sp_summon": 0x060C3258,  # Suzaku, Four Beasts
    "mar_anim_sp_summon2": 0x060C3284,
}
ANIM_GROUPS = 0x060C28A0  # her POSE_JUMP table (14 animations)
JUMP_ENDS = {n for n in EXTRA_ANIMS if n.startswith("mar_anim_sp_")}

# Her attack effects: Saturn MAR_W.CHR sprite packages (MARIA.PRG resource
# array 060C6D68; MAR_W.CHR holds them in g_MariaSpriteBanks order).
FX_RES = 0x060C6D68
FX_ORDER = (1, 2, 3, 8, 4, 6, 5, 7, 9, 10, 11, 12)
FX_NFRAMES = {1: 55, 2: 2, 3: 4, 4: 13, 5: 38, 6: 5, 7: 9, 8: 17, 9: 20, 10: 10,
              11: 14, 12: 19}
# PS1 VRAM for them: 32x128 blocks of F_GAME2's area (x 512-1023, y 256-511,
# streamed in by DRA cd.c) that its file leaves empty and that nothing else
# writes while she plays (checked 2026-10-23). Taken, so NOT used:
#   x 800 y 256-383: DRA's shared gfx banks (d_37d8.c) put the save / loading
#     rooms' objects and the "item up" text there (her art lived there until
#     2026-10-23: her shots broke after a save load);
#   x 832-895, y 256-511: the castle map (5087C.c VramPosX 0x340);
#   x 769-816 rows 504 and 508: the cross crash / Hellfire / Library card
#     beam lines (84B88.c D_800B0788, ric pl_subweapon_cross.c);
#   x 896 row 384: the debug font's CLUT (FntLoad(0x380, 0x100) at boot);
#   item icons (640,384), loading screen (672,384), HUD (704,256), font
#   (896,256), MP bar / subweapon icon (x 960-990, y 256-303).
# MarVramCheck (mar_specials.h) puts hers back when DRA's CD stream rewrites
# F_GAME2 over them (a save load)
# (the atlas at u 128-255 as before: its rows of four 32-wide images would
# not fit past u_first's empty column, and it must fit mar_buf)
FX_VRAM_X, FX_VRAM_Y, FX_VRAM_W = 992, 384, 32  # the shot / charge atlas
SP_PERM_VRAM = (960, 384)  # the owl and stars (MarInit)
SP_SLOT_VRAM = (800, 384)  # the special being cast (MarSpLoad)
SP_SLOT_ROWS = 504 - 384  # above the crash beam lines
# Byakko's flames past the slot: the rows under the shot atlas (74 rows from
# 384), within the u8 v range
SP_FLAME_VRAM = (992, 458)
SP_PUFF_VRAM = (896, 385)  # the vibhuti puff, always loaded (below the CLUT)


def tileset(x, y):
    """a sprite part's tileset (tpage << 2) for VRAM x, y"""
    return ((x // 64) + (y // 256) * 16) << 2


FX_TILESET = tileset(FX_VRAM_X, FX_VRAM_Y)


def u_first(x):
    """the first texel column an image may start at in a block at VRAM x:
    DRA draws a mirrored part from u - 1 and v - 1 (4B758.c, u8), so it
    reads the column before the image: at u 0 of its tpage that wrapped to
    255 and drew a page-wide smear (her charge fireball, 2026-10-23); at a
    block's first column it is the neighbour block's last. v 0 would wrap
    the same way (asserted against)"""
    return 1
# (bank name, Saturn package, frames)
FX_BANKS = [
    ("mar_fxbank_shot", 7, range(1, 9)),  # Square shot (060B9404)
    ("mar_fxbank_ball", 1, list(range(1, 13)) + [16]),  # charge 1-2
]
# PS1 palettes for the Saturn (package, bank) pairs, uploaded by MarInit:
# Richter's whip palettes (Maria has no whip)
FX_PALS = [(7, 0, 0x140), (1, 0, 0x139), (1, 1, 0x13A), (1, 3, 0x13B)]
# + her holy water flames (src/ric/ric.h MAR_FLAME_PAL): the Saturn draws them
# solid purple (R 96, G 16, B 144-240 in a Saturn screenshot, 2026-10-24);
# PAL_UNK_1B0 (the PS1 flames' blue, read from g_Clut) recoloured: R / G
# fixed, B by each colour's brightness, stp kept. Uploaded by MarFxPals
FLAME_PAL = (0x13F, [0x0000, 0xD84C, 0xC84C, 0xD84C, 0xE44C, 0xF04C, 0xE84C, 0xD84C,
                     0xCC4C, 0xC84C, 0xD04C, 0xD84C, 0xDC4C, 0xE44C, 0xE84C, 0xF44C])
CHARGE_PALS = 0x060C16D0  # 060A5AD4: (level * 5 + 0..4) * 16 colours


def mapcol(c):
    """PS1 column of a kept absolute Saturn column"""
    return c - (c - PHASE + 4) // 5 if PHASE != 4 else c - (c + 1) // 5


def lzss(data):
    """flag byte (LSB first): 1 = literal, 0 = match u16 (off-1) | (len-3)<<12"""
    out = bytearray()
    i = 0
    n = len(data)
    while i < n:
        flag_pos = len(out)
        out.append(0)
        flags = 0
        for bit in range(8):
            if i >= n:
                break
            best_len, best_off = 0, 0
            lo = max(0, i - 4096)
            for j in range(i - 1, lo - 1, -1):
                k = 0
                while k < 18 and i + k < n and data[j + k] == data[i + k]:
                    k += 1
                if k > best_len:
                    best_len, best_off = k, i - j
                    if k == 18:
                        break
            if best_len >= 3:
                out += struct.pack("<H", (best_off - 1) | ((best_len - 3) << 12))
                i += best_len
            else:
                flags |= 1 << bit
                out.append(data[i])
                i += 1
        out[flag_pos] = flags
    return bytes(out)


def unlzss(src, size):
    out = bytearray()
    i = 0
    while len(out) < size:
        flags = src[i]
        i += 1
        for bit in range(8):
            if len(out) >= size:
                break
            if flags & (1 << bit):
                out.append(src[i])
                i += 1
            else:
                v = src[i] | src[i + 1] << 8
                i += 2
                off, ln = (v & 0xFFF) + 1, (v >> 12) + 3
                for _ in range(ln):
                    out.append(out[-off])
    return bytes(out)


def sat_anims(prg):
    """{address: [(duration, pose)...]} including the control entry"""
    o, end = PROF["animation_start"] - SAT, PROF["animation_end"] - SAT
    out = {}
    while o + 4 <= end:
        start = o
        pairs = []
        while o + 4 <= end:
            d, q = struct.unpack_from(">HH", prg, o)
            o += 4
            pairs.append((d, q))
            if d in (0, 0xFFFE, 0xFFFF):
                break
        out[SAT + start] = pairs
    return out


def ps1_anim_blocks():
    """[(type, name, body text)] of src/ric/pl_anims.c in order"""
    src = open(os.path.join(ROOT, "src/ric/pl_anims.c")).read()
    blocks = []
    pos = 0
    import re
    for m in re.finditer(r"^(AnimationFrame\*?|FrameProperty) (\w+)\[\] = \{(.*?)\};",
                         src, re.S | re.M):
        blocks.append((m.group(1), m.group(2), m.group(3)))
    return blocks


def parse_pairs(body):
    """PS1 initializer -> [(duration, pose)] (POSE macros expanded)"""
    import re
    vals = []
    for tok in re.finditer(r"POSE\((\d+),\s*(\d+),\s*(\d+)\)|POSE_LOOP\((\d+)\)|"
                           r"POSE_END|POSE_JUMP\((\d+)\)|(0x[0-9A-Fa-f]+|\d+)", body):
        if tok.group(1):
            d, f, h = int(tok.group(1)), int(tok.group(2)), int(tok.group(3))
            vals += [d, (f & 0x1FF) | ((h & 0x7F) << 9)]
        elif tok.group(4):
            vals += [0, int(tok.group(4))]
        elif tok.group(0) == "POSE_END":
            vals += [0xFFFF, 0]
        elif tok.group(5):
            vals += [0xFFFE, int(tok.group(5))]
        else:
            vals.append(int(tok.group(6), 0))
    return list(zip(vals[0::2], vals[1::2]))


def ps1_color(c, i):
    v = c & 0x7FFF
    if i and v == 0:
        v = 0x8000
    return 0 if i == 0 else v


def fx_packages(prg):
    """{package: (images [(w, h, chr offset)], palettes, frames)}"""
    chr_ = open(os.path.join(ROOT, "disks/saturn/MAR_W.CHR"), "rb").read()

    def u16(a):
        return struct.unpack_from(">H", prg, a - SAT)[0]

    out = {}
    base = 0
    for r in FX_ORDER:
        im, pal, _, _ = struct.unpack_from(">IIHH", prg, FX_RES - SAT + r * 12)
        o = im - SAT
        recs = []
        while True:
            w, h, u = struct.unpack_from(">BBH", prg, o)
            o += 4
            if w == 0 and h == 0:
                size = u * 8
                break
            recs.append((w * 2, h * 2, base + u * 8))
        nb = u16(pal) & 0x3FFF
        pals = [[u16(pal + 2 + (k * 16 + i) * 2) for i in range(16)] for k in range(nb)]
        ftab = pal - FX_NFRAMES[r] * 4
        frames = []
        for f in range(FX_NFRAMES[r]):
            fa = struct.unpack_from(">I", prg, ftab + f * 4 - SAT)[0]
            parts = []
            for p in range((u16(fa) >> 8) & 0x3F):
                parts.append(struct.unpack_from(">HhhH", prg, fa + 4 + p * 8 - SAT))
            frames.append(parts)
        imgs = []
        for w, h, off in recs:
            d = np.frombuffer(chr_[off:off + w * h // 2], np.uint8)
            px = np.empty(w * h, np.uint8)
            px[0::2] = d >> 4
            px[1::2] = d & 15
            imgs.append(px.reshape(h, w))
        out[r] = (imgs, pals, frames)
        base += size
    assert base == len(chr_)
    return out


def fx_frame(imgs, parts):
    """compose a Saturn frame (parts centred at x, y) -> (image, x, y, bank)
    narrowed like the player: absolute columns x % 5 == PHASE dropped"""
    boxes = []
    for attr, x, y, j in parts:
        h, w = imgs[j].shape
        boxes.append((x - w // 2, y - h // 2, w, h))
    x0 = min(bx for bx, _, _, _ in boxes)
    y0 = min(by for _, by, _, _ in boxes)
    x1 = max(bx + w for bx, _, w, _ in boxes)
    y1 = max(by + h for _, by, _, h in boxes)
    out = np.zeros((y1 - y0, x1 - x0), np.uint8)
    banks = set()
    for (attr, x, y, j), (bx, by, w, h) in zip(parts, boxes):
        img = imgs[j]
        if attr & 0x10:
            img = img[:, ::-1]
        if attr & 0x20:
            img = img[::-1]
        dst = out[by - y0:by - y0 + h, bx - x0:bx - x0 + w]
        dst[img != 0] = img[img != 0]
        banks.add(attr >> 8)
    assert len(banks) == 1
    nar = b.narrow(out, x0, PHASE, True)
    c, cx, cy = b.crop(nar)
    kept = [x for x in range(x0, x1) if x % 5 != PHASE]
    return c, mapcol(kept[0]) + cx, y0 + cy, banks.pop()


def gen_fx(prg):
    """mar_fx.h: her attack effect art (one LZSS-packed VRAM block), its
    palettes, the frames (PS1 sprite-part banks) and the charge palettes"""
    pk = fx_packages(prg)
    W = FX_VRAM_W * 4
    atlas = np.zeros((256, W), np.uint8)
    # the atlas needs all 128 columns (rows of four 32-wide images): the
    # column before it is SP_PERM_VRAM's last, which the skyline packer
    # never fills (x + w <= W - 1)
    x0 = 0
    assert FX_VRAM_X & 63 and SP_PERM_VRAM == (FX_VRAM_X - FX_VRAM_W, FX_VRAM_Y)
    assert FX_VRAM_Y & 255, "v 0: see u_first"
    shelf_x, shelf_y, shelf_h = x0, 0, 0
    placed = {}
    L = ["// generated by mods/sotn-ultimate/tools/gen_mar_assets.py; do not edit"]
    banks_out = []
    for name, r, frames in FX_BANKS:
        imgs, pals, sframes = pk[r]
        names = []
        for f in range(max(frames) + 1):
            if f not in frames or not sframes[f]:
                names.append("0")
                continue
            img, x, y, bank = fx_frame(imgs, sframes[f])
            clut = [k for k, (pr, pb, _) in enumerate(FX_PALS) if pr == r and pb == bank]
            assert clut, (r, f, bank)
            h, w = img.shape
            key = img.tobytes() + bytes([w])
            if key not in placed:
                if shelf_x + w > W:
                    shelf_x, shelf_y, shelf_h = x0, shelf_y + shelf_h, 0
                assert (FX_VRAM_Y & 255) + shelf_y + h <= 255  # u8 v
                atlas[shelf_y:shelf_y + h, shelf_x:shelf_x + w] = img
                placed[key] = (shelf_x, shelf_y)
                shelf_x += w
                shelf_h = max(shelf_h, h)
            u, v = placed[key]
            u += (FX_VRAM_X & 63) * 4
            v += FX_VRAM_Y & 255
            # the part's u1 / v1 are u8 on the GPU: 256 wraps to 0 and tears
            # the sprite; drop the last column / row instead
            w = min(w, 255 - u)
            h = min(h, 255 - v)
            # flags, x, y, w, h, clut, tileset, u0, v0, u1, v1
            L.append(f"static s16 {name}_{f}[] = {{1, 0, {x}, {y}, {w}, {h}, "
                     f"{clut[0]}, {FX_TILESET}, {u}, {v}, {u + w}, {v + h}}};")
            names.append(f"{name}_{f}")
        banks_out.append(f"s16* {name}[] = {{" + ", ".join(names) + "};")
    L += banks_out
    H = shelf_y + shelf_h
    px = b.pack_psx(atlas[:H])
    packed = lzss(px)
    assert unlzss(packed, len(px)) == px
    L.append(f"#define MAR_FX_X {FX_VRAM_X}")
    L.append(f"#define MAR_FX_Y {FX_VRAM_Y}")
    L.append(f"#define MAR_FX_W {FX_VRAM_W}")
    L.append(f"#define MAR_FX_H {H}")
    L.append(f"#define MAR_FX_RAW {len(px)}")
    L.append("static u8 mar_fx_lz[] = {" + ",".join(str(v) for v in packed) + "};")
    L.append(f"static u16 mar_fx_pal_ids[] = {{" + ", ".join(
        "0x%X" % pid for _, _, pid in FX_PALS + [(0, 0, FLAME_PAL[0])]) + "};")
    rows = []
    for r, k, _ in FX_PALS:
        rows.append("{" + ", ".join("0x%04X" % ps1_color(c, i)
                                   for i, c in enumerate(pk[r][1][k])) + "}")
    rows.append("{" + ", ".join("0x%04X" % c for c in FLAME_PAL[1]) + "}")
    L.append(f"static u16 mar_fx_pal[{len(rows)}][16] = {{" + ", ".join(rows) + "};")
    # charge flash palettes 5-19 (060A5AD4: 5 * level + 0..4)
    rows = []
    for k in range(5, 20):
        a = CHARGE_PALS + k * 32
        cols = struct.unpack_from(">16H", prg, a - SAT)
        rows.append("{" + ", ".join("0x%04X" % ps1_color(c, i) for i, c in enumerate(cols)) + "}")
    L.append("static u16 mar_charge_pal[15][16] = {" + ", ".join(rows) + "};")
    open(os.path.join(OUT, "mar_fx.h"), "w").write("\n".join(L) + "\n")
    from PIL import Image
    Image.fromarray(atlas[:H] * 17).save(os.path.join(OUT, "mar_fx.png"))
    print(f"MAR fx: {len(placed)} images in {W}x{H} at ({FX_VRAM_X},{FX_VRAM_Y}), "
          f"raw {len(px)} packed {len(packed)}")
    return H


# Her command specials' art (mar_sp.h). Kept as Saturn parts (each image once,
# narrowed on its own: image column c % 5 == 4 dropped; flips as PS1 part
# flags), so it fits. Below the atlas above in the same VRAM column:
# - permanent (loaded by MarInit): the Suzaku owl and the stars;
# - one shared slot for the move being cast (MarSpLoad when it starts):
#   Guardian Knuckle, Genbu, Byakko or the Seiryuu dragon's textures.
# Each block is LZSS-packed in chunks small enough for mar_buf (unpack,
# LoadImage, DrawSync, then the player sprites are decoded again).
# one sprite bank (0x16; a stage has 24): owl frames 1-13 as they are, the
# stars' 13-18 at 14-19; palettes 0x141 (owl) and 0x142 (stars)
SP_PERM = [  # (bank, package, frames, (frame offset, clut offset), palettes)
    ("mar_fxbank_perm", 11, range(1, 14), (0, 0), [0x141]),  # 060BC320 Suzaku
    ("mar_fxbank_perm", 12, range(13, 19), (1, 1), [0x142]),  # 060BB724 stars
    # the Four Beasts countdown (060BCCE8): Saturn shared bank 4's HUD digits
    # 1-9, 0 (images 4-13) with its palette 9, as frames 20-29; palette 0x145
    ("mar_fxbank_perm", "S4", range(1, 11), (19, 4), [0x145]),
]
# the puff each vibhuti throw leaves (MarEntityAsh, Saturn 060B18C8): shared
# bank 9 (the opaque puff, 0.BIN 0x06046DC0), frames 1-14 as 30-43, with
# bank 7's palette 0x1C (060B18C8: CRAM base 06046C02 + 0x1C, a white ramp);
# palette 0x146, in its own block (SP_PUFF_VRAM) loaded with the permanent one
SP_PUFF = ("mar_fxbank_perm", "S9", range(1, 15), (29, 5), [0x146])
SP_GROUPS = [  # loaded into the slot; palettes 0x143 + Saturn bank
    ("mar_fxbank_knuckle", 8, range(1, 16)),  # 060BADA8
    ("mar_fxbank_genbu", 9, range(1, 19)),  # 060BAFF0
    ("mar_fxbank_byakko", 10, range(1, 9)),  # 060BB4AC
    ("mar_fxbank_seiryu", 12, [10, 11]),  # 060BB868 head / body textures
]
SP_PAL_SLOT = 0x143
# Byakko's flames (060BB334): Saturn shared sprite bank 11 (0.BIN
# 0x06047420, frames 1-7) with shared bank 7's palettes 12 (and 5, the silver
# flame; 060BB334 adds them to bank 7's CRAM base 06046C02). The shared banks'
# pixels are the first 86656 bytes of every stage CHR, banks 0-14 in order.
# Too big for the slot alone: packed into the slot block and SP_FLAME_VRAM
# (MarSpLoad(4) at her third Byakko animation)
SP_TRANSPOSE = {(12, 3)}  # the dragon's head, stored on its side (u = y)
SP_CHUNK = 4816  # sizeof(mar_buf)


def sp_narrow(img):
    keep = [c for c in range(img.shape[1]) if c % 5 != 4]
    return img[:, keep]


def shared_bank(i):
    """Saturn shared sprite bank i (src/saturn/zero/gen/stgsprN.h, pixels
    from the shared block of STAGE_01.CHR) -> (images, {frame: parts}, pals)"""
    import re
    src = open(os.path.join(ROOT, f"src/saturn/zero/gen/stgspr{i}.h")).read()
    sizes = []
    for k in range(15):
        t = open(os.path.join(ROOT, f"src/saturn/zero/gen/stgspr{k}.h")).read()
        m = re.findall(r"\{0, 0, (0x[0-9A-F]+)\}", t)
        sizes.append(int(m[-1], 16) * 8 if m else 0)
    base = sum(sizes[:i])
    chr_ = open(os.path.join(ROOT, "disks/saturn/STAGE_01.CHR"), "rb").read()
    m = re.search(r"Images\[\d+\] = \{(.*?)\};", src, re.S)
    recs = [tuple(int(v, 0) for v in t) for t in
            re.findall(r"\{(\d+), (\d+), (0x[0-9A-F]+)\}", m.group(1))]
    imgs = []
    for w, h, o in recs[:-1]:
        W, H = w * 2, h * 2
        d = np.frombuffer(chr_[base + o * 8:base + o * 8 + W * H // 2], np.uint8)
        px = np.empty(W * H, np.uint8)
        px[0::2] = d >> 4
        px[1::2] = d & 15
        imgs.append(px.reshape(H, W))
    frames = {}
    for fm in re.finditer(r"Frame(\d+) = \{\s*\{(0x[0-9A-F]+), (0x[0-9A-F]+)\},"
                          r"\s*\{(.*?)\},\s*\};", src, re.S):
        frames[int(fm.group(1))] = [
            tuple(int(v, 0) for v in q) for q in
            re.findall(r"\{(0x[0-9A-F]+), (-?\d+), (-?\d+), (\d+)\}", fm.group(4))]
    i0 = src.index("Palette = {")
    pals = [[int(v, 16) for v in re.findall(r"0x[0-9A-F]+", b)]
            for b in re.findall(r"\{([^{}]*)\}", src[i0:src.index("};", i0)])]
    return imgs, frames, [q for q in pals if len(q) == 16]


def gen_sp(prg):
    import pickle
    pk = fx_packages(prg)
    d_imgs, _, d_pals = shared_bank(4)
    p_imgs, p_frames, _ = shared_bank(9)
    pk["S9"] = (p_imgs, [shared_bank(7)[2][0x1C]],
                [p_frames.get(f, []) for f in range(15)])
    pk["S4"] = (d_imgs, [d_pals[9]],
                [[]] + [[(0, 0, 0, 3 + k)] for k in range(1, 11)])
    W = FX_VRAM_W * 4
    cache_path = os.path.join(OUT, "sp_cache.pkl")
    try:
        cache = pickle.load(open(cache_path, "rb"))
    except (OSError, EOFError, pickle.UnpicklingError):
        cache = {}
    L = ["// generated by mods/sotn-ultimate/tools/gen_mar_assets.py; do not edit"]
    blocks = []  # (name, y, h, raw bytes)

    def layout(banks, block, merged=None):
        """banks: [(name, pkg, frames, palette base per bank or None)], into
        the VRAM block at block (x, y); with merged, the bank arrays are the
        caller's to write (one bank across blocks)"""
        own = merged is None
        if own:
            merged = {}  # bank name -> {PS1 frame: array name}
        bx, by = block
        ytop = by & 255
        assert ytop, ("v 0: see u_first", block)
        # every image the frames use, narrowed (flips are part flags)
        imgs_used = {}
        for name, r, frames, _ in banks:
            imgs, _, sframes = pk[r]
            for f in frames:
                for attr, x, y, j in sframes[f]:
                    im = sp_narrow(imgs[j])
                    if (r, j) in SP_TRANSPOSE:
                        im = np.ascontiguousarray(im.T)
                    imgs_used[(r, j)] = im
        order = sorted(imgs_used, key=lambda k: (-imgs_used[k].shape[0],
                                                  -imgs_used[k].shape[1]))
        atlas = np.zeros((256, W), np.uint8)
        where = {}
        # skyline packer; u + w must stay <= 255 (u8); row 0 left empty: a
        # part flipped vertically reads the row before it (u_first)
        sky = [1] * (W - 1)
        for k in order:
            im = imgs_used[k]
            h, w = im.shape
            best = None
            for x in range(u_first(bx), W - w):
                top = max(sky[x:x + w])
                if best is None or top < best[0]:
                    best = (top, x)
            top, x = best
            atlas[top:top + h, x:x + w] = im
            where[k] = (x, top)
            for c in range(x, x + w):
                sky[c] = top + h
        H = max(sky)
        print("  ", banks[0][0], "at", block, "rows", H)
        assert ytop + H <= 255, (banks[0][0], ytop, H)  # v + h <= 255
        out = []
        for name, r, frames, (foff, coff) in banks:
            imgs, _, sframes = pk[r]
            slots = merged.setdefault(name, {})
            for f in frames:
                if not sframes[f]:
                    continue
                vals = [len(sframes[f])]
                for attr, x, y, j in sframes[f]:
                    h, w0 = imgs[j].shape
                    h, w = imgs_used[(r, j)].shape
                    left = int(round((x - w0 / 2) * 0.8))
                    top = y - h // 2
                    u, v = where[(r, j)]
                    u += (bx & 63) * 4
                    v += ytop
                    flags = (2 if attr & 0x10 else 0) | (1 if attr & 0x20 else 0)
                    vals += [flags, left, top, w, h, (attr >> 8) + coff,
                             tileset(bx, by), u, v, u + w, v + h]
                out.append(f"static s16 {name}_{f + foff}[] = {{" +
                           ", ".join(str(v) for v in vals) + "};")
                slots[f + foff] = f"{name}_{f + foff}"
        if own:
            out += bank_arrays(merged)
        return out, atlas[:H], H

    def bank_arrays(merged):
        return [f"s16* {name}[] = {{" + ", ".join(
            slots.get(i, "0") for i in range(max(slots) + 1)) + "};"
            for name, slots in merged.items()]

    def chunks(tag, atlas, block):
        bx, by = block
        rows = SP_CHUNK * 2 // W
        res = []
        for k, r0 in enumerate(range(0, atlas.shape[0], rows)):
            part = atlas[r0:r0 + rows]
            px = b.pack_psx(part)
            packed = cache.get(px)
            if packed is None:
                packed = cache[px] = lzss(px)
            assert unlzss(packed, len(px)) == px
            cname = f"mar_sp_lz_{tag}_{k}"
            L.append(f"static u8 {cname}[] = {{" + ",".join(str(v) for v in packed) + "};")
            res.append((cname, bx, by + r0, part.shape[0], len(px), len(packed)))
        return res

    # permanent block
    pm = {}
    out, atlas, H = layout([(n, r, f, o) for n, r, f, o, _ in SP_PERM], SP_PERM_VRAM, pm)
    L += out
    perm = chunks("perm", atlas, SP_PERM_VRAM)
    out, atlas, _ = layout([SP_PUFF[:4]], SP_PUFF_VRAM, pm)
    L += out + bank_arrays(pm)
    perm += chunks("puff", atlas, SP_PUFF_VRAM)
    L.append(f"#define MAR_SP_PERM_X {SP_PERM_VRAM[0]}")
    L.append(f"#define MAR_SP_PERM_Y {SP_PERM_VRAM[1]}")
    L.append(f"#define MAR_SP_PERM_H {H}")
    groups = []
    for g, (name, r, frames) in enumerate(SP_GROUPS):
        out, atlas, Hg = layout([(name, r, frames, (0, 0))], SP_SLOT_VRAM)
        assert Hg <= SP_SLOT_ROWS, (name, Hg)
        L += out
        groups.append(chunks(str(g), atlas, SP_SLOT_VRAM))
    # Byakko's flames: shelf-packed into the slot rows, then SP_FLAME_BINS
    fimgs, fframes, _ = shared_bank(11)
    fpal = shared_bank(7)[2]
    used = {}
    for f in range(1, 8):
        for attr, x, y, j in fframes[f]:
            used[j] = sp_narrow(fimgs[j])
    # (block, rows): v + h <= 255
    bins = [(SP_SLOT_VRAM, SP_SLOT_ROWS), (SP_FLAME_VRAM, 255 - (SP_FLAME_VRAM[1] & 255))]
    atl = [np.zeros((n, W), np.uint8) for _, n in bins]
    where = {}
    shelves = [[] for _ in bins]  # per bin: [y, height, x used]
    for j in sorted(used, key=lambda k: (-used[k].shape[0], -used[k].shape[1])):
        im = used[j]
        h, w = im.shape
        spot = None
        for bi in range(len(bins)):
            for sh in shelves[bi]:
                if sh[1] >= h and sh[2] + w <= W - 1:
                    spot = (bi, sh)
                    break
            if spot:
                break
            top = 1 + sum(q[1] for q in shelves[bi])  # row 0 empty (u_first)
            if top + h <= bins[bi][1]:
                shelves[bi].append([top, h, u_first(bins[bi][0][0])])
                spot = (bi, shelves[bi][-1])
                break
        assert spot, ("flames do not fit", j, w, h)
        bi, sh = spot
        atl[bi][sh[0]:sh[0] + h, sh[2]:sh[2] + w] = im
        where[j] = (bi, sh[2], sh[0])
        sh[2] += w
    names = ["0"]
    for f in range(1, 8):
        vals = [len(fframes[f])]
        for attr, x, y, j in fframes[f]:
            h, w0 = fimgs[j].shape
            w = used[j].shape[1]
            bi, u, v = where[j]
            fx, fy = bins[bi][0]
            u += (fx & 63) * 4
            v += fy & 255
            flags = (2 if attr & 0x10 else 0) | (1 if attr & 0x20 else 0)
            vals += [flags, int(round((x - w0 / 2) * 0.8)), y - h // 2, w, h, 0,
                     tileset(fx, fy), u, v, u + w, v + h]
        L.append(f"static s16 mar_fxbank_flame_{f}[] = {{" + ", ".join(map(str, vals)) + "};")
        names.append(f"mar_fxbank_flame_{f}")
    L.append("s16* mar_fxbank_flame[] = {" + ", ".join(names) + "};")
    fl = []
    for k, (blk, n) in enumerate(bins):
        rows_used = max([where[j][2] + used[j].shape[0] for j in where if where[j][0] == k] + [0])
        if rows_used:
            fl += chunks(f"flame{k}", atl[k][:rows_used], blk)
    groups.append(fl)
    print("   flames:", [(bins[k][0], max([where[j][2] + used[j].shape[0] for j in where if where[j][0] == k] + [0])) for k in range(len(bins))])
    # chunk table: {lz, x, y (VRAM), h(rows), raw}
    allc = perm + [c for g in groups for c in g]
    L.append("typedef struct { u8* lz; u16 x; u16 y; u16 h; u16 raw; } MarSpChunk;")
    L.append("static MarSpChunk mar_sp_chunks[] = {" + ", ".join(
        f"{{{c[0]}, {c[1]}, {c[2]}, {c[3]}, {c[4]}}}" for c in allc) + "};")
    first = [0, len(perm)]
    for g in groups:
        first.append(first[-1] + len(g))
    L.append(f"#define MAR_SP_GROUPS {len(groups)}")
    # chunks of block k (0 = permanent, 1.. = groups): first[k]..first[k+1]
    L.append("static u8 mar_sp_first[] = {" + ", ".join(str(v) for v in first) + "};")
    # palettes: permanent ones, then each group's Saturn banks
    rows = []
    ids = []
    for name, r, frames, _, pals in SP_PERM + [SP_PUFF]:
        for k, pid in enumerate(pals):
            ids.append(pid)
            rows.append(pk[r][1][k])
    L.append("static u16 mar_sp_perm_pal_ids[] = {" + ", ".join("0x%X" % i for i in ids) + "};")
    L.append(f"static u16 mar_sp_perm_pal[{len(rows)}][16] = {{" + ", ".join(
        "{" + ", ".join("0x%04X" % ps1_color(c, i) for i, c in enumerate(p)) + "}"
        for p in rows) + "};")
    gp = []
    for name, r, frames in SP_GROUPS:
        p = pk[r][1]
        p = (p + p)[:2]
        gp.append("{" + ", ".join("{" + ", ".join("0x%04X" % ps1_color(c, i)
                                                  for i, c in enumerate(q)) + "}"
                                  for q in p) + "}")
    L.append(f"#define MAR_SP_PAL_SLOT 0x{SP_PAL_SLOT:X}")
    gp.append("{" + ", ".join("{" + ", ".join("0x%04X" % ps1_color(c, i)
                                              for i, c in enumerate(q)) + "}"
                              for q in (fpal[12], fpal[5])) + "}")
    L.append(f"static u16 mar_sp_group_pal[{len(gp)}][2][16] = {{" + ", ".join(gp) + "};")
    open(os.path.join(OUT, "mar_sp.h"), "w").write("\n".join(L) + "\n")
    pickle.dump(cache, open(cache_path, "wb"))
    tot = sum(c[5] for c in allc)
    print(f"MAR sp: permanent at {SP_PERM_VRAM}, slot at {SP_SLOT_VRAM}, flames "
          f"also at {SP_FLAME_VRAM}; chunks {[(c[1], c[2], c[3], c[5]) for c in allc]} "
          f"packed total {tot}")


def main():
    os.makedirs(OUT, exist_ok=True)
    s = b.Saturn(PROF)
    prg = s.prg
    frames = s.frames()
    nimg = len(s.recs)
    # placement of each image: the first frame showing it
    first = {}
    for f, fr in enumerate(frames):
        if fr and fr[3] not in first:
            first[fr[3]] = fr
    import pickle
    cache_path = os.path.join(OUT, "lz_cache.pkl")
    try:
        cache = pickle.load(open(cache_path, "rb"))
    except (OSError, EOFError, pickle.UnpicklingError):
        cache = {}
    sprites = []  # (header+packed bytes, raw size, x0, y0, left_ps1, top)
    raw_total = packed_total = 0
    maxpix = 0
    for j in range(nimg):
        img = s.image(j)
        h, w = img.shape
        fr = first.get(j)
        left = (fr[1] - w // 2) if fr else 0
        nar = b.narrow(img, left, PHASE, True)
        c, x0, y0 = b.crop(nar)
        ch, cw = c.shape
        kept = [x for x in range(left, left + w) if x % 5 != PHASE]
        left_ps1 = mapcol(kept[0]) if kept else 0
        px = b.pack_psx(c)
        hdr = bytes([cw, ch, 0, 0])
        packed = cache.get(px)
        if packed is None:  # the packer is slow; keep what was packed before
            packed = cache[px] = lzss(px)
        assert unlzss(packed, len(px)) == px
        sprites.append((hdr + packed, len(px), x0, y0, left_ps1))
        raw_total += 4 + len(px)
        packed_total += 4 + len(packed)
        maxpix = max(maxpix, len(px))
        assert cw <= 252 and ch <= 255
    pickle.dump(cache, open(cache_path, "wb"))
    # frames: PS1 x = narrowed left of the image + crop, y = top + crop
    fr_out = []
    for f, fr in enumerate(frames):
        if not fr:
            fr_out.append(None)
            continue
        fl, x, y, j = fr
        h, w = s.image(j).shape
        left = x - w // 2
        kept = [c for c in range(left, left + w) if c % 5 != PHASE]
        _, _, x0, y0, _ = sprites[j]
        fr_out.append((j, mapcol(kept[0]) + x0, y - h // 2 + y0 + SAT_Y, 0, fl))
    # hitboxes: 4 signed bytes x, y, w, h up to the palette table
    hb = []
    o = HITBOX_TABLE - SAT
    while o + 4 <= PROF["palette_table"] - SAT:
        x, y, w, h = struct.unpack_from(">bbbb", prg, o)
        hb.append((int(round(x * 0.8)), y, int(round(w * 0.8)), h))
        o += 4
    # animations
    anims = sat_anims(prg)
    stand_pose = anims[STAND][0][1]
    lines = ["// generated by mods/sotn-ultimate/tools/gen_mar_assets.py; do not edit"]
    # her animation group (POSE_JUMP targets): the Saturn's 14-entry table
    # (060C28A0) under the PS1 names; D_8015538C is emitted at the end
    groups = struct.unpack_from(">14I", prg, ANIM_GROUPS - SAT)
    rev = {}
    for name, addr in list(ANIM_MAP.items()) + list(EXTRA_ANIMS.items()):
        rev.setdefault(addr, name)
    extra = dict(EXTRA_ANIMS)
    for addr in groups:
        if addr not in rev:
            rev[addr] = f"mar_anim_grp_{addr:X}"
            extra[rev[addr]] = addr
    for typ, name, body in ps1_anim_blocks():
        if typ == "AnimationFrame*":
            continue  # D_8015538C, below
        if typ == "FrameProperty":
            vals = ", ".join("%d, %d, %d, %d" % e for e in hb)
            lines.append(f"FrameProperty {name}[] = {{{vals}}};")
            continue
        if name in ANIM_MAP:
            pairs = anims[ANIM_MAP[name]]
        else:
            # Richter's timing; his jumps (0, 1: his stand) go to her idle
            pairs = [(d, 2 if d == 0xFFFE else
                      (q if d in (0, 0xFFFF) else stand_pose))
                     for d, q in parse_pairs(body)]
        vals = ", ".join("0x%04X, 0x%04X" % p for p in pairs)
        lines.append(f"AnimationFrame {name}[] = {{{vals}}};")
    # her animations that have no Richter counterpart (new names)
    for name, addr in extra.items():
        pairs = anims[addr]
        if name in JUMP_ENDS:
            pairs = [(0xFFFF, 0) if d == 0xFFFE else (d, q) for d, q in pairs]
        vals = ", ".join("0x%04X, 0x%04X" % p for p in pairs)
        lines.append(f"AnimationFrame {name}[] = {{{vals}}};")
    lines.append("AnimationFrame* D_8015538C[] = {" +
                 ", ".join(rev[a] for a in groups) + "};")
    open(os.path.join(OUT, "mar_anims.h"), "w").write("\n".join(lines) + "\n")
    gen_fx(prg)
    gen_sp(prg)
    # sprites
    L = ["// generated by mods/sotn-ultimate/tools/gen_mar_assets.py; do not edit",
         f"#define MAR_SPRITE_COUNT {nimg}",
         f"#define MAR_SPRITE_MAXPIX {maxpix}"]
    # DRA's RenderEntities reads the sheet at the fixed SPRITESHEET_PTR
    # (0x8013C020, right after the PlayerOvl header): richter_sprites must be
    # the first data defined, the images after it (as gen/richter.h does)
    for j, (blob, raw, *_r) in enumerate(sprites):
        L.append(f"static u8 mar_lz_{j}[{len(blob)}];")
    L.append("u8* richter_sprites[] = {" + ", ".join(f"mar_lz_{j}" for j in range(nimg)) + "};")
    L.append("u8* mar_lz[] = {" + ", ".join(f"mar_lz_{j}" for j in range(nimg)) + "};")
    L.append("u16 mar_lz_size[] = {" + ", ".join(str(sp[1]) for sp in sprites) + "};")
    for j, (blob, raw, *_r) in enumerate(sprites):
        L.append(f"static u8 mar_lz_{j}[] = {{" + ",".join(str(v) for v in blob) + "};")
    open(os.path.join(OUT, "mar_sprites.h"), "w").write("\n".join(L) + "\n")
    # frames
    F = ["// generated by mods/sotn-ultimate/tools/gen_mar_assets.py; do not edit"]
    names = []
    for f, fr in enumerate(fr_out):
        if not fr:
            names.append("0")
            continue
        j, x, y, c, _ = fr
        F.append(f"static s16 spritebank0_{f}[] = {{0x{j:X} | 0x8000, {x}, {y}, {c}}};")
        names.append(f"spritebank0_{f}")
    F.append("s16* spritebank0[] = {" + ", ".join(names) + "};")
    open(os.path.join(OUT, "mar_spritebank0.h"), "w").write("\n".join(F) + "\n")
    # palette bank 0 -> PAL_RICHTER (PS1 colour = Saturn's without the MSB;
    # black stays opaque through STP); bank 1 is her lighter copy, which the
    # Saturn shows on the frames whose flag byte is 1 (the release of a shot,
    # some special poses): MarCharge swaps it in for them (mar_frame_light)
    def ps1_pal(bank):
        out = []
        for i, c in enumerate(s.palette(bank)):
            v = c & 0x7FFF
            if i and v == 0:
                v = 0x8000
            out.append(0 if i == 0 else v)
        return out

    light = bytearray((len(fr_out) + 7) // 8)
    for f, fr in enumerate(fr_out):
        if fr and fr[4] == 1:
            light[f >> 3] |= 1 << (f & 7)
    open(os.path.join(OUT, "mar_pal.h"), "w").write(
        "// generated by mods/sotn-ultimate/tools/gen_mar_assets.py; do not edit\n"
        f"#define MAR_SPRITE_MAXPIX {maxpix}\n"
        f"#define MAR_SPRITE_COUNT {nimg}\n"
        "u16 mar_pal[16] = {" + ", ".join("0x%04X" % v for v in ps1_pal(0)) + "};\n"
        "u16 mar_pal_light[16] = {" + ", ".join("0x%04X" % v for v in ps1_pal(1)) + "};\n"
        f"#define MAR_FRAME_COUNT {len(fr_out)}\n"
        "u8 mar_frame_light[] = {" + ", ".join(str(v) for v in light) + "};\n")
    flags = sorted({fr[4] for fr in fr_out if fr})
    print(f"MAR assets: {nimg} images raw {raw_total} packed {packed_total}, "
          f"{len(fr_out)} frames (flags {flags}), {len(hb)} hitboxes, max {maxpix}")


if __name__ == "__main__":
    main()
