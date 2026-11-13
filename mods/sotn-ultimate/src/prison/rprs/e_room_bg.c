// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rprs.h"
#define BG_BLOCK_NEEDS_SCALE
#include "../../../../../src/st/e_room_bg.h"

// Saturn port: the Red Gargoyle (Saturn RSTAGE16 0x20-0x22, params 2: the
// Garden's code, its converted RSTAGE16 bank 21)
#define MOD_GARGOYLE_ANIMSET ANIMSET_OVL(7)
#include "../../enemies/e_mod_gargoyle.h"
