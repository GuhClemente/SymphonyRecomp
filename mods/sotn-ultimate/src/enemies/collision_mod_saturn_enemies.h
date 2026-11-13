// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/collision.h (at file scope, in place)
// Saturn enemies (ids 0x190+) are past the end of the 400-entry US table
#if defined(STAGE_IS_GDN) || defined(STAGE_IS_RGDN) ||                         \
    defined(STAGE_IS_PRS) || defined(STAGE_IS_RPRS) || defined(STAGE_IS_CEN)
// Saturn bestiary (mods/sotn-ultimate/tools/gen_saturn_bestiary.py, LIB
// mod_bestiary.h): the Saturn enemies with a Librarian entry are the set bits
// of this mask (bit n = enemy id 400 + n). They get bestiary bits 146.. in id
// order, so the lookup value (bit + 1) is 147 + the set bits below the id.
// Code, not a table, so the stage's data section keeps its retail size. Only
// the stages with Saturn enemies pay for it; add new ones to the condition
// above.
#define MOD_SAT_BESTIARY_MASK 0x3B7E177
static u16 ModCollSaturnLookup(u32 id) {
    u32 mask;
    u16 n;

    id -= 400;
    if (id >= 26 || !((MOD_SAT_BESTIARY_MASK >> id) & 1)) {
        return 0;
    }
    mask = MOD_SAT_BESTIARY_MASK & ((1 << id) - 1);
    for (n = 147; mask; mask >>= 1) {
        n += mask & 1;
    }
    return n;
}
#define COLL_ENEMY_LOOKUP(id)                                                  \
    ((id) < LEN(g_testCollEnemyLookup) ? g_testCollEnemyLookup[id]             \
                                       : ModCollSaturnLookup(id))
#elif defined(STAGE_IS_NO1) && defined(MOD_OUTER_WALL_GHOST)
// the cut Outer Wall skull (record 0xB1, "Ghost Skull"): Library No.165,
// bestiary bit 164 (LIB mod_bestiary.h MOD_LIB_GHOST_BIT; g_CastleFlags
// 0x1A4 / rare drop 0x1C4, mask 0x10), lookup value bit + 1. Retail's table
// entry stays 0 (NO1's data section is retail's), so the check is code.
#define COLL_ENEMY_LOOKUP(id)                                                  \
    ((id) == 0xB1                        ? 165                                 \
     : (id) < LEN(g_testCollEnemyLookup) ? g_testCollEnemyLookup[id]           \
                                         : 0)
#else
#define COLL_ENEMY_LOOKUP(id)                                                  \
    ((id) < LEN(g_testCollEnemyLookup) ? g_testCollEnemyLookup[id] : 0)
#endif
