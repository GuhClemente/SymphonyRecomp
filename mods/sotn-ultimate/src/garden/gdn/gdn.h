// SPDX-License-Identifier: AGPL-3.0-or-later
#include <stage.h>

// Saturn port: the Saturn-only music
// (mods/sotn-ultimate/tools/gen_saturn_music.py)
#include "../../../../../src/dra/mod_xa_music.h"
#define MOD_BOSS_MUSIC MU_MOD_SKELETON_LEADER
#define MOD_GDN_STAGE_MUSIC MU_MOD_UNDERGROUND_GARDEN
// the Garden's own sound bank (mods/sotn-ultimate/tools/gen_gdn_sounds.py)
#include "mod_gdn_sfx.h"
// the Saturn's Garden bank has no Corpseweed spit sound (0x7C4)
#define CORPSEWEED_NO_ATTACK_SFX

// Saturn port: Underground Garden (Saturn STAGE_15), built from the WRP
// skeleton (mods/sotn-ultimate/tools/build_gdn.sh); stage id MOD_STAGE_GDN.
#define STAGE_IS_GDN

// Saturn port: the Garden's items. Its candles drop new hand items
// (src/st/e_collect.h), and the special candle in room 0 releases prize 0,
// the Signed Pencil Board, as the Saturn's STAGE_15 PrizeDrops[0] (0x129).
#define MOD_NEW_HAND_DROPS
// the Saturn's candles (mods/sotn-ultimate/src/castle/mod_candle.h); type 1
// holds a stage item
#define MOD_SATURN_CANDLES
#define MOD_ITEM_CANDLE_TYPE 1
#define MOD_EXTRA_PRIZE_INDEX 0
#define MOD_EXTRA_PRIZE_DROP                                                   \
    (0x80 + MOD_DROP_HAND_BASE + ITEM_SIGNED_PENCIL_BOARD - NUM_HAND_ITEMS)

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
    // Saturn port: the water under room 0's bridge (Saturn STAGE_15 entities
    // 0x15-0x1B; mods/sotn-ultimate/src/garden/gdn/e_room_fg.c)
    E_ALUCARD_WATER_EFFECT,
    E_SPLASH_WATER,
    E_SURFACING_WATER,
    E_SIDE_WATER_SPLASH,
    E_SMALL_WATER_DROP,
    E_WATER_DROP,
    E_MOD_WATER_BODY,
    E_MOD_WATER_SURFACE,
    // Saturn port: the empty candle sconce
    // (mods/sotn-ultimate/src/garden/gdn/e_breakable.c)
    E_MOD_CANDLE_REMNANT,
    // Saturn port: the poison dripping from the greenhouse ceilings (Saturn
    // entities 0x1E/0x1F; mods/sotn-ultimate/src/garden/gdn/e_misc.c)
    E_MOD_DRIP_SPAWNER,
    E_MOD_DRIP,
    // Saturn port: the PS1 enemies in the Garden, with their PS1 code
    // (mods/sotn-ultimate/src/garden/gdn/e_room_bg.c); the Saturn's 0x36
    // (params: 0 Thornweed, 1 Corpseweed) and 0x20
    E_THORNWEED,
    E_CORPSEWEED,
    E_CORPSEWEED_PROJECTILE,
    E_SLINGER,
    E_SLINGER_THROWN_BONE,
    E_SLINGER_PIECES,
    // Saturn port: the Red Gargoyle, its debris and the pedestal it leaves
    // (Saturn 0x23-0x25; mods/sotn-ultimate/src/enemies/e_mod_gargoyle.h)
    E_MOD_GARGOYLE,
    E_GARGOYLE_DEBRIS,
    E_GARGOYLE_PEDESTAL,
    // Saturn port: the Skeleton Breeder, its weapon hitbox, its Skeleton Beast
    // and its death pieces (Saturn 0x40-0x43; e_mod_skeleton_breeder.h)
    E_MOD_SKBREEDER,
    E_MOD_SKBREEDER_WEAPON,
    E_MOD_SKBREEDER_BEAST,
    E_MOD_SKBREEDER_DEBRIS,
    // Saturn port: the Water Leaper and its spawner (Saturn 0x34-0x35;
    // e_mod_leaper.h)
    E_MOD_LEAPER,
    E_MOD_LEAPER_SPAWNER,
    // Saturn port: the Gardener, its thrown shears and its shears hitbox
    // (Saturn 0x3D-0x3F; e_mod_gardener.h)
    E_MOD_GARDENER,
    E_MOD_GARDENER_SHEARS,
    E_MOD_GARDENER_HITBOX,
    // Saturn port: the Human Face Tree (Saturn 0x2E-0x33; e_mod_tree.h)
    E_MOD_TREE,
    E_MOD_TREE_FACE,
    E_MOD_TREE_DROP,
    E_MOD_TREE_BLOOD,
    E_MOD_TREE_LEAF,
    E_MOD_TREE_BASE,
    // Saturn port: the Man Eater (Saturn 0x3B-0x3C; e_mod_maneater.h)
    E_MOD_MANEATER,
    E_MOD_MANEATER_DEBRIS,
    // Saturn port: the room-2 boss fight (Saturn 0x44, 0x45, 0x26-0x2D;
    // e_mod_boss.h), its death puffs and the Life Max Up (PS1 entities)
    E_MOD_BOSS_CONTROLLER,
    E_MOD_BOSS_DOOR,
    E_MOD_BOSS,
    E_MOD_BOSS_SWORD,
    E_MOD_BOSS_BONE,
    E_MOD_BOSS_BOMB,
    E_MOD_BOSS_GHOST,
    E_MOD_BOSS_DEBRIS,
    E_MOD_BOSS_BEAM,
    E_MOD_BOSS_WAVE,
    E_EXPLODE_PUFF_OPAQUE,
    E_LIFE_UP_SPAWN,
} EntityID;
#define E_EXPLOSION_PUFF_OPAQUE E_EXPLODE_PUFF_OPAQUE

// Saturn port: the Saturn draws the enemies in a sprite priority group above
// room 0's ground flowers (its per-room record: flowers NBG1 = 4, Alucard's
// group 3, the enemies' 4 and up), while Alucard stays behind them. The PSX
// foreground is at 0xA0 there (gdn_assets.py fg_priority), so the enemies
// are drawn just above it.
#define MOD_GDN_ENEMY_FRONT_Z 0xA8

// the PS1 enemies' sprites (mods/sotn-ultimate/tools/gdn_assets.py)
#include "mod_psx_sprites.h"
#define PAL_THORNWEED_CORPSEWEED MOD_THORNWEED_PAL
#define PAL_SLINGER MOD_SLINGER_PAL

extern EInit g_EInitLockCamera;
extern EInit g_EInitObtainable;
extern EInit g_EInitParticle;
extern EInit g_EInitInteractable;
extern EInit g_EInitCommon;
