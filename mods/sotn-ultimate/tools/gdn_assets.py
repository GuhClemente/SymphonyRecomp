#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: build the Underground Garden's stage assets (stage GDN).
#
# Runs the Saturn map converter (tools/saturn/assets: `map convert-psx`, which
# narrows the Saturn's 320-px-wide art back to 256 and quantizes it to PSX
# tiles) on STAGE_15, then turns its output into the decomp's stage asset
# format in assets/st/gdn (same layout as assets/st/wrp, which mods/sotn-ultimate/src/garden/gdn is
# built from). Also writes build/us/F_TE1.BIN, the stage's tile graphics:
# the Garden reuses the unused TE1 slot on the disc (mods/sotn-ultimate/tools/build_gdn.sh).
#
# Rooms (Saturn STAGE_15 room table; map cells, PSX-sized):
#   0 (4,44)-(12,45)  1 (13,44)-(16,44)  3 (17,44)-(20,44)  2 (21,44)
#   4 (3,44) loading room for the Castle Entrance shaft exit (2,44)
#   5 (3,45) loading room for the warp room exit (2,45)
#   6 (13,45) save room
# The loading rooms and the save room do not use converted art: as in every
# PSX stage, they use the shared loading/save room tilesets (byte-identical in
# all stages: VRAM page 23 and the global palettes 163-199/252-255), whose
# graphics stay put while a loading room preloads the next stage into the F_
# texture area. With the Garden's own art, the preload overwrote it while the
# room was on screen. Tiles from WRP (loading corridor) and NP3 (the save room
# with a left door only, like the Saturn's (13,45)).
# Tile texture: the Saturn Garden has far more distinct tiles (about 2900)
# than F_STAGE15.BIN holds (1984). Left alone, the converter approximates every
# background tile past its budget by the existing tile that matches best on
# every 4th pixel, which put railings in mid-air and lost the bushes, flowers
# and the red curtains under the bridge. So the converter runs uncapped
# (SOTN_MOD_UNCAPPED, tools/saturn/assets) and repack() places its tiles
# itself: F_TE1's 8 pages, plus 3 more texture pages in VRAM the game leaves
# unused during play (x 320-511, y 256: tile pages 13-15, uploaded with the red
# mist by the stage's update, mods/sotn-ultimate/src/garden/gdn/st_update.c). Whatever still doesn't
# fit is merged, cheapest first, among near-identical background tiles only
# (full-pixel comparison); foreground tiles are always exact. Palettes avoid
# the slots the shared loading/save room tiles use (163-199) and the mist's.
# Exits are transition cells (tileDefId -1) whose layerId is a teleport index
# (D_800A245C in src/dra/5087C.c, MOD_TP_* in include/game.h).
#
# Red doors: the Saturn's room 0 (layout list 1) has two, on the left edge at
# (10,97) and (10,353): x scales by 4/5 like the tiles. EntityRedDoor
# (src/st/e_red_door.h) moves itself to (x+4, y+31) and writes 4 tiles in that
# column from y-24 down: g_RedDoorTiles[params][0..3] while closed (solid) and
# [4..7] once open. The Garden's tileset is converted, so WRP's door tile ids
# mean nothing here: each door gets its own row (params = row) whose open
# tiles are the room's own doorway tiles and whose closed tiles are the same
# art made solid, as new tileset entries after the converter's.
#
# The module's globals below are the Garden's; mods/sotn-ultimate/tools/rgdn_assets.py sets
# them to the Hell Garden's (the inverted castle's Garden, Saturn RSTAGE15)
# and runs main() for it.
import json
import re
import os
import shutil
import struct
import subprocess
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import saturn_sprites
from saturn_sprites import SaturnStage, PsxSpriteSheet  # noqa: E402
import gdn_tile_swap  # noqa: E402
from mod_work import work_dir  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
SAT = os.path.join(ROOT, "disks/saturn")
OUT = os.path.join(ROOT, "assets/st/gdn")
WORK = work_dir("gdn")
TOOL_TARGET = work_dir("saturn-assets-target")
TOOL = os.path.join(TOOL_TARGET, "release/saturn-assets")
# the stage's sources (generated headers), Saturn files and outputs
SRC = os.path.join(ROOT, "mods/sotn-ultimate/src/garden/gdn")
PRG = os.path.join(SAT, "STAGE_15.PRG")
MAP = os.path.join(SAT, "STAGE_15.MAP")
CHR = os.path.join(SAT, "STAGE_15.CHR")
STAGE_BASE = 0x060DC000
CONFIG = os.path.join(ROOT, "mods/sotn-ultimate/config/mod_assets.us.yaml")
BUILD_CONFIG = os.path.join(ROOT, "build/us/gdn_assets.us.yaml")
F_FILE = os.path.join(ROOT, "build/us/F_TE1.BIN")
# the sprites (candles, decorations, enemies; sprites()); without them the
# tile pages 12-15 go alone in graphics bank PAGE_BANK (pages_only())
WITH_SPRITES = True
# the PS1 Entrance art for the stairs and stonework (gdn_tile_swap.py,
# include/game.h MOD_GDN_PS1_TILES); the Hell Garden turns the cells around
TILE_SWAP_FLIP = False

# teleport indices, keep in sync with MOD_TP_* in include/game.h
TP_GDN_TO_NP3 = 0x84
TP_GDN_TO_WRP = 0x85
# rooms 0-3 are the converter's; the special rooms 4-6 (4, 5 loading, 6 save)
# are the converter's STAGE_15 extras (None) or these cells
ORDINARY_ROOMS = 4
SPECIAL_CELLS = None
# a loading room's zPriority is the teleport it preloads for; the exits are
# transition cells next to the loading rooms
LOADING_TPS = {4: TP_GDN_TO_NP3, 5: TP_GDN_TO_WRP}
EXITS = [((2, 44), TP_GDN_TO_NP3), ((2, 45), TP_GDN_TO_WRP)]

# The red mist behind the Garden: the Saturn draws it as a third map layer
# (STAGE_15 room 0 layer 1, 256x256, repeating), which stays put on screen.
# The converter couldn't fit it in the tile budget (its shared_backdrop
# layout is one flat tile), so the mod draws the converter's full-colour
# render of it (shared_backdrop.preview.png) as two screen-sized sprites, set
# up by the stage's update (mods/sotn-ultimate/src/garden/gdn/st_update.c): 16 colours, one 4bpp
# texture page in VRAM the game leaves unused (x 256, y 256: tpage 20), its
# palette in a free palette slot of F_TE1. Not as a tile layer: the renderer
# draws at most MAXSPRT16 (640) tiles a frame and the room's own two layers
# can take about 544, so the extra layer cost the foreground tiles.
BACKDROP_BACK_COLOR = 0x1406  # STAGE15_BACK_COLOR in the converter
BACKDROP_TPAGE = 20           # VRAM (256, 256) = tile page 12 (tpage 8 + page)
BACKDROP_PAGE = 12        # tile page p is VRAM tpage 8 + p: tpage 20 = (256, 256)
BACKDROP_VRAM = (256, 256)

# The Garden's sprites, converted from the Saturn's (mods/sotn-ultimate/tools/saturn_sprites.py)
# into the stage sprite pages (tpages 18-19, VRAM x 128-255, y 256-511: 8
# quadrants of 128x128). Each Garden room loads only the sprites it uses (its
# graphics bank, by its entityGfxId), so the quadrants serve one room at a
# time. Room 0 holds more than that: its two ends (the gargoyles and Slingers
# to the west; the water, statues and Human Face Tree to the east) take turns,
# swapped as the camera crosses the empty middle (ModGdnZones in
# mods/sotn-ultimate/src/garden/gdn/st_update.c). Context -> the stage's graphics bank (gfxBanks
# index; 5 is WRP's stage title, kept):
CONTEXT_BANKS = {"0W": 8, "1": 1, "2": 2, "3": 3, "0E": 6}
# The pause menu clears VRAM x 128-511, y 256-511 (src/dra/menu.c ClearImage
# of g_Vram.D_800ACDC8): the sprites and tile pages 12-15 (PAGE_BLOCKS). On
# its way out, still black, it reloads the bank D_80097904 names and waits for
# it. So each context also has a bank with the pages added, which the stage
# puts in D_80097904 (ModGdnReloadBank, mods/sotn-ultimate/src/garden/gdn/st_update.c); the stage
# starts with gfxBanks[0], room 0's west sprites and the pages. Entering a
# room or crossing room 0's middle loads the sprites alone, without the
# pages' decompression (a stutter).
RELOAD_BANKS = {"0W": 0, "1": 9, "2": 10, "3": 11, "0E": 12}
ROOM_CONTEXT = {0: "0W", 1: "1", 2: "2", 3: "3"}
# room 0's camera centre x: the east's sprites from ZONE_EAST on, the west's
# again below ZONE_WEST (room 0's west enemies end at x ~1110, the water
# starts at ~1290)
ZONE_WEST, ZONE_EAST = 1450, 1500
# sprite groups, each packed alone into whole quadrants, and their contexts
SPRITE_GROUPS = {
    # the candles share their quadrant with room 0's two statues (both small;
    # room 0's east end needs every quadrant)
    "candles": ("0W", "0E", "1", "3"),
    "scene": ("2",),
    "drips": ("1", "3"),
    "gargoyles": ("0W", "0E"),
    "skbreeder": ("0W",),
    "leaper": ("0W", "0E"),
    "gardener": ("1",),
    "tree": ("0E",),
    "maneater": ("3",),
    "boss": ("2",),
}
# banks whose palettes are kept whole, in order, for colour variants chosen by
# the entity's palette (group, Saturn bank); mod_psx_sprites.h gets
# MOD_<NAME>_PAL, the first of them
PALETTE_BLOCKS = {
    # the Red Gargoyle (bank 24): 27 palettes, 9 per variant (+0, +9, +18)
    "GARGOYLE": ("gargoyles", 0x060F2ED8),
    # the Human Face Tree (bank 26): 6 palettes; it withers through +2, +3
    "TREE": ("tree", 0x060F6DC4),
}
# Each set is one PSX sprite bank, ANIMSET_OVL(n): (n, [(group, Saturn bank,
# frame table, frame)]); the PSX frames are numbered from 1 in that order.
SAT_BANK16 = (0x060F0858, 0x060F079C)
# the frames the Human Face Tree's code uses (SPRITE_SETS)
TREE_FRAMES = (set(range(1, 21)) | {23} | set(range(26, 34)) | {35, 47} |
               set(range(54, 67)) | set(range(68, 74)) | {87, 88})
