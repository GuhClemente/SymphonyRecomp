// SPDX-License-Identifier: AGPL-3.0-or-later
#include <stage.h>

// Saturn port: Cursed Prison (Saturn STAGE_16), built from the WRP
// skeleton (mods/sotn-ultimate/tools/build_prs.sh); stage id MOD_STAGE_PRS.
#define STAGE_IS_PRS

// Saturn port: the Prison's items. Its item holders (candle type 2) release
// PrizeDrops[] (d_prize_drops.c), two of them new hand items
// (src/st/e_collect.h)
#define MOD_NEW_HAND_DROPS
// the Saturn's candles (mods/sotn-ultimate/src/castle/mod_candle.h); type 2
// holds a stage item
#define MOD_SATURN_CANDLES
#define MOD_ITEM_CANDLE_TYPE 2

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
    // the PS1 enemies (Saturn 0x1D-0x1F, 0x18-0x19, 0x1A-0x1C)
    E_SLINGER,
    E_SLINGER_THROWN_BONE,
    E_SLINGER_PIECES,
    E_BONE_ARCHER,
    E_BONE_ARCHER_ARROW,
    E_SPEAR_GUARD,
    E_SPEAR_GUARD_BLOCK,
    E_THROWN_SPEAR,
    // the Red Gargoyle (Saturn 0x21-0x23,
    // mods/sotn-ultimate/src/enemies/e_mod_gargoyle.h)
    E_MOD_GARGOYLE,
    E_GARGOYLE_DEBRIS,
    E_GARGOYLE_PEDESTAL,
    // the floor switch, sinking block and its dust (Saturn 0x15-0x17,
    // e_mod_switch.h)
    E_MOD_PRS_DUST,
    E_MOD_PRS_SWITCH,
    E_MOD_PRS_BLOCK,
    // the Specter (Saturn 0x20, e_mod_specter.h)
    E_MOD_SPECTER,
    // the Will-o'-Wisps (Saturn 0x28-0x2C, e_mod_wisp.h)
    E_MOD_WISP_SPAWNER,
    E_MOD_WISP,
    E_MOD_WISP_FLAME,
    E_MOD_WISP_AFTERIMAGE,
    E_MOD_WISP_SPARKS,
} EntityID;

// Saturn port: the sprites' places and palettes
// (mods/sotn-ultimate/tools/prs_assets.py)
#include "mod_psx_sprites.h"
#define PAL_SLINGER MOD_SLINGER_PAL
#define PAL_BONE_ARCHER MOD_BONE_ARCHER_PAL
#define PAL_SPEAR_GUARD MOD_SPEAR_GUARD_PAL
// the Prison's own sounds (its stage bank,
// mods/sotn-ultimate/tools/gen_prs_sounds.py)
#include "mod_prs_sfx.h"

extern EInit g_EInitLockCamera;
extern EInit g_EInitObtainable;
extern EInit g_EInitParticle;
extern EInit g_EInitInteractable;
extern EInit g_EInitCommon;
