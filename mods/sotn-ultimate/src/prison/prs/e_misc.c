// SPDX-License-Identifier: AGPL-3.0-or-later
#include "prs.h"

#include "../../../../../src/st/e_misc.h"

// Saturn port: the Bone Archer (Saturn STAGE_16 0x18-0x19), with its PS1 code
// (see e_room_bg.c)
extern EInit g_EInitBoneArcher;
extern EInit g_EInitBoneArcherArrow;
#include "../../../../../src/st/e_bone_archer.h"

// Saturn port: the floor switch, the sinking block and its dust (Saturn
// STAGE_16 0x15-0x17; flag g_CastleFlags[0xF9])
#include "mod_block_tile.h"
#define MOD_PRS_SWITCH_ANIMSET ANIMSET_OVL(12)
#include "e_mod_switch.h"
