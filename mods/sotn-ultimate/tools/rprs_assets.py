#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: build the Soul Prison's stage assets (stage RPRS).
#
# The Soul Prison is the Saturn's RSTAGE16, the inverted castle's Cursed
# Prison: the same two rooms rotated 180 degrees (cells mirrored, 63-x and
# 63-y, same order) and recoloured. Tower room 0 (28,27)-(30,29), its red door
# at the bottom left (the Reverse Caverns side); corridor room 1
# (31,27)-(35,27), its red door on the right (the Black Marble Gallery side).
# Same pipeline as the Prison: this runs mods/sotn-ultimate/tools/prs_assets.py with the
# Soul Prison's values (the converter's RSTAGE16 profile) and writes
# assets/st/rprs, the mods/sotn-ultimate/src/prison/rprs headers and build/us/F_TE4.BIN: the Soul
# Prison takes the unused TE4 slot on the disc (mods/sotn-ultimate/tools/build_rprs.sh).
# Stage id MOD_STAGE_RPRS (= MOD_STAGE_PRS | STAGE_INVERTEDCASTLE_FLAG).
#
# Rooms (as the Saturn's RSTAGE16 room table):
#   0 (28,27)-(30,29) tower       1 (31,27)-(35,27) corridor
#   2 (27,29) loading room, to the Reverse Caverns (exit (26,29))
#   3 (36,27) loading room, to the Black Marble Gallery (exit (37,27))
#   4 (31,29) save room (door on its left, into the tower's bottom floor)
# The teleports are the Prison's (one table serves both castles: the engine
# flips the stage id and the arrival point in the inverted castle).
# The special rooms use the inverted castle's shared pieces: RWRP's loading
# tiles (rwrp_tiledef_rload) with the E_ROOM_FOREGROUND params 3 piece, and
# RNO4's left-door save room (rno4_tilemap_58, rno4_tiledef_rsave) with the
# params 4/5 pieces.
# Objects: the Saturn's candles (sconces in the tower, candelabras in the
# corridor, the item holders: the Library Card, the Opal and the Delicious
# Meal), their glow and stands, and the decorations (chained skeletons, hanged
# prisoners), from RSTAGE16's own bank 16 (the Prison's art turned around and
# recoloured: its frames flip the parts, the converter keeps the flips). No
# area-name popup (the Saturn's inverted stages have none). Enemies: the PS1 ones (Flying Zombie, Jack O'Bones, Nova Skeleton)
# and the Red Gargoyle, the Saturn-only Wight and Wraith.
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gdn_assets as ga  # noqa: E402
import prs_assets as pa  # noqa: E402
import wight_ps1 as wp  # noqa: E402
from mod_work import work_dir  # noqa: E402

ROOT = ga.ROOT
SAT = ga.SAT

pa.OUT = os.path.join(ROOT, "assets/st/rprs")
pa.SRC = os.path.join(ROOT, "mods/sotn-ultimate/src/prison/rprs")
pa.WORK = work_dir("rprs")
pa.PRG = os.path.join(SAT, "RSTAGE16.PRG")
pa.MAP = os.path.join(SAT, "RSTAGE16.MAP")
pa.CHR = os.path.join(SAT, "RSTAGE16.CHR")
pa.CONFIG = os.path.join(ROOT, "mods/sotn-ultimate/config/mod_rprs_assets.us.yaml")
pa.BUILD_CONFIG = os.path.join(ROOT, "build/us/rprs_assets.us.yaml")
pa.F_FILE = os.path.join(ROOT, "build/us/F_TE4.BIN")
# its own PS1-drawn stair tiles, in its own orientation (gdn_tile_swap.py)
pa.PS1_TILES = "_rprs"
# the sprites: one graphics bank per room (gfxBanks 1, 2; entityGfxId 2, 3),
# packed into the stage sprite pages (tpages 18-23, as the Prison: no backdrop
# or extra tile pages here either); the enemies' quadrants are left free
pa.WITH_SPRITES = True
pa.ROOM_BANKS = {0: 1, 1: 2}
pa.SPRITE_GROUPS = {"objects": (0, 1), "gargoyles": (0,), "wights": (0, 1),
                    "wraiths": (1,)}
# The Red Gargoyle (RSTAGE16 0x20-0x22, the Garden's code: params 2, the
# second colours; bank 21 0x060EDE90, frames 0x060EEFD0, 27 palettes) as
# ANIMSET_OVL(7)
pa.GARGOYLE = (0x060EDE90, 0x060EEFD0, 59)
# The Wight (RSTAGE16 0x15-0x17, Saturn-only: mods/sotn-ultimate/src/enemies/e_mod_wight.h):
# bank 18 0x060EA3BC (174 images, 4 palettes), frames 0x060ECA7C (100) as
# ANIMSET_OVL(8); the Saturn draws the Wight, its sword and the sword's
# trail 32 px below their entities (+0x200000): every frame +32. Frame 71
# (one image) is the spirit the death draws as a stretched polygon.
pa.WIGHT = (0x060EA3BC, 0x060ECA7C, 100)
# its sword trail turns 50% transparent from its 4th tick (the Saturn's VDP1
# mesh, sprite flag 0x80): STP colours (opaque drawing is unaffected)
ga.saturn_sprites.STP_BANKS.add(pa.WIGHT[0])
# The Wraith (RSTAGE16 0x18-0x1B, Saturn-only: mods/sotn-ultimate/src/enemies/e_mod_wraith.h),
# only in the corridor (room 1): bank 0x060ECFA0, frames 0x060ED788 (45 + the
# empty frame 0) as ANIMSET_OVL(9); the Saturn draws it and its afterimages
# 32 px below their (bobbing) height: every frame +32. Body and afterimages
# are opaque; the afterimages turn 50% transparent from their 8th tick (the
# Saturn's VDP1 mesh, sprite flag 0x80): STP colours
pa.WRAITH = (0x060ECFA0, 0x060ED788, 45)
ga.saturn_sprites.STP_BANKS.add(pa.WRAITH[0])
pa.ENEMY_SETS = [(8, "wights", pa.WIGHT, 32), (9, "wraiths", pa.WRAITH, 32)]
pa.TEX_FRAMES = {"WIGHT_SPIRIT": (8, 71)}
pa.PALETTE_BLOCKS = {"GARGOYLE": ("gargoyles", pa.GARGOYLE[0]),
                     "WIGHT": ("wights", pa.WIGHT[0])}