SPRITE_SETS = [
    # the candles, ram-head wall sconces (bank 16): 4 flame frames, then the
    # empty sconce left when one breaks (Saturn entity 0x12)
    (2, [("candles",) + SAT_BANK16 + (f,) for f in (0x26, 0x27, 0x28, 0x29, 0x2A)]),
    # the decorations (Saturn entity 0x11), one frame per variant (below)
    (3, "decorations"),
    # the poison drip (Saturn entity 0x1F, bank 16): 3 falling frames, then
    # the 5 splash frames
    (4, [("drips",) + SAT_BANK16 + (f,) for f in (1, 2, 3, 0x13, 0x14, 0x15, 0x16, 0x17)]),
    # the Red Gargoyle (Saturn entities 0x23-0x25, bank 24): its 59 frames, at
    # their positions as they are (the Saturn's sprite offset for it,
    # 0x060F2ED4 = (0, 16), drew it 16 px too low on its pedestal)
    (7, [("gargoyles", 0x060F2ED8, 0x060F4018, f) for f in range(1, 60)]),
    # the Skeleton Breeder and its Skeleton Beast (Saturn 0x40-0x43, bank
    # 0x060FA7B8): 37 frames. Unlike the gargoyle's, their frames have the
    # feet at y = 0 and the Saturn draws them below the entity (the floor
    # sensors agree): the breeder (13-31) 32 px, the beast (1-12) 16 px; the
    # death pieces (32-37) as they are
    (8, [("skbreeder", 0x060FA7B8, 0x060FAF64, f,
          32 if 13 <= f <= 31 else 16 if f <= 12 else 0) for f in range(1, 38)]),
    # the Water Leaper (Saturn 0x34, bank 27): 25 frames, as they are (they
    # already reach 16 px below the entity, the Saturn's sprite offset)
    (9, [("leaper", 0x060F82C8, 0x060F86CC, f) for f in range(1, 26)]),
    # the Gardener (Saturn 0x3D-0x3F, bank 30): 45 frames, as they are (its
    # feet already sit at the floor sensors, +24); 45 is the thrown shears
    (10, [("gardener", 0x060F96D8, 0x060FA59C, f) for f in range(1, 46)]),
    # the Human Face Tree (Saturn 0x2E-0x33, bank 26): only the 52 frames its
    # code uses (the others are left empty, PSX frame n = Saturn frame n), as
    # they are; the leaves (54-66) with palette 3 - 3 (the Saturn draws the
    # falling ones with clutBase - 3, which the PSX can't take)
    (11, [("tree", 0x060F6DC4, 0x060F7EFC, f, 0, -3 if 54 <= f <= 66 else 0)
          if f in TREE_FRAMES else None for f in range(1, 89)]),
    # the Man Eater (Saturn 0x3B-0x3C, bank 29): frames 1-55 (56-58 unused),
    # as they are (its feet at y = 0, on the walk sensor)
    (12, [("maneater", 0x060F8BFC, 0x060F9470, f) for f in range(1, 56)]),
    # the Skeleton Leader (Saturn 0x26-0x2D, bank 25): frames 1-86, as they
    # are (feet at +32, on its floor sensor)
    (13, [("boss", 0x060F43C4, 0x060F6A90, f) for f in range(1, 87)]),
    # its room's door pillar (Saturn 0x45): bank 16 frame 46
    (14, [("boss",) + SAT_BANK16 + (46,)]),
]
# Saturn entity 0x11 (STAGE_15 0x060DC4B8) is a still decoration: params picks
# a 28-byte variant at 0x060F1F90 {bank, frame table, palette, z, order, ..,
# animation {0x10, frame}}. Room 0 has the two vine-covered statues, room 2 the
# garden scene (trees, hedge, gazebo). PSX: E_ID_11 (EntityBackgroundBlock,
# BackgroundBlockInit[params], mods/sotn-ultimate/src/garden/gdn/mod_bg_blocks.h). The variants at z
# 0x20 are the room-2 scene, in front of that room's dark columns (its
# foreground, 0x60-0x67, behind the characters) and stacked by the order byte
# (hedge 1, gazebo 2, trees 3); z 0x60 (the statues) is kept.
DECORATIONS = (0x060F1F90, 6)
DECORATION_GROUPS = ["candles", "candles", "scene", "scene", "scene", "scene"]
SPRITE_QUADRANTS = [(t, q) for t in (18, 19) for q in range(4)] + [(20, 1), (20, 2), (20, 3)]
# Enemies that exist on the PS1 come with their PS1 code, frames and art (as
# the Saturn reuses them). Each takes its stage's 128x128 art block whole and
# its palettes; their frames are relative to unk5A and the palette, so the
# animset is copied unchanged. Those that draw primitives with fixed UVs keep
# their quadrant's place in a tpage. (name, stage, animset, art, palette,
# contexts, quadrant within a tpage or None, Garden animset);
# mod_psx_sprites.h gets MOD_<NAME>_ANIMSET, _UNK5A, _TPAGE and _PAL.
PSX_SPRITES = [
    ("THORNWEED", "chi", 7, "bn_gfx_thornweed_corpseweed", "bn_pal_thornweed_corpseweed",
     ("1", "3"), 0, 5),
    ("SLINGER", "no0", 10, "e_slinger", "e_slinger_pal", ("0W", "0E"), None, 6),
]
# The candle's glow (STAGE_15 0x060DC328): per flame frame, a soft round image
# of bank 16 at an offset (records {dx, dy, image, ..} at 0x060F1F16), drawn
# over the flame with colour calculation (draw mode 0x73: B + F/4). Its pixels
# are a gradient, indices 10-14; the PSX draws it additively in a dim warm
# ramp, the halo around the flames on the Saturn.
CANDLE_GLOW = (0x060F0858, 0x060F1F16, 4)
# RGB555 (bit 15 kept so no entry is transparent), indices 10-14 dark to bright
GLOW_COLOURS = [0] * 10 + [0x8044, 0x8067, 0x808A, 0x80CE, 0x8112, 0, 0, 0, 0, 0][:6]
SPRITE_CLUT_BASE = 0x210  # after WRP's palette at 0x200 (palette_def)
# the candle glow image's flip (saturn_sprites image_part: 3 = turned around,
# as the inverted castle's candles draw it)
GLOW_FLIP = 0
# banks of rotating entities (saturn_sprites.BLEED_BANKS): the Water Leaper
# and the Human Face Tree
BLEED_BANKS = (0x060F82C8, 0x060F6DC4)
# the boss's death fire palette (MOD_PUFF_OPAQUE_OFFSET)
PUFF_PALETTE = True

# room -> (tilemap, tiledef prefix, source stage) for the shared-tileset rooms
SHARED_ROOMS = {4: ("wrp_tilemap_5", "wrp_tiledef_load", "wrp"),
                5: ("wrp_tilemap_5", "wrp_tiledef_load", "wrp"),
                6: ("np3_tilemap_37", "np3_tiledef_save", "np3")}

# The loading corridor's angel statues and arch are not tiles but an entity,
# placed in every stage's loading rooms (WRP's and NP3's lists alike)
LOADING_FOREGROUND = {"x": 0, "y": 56, "id": "E_ROOM_FOREGROUND", "flags": 0, "slot": 0x80,
                      "spawnId": 0, "params": 0}

# A save room's arch and floor pieces are entities too (NP3's save rooms,
# layout 47): E_ROOM_FOREGROUND params 1 and 2
SAVE_FOREGROUND = [
    {"x": 128, "y": 48, "id": "E_ROOM_FOREGROUND", "flags": 0, "slot": 0x80, "spawnId": 0,
     "params": 1},
    {"x": 128, "y": 192, "id": "E_ROOM_FOREGROUND", "flags": 0, "slot": 0x88, "spawnId": 0,
     "params": 2},
]

# The water under room 0's bridge: as the Saturn (and NP3), its entities are
# placed at both room ends (Saturn x 160/2720 -> 128/2176) on both rows, each
# kind sharing one slot so that only one is alive. params 0x0100: one pool,
# starting at pool 0 (g_WaterXTbl, mods/sotn-ultimate/src/garden/gdn/e_room_fg.c).
WATER = [
    {"x": x, "y": y, "id": eid, "flags": 0, "slot": slot, "spawnId": 0, "params": 0x0100}
    for eid, slot in (("E_ALUCARD_WATER_EFFECT", 0x98), ("E_MOD_WATER_BODY", 0x99),
                      ("E_MOD_WATER_SURFACE", 0x9A))
    for x in (128, 2176) for y in (128, 384)]

# The Saturn's candles (STAGE_15 layout lists, room + 1), as they are: the
# record is the PSX LayoutEntity, so only x scales (4/5). params: type << 12
# | prize id (ITEMDROP_*); type 1 releases the stage item params & 0x1FF.
CANDLE_ROOMS = (0, 1, 3)

# (room, x, y, slot[, opens right]) on the PSX grid; params is the index in
# the list (| 0x100 for a door opening to the right)
RED_DOORS = [(0, 8, 97, 0x9D), (0, 8, 353, 0x9E)]
# the boss room door's solid tile (mod_boss_door.h)
BOSS_DOOR = True
# the Saturn entities placed as they are (Saturn id -> PSX enum name)
SATURN_IDS = {0x09: "E_STAGE_NAME_POPUP",
              0x11: "E_ID_11", 0x1E: "E_MOD_DRIP_SPAWNER",
              0x20: "E_SLINGER", 0x23: "E_MOD_GARGOYLE",
              0x2E: "E_MOD_TREE",
              0x35: "E_MOD_LEAPER_SPAWNER", 0x36: "E_THORNWEED",
              0x3B: "E_MOD_MANEATER", 0x3D: "E_MOD_GARDENER",
              0x44: "E_MOD_BOSS_CONTROLLER",
              0x40: "E_MOD_SKBREEDER"}


