// SPDX-License-Identifier: AGPL-3.0-or-later
#include "prs.h"
#define BG_BLOCK_NEEDS_SCALE
#include "../../../../../src/st/e_room_bg.h"

// Saturn port: the Prison's enemies (the stage objects list, WRP's, links no
// extra files). The PS1's own, with their PS1 code (Saturn STAGE_16 0x1D,
// 0x18, 0x1A: the same enemies; the Bone Archer is in e_misc.c, the Spear
// Guard in e_room_fg.c, as their tables share names)
extern EInit g_EInitSlinger;
extern EInit g_EInitSlingerPieces;
extern EInit g_EInitSlingerRib;
#include "../../../../../src/st/e_slinger.h"

// the Red Gargoyle (Saturn STAGE_16 0x21-0x23, the Garden's code)
#define MOD_GARGOYLE_ANIMSET ANIMSET_OVL(7)
#include "../../enemies/e_mod_gargoyle.h"
