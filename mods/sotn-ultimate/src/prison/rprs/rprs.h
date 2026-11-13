// SPDX-License-Identifier: AGPL-3.0-or-later
#include <stage.h>

// Saturn port: Soul Prison (Saturn RSTAGE16, the inverted castle's Cursed
// Prison), built from the WRP skeleton like mods/sotn-ultimate/src/prison/prs
// (mods/sotn-ultimate/tools/build_rprs.sh); stage id MOD_STAGE_RPRS. Rooms,
// tiles and red doors, candles, decorations and items, the enemies that have
// PS1 code and the Red Gargoyle and the Saturn-only Wight and Wraith.
#define STAGE_IS_RPRS

// the Soul Prison's own sound bank
// (mods/sotn-ultimate/tools/gen_rprs_sounds.py), for its Saturn-only enemies
#include "mod_rprs_sfx.h"

// Saturn port: the Soul Prison's items. Its item holders (candle type 2)
// release PrizeDrops[] (d_prize_drops.c), one of them a new hand item
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
    // the enemies with PS1 code (Saturn 0x1C, 0x23, 0x26), from the inverted
    // castle's stages that have them
    E_FLYING_ZOMBIE, // ../e_flying_zombie.h (RNO2)
    E_FLYING_ZOMBIE_TORSO,
    E_BLOOD_DRIPS, // ../e_bloody_zombie.h, MOD_BLOOD_ONLY
    E_BLOOD_SPLATTER,
    E_JACK_O_BONES, // ../e_jack_o_bones.h (RNO3)
    E_JACKO_DEATH_PARTS,
    E_JACKO_JACK,
    E_NOVA_SKELETON, // ../e_nova_skeleton.h (RNO3)
    E_NOVA_LASER,
    E_NOVA_PULSE,
    E_NOVA_DEATH_PARTS,
    // the Red Gargoyle (Saturn 0x20-0x22,
    // mods/sotn-ultimate/src/enemies/e_mod_gargoyle.h)
    E_MOD_GARGOYLE,
    E_GARGOYLE_DEBRIS,
    E_GARGOYLE_PEDESTAL,
    // the Wight (Saturn 0x15-0x17, e_mod_wight.h; Saturn-only)
    E_MOD_WIGHT,
    E_MOD_WIGHT_SWORD,
    E_MOD_WIGHT_TRAIL,
    // the Wraith (Saturn 0x18-0x1B, e_mod_wraith.h; Saturn-only)
    E_MOD_WRAITH,
    E_MOD_WRAITH_AFTERIMAGE,
    E_MOD_WRAITH_DRAIN,
    E_MOD_WRAITH_WISPS,
} EntityID;

// Saturn port: the sprites' places and palettes
// (mods/sotn-ultimate/tools/rprs_assets.py)
#include "mod_psx_sprites.h"
#define PAL_FLYING_ZOMBIE MOD_FLYING_ZOMBIE_PAL
#define PAL_JACK_O_BONES MOD_JACK_O_BONES_PAL
#define PAL_NOVA_SKELETON MOD_NOVA_SKELETON_PAL

extern EInit g_EInitLockCamera;
extern EInit g_EInitObtainable;
extern EInit g_EInitParticle;
extern EInit g_EInitInteractable;
extern EInit g_EInitCommon;