def converter():
    if not os.path.exists(TOOL):
        env = dict(os.environ)
        tc = os.path.expanduser("~/.rustup/toolchains")  # a rustup install's cargo
        if os.path.isdir(tc) and os.listdir(tc):
            env["PATH"] = os.path.join(tc, sorted(os.listdir(tc))[0], "bin") + ":" + env["PATH"]
        env["CARGO_TARGET_DIR"] = TOOL_TARGET
        subprocess.run(["cargo", "build", "--release", "--offline"],
                       cwd=os.path.join(ROOT, "tools/saturn/assets"), env=env, check=True)
    if os.path.exists(WORK):
        shutil.rmtree(WORK)
    os.makedirs(WORK)
    subprocess.run([TOOL, "map", "convert-psx", PRG, MAP, WORK],
                   check=True, stdout=subprocess.DEVNULL,
                   env=dict(os.environ, SOTN_MOD_UNCAPPED="1"))


def main():
    converter()
    swap = gdn_tile_swap.apply(WORK, TILE_SWAP_FLIP)
    if swap:
        print("ps1 tiles: %(cells)d cells, %(tiles)d tiles, %(graphics)d graphics, "
              "%(palettes_new)d new palettes, %(palettes_reused)d on existing ones, "
              "%(nearest)d on the nearest one (max colour error %(max_err)d)" % swap)
    conv_rooms = json.load(open(os.path.join(WORK, "rooms.roomdef.json")))[:7]
    conv_layers = json.load(open(os.path.join(WORK, "rooms.layers.json")))[:7]
    if SPECIAL_CELLS is not None:
        conv_rooms = conv_rooms[:ORDINARY_ROOMS]
        conv_layers = conv_layers[:ORDINARY_ROOMS]
        for room, (x, y) in sorted(SPECIAL_CELLS.items()):
            assert room == len(conv_rooms)
            cell = {"left": x, "top": y, "right": x, "bottom": y}
            conv_rooms.append(dict(cell, tileLayoutId=room))
            conv_layers.append({"fg": cell})
    assert [r["tileLayoutId"] for r in conv_rooms] == list(range(7)), conv_rooms

    if os.path.exists(OUT):
        shutil.rmtree(OUT)
    # WRP's entity assets (sprite banks, palettes, stage title) for now
    shutil.copytree(os.path.join(ROOT, "assets/st/wrp"), OUT,
                    ignore=shutil.ignore_patterns("wrp_tile*", "layers.json", "rooms.json",
                                                  "entity_layouts.json"))

    # tile definitions: 4 tables of 0x1000 entries. The asset tool's names are
    # the reverse of their use: "tiles" fills TileDefinition.gfxPage and
    # "pages" fills gfxIndex (retail "tiles" files hold small page numbers).
    td, n_tiles = {}, None
    for src, dst in (("gfx_page", "tiles"), ("gfx_index", "pages"), ("clut", "cluts"),
                     ("collision", "cols")):
        data = open(os.path.join(WORK, f"tiledefs.{src}.bin"), "rb").read()
        assert len(data) <= 0x1000 and n_tiles in (None, len(data))
        n_tiles = len(data)
        td[dst] = bytearray(data + bytes(0x1000 - len(data)))
    fte1, vram4 = repack(td, n_tiles, conv_layers)
    json.dump({"tiles": "gdn_tiledef_gdn_tiles.bin", "pages": "gdn_tiledef_gdn_pages.bin",
               "cluts": "gdn_tiledef_gdn_cluts.bin", "collisions": "gdn_tiledef_gdn_cols.bin"},
              open(os.path.join(OUT, "gdn_tiledef_gdn.json"), "w"), indent=2)

    shared = {}
    for room, (tmap, tdef, st) in SHARED_ROOMS.items():
        kind = tdef.rsplit("_", 1)[1]
        new_tdef, new_tmap = f"gdn_tiledef_{kind}", f"gdn_tilemap_{kind}"
        if new_tdef + ".json" not in [v[1] for v in shared.values()]:
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
        if i in shared:
            data, tdef = shared[i]
            c = cl["fg"]
            loading = i in (4, 5)
            # a loading room's flags byte is 0x40 with its exit cell on the
            # left, 0x41 on the right (every retail loading room)
            exit_right = any(ex == (c["right"] + 1, c["top"]) for ex, _ in EXITS)
            layers.append({"fg": {
                "data": data, "tiledef": tdef,
                "left": c["left"], "top": c["top"], "right": c["right"], "bottom": c["bottom"],
                # as NP3's loading rooms and its (3,42) save room
                "scrollMode": (1 if exit_right else 0) if loading else 2,
                "hideOnMap": False, "isSaveRoom": not loading, "isLoadingRoom": loading,
                "unusedFlag": False,
                "zPriority": LOADING_TPS[i] if loading else 96,
                "flags": 515}})
            if data not in tilemaps:
                tilemaps.append(data)
            continue
        entry = {}
        for part in ("fg", "bg"):
            c = cl[part]
            name = f"gdn_tilemap_{len(tilemaps)}.bin"
            shutil.copy(os.path.join(WORK, c["data"] + ".tilelayout.bin"), os.path.join(OUT, name))
            tilemaps.append(name)
            loading = i in (4, 5)
            save = i == 6
            entry[part] = {
                "data": name, "tiledef": "gdn_tiledef_gdn.json",
                "left": c["left"], "top": c["top"], "right": c["right"], "bottom": c["bottom"],
                "scrollMode": 0 if (loading or save) else 1,
                "hideOnMap": False, "isSaveRoom": save and part == "fg",
                "isLoadingRoom": loading and part == "fg", "unusedFlag": False,
                # a loading room's zPriority is the teleport it preloads for.
                # The foreground goes in front of or behind the characters
                # as on the Saturn for that room (fg_priority).
                "zPriority": (LOADING_TPS[i] if loading else fg_priority(i))
                if part == "fg" else 32,
                "flags": 515 if (loading or save) and part == "fg" else 3,
            }
        layers.append(entry)
    json.dump(layers, open(os.path.join(OUT, "layers.json"), "w"), indent=2)
    trim_shared_tiledefs(layers)

    # red door tiles (see the top of this file)
    door_rows, solid = [], {}
    for door in RED_DOORS:
        room, x, y, slot = door[:4]
        fgl = layers[room]["fg"]
        d = open(os.path.join(OUT, fgl["data"]), "rb").read()
        tm = [d[i] | d[i + 1] << 8 for i in range(0, len(d), 2)]
        w = (fgl["right"] - fgl["left"] + 1) * 16
        # a door opening to the right (params 0x100) sits at x - 4
        col, row = (x + (-4 if door_right(door) else 4)) >> 4, (y + 31 - 24) >> 4
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
    # the boss room's door (mods/sotn-ultimate/src/garden/gdn/e_mod_boss.h) closes its doorway with
    # a blank, solid tile: tile 0's art with collision 3
    if BOSS_DOOR:
        assert n_tiles < 0x1000, "tileset full"
        for k in ("tiles", "pages", "cluts"):
            td[k][n_tiles] = td[k][0]
        td["cols"][n_tiles] = 3
        boss_door_tile = n_tiles
        n_tiles += 1
        with open(os.path.join(SRC, "mod_boss_door.h"), "w") as f:
            f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n")
            f.write("// the boss room's door: blank art, solid\n")
            f.write("#define MOD_BOSS_DOOR_TILE 0x%03X\n" % boss_door_tile)
    with open(os.path.join(SRC, "mod_red_door_tiles.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n")
        f.write("// [params][0..3] closed (solid), [4..7] open: the doorway's own tiles\n")
        f.write("u16 g_RedDoorTiles[][8] = {\n")
        for r in door_rows:
            f.write("    {" + ", ".join("0x%03X" % v for v in r) + "},\n")
        f.write("};\n")
    n_tiles = backdrop(td, n_tiles, fte1, vram4)
    # only the tiles in use (the converted ones, the red doors' and the boss
    # door's): the tables don't need their 0x1000 entries, and the overlay is
    # short of room
    for k, data in td.items():
        open(os.path.join(OUT, f"gdn_tiledef_gdn_{k}.bin"), "wb").write(data[:n_tiles])

    rooms = []
    for i, r in enumerate(conv_rooms):
        rooms.append({"left": r["left"], "top": r["top"], "right": r["right"],
                      "bottom": r["bottom"], "layerId": i, "tileDefId": 0,
                      "entityGfxId": CONTEXT_BANKS[ROOM_CONTEXT[i]] + 1
                      if i in ROOM_CONTEXT else 0, "entityLayoutId": i + 1})
    # exits: transition cells next to the loading rooms
    for (x, y), tp in EXITS:
        # the asset tool keeps layerId as a signed byte; the game reads the
        # same byte unsigned, so teleports past 127 are written negative
        rooms.append({"left": x, "top": y, "right": x, "bottom": y,
                      "layerId": tp - 256 if tp > 127 else tp,
                      "tileDefId": -1, "entityGfxId": 0, "entityLayoutId": 0})
    json.dump(rooms, open(os.path.join(OUT, "rooms.json"), "w"), indent=2)
    if WITH_SPRITES:
        sprite_zones(conv_rooms)

    # entity layouts. Index i+1 = room i; the tool wants a head and tail per list.
    def layout(ents):
        out = [{"x": -2, "y": -2, "id": "E_NONE", "flags": 0, "slot": 0, "spawnId": 0,
                "params": 0, "yOrder": 0}]
        for n, e in enumerate(ents):
            out.append(dict(e, yOrder=n + 1))
        out.append({"x": -1, "y": -1, "id": "E_NONE", "flags": 0, "slot": 0, "spawnId": 0,
                    "params": 0, "yOrder": len(ents) + 1})
        return out
    lists = [layout([])]  # layout 0
    for i in range(7):
        doors = [(n, d) for n, d in enumerate(RED_DOORS) if d[0] == i]
        ents = [{"x": d[1], "y": d[2], "id": "E_RED_DOOR", "flags": 0, "slot": d[3],
                 "spawnId": 0, "params": n | (0x100 if door_right(d) else 0)}
                for n, d in sorted(doors, key=lambda e: e[1][2])]
        if i in (4, 5):
            ents.append(dict(LOADING_FOREGROUND))
        if i == 6:
            ents += [dict(e) for e in SAVE_FOREGROUND]
        if i == 0:
            ents += [dict(e) for e in WATER]
        if i in CANDLE_ROOMS:
            ents += saturn_candles(i)
        ents += saturn_entities(i, SATURN_IDS)
        lists.append(layout(ents))
    json.dump({"entities": lists, "indices": list(range(len(lists)))},
              open(os.path.join(OUT, "entity_layouts.json"), "w"), indent=2)

    if WITH_SPRITES:
        sprites()
    else:
        pages_only()

    # the includes stage_data.c needs for the generated tilemaps
    with open(os.path.join(SRC, "mod_tilemaps.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n")
        for t in tilemaps:
            f.write("#include <gen/%s.h>\n" % t[:-4])
        for t in ["gdn_tiledef_gdn"] + sorted({v[1][:-5] for v in shared.values()}):
            f.write("#include <gen/%s.h>\n" % t)

    os.makedirs(os.path.join(ROOT, "build/us"), exist_ok=True)
    open(F_FILE, "wb").write(fte1)
    print(f"gdn: {len(rooms)} rooms, {len(layers)} layers, {len(tilemaps)} tilemaps")


def door_right(door):
    """a RED_DOORS entry (room, x, y, slot[, opens right])"""
    return len(door) > 4 and door[4]


def zoned():
    """room 0's two ends take turns (the contexts "0W" and "0E")"""
    return "0W" in CONTEXT_BANKS and "0E" in CONTEXT_BANKS


def sprite_zones(conv_rooms):
    with open(os.path.join(SRC, "mod_sprite_zones.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n")
        if zoned():
            f.write("// room 0's two ends take turns in the sprite pages (ZONE_WEST/EAST)\n")
            f.write("#define MOD_ZONE_ROOM_LEFT %d\n#define MOD_ZONE_ROOM_TOP %d\n"
                    % (conv_rooms[0]["left"], conv_rooms[0]["top"]))
            f.write("#define MOD_ZONE_WEST %d\n#define MOD_ZONE_EAST %d\n"
                    % (ZONE_WEST, ZONE_EAST))
            f.write("#define MOD_ZONE_WEST_GFX ANIMSET_OVL(%d)\n"
                    "#define MOD_ZONE_EAST_GFX ANIMSET_OVL(%d)\n"
                    % (CONTEXT_BANKS["0W"], CONTEXT_BANKS["0E"]))
        f.write("// a room's sprite bank -> the same with the tile pages (RELOAD_BANKS),\n"
                "// for the menu's reload; rooms without sprites reload the pages alone\n")
        f.write("static const u16 g_ModReloadBanks[][2] = {\n")
        for c, index in CONTEXT_BANKS.items():
            f.write("    {ANIMSET_OVL(%d), ANIMSET_OVL(%d)},\n" % (index, RELOAD_BANKS[c]))
        f.write("};\n#define MOD_PAGES_GFX ANIMSET_OVL(%d)\n" % PAGE_BANK)


EXTRA_PAGES = (13, 14, 15)    # tile page p is VRAM tpage 8 + p: x 320-511, y 256
MIST_CLUT = 251
SHIMMER_CLUT = 250
RESERVED_CLUTS = set(range(163, 200)) | set(range(250, 256))  # shared tiles, shimmer, mist

# The water's surface shimmer (Saturn STAGE_15 entity 0x16, sprite bank 22 at
# 0x060F0B30): 3 frames of 4 strips 2px tall (128, 128, 128, 24 wide), drawn
# as 5 segments (strips 0, 1, 2, 0, 3; the last 20 wide) along the water's top
# edge. Narrowed by 4/5 like everything else, they sit under the mist in its
# texture page (rows 240-255, which the mist sprites don't show).
SHIMMER_BANK = 0x060F0B30  # None: no water, no shimmer
# the mist's first row: drawn stretched over the screen's 240 lines, it shows
# 240 of the backdrop's 256 (the inverted Garden's is rotated: the last 240)
BACKDROP_ROW0 = 0
SHIMMER_SEGMENTS = [(0, 128), (1, 128), (2, 128), (0, 128), (3, 20)]
# the inverted Garden's ceiling water (RSTAGE15 0x16) draws the strips flipped
# both ways (CMDCTRL 0x30), at their full width: stored rotated 180 degrees,
# segments {u, v, the image's narrowed width}
SHIMMER_FLIP = False
# True: the shimmer's palette goes after the sprite palettes (sprites(),
# MOD_SHIMMER_PAL in mod_psx_sprites.h) instead of tile palette SHIMMER_CLUT
SHIMMER_SPRITE_PAL = False
SHIMMER_PALETTE = []


def saturn_candles(room):
    prg = open(PRG, "rb").read()
    base = STAGE_BASE
    lists = struct.unpack_from(">I", prg, 8)[0] - base
    p = struct.unpack_from(">I", prg, lists + 4 * (room + 1))[0] - base
    out = []
    while True:
        x, y, eid, slot, params = struct.unpack_from(">HHHHH", prg, p)
        p += 10
        if x == 0xFFFF:
            break
        if x != 0xFFFE and eid & 0x1FFF == 1:
            out.append({"x": x * 4 // 5, "y": y, "id": "E_BREAKABLE", "flags": (eid & 0xE000) >> 8,
                        "slot": slot & 0xFF, "spawnId": slot >> 8, "params": params})
    return out


def write_indexed_png(path, w, h, pixels):
    """An 8-bit indexed PNG with a 16-colour grey palette, the form the asset
    tool reads as 4bpp art (tools/sotn-assets/util/png: 8-bit samples, at most
    16 palette colours); PIL would write 16 colours as a 4-bit PNG."""
    def chunk(kind, body):
        return (struct.pack(">I", len(body)) + kind + body +
                struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF))
    rows = b"".join(b"\0" + bytes(pixels[y * w:(y + 1) * w]) for y in range(h))
    png = (b"\x89PNG\r\n\x1a\n" +
           chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 3, 0, 0, 0)) +
           chunk(b"PLTE", bytes(v for i in range(16) for v in (i * 17,) * 3)) +
           chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b""))
    open(path, "wb").write(png)