# the Wight's PS1-style spirit / glow (include/game.h MOD_WIGHT_PS1_SPIRIT,
# MOD_WIGHT_PS1_GLOW; mods/sotn-ultimate/tools/wight_ps1.py): their palettes after the rest
WIGHT_PS1 = wp.switches()
if WIGHT_PS1:
    def wight_ps1_pals(pals, sheets):
        first = sheets["wights"].blocks[pa.WIGHT[0]] - pa.SPRITE_CLUT_BASE
        return wp.palettes(pals[first:first + 5], WIGHT_PS1)
    pa.EXTRA_PALS = wight_ps1_pals
# The PS1 enemies the Saturn placed here, from the inverted castle's stages
# that have them (their art blocks, palettes from the retail overlay, animset):
# the Flying Zombie (RNO2 room 15's bank, ANIMSET_OVL(7)), Jack O'Bones and
# the Nova Skeleton (RNO3, ANIMSET_OVL(4) / (5)). The Nova Skeleton's laser
# draws primitives from its second block at tpage + u 128 (e_nova_skeleton.h):
# its two blocks go to quadrants 0 and 1 of one tpage (MOD_*_TPAGE).
pa.PSX_SPRITES = [
    ("FLYING_ZOMBIE", "rno2", 7, ["D_8018F740"], (0x21E, 2), (0,), 4),
    ("JACK_O_BONES", "rno3", 4, ["e_jacko"], (0x20C, 2), (0,), 5),
    ("NOVA_SKELETON", "rno3", 5, ["e_nova_skeleton", "e_nova_laser"], (0x214, 5), (0,), 6, 0),
]
# RSTAGE16's bank 16 (0x060E88F0, frames 0x060E8898): the Prison's frames with
# every part flipped both ways (turned around about the entity), so the same
# frame numbers (prs_assets.CANDLE_FRAMES); the candle configs at 0x060E99C8
pa.SAT_BANK16 = (0x060E88F0, 0x060E8898)
# the glow records (tall 0x060E99A0, short 0x060E9978) place the glow turned
# around, and the Saturn draws its image flipped both ways (060DC318: xor 0x30)
pa.CANDLE_GLOW = [(0x060E99A0, f) for f in range(4)] + [(0x060E9978, f) for f in range(4)]
pa.GLOW_FLIP = 3
# the decorations' variants (entity 0x11)
pa.DECORATIONS = (0x060E9A30, 6)
pa.STAGE_NAME_ROOMS = ()

# the Prison's teleports (MOD_TP_* in include/game.h)
TP_PRS_TO_NO4 = pa.TP_PRS_TO_NO4
TP_PRS_TO_NO0 = pa.TP_PRS_TO_NO0
pa.SHARED_ROOMS = {
    2: ("rwrp_tilemap_5", "rwrp_tiledef_rload", "rwrp", (27, 29), TP_PRS_TO_NO4),
    3: ("rwrp_tilemap_5", "rwrp_tiledef_rload", "rwrp", (36, 27), TP_PRS_TO_NO0),
    4: ("rno4_tilemap_58", "rno4_tiledef_rsave", "rno4", (31, 29), None),
}
pa.EXITS = [((26, 29), TP_PRS_TO_NO4), ((37, 27), TP_PRS_TO_NO0)]
# RSTAGE16 lists 1 and 2: (10,609) params 0 and (1590,97) params 0x100, slot
# 0x80 (x 4/5)
pa.RED_DOORS = [(0, 8, 609, 0x80, False), (1, 1272, 97, 0x80, True)]
# the inverted castle's pieces (RWRP rooms 5-9, RNO4 rooms 31-37)
pa.LOADING_FOREGROUND = dict(ga.LOADING_FOREGROUND, params=3)
pa.SAVE_FOREGROUND = [
    {"x": 128, "y": 208, "id": "E_ROOM_FOREGROUND", "flags": 0, "slot": 0x80, "spawnId": 0,
     "params": 4},
    {"x": 128, "y": 64, "id": "E_ROOM_FOREGROUND", "flags": 0, "slot": 0x88, "spawnId": 0,
     "params": 5},
]
# the Saturn objects placed as they are (prs_assets.saturn_entities): the
# candles (params type << 12 | prize; type 2 releases PrizeDrops[params &
# 0x1FF], mods/sotn-ultimate/src/prison/rprs/d_prize_drops.c) and the decorations, and the enemies
pa.SATURN_IDS = {0x01: "E_BREAKABLE", 0x11: "E_ID_11",
                 # the enemies with PS1 code (the decomp's RSTAGE16 names are
                 # partly wrong: 0x1C is the Flying Zombie, TekiInit 0x0F)
                 0x1C: "E_FLYING_ZOMBIE", 0x20: "E_MOD_GARGOYLE",
                 0x23: "E_JACK_O_BONES", 0x26: "E_NOVA_SKELETON",
                 # Saturn-only
                 0x15: "E_MOD_WIGHT", 0x18: "E_MOD_WRAITH"}

if __name__ == "__main__":
    pa.main()
