#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: build the Cursed Prison's stage assets (stage PRS).
#
# The Prison is the Saturn's STAGE_16: a five-floor cell-block tower (room 0,
# (33,34)-(35,36)) and a corridor (room 1, (28,36)-(32,36)), between the
# Underground Caverns (NO4 room 5, by the tower's top right) and the Marble
# Gallery (NO0 room 0, by the corridor's left end). Same pipeline as the
# Garden (mods/sotn-ultimate/tools/gdn_assets.py, whose generic helpers this uses): the
# Saturn map converter (tools/saturn/assets `map convert-psx`, STAGE_16
# profile) narrows the art to 256 px screens and quantizes it to PSX tiles;
# its tiles fit the stage's F_ file whole (about 280 graphics, no merging).
# Writes assets/st/prs (WRP's entity assets plus the rooms), the mods/sotn-ultimate/src/prison/prs
# headers it includes, and build/us/F_TE2.BIN: the Prison takes the unused TE2
# slot on the disc (mods/sotn-ultimate/tools/build_prs.sh). Stage id MOD_STAGE_PRS.
#
# Rooms:
#   0 (33,34)-(35,36) tower       1 (28,36)-(32,36) corridor
#   2 (27,36) loading room, to the Marble Gallery (exit (26,36))
#   3 (36,34) loading room, to the Underground Caverns (exit (37,34))
#   4 (32,34) save room (door on its right, into the tower's top floor)
# The loading rooms and the save room use the shared tilesets (see the top of
# gdn_assets.py); the save room is the standard right-door one (NP3's).
import json
import os
import re
import shutil
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gdn_assets as ga  # noqa: E402
import gdn_tile_swap  # noqa: E402
from mod_work import work_dir  # noqa: E402

ROOT = ga.ROOT
SAT = ga.SAT
OUT = os.path.join(ROOT, "assets/st/prs")
SRC = os.path.join(ROOT, "mods/sotn-ultimate/src/prison/prs")
WORK = work_dir("prs")
PRG = os.path.join(SAT, "STAGE_16.PRG")
MAP = os.path.join(SAT, "STAGE_16.MAP")
CHR = os.path.join(SAT, "STAGE_16.CHR")
STAGE_BASE = 0x060DC000
# the asset config this extends, the one it writes for the asset tool, and
# the stage's F_ file (mods/sotn-ultimate/tools/rprs_assets.py, the Soul Prison, reuses this
# module with its own values)
CONFIG = os.path.join(ROOT, "mods/sotn-ultimate/config/mod_prs_assets.us.yaml")
BUILD_CONFIG = os.path.join(ROOT, "build/us/prs_assets.us.yaml")
F_FILE = os.path.join(ROOT, "build/us/F_TE2.BIN")
# the converted Saturn sprites (sprites()); off, the stage keeps WRP's
WITH_SPRITES = True

# teleport indices, keep in sync with MOD_TP_* in include/game.h
TP_NO4_TO_PRS = 0x87
TP_PRS_TO_NO4 = 0x88
TP_NO0_TO_PRS = 0x89
TP_PRS_TO_NO0 = 0x8A

# room -> (tilemap, tiledef, source stage, cell, teleport for loading rooms)
SHARED_ROOMS = {
    2: ("wrp_tilemap_5", "wrp_tiledef_load", "wrp", (27, 36), TP_PRS_TO_NO0),
    3: ("wrp_tilemap_5", "wrp_tiledef_load", "wrp", (36, 34), TP_PRS_TO_NO4),
    4: ("np3_tilemap_38", "np3_tiledef_save", "np3", (32, 34), None),
}
# exits: transition cells past the loading rooms
EXITS = [((26, 36), TP_PRS_TO_NO0), ((37, 34), TP_PRS_TO_NO4)]
# the loading rooms' and the save room's entities (the inverted castle uses
# other pieces)
LOADING_FOREGROUND = ga.LOADING_FOREGROUND
SAVE_FOREGROUND = ga.SAVE_FOREGROUND
# both rooms' foreground is behind the characters (Saturn priorities
# {7,4,2,3, sprites 4}: ties go to the sprites; gdn_assets.fg_priority)
FG_PRIORITY = 0x60

# (room, x, y, slot, opens to the right) on the PSX grid (the Saturn layout
# lists 1 and 2, x 4/5); the door's params are its row in g_RedDoorTiles
# (| 0x100 when it opens to the right)
RED_DOORS = [(0, 760, 97, 0x9D, True), (1, 8, 97, 0x9E, False)]


# Sprites (see gdn_assets.py for the conversion): the Saturn's stage objects
# and enemies, converted, and the PS1's own enemies, packed into the stage
# sprite pages (tpages 18-19: 8 quadrants of 128x128), one graphics bank per
# room. The engine loads a room's bank as it enters (its entityGfxId) and
# reloads it after the pause menu, which clears those pages.
# tpages 20-23 (VRAM x 256-511, y 256-511) are free in the Prison: no
# backdrop or extra tile pages as in the Garden (the pause menu clears and
# reloads them with the room's bank too)
SPRITE_QUADRANTS = [(t, q) for t in (18, 19, 20, 21, 22, 23) for q in range(4)]
# room -> graphics bank (gfxBanks index; 5 is WRP's stage title, kept)
ROOM_BANKS = {0: 1, 1: 2}
# converted sprite groups (each packed alone into whole quadrants) and the
# rooms using them
SPRITE_GROUPS = {"objects": (0, 1), "gargoyles": (1,), "specters": (1,), "wisps": (1,),
                 "switch": (0,)}
