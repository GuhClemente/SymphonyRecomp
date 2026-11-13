#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: build the Hell Garden's stage assets (stage RGDN).
#
# The Hell Garden is the Saturn's RSTAGE15, the inverted castle's Underground
# Garden: the same rooms rotated 180 degrees (cells mirrored, 63-x and 63-y,
# same order), the same art (and the red mist behind room 0, rotated too).
# Same pipeline as the Garden: this runs mods/sotn-ultimate/tools/gdn_assets.py with the Hell
# Garden's values (the converter's RSTAGE15 profile) and writes
# assets/st/rgdn, the mods/sotn-ultimate/src/garden/rgdn headers and build/us/F_TE3.BIN: the Hell
# Garden takes the unused TE3 slot on the disc (mods/sotn-ultimate/tools/build_rgdn.sh).
# Stage id MOD_STAGE_RGDN (= MOD_STAGE_GDN | STAGE_INVERTEDCASTLE_FLAG).
#
# Rooms (as the Saturn's RSTAGE15 room table):
#   0 (51,18)-(59,19)  1 (47,19)-(50,19)  2 (42,19)  3 (43,19)-(46,19)
#   4 (60,18) loading room, to the Reverse Warp Rooms (exit (61,18))
#   5 (60,19) loading room, to the Reverse Entrance shaft (exit (61,19))
#   6 (50,18) save room (door on its right, into room 0's top row)
# The teleports are the Garden's (one table serves both castles: the engine
# flips the stage id and the arrival point in the inverted castle). As in the
# Soul Prison, the Saturn's loading rooms are swapped against the mirror of
# the Garden's: (3,44) -> Castle Entrance would mirror to (60,19).
# The special rooms use the inverted castle's shared pieces: RWRP's loading
# tiles (rwrp_tiledef_rload) with the E_ROOM_FOREGROUND params 3 piece, and
# RNO4's right-door save room (rno4_tilemap_59, rno4_tiledef_rsave) with the
# params 4/5 pieces.
# Room 0's tiles past F_TE3 and the mist go to VRAM tile pages 12-15 as the
# Garden's (mods/sotn-ultimate/src/garden/rgdn/st_update.c).
# Objects: the Saturn's candles (the ram-head sconces, hanging: RSTAGE15's
# bank 16 is the Garden's art with every frame's parts flipped about the
# entity) with their glow and stands, the decorations (room 0's two statues,
# room 2's garden scene) and the Alucard Spear lying in room 2 (the Garden's
# boss room: the Saturn's Hell Garden has no boss there, only this item).
# No area-name popup (the Saturn's inverted stages have none). Enemies: the
# PS1 ones (Cave Troll, Bitterfly, Cloaked Knight, Blue Venus Weed). Not yet: the water on the ceiling (Saturn 0x15-0x17). The Skeleton
# Guardian, the Copper Armor and the Ifrit: their Saturn banks as the
# Garden's own enemies' (SPRITE_SETS, ANIMSET_OVL(8), (9) and (10)).
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gdn_assets as ga  # noqa: E402
from mod_work import work_dir  # noqa: E402

ROOT = ga.ROOT
SAT = ga.SAT

