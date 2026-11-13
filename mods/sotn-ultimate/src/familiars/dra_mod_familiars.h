// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/dra.h (at file scope, in place)
#if defined(MOD_SATURN)
// Saturn port: familiar stats, the JP-only familiars' included (5D5BC.c)
void ModGetFamiliarStats(s32 i, FamiliarStats* out);
void ModSetFamiliarStats(s32 i, FamiliarStats* in);
void ModCountFamiliarSummon(void);
#endif
#if defined(MOD_GODSPEED)
// Saturn port: God Speed Shoes sprint. Alucard reuses step 19
// (Player_FlameWhip), which has no Alucard handler on PSX.
#define GODSPEED_SPEED 0x2D000 // FIX(2.8125), Saturn sprint speed
#define GODSPEED_JUMP_FLAG                                                     \
    0x200 // set in g_Player.unk44 when jumping mid-sprint
// Frames left to double tap into a sprint (Saturn g_Player timer 16)
extern s32 g_GodspeedTapTimer;
// Lets sprinting Alucard do whatever a walking one can
#define IS_WALK_STEP(step) ((step) == Player_Walk || (step) == Player_FlameWhip)
#else
#define IS_WALK_STEP(step) ((step) == Player_Walk)
void GodspeedStartSprint(void);
void PlayerStepGodspeedSprint(void);
#endif