# bank 16: the stage objects (STAGE_16 0x060E8F60, frames 0x060E8F08)
SAT_BANK16 = (0x060E8F60, 0x060E8F08)
# The candles (Saturn entity 0x01, STAGE_16 0x060DC040; configs 0x060EA164:
# {bank, frames, glow offsets, animation, hitbox height, .., hitboxOffY}),
# type = params >> 12: 0 the short wall sconce (frames 6-9, room 0), 1 the
# tall candelabra (frames 1-4, room 1), 2 the item holder (frame 0xC, no
# flame). A broken sconce or candelabra leaves its stand (Saturn entity 0x12:
# frame 10 / 5). PSX ANIMSET_OVL(2) frames, in order:
CANDLE_FRAMES = [1, 2, 3, 4,      # 1-4 tall candelabra
                 6, 7, 8, 9,      # 5-8 short sconce
                 0x0C,            # 9 item holder
                 0x0A, 0x05]      # 10 the sconce's stand, 11 the candelabra's
# the flames' glow (as the Garden's, gdn_assets.CANDLE_GLOW): per PSX flame
# frame 1-8 above, a record {dx, dy, image} (tall 0x060EA13C, short
# 0x060EA114)
CANDLE_GLOW = [(0x060EA13C, f) for f in range(4)] + [(0x060EA114, f) for f in range(4)]
# the glow image's flip (saturn_sprites image_part: 3 = turned around, as the
# inverted castle's candles draw it)
GLOW_FLIP = 0
# Saturn entity 0x11, the still decorations (chained skeletons, hanged
# prisoners): 28-byte variants at 0x060EA1CC, as the Garden's
DECORATIONS = (0x060EA1CC, 6)
# The Red Gargoyle (Saturn entities 0x21-0x23, the Garden's 0x23-0x25: the
# same code, mods/sotn-ultimate/src/enemies/e_mod_gargoyle.h; bank 24 0x060ECEF0, frames
# 0x060EE030): its 59 frames as ANIMSET_OVL(7), its palettes kept whole
# (variants by palette offset)
GARGOYLE = (0x060ECEF0, 0x060EE030, 59)
# The Saturn-only enemies of room 1, whole banks (PSX frame n = Saturn frame
# n, palettes kept in order): the Specter (Saturn 0x20, bank 23) as
# ANIMSET_OVL(10), the Will-o'-Wisps (Saturn 0x28-0x2C, bank 26) as
# ANIMSET_OVL(11)
SPECTER = (0x060ECB58, 0x060ECE10, 29)
# the floor switch and sinking block (Saturn 0x16/0x17, bank 19 0x060E900C,
# frames 0x060E9000: 1 the block, 2 the plate) as ANIMSET_OVL(12)
SWITCH = (0x060E900C, 0x060E9000, 2)
WISP = (0x060EEC7C, 0x060EEFBC, 41)
# More Saturn-only enemies' whole banks (PSX frame n = Saturn frame n,
# palettes as PALETTE_BLOCKS): (animset, group, (bank, frames, count), dy),
# dy = the Saturn's sprite offset from its entity, baked into the frames
ENEMY_SETS = []
# frames an enemy's code draws as a polygon (one part each): name ->
# (animset, frame); mod_psx_sprites.h gets MOD_<NAME>_TEX_U/V/W/H/TPAGE/CLUT
TEX_FRAMES = {}
PALETTE_BLOCKS = {"GARGOYLE": ("gargoyles", GARGOYLE[0]), "SPECTER": ("specters", SPECTER[0]),
                  "WISP": ("wisps", WISP[0])}
# Enemies that exist on the PS1 come with their PS1 code, frames and art, as
# in the Garden (gdn_assets.PSX_SPRITES): each takes its stage's 128x128 art
# blocks whole (consecutive quadrants: the frames' tileset counts from the
# entity's unk5A) and its palettes. (name, stage, animset there, art blocks,
# palette assets, rooms, the Prison's animset); mod_psx_sprites.h gets
# MOD_<NAME>_ANIMSET, _UNK5A and _PAL.
PSX_SPRITES = [
    ("SLINGER", "no0", 10, ["e_slinger"], ["e_slinger_pal"], (0,), 6),
    ("BONE_ARCHER", "no4", 8, ["D_80189ECC"], ["D_8018E094"], (0,), 8),
    ("SPEAR_GUARD", "no4", 2, ["D_80185994", "D_80186260"], ["D_8018DA54"], (0,), 9),
]
SPRITE_CLUT_BASE = 0x210  # after WRP's palette at 0x200 (palette_def)
# extra palettes after all the others: None, or f(palettes so far, sheets)
# -> [(name, [palettes])], each written as MOD_<name>_PAL (its first clut)
EXTRA_PALS = None
# the rooms with a first-visit area name popup (its image goes in their
# banks); () for a stage without one
STAGE_NAME_ROOMS = (0, 1)


def allocate(items):
    """[(key, quadrants, rooms)] -> {key: first index in SPRITE_QUADRANTS}:
    runs of consecutive quadrants (consecutive tilesets), never shared
    within a room"""
    used = {r: set() for r in ROOM_BANKS}
    out = {}
    for item in sorted(items, key=lambda it: (-len(it[2]), -it[1])):
        key, n, rooms = item[:3]
        # a 4th value: the first quadrant's place in its tpage, the run within
        # that tpage (enemies whose primitives use their art's tpage)
        quad = item[3] if len(item) > 3 else None
        for i in range(len(SPRITE_QUADRANTS) - n + 1):
            run = set(range(i, i + n))
            if quad is not None and (SPRITE_QUADRANTS[i][1] != quad or
                                     SPRITE_QUADRANTS[i + n - 1][0] != SPRITE_QUADRANTS[i][0]):
                continue
            if not any(run & used[r] for r in rooms):
                break
        else:
            raise AssertionError(("sprite quadrants full", key, n, rooms))
        out[key] = i
        for r in rooms:
            used[r] |= run
    return out


