// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/lib/e_shop.c (at file scope, in place)
// Saturn port: the Saturn's 164-entry list
// (mods/sotn-ultimate/tools/gen_saturn_bestiary.py). The list position (and the
// "No.") is the Saturn's; the bestiary bit and everything looked up with it
// (enemy, description, preview) is LIB_BIT.
#include "../../../../src/st/lib/mod_bestiary.h"
#define MOD_LIB_DESC_Y 0x1C0
#define LIB_BIT(i) g_ModLibBit[i]
#define LIB_ENEMY(i) g_ModLibEnemy[LIB_BIT(i)]
// two names a row, 7 rows shown: the last top row (an odd count, i.e. the
// Ghost Skull entry, leaves the last row's right cell empty, see
// mod_bestiary.h)
#define LIB_TOP_MAX ((MOD_LIB_NUM_ENEMIES + 1) / 2 - 7)
// Maria (No.157, the bit of the PSP description) with the PSP boss chosen
// (MOD_MARIA_BOSS_PSP): the PSP port's picture, BIN/MONSTER2.BIN record 18
// (mods/sotn-ultimate/tools/build_monster2.py), and her PSP stats
// (include/game.h)
#define LIB_MARIA_PSP(i)                                                       \
    (LIB_BIT(i) == MOD_LIB_DESC_PSP_BIT && MOD_MARIA_BOSS_PSP())
#define MOD_LIB_PSP_MARIA_PIC MOD_LIB_SATURN_ENEMIES
// the picture to load: Maria's PSP one, the cut Outer Wall skull's own
// (MOD_OUTER_WALL_GHOST: bit 164 -> record 19 = index 165, as 164 is
// Maria's), else the bit (bits >= 146 come from BIN/MONSTER2.BIN)
#ifdef MOD_OUTER_WALL_GHOST
#define LIB_PIC(i)                                                             \
    (LIB_MARIA_PSP(i)                  ? MOD_LIB_PSP_MARIA_PIC                 \
     : LIB_BIT(i) == MOD_LIB_GHOST_BIT ? MOD_LIB_GHOST_PIC                     \
                                       : LIB_BIT(i))
#else
#define LIB_PIC(i) (LIB_MARIA_PSP(i) ? MOD_LIB_PSP_MARIA_PIC : LIB_BIT(i))
#endif