def trim_shared_tiledefs(layers):
    """The loading and save rooms' tile definitions are other stages' whole
    tables (0x1000 entries, 16 KB each); keep the entries their tilemaps use."""
    used = {}
    for entry in layers:
        for part in ("fg", "bg"):
            lay = entry.get(part) if isinstance(entry, dict) else None
            if not lay or lay["tiledef"] == "gdn_tiledef_gdn.json":
                continue
            d = open(os.path.join(OUT, lay["data"]), "rb").read()
            top = max(d[i] | d[i + 1] << 8 for i in range(0, len(d), 2))
            used[lay["tiledef"]] = max(used.get(lay["tiledef"], 0), top + 1)
    for tdef, n in used.items():
        names = json.load(open(os.path.join(OUT, tdef)))
        for f in names.values():
            path = os.path.join(OUT, f)
            data = open(path, "rb").read()
            open(path, "wb").write(data[:n])


def allocate(items):
    """[(key, quadrants, contexts, quadrant in tpage or None[, consecutive])] ->
    {key: [slot]}: whole quadrants of SPRITE_QUADRANTS, never shared within a
    context. A consecutive item (a PS1 enemy's art blocks: its frames count
    the tilesets from its unk5A) takes a run of quadrants with consecutive
    tilesets (tpage << 2 | quadrant), the first one at its quadrant in tpage."""
    used = {c: set() for c in CONTEXT_BANKS}
    out = {}
    for item in sorted(items, key=lambda it: (-len(it[2]), -it[1])):
        key, n, ctx, quad = item[:4]
        free = [i for i, (_, q) in enumerate(SPRITE_QUADRANTS)
                if all(i not in used[c] for c in ctx) and (quad is None or q == quad)]
        if len(item) > 4 and item[4] and n > 1:
            tiles = [t << 2 | q for t, q in SPRITE_QUADRANTS]
            runs = [[SPRITE_QUADRANTS.index(((tiles[i] + k) >> 2, (tiles[i] + k) & 3))
                     for k in range(n)] for i in free
                    if all(((tiles[i] + k) >> 2, (tiles[i] + k) & 3) in SPRITE_QUADRANTS
                           for k in range(n))]
            runs = [r for r in runs if all(j not in used[c] for j in r for c in ctx)]
            assert runs, ("no consecutive sprite quadrants", key, ctx, n)
            free = runs[0]
        assert len(free) >= n, ("sprite quadrants full", key, ctx, n, len(free))
        out[key] = free[:n]
        for c in ctx:
            used[c].update(free[:n])
    return out


def psx_art_blocks(art):
    """a PSX_SPRITES art entry: one block's name or a list of them"""
    return [art] if isinstance(art, str) else list(art)


# Write MOD_<NAME>_SPRITES (the copied animset's sprite bank symbol) to
# mod_psx_sprites.h, for enemies that draw their frames' parts themselves
PSX_SPRITE_SYMBOLS = False


