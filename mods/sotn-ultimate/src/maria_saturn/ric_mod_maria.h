// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/ric/ric.h (at file scope, in place)
#if defined(MOD_MARIA_SAT)
// Saturn port: her Saturn sounds, MAR_SFX_*
// (mods/sotn-ultimate/tools/gen_mar_sounds.py). RIC_OR_MAR(r, m): Richter's
// sound r, hers m where Richter's code plays a different sound for her than his
// id's slot holds in her bank
#include "mar_sfx.h"
#define RIC_OR_MAR(r, m) (m)
// her holy water flames' palette (gen_mar_assets.py FLAME_PAL; the id is
// unused by Richter, Maria and DRA)
#define MAR_FLAME_PAL 0x13F
#else
#define RIC_OR_MAR(r, m) (r)
#endif
#if defined(MOD_MARIA_SAT)
// Saturn port: MAR.BIN, the Saturn Maria
// (mods/sotn-ultimate/tools/build_mar.sh)
void MarInit(void);
void MarDecodeSprites(void);
void MarSetDoubleJump(void);
// her attacks (mar_attacks.h, included by pl_blueprints.c)
s32 MarDoAttack(s32 charged);
void MarDoKick(void);
void MarSetBackdash(void);
// her buttons. The Saturn's C (kick) and R (backdash) first went to the PS1's
// Triangle and R1; players asked for Circle and Triangle, the PS1 Alucard's
// backdash button, keeping R1 for the backdash too
#define MAR_PAD_KICK PAD_CIRCLE
#define MAR_PAD_BACKDASH (PAD_TRIANGLE | PAD_R1)
void MarCharge(s32 allowed);
void MarSetDiveKick(s32 moving);
void MarLand(void);
void MarLand(void);
// her animations with no Richter counterpart (gen_mar_assets.py EXTRA_ANIMS)
extern AnimationFrame mar_anim_atk_air_down[];
extern AnimationFrame mar_anim_kick[];
extern AnimationFrame mar_anim_kick_crouch[];
extern AnimationFrame mar_anim_kick_air_fall[];
extern AnimationFrame mar_anim_kick_air_rise[];
extern AnimationFrame mar_anim_atk_end[];
extern AnimationFrame mar_anim_atk_end_b[];
extern AnimationFrame mar_anim_kick_end[];
extern AnimationFrame mar_anim_kick_end_b[];
extern AnimationFrame mar_anim_kick_crouch_end[];
extern AnimationFrame mar_anim_dive[];
extern AnimationFrame mar_anim_dive_down[];
extern AnimationFrame mar_anim_uppercut_diag[];
extern AnimationFrame mar_anim_stand_edge[];
extern AnimationFrame mar_anim_roll[];
extern AnimationFrame mar_anim_backdash[];
extern AnimationFrame mar_anim_backdash_end[];
extern AnimationFrame mar_anim_backdash_end_b[];
extern AnimationFrame mar_anim_throw[];
extern AnimationFrame mar_anim_throw_b[];
// her command specials (mar_specials.h)
void MarCheckSpecials(void);
void MarStepSpecial(void);
void MarChainAnim(void);
void MarBeastsPal(s32 on);
#define PL_S_MAR_SPECIAL 0x30 // 0x30-0x35, Saturn 060A9064
#endif