ga.OUT = os.path.join(ROOT, "assets/st/rgdn")
ga.SRC = os.path.join(ROOT, "mods/sotn-ultimate/src/garden/rgdn")
ga.WORK = work_dir("rgdn")
ga.PRG = os.path.join(SAT, "RSTAGE15.PRG")
ga.MAP = os.path.join(SAT, "RSTAGE15.MAP")
ga.CHR = os.path.join(SAT, "RSTAGE15.CHR")
ga.CONFIG = os.path.join(ROOT, "mods/sotn-ultimate/config/mod_rgdn_assets.us.yaml")
ga.BUILD_CONFIG = os.path.join(ROOT, "build/us/rgdn_assets.us.yaml")
ga.F_FILE = os.path.join(ROOT, "build/us/F_TE3.BIN")
# the Garden's PS1 stair / stone tiles (gdn_tile_swap.py), turned around
ga.TILE_SWAP_FLIP = True
# the tile pages 12-15 (the mist and the tiles past F_TE3) are gfxBanks[0],
# which the stage loads when it starts
ga.PAGE_BANK = 0
# The sprites, as the Garden's (gdn_assets.sprites): one graphics bank per
# room (gfxBanks 1-4, entityGfxId 2-5; 5 is WRP's stage title, kept) and the
# same with the tile pages (6-9) for the pause menu's reload
# (mods/sotn-ultimate/src/garden/rgdn/st_update.c). No zone streaming in room 0: of its 11
# quadrants the candles and statues take 1, the Cave Troll 3, the Bitterfly
# 2, the Copper Armor 1 and the Ifrit 4 (its images packed trimmed, see
# IFRIT_BANK): all 11. Room 1: candles 1 + Cloaked Knight 1 + Blue Venus Weed
# 2; room 3: candles 1 + Bitterfly 2 + Venus Weed 2 + Skeleton Guardian 2. If
# room 0 ever runs short, the Garden's west/east streaming (contexts
# "0W"/"0E", gdn_assets.zoned()) can come back.
ga.WITH_SPRITES = True
ga.CONTEXT_BANKS = {"0": 1, "1": 2, "2": 3, "3": 4}
ga.RELOAD_BANKS = {"0": 6, "1": 7, "2": 8, "3": 9}
ga.ROOM_CONTEXT = {0: "0", 1: "1", 2: "2", 3: "3"}
ga.SPRITE_GROUPS = {"candles": ("0", "1", "3"), "scene": ("2",),
                    "skguardian": ("3",), "copper": ("0",), "ifrit": ("0",)}
