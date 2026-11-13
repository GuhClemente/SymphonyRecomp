// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rgdn.h"

#include "../../../../../src/st/e_misc.h"

// Saturn port: the Bitterfly (Saturn RSTAGE15 0x33, RNZ0's PS1 code)
#include "../../../../../src/st/e_bitterfly.h"

// Saturn port: the Copper Armor (Saturn RSTAGE15 0x21/0x22, Saturn-only; its
// converted bank 23 as ANIMSET_OVL(9), mods/sotn-ultimate/tools/rgdn_assets.py)
#define MOD_COPPER_ARMOR_ANIMSET ANIMSET_OVL(9)
#include "../../enemies/e_mod_copper_armor.h"

// Saturn port: the Ifrit (Saturn RSTAGE15 0x23-0x2B, Saturn-only; its
// converted bank 24 as ANIMSET_OVL(10),
// mods/sotn-ultimate/tools/rgdn_assets.py)
#define MOD_IFRIT_ANIMSET ANIMSET_OVL(10)
// its fireballs: the opaque puff with the fire palette (as the Garden's
// boss's death fire, mods/sotn-ultimate/tools/gdn_assets.py PUFF_PALETTE)
#define MOD_IFRIT_PUFF_PAL (MOD_PUFF_OPAQUE_OFFSET + 4)
#include "../../enemies/e_mod_ifrit.h"
