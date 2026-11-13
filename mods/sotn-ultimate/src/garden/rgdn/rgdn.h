// SPDX-License-Identifier: AGPL-3.0-or-later
#include <stage.h>

// Saturn port: Hell Garden (Saturn RSTAGE15, the inverted castle's
// Underground Garden), built from the WRP skeleton like
// mods/sotn-ultimate/src/garden/gdn (mods/sotn-ultimate/tools/build_rgdn.sh);
// stage id MOD_STAGE_RGDN. Rooms, tiles, the red mist, the red doors, the
// candles, the decorations and the Alucard Spear so far: the Garden's own
// entities (mods/sotn-ultimate/src/garden/gdn) are left out; the enemies that
// have PS1 code (Cave Troll, Bitterfly, Cloaked Knight, Blue Venus Weed); the
// Skeleton Guardian (Saturn-only, room 3), the Copper Armor and the Ifrit
// (Saturn-only, room 0); the ceiling water (room 0).
#define STAGE_IS_RGDN

// the Hell Garden's own sound bank
// (mods/sotn-ultimate/tools/gen_rgdn_sounds.py), for its Saturn-only enemies
#include "mod_rgdn_sfx.h"

// Saturn port: the Hell Garden's item, the Alucard Spear (a new hand item,
// src/st/e_collect.h), lies in room 2 (PrizeDrops[0], d_prize_drops.c)
#define MOD_NEW_HAND_DROPS
// the Saturn's candles (mods/sotn-ultimate/src/castle/mod_candle.h); none holds
// an item (RSTAGE15 has no item candle: its candles are all type 0)
#define MOD_SATURN_CANDLES
#define MOD_ITEM_CANDLE_TYPE 0xFF

enum Palettes {
    PAL_NONE = 0,
};

typedef enum EntityID {
    E_NONE,
    E_BREAKABLE,
    E_EXPLOSION,
    E_PRIZE_DROP,
    E_NUMERIC_DAMAGE,
    E_RED_DOOR,
    E_INTENSE_EXPLOSION,
    E_SOUL_STEAL_ORB,
    E_ROOM_FOREGROUND,
    E_STAGE_NAME_POPUP,
    E_EQUIP_ITEM_DROP,
    E_RELIC_ORB,
    E_PERSISTENT_ITEM_DROP,
    E_ENEMY_BLOOD,
    E_MESSAGE_BOX,
    E_DUMMY_0F,
    E_DUMMY_10,
    E_ID_11,
    E_ID_12,
    E_ID_13,
    E_EXPLOSION_VARIANTS,
    E_GREY_PUFF,
    // Saturn port: a broken candle's stand (Saturn entity 0x12)
    E_MOD_CANDLE_REMNANT,
    // the enemies with PS1 code (Saturn 0x1E, 0x33, 0x34, 0x2C), from the
    // inverted castle's stages that have them
    E_CAVE_TROLL, // ../e_cave_troll.h (RNO4)
    E_CAVE_TROLL_BEAM,
    E_CAVE_TROLL_GLOW,
    E_BITTERFLY,      // ../e_bitterfly.h (RNZ0)
    E_CLOAKED_KNIGHT, // ../e_cloaked_knight.h (RNZ1)
    E_CLOAKED_KNIGHT_CLOAK,
    E_CLOAKED_KNIGHT_SWORD,
    E_CLOAKED_KNIGHT_AURA,
    E_VENUS_WEED, // ../e_venus_weed.h (RNO3, the blue one)
    E_VENUS_WEED_FLOWER,
    E_VENUS_WEED_TENDRIL,
    E_VENUS_WEED_DART,
    E_VENUS_WEED_SPIKE,
    // the Saturn-only Skeleton Guardian (Saturn 0x38-0x3B,
    // ../gdn/e_mod_skeleton_guardian.h): spear in self + 1, shield in self + 2
    E_MOD_SKGUARDIAN,
    E_MOD_SKGUARDIAN_SPEAR,
    E_MOD_SKGUARDIAN_SHIELD,
    E_MOD_SKGUARDIAN_DEBRIS,
    // the Saturn-only Copper Armor (Saturn 0x21/0x22, e_mod_copper_armor.h):
    // its sword's hitbox in self + 1; its death pieces are E_MOD_COPPER_ARMOR
    E_MOD_COPPER_ARMOR,
    E_MOD_COPPER_ARMOR_HITBOX,
    // the Saturn-only Ifrit (Saturn 0x23-0x2B, e_mod_ifrit.h): its flames in
    // self + 1 and self + 2, the rest in the free slots
    E_MOD_IFRIT,
    E_MOD_IFRIT_FLAME,
    E_MOD_IFRIT_FLAME2,
    E_MOD_IFRIT_AFTERIMAGE,
    E_MOD_IFRIT_PUFF,
    E_MOD_IFRIT_RING,
    E_MOD_IFRIT_EMBERS,
    E_MOD_IFRIT_PILLAR,
    E_MOD_IFRIT_ASHES,
    // the ceiling water of room 0 (Saturn 0x15-0x1C, e_room_fg.c): Alucard
    // in the water and its splashes and drops are the PS1's inverted-castle
    // water (../water_effects_rev.h, as RNO3), the body and the shimmer the
    // Saturn's
    E_ALUCARD_WATER_EFFECT,
    E_SPLASH_WATER,
    E_SURFACING_WATER,
    E_SIDE_WATER_SPLASH,
    E_SMALL_WATER_DROP,
    E_WATER_DROP,
    E_MOD_CEILING_WATER,
    E_MOD_CEILING_SHIMMER,
} EntityID;

// Saturn port: the sprites' places and palettes
// (mods/sotn-ultimate/tools/rgdn_assets.py)
#include "mod_psx_sprites.h"
#define PAL_CAVE_TROLL MOD_CAVE_TROLL_PAL
#define PAL_BITTERFLY MOD_BITTERFLY_PAL
#define PAL_CLOAKED_KNIGHT MOD_CLOAKED_KNIGHT_PAL
#define PAL_VENUS_WEED MOD_VENUS_WEED_PAL

extern EInit g_EInitLockCamera;
extern EInit g_EInitObtainable;
extern EInit g_EInitParticle;
extern EInit g_EInitInteractable;
extern EInit g_EInitCommon;