# the Ifrit's bank (24, 0x060F04E4, 5 palettes) as a block: its colours carry
# the STP bit (saturn_sprites.STP_BANKS) so that its fading afterimages and
# its death pillar's mesh can be drawn translucent; opaque drawing is
# unchanged
IFRIT_BANK = 0x060F04E4
ga.PALETTE_BLOCKS = {"IFRIT": ("ifrit", IFRIT_BANK)}
ga.saturn_sprites.STP_BANKS.add(IFRIT_BANK)
# packed trimmed to their content (saturn_sprites.TRIM_BANKS): whole, its 81
# frames take 5 quadrants; trimmed, 4. Image 70 (the fire pillar, frame 69)
# stays whole: its primitives stretch it whole, as the Saturn's
ga.saturn_sprites.TRIM_BANKS[IFRIT_BANK] = {70}
# The enemies that have PS1 code come from the inverted castle's stages that
# have them, as the Garden's PS1 enemies: (name, stage, animset there, art
# blocks (consecutive tilesets from unk5A), palettes ((first clut, count) of
# that stage's retail overlay), contexts, the first block's quadrant in its
# tpage (for primitives drawn from the art's tpage) or None, Hell Garden
# animset). The Cave Troll (RNO4 room 15's bank, PAL 0x227 + its second
# clut), the Bitterfly (RNZ0), the Cloaked Knight (RNZ1, its 5 palettes: the
# aura adds 1), the Blue Venus Weed (RNO3 room 5's bank, 41 palettes: the
# darts', the plant's and the spikes' cluts are fixed offsets; its leaves,
# stem and spikes are primitives from tpage + quadrants 0/1, so its blocks
# take quadrants 0 and 1 of one tpage)
ga.PSX_SPRITES = [
    ("CAVE_TROLL", "rno4", 4, ["D_80185E40", "D_80186D50", "D_801874E8"], (0x227, 2),
     ("0",), None, 4),
    ("BITTERFLY", "rnz0", 10, ["D_8018DFF0", "D_8018EF38"], (0x247, 5), ("0", "3"), None, 5),
    ("CLOAKED_KNIGHT", "rnz1", 6, ["D_8018AF60"], (0x210, 5), ("1",), None, 6),
    ("VENUS_WEED", "rno3", 7, ["D_8018C860", "D_8018D3EC"], (0x240, 41), ("1", "3"), 0, 7),
]
# the Venus Weed's spikes draw their frames' parts themselves (SPIKE_SPRITES)
ga.PSX_SPRITE_SYMBOLS = True
ga.BLEED_BANKS = ()
# the Castle Entrance's puff fire palette (MOD_PUFF_OPAQUE_OFFSET + 4), for
# the Ifrit's fireballs (the PS1's opaque puff in DRA's animset 14)
ga.PUFF_PALETTE = True
ga.STAGE_NAME_ROOMS = ()
# RSTAGE15's bank 16 (0x060ECC88, frames 0x060ECBCC; candle config
# 0x060EE440): the Garden's frame numbers, the sconce 0x26-0x29 and its empty
# stand 0x2A
RBANK16 = (0x060ECC88, 0x060ECBCC)
ga.SPRITE_SETS = [
    (2, [("candles",) + RBANK16 + (f,) for f in (0x26, 0x27, 0x28, 0x29, 0x2A)]),
    (3, "decorations"),
    # the Skeleton Guardian (Saturn 0x38-0x3B, bank 28 0x060F2E08, frames
    # 0x060F33B4, 2 palettes): 26 frames. Their knees/feet are at y = 0 and
    # the Saturn draws them 32 px below the entity (its floor sensor is
    # {0, 32}): frames 1-18 +32; the death pieces (19-26) as they are
    (8, [("skguardian", 0x060F2E08, 0x060F33B4, f, 32 if f <= 18 else 0)
         for f in range(1, 27)]),
    # the Copper Armor (Saturn 0x21/0x22, bank 23 0x060EF3F4, frames
    # 0x060F0240, 1 palette): 60 frames (53-60 its death pieces). Drawn at the
    # entity (its floor sensor is {0, 0}, the feet at y 0): as they are
    (9, [("copper", 0x060EF3F4, 0x060F0240, f, 0) for f in range(1, 61)]),
    # the Ifrit (Saturn 0x23-0x2B, bank 24 0x060F04E4, frames 0x060F1648, 5
    # palettes): 81 frames (1-14 its flames, 63-65 the fire ring, 69 the fire
    # pillar's image, which its primitives draw). The Saturn draws it at its
    # (bobbing) entity position: as they are
    (10, [None if f in (70, 81) else ("ifrit", IFRIT_BANK, 0x060F1648, f, 0)
          for f in range(1, 82)]),
]
# the decorations' variants (entity 0x11, 0x060EE478): 0, 1 the statues
# (with the candles), 2-5 room 2's scene (gdn_assets.DECORATION_GROUPS)
ga.DECORATIONS = (0x060EE478, 6)
# the glow records (0x060EE416) place the glow turned around, and the Saturn
# draws its image flipped both ways (060DC2A4: xor 0x30)
ga.CANDLE_GLOW = (RBANK16[0], 0x060EE416, 4)
ga.GLOW_FLIP = 3

# the Garden's teleports (MOD_TP_* in include/game.h)
TP_GDN_TO_NP3 = ga.TP_GDN_TO_NP3
TP_GDN_TO_WRP = ga.TP_GDN_TO_WRP
ga.SPECIAL_CELLS = {4: (60, 18), 5: (60, 19), 6: (50, 18)}
ga.LOADING_TPS = {4: TP_GDN_TO_WRP, 5: TP_GDN_TO_NP3}
ga.EXITS = [((61, 18), TP_GDN_TO_WRP), ((61, 19), TP_GDN_TO_NP3)]
ga.SHARED_ROOMS = {4: ("rwrp_tilemap_5", "rwrp_tiledef_rload", "rwrp"),
                   5: ("rwrp_tilemap_5", "rwrp_tiledef_rload", "rwrp"),
                   6: ("rno4_tilemap_59", "rno4_tiledef_rsave", "rno4")}