def psx_art(stage, art, pal):
    """a PS1 enemy's art blocks (4bpp, packed) and palettes (RGB555)"""
    from PIL import Image
    pixels = []
    for name in art:
        im = Image.open(os.path.join(ROOT, "assets/st", stage, name + ".png"))
        assert im.mode == "P" and im.size == (128, 128), (name, im.mode, im.size)
        px = list(im.getdata())
        pixels.append(bytes((px[i] & 15) | (px[i + 1] & 15) << 4 for i in range(0, len(px), 2)))
    if isinstance(pal, tuple):
        # (first clut, count) of the stage's retail overlay
        return pixels, ga.psx_palettes(stage, pal)
    palettes = []
    for name in pal:
        meta = open(os.path.join(ROOT, "assets/st", stage, name + ".yaml")).read()
        for png in re.findall(r"^- (.*\.png)$", meta, re.M):
            cols = Image.open(os.path.join(ROOT, "assets/st", stage, png)).convert("RGBA")
            # the asset tool writes RGB555 colours with alpha 127 for the STP bit
            palettes.append([round(r * 31 / 255) | round(g * 31 / 255) << 5 |
                             round(b * 31 / 255) << 10 | (0x8000 if a == 127 else 0)
                             for r, g, b, a in cols.getdata()])
    return pixels, palettes


