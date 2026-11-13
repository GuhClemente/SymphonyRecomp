// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gdn.h"
#define BG_BLOCK_NEEDS_SCALE
#include "../../../../../src/st/e_room_bg.h"

// Saturn port: the PS1 enemies in the Garden (Saturn STAGE_15 0x36 and 0x20)
extern EInit g_EInitThornweed;
extern EInit g_EInitCorpseweed;
extern EInit g_EInitCorpseweedProjectile;
#define CORPSEWEED_TPAGE MOD_THORNWEED_TPAGE
#define CORPSEWEED_PAL PAL_THORNWEED_CORPSEWEED
#define CORPSEWEED_PROJ_PAL (PAL_THORNWEED_CORPSEWEED + 3)
#include "../../../../../src/st/e_thornweed_corpseweed.h"

extern EInit g_EInitSlinger;
extern EInit g_EInitSlingerPieces;
extern EInit g_EInitSlingerRib;
#include "../../../../../src/st/e_slinger.h"

// Saturn port: the Red Gargoyle (Saturn STAGE_15 0x23-0x25)
#define MOD_GARGOYLE_ANIMSET ANIMSET_OVL(7)
#include "../../enemies/e_mod_gargoyle.h"

// Saturn port: the Skeleton Breeder and its Skeleton Beast (Saturn 0x40-0x43)
#define MOD_SKBREEDER_ANIMSET ANIMSET_OVL(8)
#include "../../enemies/e_mod_skeleton_breeder.h"

// Saturn port: the Water Leaper and its spawner (Saturn 0x34-0x35)
#define MOD_LEAPER_ANIMSET ANIMSET_OVL(9)
#include "../../enemies/e_mod_leaper.h"

// Saturn port: the Gardener, its thrown shears and its shears hitbox (Saturn
// 0x3D-0x3F)
#define MOD_GARDENER_ANIMSET ANIMSET_OVL(10)
#include "../../enemies/e_mod_gardener.h"

// Saturn port: the Human Face Tree, its faces, falling faces, blood, leaves
// and trunk base (Saturn 0x2E-0x33)
#define MOD_TREE_ANIMSET ANIMSET_OVL(11)
#include "e_mod_tree.h"

// Saturn port: the Man Eater and its burst pieces (Saturn 0x3B-0x3C)
#define MOD_MANEATER_ANIMSET ANIMSET_OVL(12)
#include "../../enemies/e_mod_maneater.h"

// Saturn port: the Skeleton Leader boss fight of room 2 (Saturn 0x44, 0x45,
// 0x26-0x2D), with the PS1's death puffs and Life Max Up (Saturn 0x46, 0x47)
#include "mod_boss_door.h"
#define MOD_BOSS_ANIMSET ANIMSET_OVL(13)
#define MOD_BOSS_DOOR_ANIMSET ANIMSET_OVL(14)
#define MOD_BOSS_DOOR_FRAME 1
#include "e_mod_boss.h"

// the puffs as in the Castle Entrance (params 3: the boss's fire); 1 and 2
// would use another stage's animset and are not used here
#define PAL_PUFF_OPAQUE_OFFSET MOD_PUFF_OPAQUE_OFFSET
#define PUFF_PARAMS_0                                                          \
    {                                                                          \
        ANIMSET_DRA(0xE), 0x0079, 0x0000, DRAW_TPAGE | DRAW_TPAGE2,            \
            puff_anim_one                                                      \
    }
#define PUFF_PARAMS_1 PUFF_PARAMS_0
#define PUFF_PARAMS_2 PUFF_PARAMS_0
#define PUFF_PARAMS_3                                                          \
    {                                                                          \
        ANIMSET_DRA(0xE), 0x0079, 0x0004, DRAW_TPAGE | DRAW_TPAGE2,            \
            puff_anim_one                                                      \
    }
#include "../../../../../src/st/e_explosion_puff_opaque.h"
#include "../../../../../src/st/e_life_up.h"
