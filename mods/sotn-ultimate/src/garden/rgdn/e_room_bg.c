// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rgdn.h"
#define BG_BLOCK_NEEDS_SCALE
#include "../../../../../src/st/e_room_bg.h"

// Saturn port: the Blue Venus Weed (Saturn RSTAGE15 0x2C, RNO3's PS1 code;
// its primitives from the art's tpage, its fixed cluts as RNO3's from the
// first of its 41 palettes)
#define BLUE
#define SPIKE_SPRITES MOD_VENUS_WEED_SPRITES
#define TENDRIL_COUNT 8
#define DART_CLUT_START (PAL_VENUS_WEED + 0x00)
#define DEATH_CLUT (PAL_VENUS_WEED + 0x06)
#define PLANT_CLUT (PAL_VENUS_WEED + 0x10)
#define SPIKE_CLUT_START (PAL_VENUS_WEED + 0x1A)
#define SPIKE_CLUT_END (PAL_VENUS_WEED + 0x28)
#define GROWTH_SPEED 2
#define VENUS_WEED_TPAGE MOD_VENUS_WEED_TPAGE
#include "../../../../../src/st/e_venus_weed.h"

// Saturn port: the Skeleton Guardian (Saturn RSTAGE15 0x38-0x3B; its death
// pieces are the Garden's Skeleton Breeder's code)
#define MOD_SKGUARDIAN_ANIMSET ANIMSET_OVL(8)
#include "../../enemies/e_mod_skeleton_guardian.h"