# the inverted castle's pieces (RWRP rooms 5-9, RNO4 rooms 31-37)
ga.LOADING_FOREGROUND = dict(ga.LOADING_FOREGROUND, params=3)
ga.SAVE_FOREGROUND = [
    {"x": 128, "y": 208, "id": "E_ROOM_FOREGROUND", "flags": 0, "slot": 0x80, "spawnId": 0,
     "params": 4},
    {"x": 128, "y": 64, "id": "E_ROOM_FOREGROUND", "flags": 0, "slot": 0x88, "spawnId": 0,
     "params": 5},
]
# RSTAGE15 list 1: (2870,97) slot 0x9E and (2870,353) slot 0x9D, both params
# 0x100 (opening to the right; x 4/5)
ga.RED_DOORS = [(0, 2296, 97, 0x9E, True), (0, 2296, 353, 0x9D, True)]
ga.BOSS_DOOR = False  # no boss: room 2 holds an item
# the ceiling water: a later step
ga.WATER = []
# the candles (all type 0: the Saturn's item candle, the Garden's type 1, is
# gone) and the Saturn objects placed as they are: the decorations and room
# 2's item (Saturn 0x0C (160,201) params 0: PrizeDrops[0], the Alucard Spear,
# mods/sotn-ultimate/src/garden/rgdn/d_prize_drops.c; collected bit 808)
ga.CANDLE_ROOMS = (0, 1, 3)
ga.SATURN_IDS = {0x11: "E_ID_11", 0x0C: "E_PERSISTENT_ITEM_DROP",
                 # the enemies with PS1 code (the decomp's RSTAGE15 names are
                 # partly wrong: 0x34, "CopperArmor", is the Cloaked Knight,
                 # TekiInit 0x10F)
                 0x1E: "E_CAVE_TROLL", 0x33: "E_BITTERFLY",
                 0x34: "E_CLOAKED_KNIGHT", 0x2C: "E_VENUS_WEED",
                 # Saturn-only, ported: the Skeleton Guardian (TekiInit 0x1A9;
                 # room 3's six, at slots 3 apart: spear and shield in the
                 # next two)
                 0x38: "E_MOD_SKGUARDIAN",
                 # the Copper Armor (TekiInit 0x191; its sword's hitbox in
                 # the next slot)
                 0x21: "E_MOD_COPPER_ARMOR",
                 # the Ifrit (TekiInit 0x192; its two flames in the next
                 # two slots)
                 0x23: "E_MOD_IFRIT"}
# The ceiling water (room 0; RSTAGE15 0x15-0x1C, the Garden's pool turned
# over: mods/sotn-ultimate/src/garden/rgdn/e_room_fg.c). The shimmer is RSTAGE15's bank 21
# (0x060ECEF0: the same strips and palette as STAGE_15's bank 22), drawn
# flipped both ways (stored rotated), in the mist's page under the mist as
# in the Garden.
ga.SHIMMER_BANK = 0x060ECEF0
ga.SHIMMER_FLIP = True
# RSTAGE15's tiles, narrowed, take 213 palettes (STAGE_15's 204), one more
# than the Garden leaves: the shimmer's tile palette slot goes to the tiles
# and the shimmer's palette goes after the sprite palettes instead
# (MOD_SHIMMER_PAL)
ga.RESERVED_CLUTS = ga.RESERVED_CLUTS - {ga.SHIMMER_CLUT}
ga.SHIMMER_SPRITE_PAL = True
# The Saturn's room 0 places each water entity at both ends (x 160 / 2720 ->
# 128 / 2176) on both rows, each kind in one slot (as the Garden): 0x15 the
# water body (slot 0x98, pool 0), 0x17 Alucard in the water (0x99, params
# 0x0100: one pool from pool 0), 0x16 the shimmer (0x9B, pool 0). (0x1D in
# slot 0x9A sets the VDP2 back colour, as STAGE_15's: not ported, as in the
# Garden.)
ga.WATER = [
    {"x": x, "y": y, "id": eid, "flags": 0, "slot": slot, "spawnId": 0, "params": params}
    for eid, slot, params in (("E_MOD_CEILING_WATER", 0x98, 0),
                              ("E_ALUCARD_WATER_EFFECT", 0x99, 0x0100),
                              ("E_MOD_CEILING_SHIMMER", 0x9B, 0))
    for x in (128, 2176) for y in (384, 128)]
# the mist, rotated: the backdrop's last 240 rows (the Garden shows its first)
ga.BACKDROP_ROW0 = 16

if __name__ == "__main__":
    ga.main()
