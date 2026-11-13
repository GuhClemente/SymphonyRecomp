// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/cen/cen.h (at file scope, in place)
// Saturn port: the Saturn's Maria boss fight (mod_maria_boss.h, in
// e_maria.c), in the retail dummy slot 0x0F. Alucard only, as on the
// Saturn (whose fight has no other condition than MET_MARIA_IN_CEN unset).
// MOD_CENP: the build of BIN/CENP.BIN (mods/sotn-ultimate/tools/build_cenp.sh),
// the CEN that src/dra/cd.c loads instead when the select screen's "Maria boss"
// is "PSP" (MOD_MARIA_BOSS_PSP()): the PSP's Maria fight (src/st/cen_psp: the
// PSP boss's player engine driven by its fight logic, built with mipsel gcc,
// glue in mods/sotn-ultimate/src/maria_psp/mod_cenp.c) in the Saturn fight's
// place. It keeps the Saturn fight's scene split (its two scripts, the
// platform's steps 10-13).
#define MOD_MARIA_BOSS
#define E_MOD_MARIA_BOSS E_DUMMY_0F
#define MOD_MARIA_BOSS_ON() (g_PlayableCharacter == PLAYER_ALUCARD)
void EntityModMariaBoss(Entity* self);
extern s32 g_ModMariaFight;
extern u8 g_MbScriptPre[];
extern u8 g_MbScriptPost[];
#if defined(MOD_CENP)
void EntityMariaPsp(
    Entity* self); // mods/sotn-ultimate/src/maria_psp/mod_cenp.c
u16 CenpDealDamage(Entity* enemy, Entity* attacker); // (collision.h)
#endif
