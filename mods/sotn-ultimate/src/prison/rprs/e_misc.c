// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rprs.h"

#include "../../../../../src/st/e_misc.h"

// Saturn port: Jack O'Bones (Saturn RSTAGE16 0x23, RNO3's PS1 code)
#include "../../../../../src/st/e_jack_o_bones.h"

// Saturn port: the Wight (Saturn RSTAGE16 0x15-0x17, Saturn-only; its
// converted bank 18 as ANIMSET_OVL(8), mods/sotn-ultimate/tools/rprs_assets.py)
#define MOD_WIGHT_ANIMSET ANIMSET_OVL(8)
#include "../../enemies/e_mod_wight.h"

// Saturn port: the Wraith (Saturn RSTAGE16 0x18-0x1B, Saturn-only; its
// converted bank 0x060ECFA0 as ANIMSET_OVL(9),
// mods/sotn-ultimate/tools/rprs_assets.py)
#define MOD_WRAITH_ANIMSET ANIMSET_OVL(9)
#include "../../enemies/e_mod_wraith.h"