def stage_bin_cluts(stage):
    """{clut: 16 RGB555 colours} the PS1 stage's overlay loads (its header's
    cluts table, PAL_BULK entries), from the retail disc's ST/<STAGE>.BIN"""
    st = stage.upper()
    d = open(os.path.join(ROOT, "disks/us/ST", st, st + ".BIN"), "rb").read()
    base = 0x80180000

    def rd(a):
        return struct.unpack_from("<I", d, a - base)[0]
    out = {}
    tab = rd(base + 0x18)
    for i in range(64):
        p = rd(tab + 4 * i)
        if not base <= p < base + len(d) or rd(p) != 5:
            break
        q = p + 4
        while rd(q) != 0xFFFFFFFF:
            dst, n, src = rd(q), rd(q + 4), rd(q + 8)
            q += 12
            for k in range(n // 16):
                out[dst // 16 + k] = list(struct.unpack_from("<16H", d, src - base + k * 32))
    return out


def psx_palettes(stage, pal):
    """a PSX_SPRITES palette entry: a palette asset's name (its PNGs), or
    (first clut, count) read from the stage's retail overlay"""
    if not isinstance(pal, str):
        first, count = pal
        cl = stage_bin_cluts(stage)
        return [cl[first + k] for k in range(count)]
    from PIL import Image
    meta = open(os.path.join(ROOT, "assets/st", stage, pal + ".yaml")).read()
    palettes = []
    for png in re.findall(r"^- (.*\.png)$", meta, re.M):
        cols = Image.open(os.path.join(ROOT, "assets/st", stage, png)).convert("RGBA")
        # the asset tool writes RGB555 colours with alpha 127 for the STP bit
        palettes.append([round(r * 31 / 255) | round(g * 31 / 255) << 5 |
                         round(b * 31 / 255) << 10 | (0x8000 if a == 127 else 0)
                         for r, g, b, a in cols.getdata()])
    return palettes


def sprites():
    st = SaturnStage(PRG, CHR, os.path.join(SAT, "0.BIN"))
    deco_tab, deco_n = DECORATIONS
    decos = []
    for v in range(deco_n):
        o = deco_tab - st.base + v * 28
        bank, frames, pal, z, order = struct.unpack_from(">IIHHB", st.prg, o)
        anim = struct.unpack_from(">I", st.prg, o + 24)[0]
        frame = st.prg[anim - st.base + 1]
        decos.append(((DECORATION_GROUPS[v], bank, frames, frame),
                      0x67 + order if z == 0x20 else z))
    sets = [(n, [d[0] for d in decos] if f == "decorations" else f) for n, f in SPRITE_SETS]
    glow_bank, glow_recs, glow_n = CANDLE_GLOW
    glow_imgs = [struct.unpack_from(">hhh", st.prg, glow_recs - st.base + f * 10)
                 for f in range(glow_n)]

    def shifted(parts, dy):
        # a frame drawn dy px lower (a Saturn sprite offset from its entity)
        return [dict(p, y=p["y"] + dy) for p in parts]

    # the rotating banks (see saturn_sprites.BLEED_BANKS); pass 1 below
    # gathers their edges before any image is drawn into a quadrant
    saturn_sprites.BLEED_BANKS.update(BLEED_BANKS)

    def fill(sheets):
        for g, bank in PALETTE_BLOCKS.values():
            sheets[g].palette_block(st, bank)
        banks = [(n, [None if e is None else
                      shifted(sheets[e[0]].frame(st, *e[1:4], e[5] if len(e) > 5 else 0),
                              e[4] if len(e) > 4 else 0)
                      for e in frames])
                 for n, frames in sets]
        glow = [sheets["candles"].image_part(st, glow_bank, img, dx, dy, GLOW_COLOURS,
                                             GLOW_FLIP)
                for dx, dy, img in glow_imgs]
        return banks, glow

    # pass 1 lists each group's images, pass 2 measures them packed tallest
    # first, pass 3 packs them into the group's quadrants
    many = [(0, 0)] * 16
    seen = {g: PsxSpriteSheet(many, SPRITE_CLUT_BASE) for g in SPRITE_GROUPS}
    fill(seen)
    # measured on quadrants with the usable sizes of the real ones (see
    # PsxSpriteSheet._limits); a group that then doesn't fit the quadrants it
    # was given gets one more and everything is allocated again
    sizes = {}
    for g in SPRITE_GROUPS:
        probe = PsxSpriteSheet(SPRITE_QUADRANTS * 2, SPRITE_CLUT_BASE)
        probe.prepack(st, seen[g].requests)
        sizes[g] = probe.used_quadrants()
    while True:
        slots = allocate([(g, sizes[g], ctx, None) for g, ctx in SPRITE_GROUPS.items()] +
                         [(p[0], len(psx_art_blocks(p[3])), p[5], p[6], True)
                          for p in PSX_SPRITES])
        cluts = []
        sheets = {g: PsxSpriteSheet([SPRITE_QUADRANTS[i] for i in slots[g]],
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
    banks, glow = fill(sheets)

    path = os.path.join(OUT, "us/sprite_banks.json")
    sb = json.load(open(path))
    for animset, frames in banks:
        sb["banks"].append([None] + frames)
        sb["indices"][animset] = len(sb["banks"]) - 1
    psx = psx_sprites(sb, {p[0]: [SPRITE_QUADRANTS[i] for i in slots[p[0]]]
                           for p in PSX_SPRITES})
    json.dump(sb, open(path, "w"), indent=2)
    with open(os.path.join(SRC, "mod_bg_blocks.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n// clang-format off\n")
        f.write("// the Saturn's decorations (entity 0x11): ANIMSET_OVL(3) frame n + 1\n")
        for v, (_, z) in enumerate(decos):
            f.write("static u8 g_ModBgBlockAnim%d[] = {0x40, %d, 0xFF, 0x00};\n" % (v, v + 1))
        f.write("ObjInit2 BackgroundBlockInit[] = {\n")
        for v, (_, z) in enumerate(decos):
            f.write("    {.animSet = ANIMSET_OVL(3), .zPriority = 0x%02X, .unk5A = 0, .palette = 0,\n"
                    "     .drawFlags = ENTITY_DEFAULT, .blendMode = BLEND_NO, .flags = 0,\n"
                    "     .animFrames = g_ModBgBlockAnim%d},\n" % (z, v))
        f.write("};\n// clang-format on\n")

    # the art blocks, then one graphics bank per context
    blocks = []  # (quadrant, array name, 4bpp pixels, contexts)
    for g, ctx in SPRITE_GROUPS.items():
        for k in range(sheets[g].used_quadrants()):
            blocks.append((sheets[g].quadrants[k], "g_ModGdn_%s%d" % (g, k),
                           sheets[g].quadrant_pixels(k), ctx))
    for p, spec in zip(psx, PSX_SPRITES):
        for k, (quadrant, pixels) in enumerate(zip(p["quadrants"], p["blocks"])):
            blocks.append((quadrant, "g_ModGdn%sGfx%s" % (p["name"].title(),
                                                          k if len(p["blocks"]) > 1 else ""),
                           pixels, spec[5]))
    # the area name for the first-visit popup (tpage 17, as the retail stages')
    if STAGE_NAME_ROOMS:
        blocks.append(((17, 0), "g_ModGdnStageName", stage_name_pixels(), STAGE_NAME_ROOMS))
    # room 0's halves: the blocks one has and the other hasn't, streamed in by
    # the stage when the camera crosses (st_update.c ModZoneStream); a stage
    # without the "0W"/"0E" contexts has none
    path = os.path.join(SRC, "mod_zone_stream.h") if zoned() else os.devnull
    with open(path, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n")
        names = {}
        for side, other in (("0W", "0E"), ("0E", "0W")):
            names[side] = [(n, q) for q, n, _, ctx in blocks if side in ctx and other not in ctx]
        for n, _ in names["0W"] + names["0E"]:
            f.write("extern u8 %s[];\n" % n)
        for side, label in (("0W", "West"), ("0E", "East")):
            f.write("static const ModZoneBlock g_ModZone%s[] = {\n" % label)
            for n, (tpage, quad) in names[side]:
                f.write("    {%s, %d, %d},\n" % (n, (tpage & 15) * 64 + (quad & 1) * 32,
                                                 (tpage >> 4) * 256 + (quad >> 1) * 128))
            f.write("};\n")
    path = os.path.join(OUT, "graphics_banks.json")
    gb = json.load(open(path))
    for c, index in CONTEXT_BANKS.items():
        entries = []
        for (tpage, quad), name, _, ctx in blocks:
            if c not in ctx:
                continue
            vx = (tpage & 15) * 64 + (quad & 1) * 32
            vy = (tpage >> 4) * 256 + (quad >> 1) * 128
            # GFX_ENTRY takes VRAM y first, then x (LoadTPage(.., xy >> 16, (u16)xy))
            entries.append({"x": vy, "y": vx, "width": 128, "height": 128, "name": name})
        gb["banks"].append({"kind": 4, "entries": entries})
        gb["indices"][index] = len(gb["banks"]) - 1
    assert PAGE_BLOCKS, "backdrop() runs first"
    pages = [{"x": vy, "y": vx, "width": 128, "height": 128, "name": name}
             for vx, vy, name, _ in PAGE_BLOCKS]
    gb["banks"].append({"kind": 4, "entries": pages})
    gb["indices"][PAGE_BANK] = len(gb["banks"]) - 1
    for c, index in RELOAD_BANKS.items():
        gb["banks"].append({"kind": 4, "entries":
                            gb["banks"][gb["indices"][CONTEXT_BANKS[c]]]["entries"] + pages})
        gb["indices"][index] = len(gb["banks"]) - 1
    json.dump(gb, open(path, "w"), indent=2)
    blocks += [(None, name, data, ()) for _, _, name, data in PAGE_BLOCKS]
    # each block is compressed by the asset tool (cmpgfx) from a PNG, as the
    # retail stages' sprite art: build/us/gdn_assets.us.yaml is
    # mods/sotn-ultimate/config/mod_assets.us.yaml plus one cmpgfx entry per block (the offsets
    # only matter when extracting, which that copy is never used for)
    for _, name, data, _ in blocks:
        write_indexed_png(os.path.join(OUT, name + ".png"), 128, 128,
                          [v for b in data for v in (b & 15, b >> 4)])
    conf = open(CONFIG).read().rstrip("\n") + "\n"
    last = "          - [0x6D88, skip] # sprites\n"
    assert conf.endswith(last), CONFIG + " must end with the sprites skip"
    for k, (_, name, _, _) in enumerate(blocks):
        conf += "          - [0x%X, cmpgfx, %s, 128, 128, 4]\n" % (0x6D90 + k * 4, name)
    os.makedirs(os.path.join(ROOT, "build/us"), exist_ok=True)
    open(BUILD_CONFIG, "w").write(conf)

    psx_pals = [c for p in psx for c in p["palettes"]]
    puff_clut = None
    # the boss's death fire (PS1 EntityExplosionPuffOpaque params 3) uses a
    # fire palette at its stage's PAL_PUFF_OPAQUE_OFFSET + 4: the Castle
    # Entrance's (NO3 loads its puff palettes, e_explosion_p, at 0xD0; the
    # fire is the fifth). PNG alpha 127 = the STP bit, as in psx_sprites().
    if PUFF_PALETTE:
        from PIL import Image
        cols = Image.open(os.path.join(ROOT, "assets/st/no3/e_explosion_p/e_explosion_p_4.png"))
        psx_pals.append([round(r * 31 / 255) | round(g * 31 / 255) << 5 |
                         round(b * 31 / 255) << 10 | (0x8000 if a == 127 else 0)
                         for r, g, b, a in cols.convert("RGBA").getdata()])
        puff_clut = SPRITE_CLUT_BASE + len(cluts) + len(psx_pals) - 1
    shimmer_clut = None
    if SHIMMER_SPRITE_PAL and SHIMMER_PALETTE:
        psx_pals.append(list(SHIMMER_PALETTE))
        shimmer_clut = SPRITE_CLUT_BASE + len(cluts) + len(psx_pals) - 1
    path = os.path.join(OUT, "palette_def.json")
    pd = json.load(open(path))
    pd["defs"][0]["entries"].append({"destination": SPRITE_CLUT_BASE * 16,
                                     "length": 16 * (len(cluts) + len(psx_pals)),
                                     "name": "g_ModGdnSpritePals.palbin"})
    json.dump(pd, open(path, "w"), indent=2)

    with open(os.path.join(SRC, "mod_sprite_gfx.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n// clang-format off\n")
        f.write("// the Garden's sprites, 4bpp 128x128 quadrants of tpages 18-19,\n")
        f.write("// compressed by the asset tool from assets/st/gdn/<name>.png\n")
        for _, name, _, _ in blocks:
            f.write("u8 %s[] __attribute__((aligned(4))) = {\n" % name)
            f.write("#include \"gen/%s.h\"\n};\n" % name)
        f.write("// clang-format on\n")
    with open(os.path.join(SRC, "mod_sprite_pals.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n// clang-format off\n")
        f.write("// the Garden's sprite palettes, g_ClutIds 0x%X on\n" % SPRITE_CLUT_BASE)
        f.write("u16 g_ModGdnSpritePals[] = {\n")
        for pal in cluts + psx_pals:
            f.write("    " + ", ".join("0x%04X" % c for c in pal) + ",\n")
        f.write("};\n// clang-format on\n")
    clut = SPRITE_CLUT_BASE + len(cluts)
    with open(os.path.join(SRC, "mod_psx_sprites.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n")
        f.write("// the Saturn enemies' palette blocks (PALETTE_BLOCKS)\n")
        for name, (g, bank) in PALETTE_BLOCKS.items():
            f.write("#define MOD_%s_PAL 0x%03X\n" % (name, sheets[g].blocks[bank]))
        if puff_clut is not None:
            f.write("// the boss's death fire palette (PAL_PUFF_OPAQUE_OFFSET + 4)\n")
            f.write("#define MOD_PUFF_OPAQUE_OFFSET 0x%03X\n" % (puff_clut - 4))
        if shimmer_clut is not None:
            f.write("// the water shimmer's palette (SHIMMER_SPRITE_PAL)\n")
            f.write("#define MOD_SHIMMER_PAL 0x%03X\n" % shimmer_clut)
        f.write("// the PS1 enemies' sprites in the Garden (PSX_SPRITES)\n")
        for p in psx:
            tpage, quad = p["quadrants"][0]
            f.write("#define MOD_%s_ANIMSET ANIMSET_OVL(%d)\n" % (p["name"], p["animset"]))
            f.write("#define MOD_%s_UNK5A 0x%02X\n" % (p["name"], tpage << 2 | quad))
            f.write("#define MOD_%s_TPAGE 0x%02X\n" % (p["name"], tpage))
            f.write("#define MOD_%s_PAL 0x%03X\n" % (p["name"], clut))
            if PSX_SPRITE_SYMBOLS:
                f.write("#define MOD_%s_SPRITES sprites_%s_%d\n"
                        % (p["name"], os.path.basename(SRC), p["bank"]))
            clut += len(p["palettes"])
    with open(os.path.join(SRC, "mod_candle_glow.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n// clang-format off\n")
        f.write("// per candle flame frame: x, y, w, h, u, v, clut, tpage\n")
        f.write("static const s16 g_ModCandleGlow[][8] = {\n")
        for p in glow:
            quad = p["tileset"] & 3
            f.write("    {%d, %d, %d, %d, %d, %d, 0x%X, %d},\n" % (
                p["x"], p["y"], p["width"], p["height"], p["left"] + (quad & 1) * 128,
                p["top"] + (quad >> 1) * 128, p["clut"], p["tileset"] >> 2))
        f.write("};\n// clang-format on\n")
    for c in CONTEXT_BANKS:
        n = sum(len(sh.placed) for g, sh in sheets.items() if c in SPRITE_GROUPS[g])
        q = len({blk[0] for blk in blocks if c in blk[3] and blk[0] in SPRITE_QUADRANTS})
        print(f"gdn: sprites for {c}: {q} of {len(SPRITE_QUADRANTS)} quadrants")
    print(f"gdn: {sum(len(sh.placed) for sh in sheets.values())} sprite images, "
          f"{len(cluts) + len(psx_pals)} palettes")

def pages_only():
    """No sprites: graphics bank PAGE_BANK holds the tile pages 12-15 alone (the
    mist and the tiles past F_*.BIN). The stage loads gfxBanks[0] when it
    starts and the room's bank when a room is entered; the pause menu reloads
    D_80097904, which the stage keeps at the pages' bank."""
    assert PAGE_BLOCKS, "backdrop() runs first"
    path = os.path.join(OUT, "graphics_banks.json")
    gb = json.load(open(path))
    gb["banks"].append({"kind": 4, "entries": [
        {"x": vy, "y": vx, "width": 128, "height": 128, "name": name}
        for vx, vy, name, _ in PAGE_BLOCKS]})
    gb["indices"][PAGE_BANK] = len(gb["banks"]) - 1
    json.dump(gb, open(path, "w"), indent=2)
    conf = open(CONFIG).read().rstrip("\n") + "\n"
    assert conf.endswith("          - [0x6D88, skip] # sprites\n"), \
        CONFIG + " must end with the sprites skip"
    for k, (_, _, name, data) in enumerate(PAGE_BLOCKS):
        write_indexed_png(os.path.join(OUT, name + ".png"), 128, 128,
                          [v for b in data for v in (b & 15, b >> 4)])
        conf += "          - [0x%X, cmpgfx, %s, 128, 128, 4]\n" % (0x6D90 + k * 4, name)
    os.makedirs(os.path.join(ROOT, "build/us"), exist_ok=True)
    open(BUILD_CONFIG, "w").write(conf)
    with open(os.path.join(SRC, "mod_sprite_gfx.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n// clang-format off\n")
        f.write("// tile pages 12-15, 4bpp 128x128 blocks compressed by the asset tool\n")
        for _, _, name, _ in PAGE_BLOCKS:
            f.write("u8 %s[] __attribute__((aligned(4))) = {\n" % name)
            f.write("#include \"gen/%s.h\"\n};\n" % name)
        f.write("// clang-format on\n")


def psx_sprites(sb, quadrants):
    """Copies the PSX_SPRITES animsets into the Garden's sprite banks; returns
    their art (4bpp, for the given quadrants) and palettes (RGB555)."""
    from PIL import Image
    out = []
    for name, stage, animset, art, pal, _, _, gdn_animset in PSX_SPRITES:
        path = os.path.join(ROOT, "assets/st", stage, "sprite_banks.json")
        if not os.path.exists(path):
            path = os.path.join(ROOT, "assets/st", stage, "us/sprite_banks.json")
        src = json.load(open(path))
        sb["banks"].append(src["banks"][src["indices"][animset]])
        sb["indices"][gdn_animset] = len(sb["banks"]) - 1
        blocks = []
        for a in psx_art_blocks(art):
            im = Image.open(os.path.join(ROOT, "assets/st", stage, a + ".png"))
            assert im.mode == "P" and im.size == (128, 128), (a, im.mode, im.size)
            px = list(im.getdata())
            blocks.append(bytes((px[i] & 15) | (px[i + 1] & 15) << 4
                                for i in range(0, len(px), 2)))
        out.append({"name": name, "quadrants": quadrants[name], "animset": gdn_animset,
                    "bank": len(sb["banks"]) - 1, "blocks": blocks,
                    "palettes": psx_palettes(stage, pal)})
    return out


# The first-visit area name, "Underground Garden", in the US names' lettering:
# composed from glyphs of "Underground Caverns" (NO4) and "Marble Gallery"
# (NO0). A name image is 4bpp grey levels, drawn by src/st/e_stage_name_us.h
# as two 116x40 halves: rows 0-40 x 4-120, then rows 64-104 x 8-124.
STAGE_NAME_ROOMS = ("0W", "0E")  # loaded with room 0, where the popup is
STAGE_NAME_SOURCES = {"no4": "assets/st/no4/D_80185400.png",
                      "no0": "assets/st/no0/stage_title_no0.png"}
# (source, the glyph's left x in that name's strip, x in the new strip);
# glyphs with the same new x as source x keep the source's spacing
STAGE_NAME_GLYPHS = [
    ("no4", x, x) for x in (4, 23, 37, 49, 59, 66, 81, 91, 102, 115, 129)  # Underground
] + [
    ("no0", 121, 152), ("no0", 139, 170),  # Ga (as in "Gallery")
    ("no0", 181, 184),                     # r
    ("no4", 37, 196), ("no4", 49, 208),    # de (as in "Underground")
    ("no4", 23, 218),                      # n
]


def name_strip(path):
    from PIL import Image
    px = Image.open(os.path.join(ROOT, path)).load()
    return [[px[4 + x, y] if x < 116 else px[8 + x - 116, 64 + y] for x in range(232)]
            for y in range(40)]


def glyph_masks(strip):
    """{left x: set of (x, y)} of the strip's connected shapes (ink > 1),
    each grown by a pixel to keep its faint edge"""
    seen, out = set(), {}
    for x in range(232):
        for y in range(40):
            if strip[y][x] > 1 and (x, y) not in seen:
                todo, pts = [(x, y)], set()
                seen.add((x, y))
                while todo:
                    a, b = todo.pop()
                    pts.add((a, b))
                    for c in (a - 1, a, a + 1):
                        for d in (b - 1, b, b + 1):
                            if 0 <= c < 232 and 0 <= d < 40 and (c, d) not in seen \
                                    and strip[d][c] > 1:
                                seen.add((c, d))
                                todo.append((c, d))
                if len(pts) > 3:
                    grown = {(c, d) for a, b in pts for c in (a - 1, a, a + 1)
                             for d in (b - 1, b, b + 1) if 0 <= c < 232 and 0 <= d < 40}
                    out[min(a for a, _ in pts)] = grown
    return out


def stage_name_pixels():
    """the 128x128 4bpp name image, as packed bytes (low nibble first)"""
    strips = {k: name_strip(p) for k, p in STAGE_NAME_SOURCES.items()}
    masks = {k: glyph_masks(s) for k, s in strips.items()}
    new = [[0] * 232 for _ in range(40)]
    for src, sx, nx in STAGE_NAME_GLYPHS:
        mask = masks[src][sx]
        for x, y in mask:
            v = strips[src][y][x]
            tx = x - sx + nx
            if 0 <= tx < 232 and v > new[y][tx]:
                new[y][tx] = v
    img = [[0] * 128 for _ in range(128)]
    for y in range(40):
        for x in range(116):
            img[y][4 + x] = new[y][x]
            img[64 + y][8 + x] = new[y][116 + x]
    return bytes(img[y][x] | img[y][x + 1] << 4 for y in range(128) for x in range(0, 128, 2))


def saturn_entities(room, ids):
    """The Saturn room's layout records whose entity id is in ids (Saturn id
    -> PSX enum name), as they are but for x (4/5)."""
    prg = open(PRG, "rb").read()
    base = STAGE_BASE
    lists = struct.unpack_from(">I", prg, 8)[0] - base
    p = struct.unpack_from(">I", prg, lists + 4 * (room + 1))[0] - base
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


def fg_priority(room):
    """PSX zPriority for a room's foreground, from the Saturn's per-room layer
    priorities (PRG +0x24: 12-byte records, NBG0-3 then the 8 sprite priority
    groups; tools/saturn/tile_dumper.py). The Garden's foreground is NBG1 and
    the characters are sprite group 0; the Saturn draws a sprite in front of a
    layer of equal priority. In front of the characters: 0xA0 (retail stages'
    foreground; Alucard is at 0x94), e.g. room 0's small flowers on the ground.
    Behind them: 0x60, e.g. the plants in the greenhouse rooms."""
    prg = open(PRG, "rb").read()
    table = struct.unpack_from(">I", prg, 0x24)[0] - STAGE_BASE
    rec = prg[table + room * 12:table + room * 12 + 12]
    return 0xA0 if rec[4] < rec[1] else 0x60


def fte1_off(X, Y):
    """F_*.BIN file offset of VRAM halfword (0x200 + X, Y)."""
    blk = (X // 64) << 2 | (Y // 128) << 1 | (X % 64) // 32
    return blk * 0x2000 + ((Y % 128) * 32 + X % 32) * 2


def vram_put(buf, width_hw, x, y, value):
    """4bpp pixel (x, y) of a little-endian halfword image width_hw wide."""
    o = (y * width_hw + x // 4) * 2
    hw = buf[o] | buf[o + 1] << 8
    shift = (x % 4) * 4
    hw = (hw & ~(15 << shift)) | (value << shift)
    buf[o], buf[o + 1] = hw & 0xFF, hw >> 8


def conv_slot(page, index):
    """Converter slot of (page, index): pages 0-3 hold 15 rows (palettes below)."""
    return sum(240 if p < 4 else 256 for p in range(page)) + (index >> 4) * 16 + (index & 15)


def repack(td, n_tiles, conv_layers):
    graphics = open(os.path.join(WORK, "graphics.bin"), "rb").read()
    pal_raw = open(os.path.join(WORK, "palettes.bin"), "rb").read()
    palettes = [struct.unpack_from("<16H", pal_raw, i) for i in range(0, len(pal_raw), 32)]
    slot = [conv_slot(td["tiles"][t], td["pages"][t]) for t in range(n_tiles)]

    def rgb(c):
        return ((c & 31) * 8, ((c >> 5) & 31) * 8, ((c >> 10) & 31) * 8)

    def render(t):
        g = graphics[slot[t] * 256:(slot[t] + 1) * 256]
        p = palettes[td["cluts"][t]]
        return [None if i == 0 else rgb(p[i]) for i in g]

    # tiles the Garden's own rooms use (the shared-tileset rooms don't)
    layouts = {}
    for i, cl in enumerate(conv_layers):
        if i in SHARED_ROOMS:
            continue
        for part in ("fg", "bg"):
            path = os.path.join(WORK, cl[part]["data"] + ".tilelayout.bin")
            d = open(path, "rb").read()
            layouts[path] = (part, list(struct.unpack("<%dH" % (len(d) // 2), d)))
    uses = {}
    fg = set()
    for part, tm in layouts.values():
        for t in tm:
            if t:
                uses[t] = uses.get(t, 0) + 1
                if part == "fg":
                    fg.add(t)
    used = set(uses)

    f_slots = [(p, r * 16 + c) for p in range(8) for r in range(15 if p < 4 else 16)
               for c in range(16)][1:]                        # (0, 0) is the empty graphic
    x_slots = [(p, i) for p in EXTRA_PAGES for i in range(256)]
    cap = len(f_slots) + len(x_slots)

    # merge near-identical background tiles until the graphics fit
    remap = {}
    refs = {}
    for t in used:
        refs[slot[t]] = refs.get(slot[t], 0) + 1
    needed = lambda: sum(1 for s_, n in refs.items() if n and s_ != 0)
    px_cache = {}

    def px(t):
        if t not in px_cache:
            px_cache[t] = render(t)
        return px_cache[t]

    def cost(a, b):
        e = 0
        for p, q in zip(px(a), px(b)):
            if (p is None) != (q is None):
                e += 3 * 255 * 255
            elif p is not None:
                e += (p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + (p[2] - q[2]) ** 2
        return e / 256

    def signature(t, shift):
        out = []
        pix = px(t)
        for by in range(4):
            for bx in range(4):
                acc = [0, 0, 0, 0]
                for y in range(4):
                    for x in range(4):
                        p = pix[(by * 4 + y) * 16 + bx * 4 + x]
                        if p is not None:
                            acc[0] += p[0]; acc[1] += p[1]; acc[2] += p[2]; acc[3] += 1
                n = acc[3] or 1
                out.append((acc[0] // n >> shift, acc[1] // n >> shift, acc[2] // n >> shift,
                            acc[3] >> 2))
        return tuple(out)

    merged_costs = []
    targets = set()  # tiles other tiles were merged into
    shift = 4
    while needed() > cap:
        assert shift <= 8, "Garden tiles don't fit even merging coarsely"
        buckets = {}
        for t in used:
            if t not in remap:
                buckets.setdefault(signature(t, shift), []).append(t)
        cands = []
        for group in buckets.values():
            if len(group) < 2:
                continue
            rep = max(group, key=lambda t: (t in fg, uses[t]))
            for a in group:
                if a != rep and a not in fg:
                    # repeated tiles form patterns (the curtains under the
                    # bridge), so they weigh more than once per use
                    cands.append((cost(a, rep) * uses[a] ** 2, a, rep))
        cands.sort()
        for c, a, rep in cands:
            if needed() <= cap:
                break
            if a in remap or rep in remap or a in targets or refs[slot[a]] != 1:
                continue      # keep graphics other tiles share; never chain
            # (a target merged away in a later, coarser pass left the tiles
            # merged into it pointing at a dropped, empty tile: holes in the
            # background, e.g. a pipe junction in both Gardens' room 3)
            remap[a] = rep
            targets.add(rep)
            refs[slot[a]] -= 1
            merged_costs.append(c)
        shift += 1
    kept = sorted(t for t in used if t not in remap)
    for path, (part, tm) in layouts.items():
        new = [remap.get(t, t) for t in tm]
        open(path, "wb").write(struct.pack("<%dH" % len(new), *new))

    # graphics into slots (foreground first, so it stays in F_TE1), palettes into ids
    order = sorted({slot[t] for t in kept} - {0},
                   key=lambda s_: (not any(slot[t] == s_ for t in fg), s_))
    place = {0: (0, 0)}
    for s_, dst in zip(order, f_slots + x_slots):
        place[s_] = dst
    assert len(order) <= cap
    free_cluts = [k for k in range(1, 256) if k not in RESERVED_CLUTS]
    pals = sorted({td["cluts"][t] for t in kept} - {0})
    assert len(pals) <= len(free_cluts), f"{len(pals)} palettes, {len(free_cluts)} slots"
    clut_of = {0: 0}
    clut_of.update(zip(pals, free_cluts))

    fte1 = bytearray(0x40000)
    vram4 = bytearray(256 * 2 * 256)            # pages 12-15, 64 halfwords each
    for s_, (page, index) in place.items():
        g = graphics[s_ * 256:(s_ + 1) * 256]
        u, v = (index & 15) * 16, index & 0xF0
        for y in range(16):
            for x in range(16):
                val = g[y * 16 + x]
                if page < 8:
                    X, Y = page * 64 + (u + x) // 4, v + y
                    o = fte1_off(X, Y)
                    hw = fte1[o] | fte1[o + 1] << 8
                    sh = ((u + x) % 4) * 4
                    hw = (hw & ~(15 << sh)) | (val << sh)
                    fte1[o], fte1[o + 1] = hw & 0xFF, hw >> 8
                else:
                    vram_put(vram4, 256, (page - 12) * 256 + u + x, v + y, val)
    for old, new in clut_of.items():
        for j, c in enumerate(palettes[old]):
            o = fte1_off(16 * (new % 16) + j, 0xF0 + new // 16)
            fte1[o], fte1[o + 1] = c & 0xFF, c >> 8
    for t in range(n_tiles):
        if t in remap or t not in used:
            td["tiles"][t] = td["pages"][t] = td["cluts"][t] = 0
            continue
        td["tiles"][t], td["pages"][t] = place[slot[t]]
        td["cluts"][t] = clut_of[td["cluts"][t]]
    in_f = sum(1 for s_ in order if place[s_][0] < 8)
    print(f"gdn: {len(order)} tile graphics ({in_f} in F_TE1, {len(order) - in_f} in pages "
          f"{EXTRA_PAGES[0]}-{EXTRA_PAGES[-1]}), {len(pals)} palettes, {len(remap)} background "
          f"tiles merged (cost mean {sum(merged_costs) / max(1, len(merged_costs)):.0f}, "
          f"max {max(merged_costs, default=0):.0f})")
    return fte1, vram4


def shimmer(vram4, fte1):
    """Decode the Saturn shimmer strips (sprite bank records {w/2, h/2}, 4bpp
    pixels packed one after another in STAGE_15.CHR, as the bank walk in
    mods/sotn-ultimate/tools/port_saturn_food.py), narrow them, put them in page 12 rows
    240-255 and their palette in SHIMMER_CLUT. Returns the segments per frame
    as (u, v, width)."""
    prg = open(PRG, "rb").read()
    zero = open(os.path.join(SAT, "0.BIN"), "rb").read()
    chr_ = open(CHR, "rb").read()
    zb, lb = 0x06004080, STAGE_BASE

    def res(a):
        if lb <= a < lb + len(prg):
            return prg, a - lb
        if zb <= a < zb + len(zero):
            return zero, a - zb
        return None

    u32 = lambda d, o: struct.unpack_from(">I", d, o)[0]
    tab = u32(prg, 4) - lb
    cursor, seen, bank = 0, set(), None
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
        if a == SHIMMER_BANK:
            bank = (recs, pa)
    assert bank and len(bank[0]) == 12, "shimmer bank not found"
    recs, pa = bank
    pd, po = res(pa)
    pal = struct.unpack_from(">16H", pd, po + 2)
    SHIMMER_PALETTE.clear()
    for j, c in enumerate(pal):
        c = 0 if j == 0 else (c & 0x7FFF) or 0x8000
        if SHIMMER_SPRITE_PAL:
            SHIMMER_PALETTE.append(c)
            continue
        o = fte1_off(16 * (SHIMMER_CLUT % 16) + j, 0xF0 + SHIMMER_CLUT // 16)
        fte1[o], fte1[o + 1] = c & 0xFF, c >> 8

    def strip(rec):
        w, h, off = rec
        assert h == 2, rec
        data = chr_[off:off + w * h // 2]
        px = [v for b in data for v in (b >> 4, b & 15)]
        nw = w * 4 // 5
        rows = [[px[y * w + x * 5 // 4] for x in range(nw)] for y in range(h)]
        if SHIMMER_FLIP:
            rows = [row[::-1] for row in rows[::-1]]
        return rows

    out = []
    for f in range(3):
        strips = [strip(recs[f * 4 + i]) for i in range(4)]
        # one strip per 2px band, in the mist's quadrant under the mist
        place = {}
        for i in range(4):
            u, v = 0, MIST_H + f * 8 + i * 2
            assert v + 2 <= 128 and len(strips[i][0]) <= 128
            for y, row in enumerate(strips[i]):
                for x, val in enumerate(row):
                    vram_put(vram4, 256, u + x, v + y, val)
            place[i] = (u, v, len(strips[i][0]))
        out.append([(place[i][0], place[i][1],
                     place[i][2] if SHIMMER_FLIP else min(place[i][2], w * 4 // 5))
                    for i, w in SHIMMER_SEGMENTS])
    return out


def read_png_rgba(path):
    """8-bit RGBA, non-interlaced PNG (the converter's previews)."""
    d = open(path, "rb").read()
    p, idat = 8, b""
    while p < len(d):
        n, t = struct.unpack(">I4s", d[p:p + 8])
        if t == b"IHDR":
            w, h, depth, ctype, _, _, inter = struct.unpack(">IIBBBBB", d[p + 8:p + 21])
            assert (depth, ctype, inter) == (8, 6, 0), path
        elif t == b"IDAT":
            idat += d[p + 8:p + 8 + n]
        p += 12 + n
    raw = zlib.decompress(idat)
    stride, bpp = w * 4, 4
    out, prev, o = bytearray(), bytearray(stride), 0
    for _ in range(h):
        f, line = raw[o], bytearray(raw[o + 1:o + 1 + stride])
        o += 1 + stride
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 0xFF
            elif f == 2:
                line[i] = (line[i] + b) & 0xFF
            elif f == 3:
                line[i] = (line[i] + (a + b) // 2) & 0xFF
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else b if pb <= pc else c
                line[i] = (line[i] + pr) & 0xFF
        out += line
        prev = line
    return w, h, bytes(out)


def quantize16(colors):
    """k-means over the distinct colours (weighted), 16 RGB centres."""
    from collections import Counter
    cnt = Counter(colors)
    pts = list(cnt.items())
    pts.sort(key=lambda kv: -kv[1])
    # start from colours spread by luminance
    by_lum = sorted(cnt, key=lambda c: c[0] * 3 + c[1] * 6 + c[2])
    centres = [by_lum[i * (len(by_lum) - 1) // 15] for i in range(16)]
    for _ in range(12):
        acc = [[0, 0, 0, 0] for _ in centres]
        for c, n in pts:
            j = min(range(16), key=lambda j: sum((c[k] - centres[j][k]) ** 2 for k in range(3)))
            a = acc[j]
            a[0] += c[0] * n; a[1] += c[1] * n; a[2] += c[2] * n; a[3] += n
        centres = [(a[0] // a[3], a[1] // a[3], a[2] // a[3]) if a[3] else centres[j]
                   for j, a in enumerate(acc)]
    lut = {c: min(range(16), key=lambda j: sum((c[k] - centres[j][k]) ** 2 for k in range(3)))
           for c in cnt}
    return centres, lut


# the mist's size in page 12 (its top-left quadrant; the shimmer below it)
MIST_W, MIST_H = 128, 104
# tile pages 12-15 (the mist, the shimmer and the extra Garden tiles) as
# (VRAM x, VRAM y, array name, 4bpp 128x128) blocks, and the stage graphics
# bank they are loaded with (gfxBanks index; see CONTEXT_BANKS)
PAGE_BLOCKS = []
PAGE_BANK = 7


def backdrop(td, n_tiles, fte1, vram4):
    w, h, rgba = read_png_rgba(os.path.join(WORK, "shared_backdrop.preview.png"))
    assert (w, h) == (256, 256)
    pix = [tuple(rgba[i:i + 3]) for i in range(0, len(rgba), 4)]
    centres, lut = quantize16(pix)
    pal = []
    for r, g, b in centres:
        c = (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10
        pal.append(c or 0x8000)  # 0x0000 would be transparent
    k = MIST_CLUT
    for j, c in enumerate(pal):
        o = fte1_off(16 * (k % 16) + j, 0xF0 + k // 16)
        fte1[o], fte1[o + 1] = c & 0xFF, c >> 8
    # stored at MIST_W x MIST_H (a quadrant's top), drawn stretched over the
    # screen's 256 x 240: the page's other three quadrants are sprite space
    # (SPRITE_QUADRANTS; room 0 has more enemies than tpages 18-19 hold)
    for y in range(256):
        for x in range(256):
            vram_put(vram4, 256, x, y, 0)
    for y in range(MIST_H):
        y0, y1 = y * 240 // MIST_H, max(y * 240 // MIST_H + 1, (y + 1) * 240 // MIST_H)
        y0, y1 = y0 + BACKDROP_ROW0, y1 + BACKDROP_ROW0
        for x in range(MIST_W):
            box = [pix[sy * 256 + sx] for sy in range(y0, y1) for sx in (2 * x, 2 * x + 1)]
            avg = tuple(sum(c[k] for c in box) // len(box) for k in range(3))
            j = min(range(16), key=lambda j: sum((avg[k] - centres[j][k]) ** 2 for k in range(3)))
            vram_put(vram4, 256, x, y, j)
    segs = shimmer(vram4, fte1) if SHIMMER_BANK else None
    # the pages go to VRAM as 16 compressed 128x128 blocks of a graphics bank
    # (sprites() writes it), not as one raw image in the overlay: 128 KB less
    PAGE_BLOCKS.clear()
    for by in range(2):
        for bx in range(8):
            if bx < 2 and (bx, by) != (0, 0):
                continue  # page 12 beyond the mist's quadrant: sprite space
            data = b"".join(bytes(vram4[(by * 128 + y) * 512 + bx * 64:
                                        (by * 128 + y) * 512 + bx * 64 + 64])
                            for y in range(128))
            PAGE_BLOCKS.append((BACKDROP_VRAM[0] + bx * 32, BACKDROP_VRAM[1] + by * 128,
                                "g_ModGdnPage%d_%d" % (by, bx), data))
    with open(os.path.join(SRC, "mod_backdrop.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n// clang-format off\n")
        f.write("// tile pages 12 (the mist) to 15 (Garden tiles), VRAM x 256-511, y 256-511\n")
        f.write("#define MOD_BACKDROP_VRAM_X %d\n#define MOD_BACKDROP_VRAM_Y %d\n" % BACKDROP_VRAM)
        f.write("#define MOD_BACKDROP_VRAM_W %d\n" % (64 * (1 + len(EXTRA_PAGES))))
        f.write("#define MOD_BACKDROP_TPAGE %d\n#define MOD_BACKDROP_CLUT %d\n"
                % (BACKDROP_TPAGE, k))
        f.write("// stored at half resolution, drawn stretched over the screen\n")
        f.write("#define MOD_BACKDROP_W %d\n#define MOD_BACKDROP_H %d\n" % (MIST_W, MIST_H))
        f.write("// the pages' compressed graphics bank (PAGE_BANK)\n")
        f.write("#define MOD_BACKDROP_GFX ANIMSET_OVL(%d)\n// clang-format on\n" % PAGE_BANK)
    # the shimmer's tables, for the water entity (mods/sotn-ultimate/src/garden/gdn/e_room_fg.c); the
    # texture itself is uploaded with the mist's (PAGE_BLOCKS above)
    if segs is None:
        print(f"gdn: red mist backdrop, palette {k}, tpage {BACKDROP_TPAGE}")
        return n_tiles
    with open(os.path.join(SRC, "mod_shimmer.h"), "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n// clang-format off\n")
        f.write("#define MOD_SHIMMER_TPAGE %d\n#define MOD_SHIMMER_CLUT %s\n"
                % (BACKDROP_TPAGE, "MOD_SHIMMER_PAL" if SHIMMER_SPRITE_PAL
                   else SHIMMER_CLUT))
        f.write("// per frame, 5 segments {u, v, width}, 2px tall%s\n"
                % (", stored rotated 180 degrees" if SHIMMER_FLIP else ""))
        f.write("static const u8 g_ModShimmerSegs[3][%d][3] = {\n" % len(SHIMMER_SEGMENTS))
        for frame in segs:
            f.write("    {" + ", ".join("{%d, %d, %d}" % s_ for s_ in frame) + "},\n")
        f.write("};\n// clang-format on\n")
    print(f"gdn: red mist backdrop, palette {k}, tpage {BACKDROP_TPAGE}")
    return n_tiles


if __name__ == "__main__":
    main()