def sprites():
    """Writes the sprite banks, graphics banks, palettes and their headers;
    returns the asset config's cmpgfx lines"""
    st = ga.SaturnStage(PRG, CHR, os.path.join(SAT, "0.BIN"))
    bank, frames = SAT_BANK16
    decos = []
    for v in range(DECORATIONS[1]):
        o = DECORATIONS[0] - st.base + v * 28
        dbank, dframes, pal, z, order = struct.unpack_from(">IIHHB", st.prg, o)
        anim = struct.unpack_from(">I", st.prg, o + 24)[0]
        assert (dbank, dframes) == SAT_BANK16, hex(o)
        decos.append((st.prg[anim - st.base + 1], z))
    glow_recs = [struct.unpack_from(">hhh", st.prg, rec - st.base + f * 10)
                 for rec, f in CANDLE_GLOW]

    def fill(sheets):
        for g, pbank in PALETTE_BLOCKS.values():
            sheets[g].palette_block(st, pbank)
        obj = sheets["objects"]
        sets = [(2, [obj.frame(st, bank, frames, f) for f in CANDLE_FRAMES]),
                (3, [obj.frame(st, bank, frames, f) for f, _ in decos])]
        # the enemies' sets, for the groups the stage has (SPRITE_GROUPS)
        for n, g, (ebank, eframes, count) in ((7, "gargoyles", GARGOYLE),
                                              (10, "specters", SPECTER),
                                              (11, "wisps", WISP), (12, "switch", SWITCH)):
            if g in sheets:
                sets.append((n, [sheets[g].frame(st, ebank, eframes, f)
                                 for f in range(1, count + 1)]))
        for n, g, (ebank, eframes, count), dy in ENEMY_SETS:
            sets.append((n, [[dict(p, y=p["y"] + dy) for p in sheets[g].frame(st, ebank, eframes, f)]
                             for f in range(1, count + 1)]))
        glow = [obj.image_part(st, bank, img, dx, dy, ga.GLOW_COLOURS, GLOW_FLIP)
                for dx, dy, img in glow_recs]
        return sets, glow

    # pass 1 lists each group's images, pass 2 measures them packed tallest
    # first, pass 3 packs them into the quadrants they were given
    seen = {g: ga.PsxSpriteSheet([(0, 0)] * 16, SPRITE_CLUT_BASE) for g in SPRITE_GROUPS}
    fill(seen)
    sizes = {}
    for g in SPRITE_GROUPS:
        probe = ga.PsxSpriteSheet(SPRITE_QUADRANTS * 2, SPRITE_CLUT_BASE)
        probe.prepack(st, seen[g].requests)
        sizes[g] = probe.used_quadrants()
    while True:
        first = allocate([(g, sizes[g], rooms) for g, rooms in SPRITE_GROUPS.items()] +
                         [(p[0], len(p[3]), p[5]) + tuple(p[7:8]) for p in PSX_SPRITES])
        cluts = []
        sheets = {g: ga.PsxSpriteSheet(SPRITE_QUADRANTS[first[g]:first[g] + sizes[g]],
                                       SPRITE_CLUT_BASE, cluts)
                  for g in SPRITE_GROUPS}
        short = None
        for g in SPRITE_GROUPS:
            try:
                sheets[g].prepack(st, seen[g].requests)
            except AssertionError:
                short = g
                break
        if short is None:
            break
        sizes[short] += 1
    sets, glow = fill(sheets)

    path = os.path.join(OUT, "us/sprite_banks.json")
    sb = json.load(open(path))
    for animset, frame_parts in sets:
        sb["banks"].append([None] + frame_parts)
        sb["indices"][animset] = len(sb["banks"]) - 1

    # art blocks: (quadrant, array name, 4bpp pixels, rooms)
    blocks = []
    for g, rooms in SPRITE_GROUPS.items():
        for k in range(sheets[g].used_quadrants()):
            blocks.append((sheets[g].quadrants[k], "g_ModPrs_%s%d" % (g, k),
                           sheets[g].quadrant_pixels(k), rooms))
    psx_pals, psx_defs = [], []
    clut = SPRITE_CLUT_BASE + len(cluts)
    for name, stage, animset, art, pal, rooms, prs_animset, *quad_req in PSX_SPRITES:
        src_path = os.path.join(ROOT, "assets/st", stage, "sprite_banks.json")
        if not os.path.exists(src_path):
            src_path = os.path.join(ROOT, "assets/st", stage, "us/sprite_banks.json")
        src = json.load(open(src_path))
        sb["banks"].append(src["banks"][src["indices"][animset]])
        sb["indices"][prs_animset] = len(sb["banks"]) - 1
        pixels, palettes = psx_art(stage, art, pal)
        for k, px in enumerate(pixels):
            blocks.append((SPRITE_QUADRANTS[first[name] + k],
                           "g_ModPrs%sGfx%d" % (name.title().replace("_", ""), k), px, rooms))
        tpage, quad = SPRITE_QUADRANTS[first[name]]
        psx_defs.append((name, prs_animset, tpage << 2 | quad, clut + len(psx_pals),
                         tpage if quad_req else None))
        psx_pals += palettes
    json.dump(sb, open(path, "w"), indent=2)
    # the area name for the first-visit popups (both rooms have one; tpage 17,
    # as the retail stages')
    if STAGE_NAME_ROOMS:
        blocks.append(((17, 0), "g_ModPrsStageName", stage_name_pixels(), STAGE_NAME_ROOMS))

    path = os.path.join(OUT, "graphics_banks.json")
    gb = json.load(open(path))
    for room, index in ROOM_BANKS.items():
        entries = []
        for (tpage, quad), name, _, rooms in blocks:
            if room in rooms:
                vx = (tpage & 15) * 64 + (quad & 1) * 32
                vy = (tpage >> 4) * 256 + (quad >> 1) * 128
                # GFX_ENTRY takes VRAM y first, then x
                entries.append({"x": vy, "y": vx, "width": 128, "height": 128, "name": name})
        gb["banks"].append({"kind": 4, "entries": entries})
        gb["indices"][index] = len(gb["banks"]) - 1
    json.dump(gb, open(path, "w"), indent=2)
    lines = []
    for k, (_, name, data, _) in enumerate(blocks):
        ga.write_indexed_png(os.path.join(OUT, name + ".png"), 128, 128,
                             [v for b in data for v in (b & 15, b >> 4)])
        lines.append("          - [0x%X, cmpgfx, %s, 128, 128, 4]\n" % (0x6D90 + k * 4, name))

    all_pals = [list(c) for c in cluts] + psx_pals
    extra_defs = []
    for name, pals in (EXTRA_PALS(all_pals, sheets) if EXTRA_PALS else []):
        extra_defs.append((name, SPRITE_CLUT_BASE + len(all_pals)))
        all_pals += [list(c) for c in pals]
    path = os.path.join(OUT, "palette_def.json")
    pd = json.load(open(path))
    pd["defs"][0]["entries"].append({"destination": SPRITE_CLUT_BASE * 16,
                                     "length": 16 * len(all_pals),
                                     "name": "g_ModPrsSpritePals.palbin"})
    json.dump(pd, open(path, "w"), indent=2)

    gen = "// Generated by mods/sotn-ultimate/tools/prs_assets.py, do not edit\n// clang-format off\n"
    with open(os.path.join(SRC, "mod_sprite_gfx.h"), "w") as f:
        f.write(gen + "// the Prison's sprites, 4bpp 128x128 quadrants, compressed by the\n"
                "// asset tool from assets/st/prs/<name>.png\n")
        for _, name, _, _ in blocks:
            f.write("u8 %s[] __attribute__((aligned(4))) = {\n#include \"gen/%s.h\"\n};\n"
                    % (name, name))
        f.write("// clang-format on\n")
    with open(os.path.join(SRC, "mod_sprite_pals.h"), "w") as f:
        f.write(gen + "// the Prison's sprite palettes, g_ClutIds 0x%X on\n" % SPRITE_CLUT_BASE)
        f.write("u16 g_ModPrsSpritePals[] = {\n")
        for pal in all_pals:
            f.write("    " + ", ".join("0x%04X" % c for c in pal) + ",\n")
        f.write("};\n// clang-format on\n")
    with open(os.path.join(SRC, "mod_psx_sprites.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/prs_assets.py, do not edit\n")
        f.write("// the Saturn enemies' palette blocks (PALETTE_BLOCKS)\n")
        for name, (g, pbank) in PALETTE_BLOCKS.items():
            f.write("#define MOD_%s_PAL 0x%03X\n" % (name, sheets[g].blocks[pbank]))
        f.write("// the PS1 enemies' sprites in the Prison (PSX_SPRITES)\n")
        for name, animset, unk5a, pal, tpage in psx_defs:
            f.write("#define MOD_%s_ANIMSET ANIMSET_OVL(%d)\n" % (name, animset))
            f.write("#define MOD_%s_UNK5A 0x%02X\n" % (name, unk5a))
            f.write("#define MOD_%s_PAL 0x%03X\n" % (name, pal))
            if tpage is not None:
                f.write("#define MOD_%s_TPAGE 0x%02X\n" % (name, tpage))
        if extra_defs:
            f.write("// extra palettes (EXTRA_PALS)\n")
        for name, clut in extra_defs:
            f.write("#define MOD_%s_PAL 0x%03X\n" % (name, clut))
        if TEX_FRAMES:
            f.write("// frames drawn as polygons (TEX_FRAMES)\n")
        for name, (animset, frame) in TEX_FRAMES.items():
            parts = dict(sets)[animset][frame - 1]
            assert len(parts) == 1, (name, frame, len(parts))
            p = parts[0]
            quad = p["tileset"] & 3
            for k, v in (("U", p["left"] + (quad & 1) * 128), ("V", p["top"] + (quad >> 1) * 128),
                         ("W", p["width"]), ("H", p["height"]), ("TPAGE", p["tileset"] >> 2),
                         ("CLUT", p["clut"])):
                f.write("#define MOD_%s_TEX_%s 0x%X\n" % (name, k, v))
    # the Specter's texture per frame (mods/sotn-ultimate/src/enemies/e_mod_specter.h draws its
    # one-part frames as a polygon): u, v, w, h, tpage; row n = frame n
    # (stages without the Specter: none)
    specter = dict(sets).get(10, [])
    path = os.path.join(SRC, "mod_specter_tex.h") if specter else os.devnull
    with open(path, "w") as f:
        f.write(gen + "// the Specter's frames (ANIMSET_OVL(10)): u, v, w, h, tpage\n")
        f.write("static const s16 g_ModSpecterTex[][5] = {\n    {0, 0, 0, 0, 0},\n")
        for parts in specter:
            if not parts:
                f.write("    {0, 0, 0, 0, 0},\n")
                continue
            assert len(parts) == 1, "a Specter frame with several parts"
            p = parts[0]
            quad = p["tileset"] & 3
            f.write("    {%d, %d, %d, %d, %d},\n" % (
                p["left"] + (quad & 1) * 128, p["top"] + (quad >> 1) * 128,
                p["width"], p["height"], p["tileset"] >> 2))
        f.write("};\n// clang-format on\n")
    with open(os.path.join(SRC, "mod_candle_glow.h"), "w") as f:
        f.write(gen + "// per candle flame frame (ANIMSET_OVL(2) 1-8): x, y, w, h, u, v, clut, tpage\n")
        f.write("static const s16 g_ModCandleGlow[][8] = {\n")
        for p in glow:
            quad = p["tileset"] & 3
            f.write("    {%d, %d, %d, %d, %d, %d, 0x%X, %d},\n" % (
                p["x"], p["y"], p["width"], p["height"], p["left"] + (quad & 1) * 128,
                p["top"] + (quad >> 1) * 128, p["clut"], p["tileset"] >> 2))
        f.write("};\n// clang-format on\n")
    with open(os.path.join(SRC, "mod_bg_blocks.h"), "w") as f:
        f.write(gen + "// the Saturn's decorations (entity 0x11): ANIMSET_OVL(3) frame n + 1\n")
        for v in range(len(decos)):
            f.write("static u8 g_ModBgBlockAnim%d[] = {0x40, %d, 0xFF, 0x00};\n" % (v, v + 1))
        f.write("ObjInit2 BackgroundBlockInit[] = {\n")
        for v, (_, z) in enumerate(decos):
            f.write("    {.animSet = ANIMSET_OVL(3), .zPriority = 0x%02X, .unk5A = 0, .palette = 0,\n"
                    "     .drawFlags = ENTITY_DEFAULT, .blendMode = BLEND_NO, .flags = 0,\n"
                    "     .animFrames = g_ModBgBlockAnim%d},\n" % (z, v))
        f.write("};\n// clang-format on\n")
    for room in ROOM_BANKS:
        q = sorted({SPRITE_QUADRANTS.index(blk[0]) for blk in blocks
                    if room in blk[3] and blk[0] in SPRITE_QUADRANTS})
        print(f"prs: room {room} sprites use {len(q)} of {len(SPRITE_QUADRANTS)} quadrants")
    print(f"prs: {sum(len(sh.placed) for sh in sheets.values())} converted sprite images, "
          f"{len(all_pals)} palettes")
    return lines


# The first-visit area name, "Cursed Prison", in the US names' lettering (the
# Saturn's name images are Japanese): glyphs of other areas' names (see
# gdn_assets.stage_name_pixels), laid out left to right and centred. No US
# name has a capital P: it is the R of "Royal Chapel" without its leg.
NAME_SOURCES = {"no4": "assets/st/no4/D_80185400.png",        # Underground Caverns
                "are": "assets/st/are/gfx_stage_name_en.png",  # Colosseum
                "cat": "assets/st/cat/gfx_stage_name_en.png",  # Catacombs
                "chi": "assets/st/chi/bn_gfx_stage_name_en.png",  # Abandoned Mine
                "dai": "assets/st/dai/gfx_stage_name_en.png"}  # Royal Chapel
# (source, the glyph's left x in its strip, pixels to drop (x, y) -> bool,
# gap after it); None = a word space
NAME_GLYPHS = [
    ("cat", 55, None, 1),   # C
    ("no4", 102, None, 1),  # u
    ("no4", 59, None, 0),   # r
    ("are", 107, None, 2),  # s
    ("no4", 49, None, 1),   # e
    ("no4", 37, None, 1),   # d
    None,
    ("dai", 35, lambda x, y: x >= 46 and y >= 19, 1),  # P (R without the leg)
    ("no4", 59, None, 0),   # r
    ("chi", 181, None, 1),  # i
    ("are", 107, None, 2),  # s
    ("no4", 91, None, 2),   # o
    ("no4", 115, None, 0),  # n
]
NAME_SPACE = 8


def stage_name_pixels():
    """the 128x128 4bpp name image, as packed bytes (low nibble first)"""
    strips = {k: ga.name_strip(p) for k, p in NAME_SOURCES.items()}
    masks = {k: ga.glyph_masks(s) for k, s in strips.items()}
    placed, cursor = [], 0
    for g in NAME_GLYPHS:
        if g is None:
            cursor += NAME_SPACE
            continue
        src, sx, drop, gap = g
        pts = [(x, y) for x, y in masks[src][sx] if not (drop and drop(x, y))]
        right = max(x for x, y in pts if strips[src][y][x] > 1)
        placed.append((src, pts, cursor - sx))
        cursor += right - sx + 1 + gap
    shift = (232 - cursor) // 2
    new = [[0] * 232 for _ in range(40)]
    for src, pts, dx in placed:
        for x, y in pts:
            v, tx = strips[src][y][x], x + dx + shift
            if 0 <= tx < 232 and v > new[y][tx]:
                new[y][tx] = v
    img = [[0] * 128 for _ in range(128)]
    for y in range(40):
        for x in range(116):
            img[y][4 + x] = new[y][x]
            img[64 + y][8 + x] = new[y][116 + x]
    return bytes(img[y][x] | img[y][x + 1] << 4 for y in range(128) for x in range(0, 128, 2))


def converter():
    if os.path.exists(WORK):
        shutil.rmtree(WORK)
    os.makedirs(WORK)
    subprocess.run([ga.TOOL, "map", "convert-psx", PRG, MAP, WORK],
                   check=True, stdout=subprocess.DEVNULL,
                   env=dict(os.environ, SOTN_MOD_UNCAPPED="1"))


# the Saturn entities placed as they are (Saturn id -> PSX enum name)
SATURN_IDS = {0x01: "E_BREAKABLE", 0x09: "E_STAGE_NAME_POPUP",
              0x11: "E_ID_11", 0x18: "E_BONE_ARCHER",
              0x1A: "E_SPEAR_GUARD", 0x1D: "E_SLINGER",
              0x21: "E_MOD_GARGOYLE", 0x16: "E_MOD_PRS_SWITCH",
              0x17: "E_MOD_PRS_BLOCK", 0x20: "E_MOD_SPECTER",
              0x28: "E_MOD_WISP_SPAWNER"}


def saturn_entities(room, ids):
    """the stage's (PRG's) layout records for room (list room + 1) whose id is in ids
    (Saturn id -> PSX enum name), x scaled 4/5"""
    prg = open(PRG, "rb").read()
    lists = struct.unpack_from(">I", prg, 8)[0] - STAGE_BASE
    p = struct.unpack_from(">I", prg, lists + 4 * (room + 1))[0] - STAGE_BASE
    out = []
    while True:
        x, y, eid, slot, params = struct.unpack_from(">HHHHH", prg, p)
        p += 10
        if x == 0xFFFF:
            break
        if x != 0xFFFE and eid & 0x1FFF in ids:
            out.append({"x": x * 4 // 5, "y": y, "id": ids[eid & 0x1FFF],
                        "flags": (eid & 0xE000) >> 8, "slot": slot & 0xFF,
                        "spawnId": slot >> 8, "params": params})
    return out


def trim_shared_tiledefs(layers):
    """keep only the shared tilesets' entries their tilemaps use"""
    used = {}
    for entry in layers:
        for lay in entry.values():
            if lay["tiledef"] == "prs_tiledef_prs.json":
                continue
            d = open(os.path.join(OUT, lay["data"]), "rb").read()
            top = max(d[i] | d[i + 1] << 8 for i in range(0, len(d), 2))
            used[lay["tiledef"]] = max(used.get(lay["tiledef"], 0), top + 1)
    for tdef, n in used.items():
        for f in json.load(open(os.path.join(OUT, tdef))).values():
            path = os.path.join(OUT, f)
            data = open(path, "rb").read()  # (read before "wb" truncates it)
            open(path, "wb").write(data[:n])


# the PS1-drawn stair / ceiling slope tiles (gdn_tile_swap.py, include/game.h
# MOD_PRS_PS1_TILES): the data set in mods/sotn-ultimate/tools/prs_ps1_tiles ("" the Prison,
# rprs_assets.py sets "_rprs")
PS1_TILES = ""


def main():
    converter()
    swap = gdn_tile_swap.apply(WORK, data=gdn_tile_swap.PRS_DATA, switch=gdn_tile_swap.PRS_SWITCH,
                               own=PS1_TILES, empty=True)
    if swap:
        print("ps1 tiles: %(cells)d cells, %(tiles)d tiles, %(graphics)d graphics, "
              "%(palettes_new)d new palettes, %(palettes_reused)d on existing ones, "
              "%(nearest)d on the nearest one (max colour error %(max_err)d)" % swap)
    conv_rooms = json.load(open(os.path.join(WORK, "rooms.roomdef.json")))[:2]
    conv_layers = json.load(open(os.path.join(WORK, "rooms.layers.json")))[:2]
    assert [r["tileLayoutId"] for r in conv_rooms] == [0, 1], conv_rooms

    if os.path.exists(OUT):
        shutil.rmtree(OUT)
    # WRP's entity assets (sprite banks, palettes, stage title) for now
    shutil.copytree(os.path.join(ROOT, "assets/st/wrp"), OUT,
                    ignore=shutil.ignore_patterns("wrp_tile*", "layers.json", "rooms.json",
                                                  "entity_layouts.json"))

    # tile definitions (the asset tool's "tiles" fills gfxPage and "pages"
    # gfxIndex, see gdn_assets.py), placed by the Garden's repacker
    td, n_tiles = {}, None
    for src, dst in (("gfx_page", "tiles"), ("gfx_index", "pages"), ("clut", "cluts"),
                     ("collision", "cols")):
        data = open(os.path.join(WORK, f"tiledefs.{src}.bin"), "rb").read()
        assert len(data) <= 0x1000 and n_tiles in (None, len(data))
        n_tiles = len(data)
        td[dst] = bytearray(data + bytes(0x1000 - len(data)))
    ga.WORK, ga.SHARED_ROOMS = WORK, {}
    fte, vram4 = ga.repack(td, n_tiles, conv_layers)
    assert not any(vram4), "the Prison's tiles must fit its F_ file"
    json.dump({"tiles": "prs_tiledef_prs_tiles.bin", "pages": "prs_tiledef_prs_pages.bin",
               "cluts": "prs_tiledef_prs_cluts.bin", "collisions": "prs_tiledef_prs_cols.bin"},
              open(os.path.join(OUT, "prs_tiledef_prs.json"), "w"), indent=2)

    shared = {}
    for room, (tmap, tdef, st, cell, tp) in SHARED_ROOMS.items():
        kind = tdef.rsplit("_", 1)[1]
        new_tdef, new_tmap = f"prs_tiledef_{kind}", f"prs_tilemap_{kind}"
        for part in ("tiles", "pages", "cluts", "cols"):
            shutil.copy(os.path.join(ROOT, f"assets/st/{st}/{tdef}_{part}.bin"),
                        os.path.join(OUT, f"{new_tdef}_{part}.bin"))
        json.dump({"tiles": f"{new_tdef}_tiles.bin", "pages": f"{new_tdef}_pages.bin",
                   "cluts": f"{new_tdef}_cluts.bin", "collisions": f"{new_tdef}_cols.bin"},
                  open(os.path.join(OUT, new_tdef + ".json"), "w"), indent=2)
        shutil.copy(os.path.join(ROOT, f"assets/st/{st}/{tmap}.bin"),
                    os.path.join(OUT, new_tmap + ".bin"))
        shared[room] = (new_tmap + ".bin", new_tdef + ".json")

    layers, tilemaps = [], []
    for i, cl in enumerate(conv_layers):
        entry = {}
        for part in ("fg", "bg"):
            c = cl[part]
            name = f"prs_tilemap_{len(tilemaps)}.bin"
            shutil.copy(os.path.join(WORK, c["data"] + ".tilelayout.bin"), os.path.join(OUT, name))
            tilemaps.append(name)
            entry[part] = {
                "data": name, "tiledef": "prs_tiledef_prs.json",
                "left": c["left"], "top": c["top"], "right": c["right"], "bottom": c["bottom"],
                "scrollMode": 1, "hideOnMap": False, "isSaveRoom": False,
                "isLoadingRoom": False, "unusedFlag": False,
                "zPriority": FG_PRIORITY if part == "fg" else 32, "flags": 3,
            }
        layers.append(entry)
    for room, (tmap, tdef, st, (x, y), tp) in SHARED_ROOMS.items():
        data, tdef = shared[room]
        loading = tp is not None
        # a loading room's flags byte is 0x40 with its exit cell on the left,
        # 0x41 on the right (every retail loading room): scrollMode 0 / 1
        exit_right = any(ex == (x + 1, y) for ex, _ in EXITS)
        layers.append({"fg": {
            "data": data, "tiledef": tdef, "left": x, "top": y, "right": x, "bottom": y,
            # as the retail loading rooms and save rooms
            "scrollMode": (1 if exit_right else 0) if loading else 2,
            "hideOnMap": False, "isSaveRoom": not loading, "isLoadingRoom": loading,
            "unusedFlag": False, "zPriority": tp if loading else 96, "flags": 515}})
        if data not in tilemaps:
            tilemaps.append(data)
    json.dump(layers, open(os.path.join(OUT, "layers.json"), "w"), indent=2)
    trim_shared_tiledefs(layers)

    # red doors: closed = the doorway's own tiles made solid, open = as they are
    door_rows, solid = [], {}
    for room, x, y, slot, right in RED_DOORS:
        fgl = layers[room]["fg"]
        d = open(os.path.join(OUT, fgl["data"]), "rb").read()
        tm = [d[i] | d[i + 1] << 8 for i in range(0, len(d), 2)]
        w = (fgl["right"] - fgl["left"] + 1) * 16
        col, row = (x + (-4 if right else 4)) >> 4, (y + 31 - 24) >> 4
        opened = [tm[(row + i) * w + col] for i in range(4)]
        closed = []
        for t in opened:
            if td["cols"][t] != 3 and t not in solid:
                assert n_tiles < 0x1000, "tileset full"
                for k in ("tiles", "pages", "cluts"):
                    td[k][n_tiles] = td[k][t]
                td["cols"][n_tiles] = 3
                solid[t] = n_tiles
                n_tiles += 1
            closed.append(t if td["cols"][t] == 3 else solid[t])
        door_rows.append(closed + opened)
    with open(os.path.join(SRC, "mod_red_door_tiles.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/prs_assets.py, do not edit\n")
        f.write("// [params][0..3] closed (solid), [4..7] open: the doorway's own tiles\n")
        f.write("u16 g_RedDoorTiles[][8] = {\n")
        for r in door_rows:
            f.write("    {" + ", ".join("0x%03X" % v for v in r) + "},\n")
        f.write("};\n")
    # the sinking block's wall (mods/sotn-ultimate/src/prison/prs/e_mod_switch.h): tile 0's (blank)
    # art, solid
    assert n_tiles < 0x1000, "tileset full"
    for k in ("tiles", "pages", "cluts"):
        td[k][n_tiles] = td[k][0]
    td["cols"][n_tiles] = 3
    with open(os.path.join(SRC, "mod_block_tile.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/prs_assets.py, do not edit\n")
        f.write("// the sinking block's wall: blank art, solid\n")
        f.write("#define MOD_BLOCK_SOLID_TILE 0x%03X\n" % n_tiles)
    n_tiles += 1
    for k, data in td.items():
        open(os.path.join(OUT, f"prs_tiledef_prs_{k}.bin"), "wb").write(data[:n_tiles])

    rooms = []
    for i, r in enumerate(conv_rooms):
        rooms.append({"left": r["left"], "top": r["top"], "right": r["right"],
                      "bottom": r["bottom"], "layerId": i, "tileDefId": 0,
                      "entityGfxId": ROOM_BANKS[i] + 1, "entityLayoutId": i + 1})
    for room, (_, _, _, (x, y), _) in SHARED_ROOMS.items():
        rooms.append({"left": x, "top": y, "right": x, "bottom": y, "layerId": room,
                      "tileDefId": 0, "entityGfxId": 0, "entityLayoutId": room + 1})
    for (x, y), tp in EXITS:
        # the asset tool keeps layerId as a signed byte (see gdn_assets.py)
        rooms.append({"left": x, "top": y, "right": x, "bottom": y,
                      "layerId": tp - 256 if tp > 127 else tp,
                      "tileDefId": -1, "entityGfxId": 0, "entityLayoutId": 0})
    json.dump(rooms, open(os.path.join(OUT, "rooms.json"), "w"), indent=2)

    def layout(ents):
        out = [{"x": -2, "y": -2, "id": "E_NONE", "flags": 0, "slot": 0, "spawnId": 0,
                "params": 0, "yOrder": 0}]
        for n, e in enumerate(ents):
            out.append(dict(e, yOrder=n + 1))
        out.append({"x": -1, "y": -1, "id": "E_NONE", "flags": 0, "slot": 0, "spawnId": 0,
                    "params": 0, "yOrder": len(ents) + 1})
        return out
    lists = [layout([])]  # layout 0; room i uses list i + 1
    for i in range(5):
        ents = [{"x": x, "y": y, "id": "E_RED_DOOR", "flags": 0, "slot": slot, "spawnId": 0,
                 "params": n | (0x100 if right else 0)}
                for n, (room, x, y, slot, right) in enumerate(RED_DOORS) if room == i]
        if i in (2, 3):
            ents.append(dict(LOADING_FOREGROUND))
        if i == 4:
            ents += [dict(e) for e in SAVE_FOREGROUND]
        if i in (0, 1):
            # the Saturn's objects, as they are but for x (4/5): candles
            # (params: type << 12 | prize; type 2 releases PrizeDrops[params &
            # 0x1FF]), the area name popups, the decorations (params = variant)
            # and the enemies (the PS1's Bone Archer, Spear Guard and Slinger,
            # the Saturn's Red Gargoyle: params 1, its first colours)
            ents += saturn_entities(i, SATURN_IDS)
        lists.append(layout(ents))
    json.dump({"entities": lists, "indices": list(range(len(lists)))},
              open(os.path.join(OUT, "entity_layouts.json"), "w"), indent=2)

    with open(os.path.join(SRC, "mod_tilemaps.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/prs_assets.py, do not edit\n")
        for t in tilemaps:
            f.write("#include <gen/%s.h>\n" % t[:-4])
        for t in ["prs_tiledef_prs"] + sorted({v[1][:-5] for v in shared.values()}):
            f.write("#include <gen/%s.h>\n" % t)
    # the asset config for the asset tool: mods/sotn-ultimate/config/mod_prs_assets.us.yaml plus
    # one cmpgfx entry per sprite block (as gdn_assets.py)
    conf = open(CONFIG).read().rstrip("\n") + "\n"
    assert conf.endswith("          - [0x6D88, skip] # sprites\n"), "must end with the sprites skip"
    if WITH_SPRITES:
        conf += "".join(sprites())
    os.makedirs(os.path.join(ROOT, "build/us"), exist_ok=True)
    open(BUILD_CONFIG, "w").write(conf)
    open(F_FILE, "wb").write(fte)
    print(f"prs: {len(rooms)} rooms, {len(layers)} layers, {len(tilemaps)} tilemaps, "
          f"{n_tiles} tiles")


if __name__ == "__main__":
    # The Specter's and the Wisps' banks are 16-colour bank mode (palette
    # header 0x4000) and their colour codes carry sprite priority 5 (the
    # Specter's primitive ORs 0x5000, the Wisp's sprites set flags & 7 = 5):
    # with the Saturn's VDP2 setup (sprite type 5, S5PRIN = 6, colour
    # calculation when priority == 6, CCMD 1 = add as is) they are drawn
    # added to the background. The PS1 blends only STP texels: STP colours.
    # (Here and not at import: rprs_assets.py reuses this module.)
    ga.saturn_sprites.STP_BANKS.update((SPECTER[0], WISP[0]))
    main()
