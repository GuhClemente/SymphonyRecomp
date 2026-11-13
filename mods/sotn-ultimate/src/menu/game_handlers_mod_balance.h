// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/game_handlers.c (at file scope, in place)
// Saturn port: the Player Select's Balance row (game.h MOD_FLAG_PS1_BALANCE).
// g_EquipDefs / g_EnemyDefs keep the PS1 values; this writes either the PS1
// or the Saturn value of each field the Saturn changed, every frame of play,
// so a new game, a loaded save and a live flag change all take effect (DRA
// is loaded once, never reloaded; stages read these tables through g_api).
typedef struct {
    u16 off; // byte offset in the table
    u8 size;
    u8 enemy; // 1 g_EnemyDefs, 2 the bare-hand hitboxes, else g_EquipDefs
    u32 ps1;
    u32 sat;
} ModBalanceField;
#define MOD_BAL_FIELD(T, i, f, enemy, a, b)                                    \
    { (i) * sizeof(T) + (u32) & ((T*)0)->f, sizeof(((T*)0)->f), enemy, a, b }
#define MOD_BAL_EQ(i, f, a, b) MOD_BAL_FIELD(Equipment, i, f, 0, a, b)
#define MOD_BAL_EN(i, f, a, b) MOD_BAL_FIELD(EnemyDef, i, f, 1, a, b)
// the bare-hand punches' hitboxes (D_800B0188, FrameProperty x of each
// punch's hitting frame): the Saturn's reach further (ALUCARD.PRG
// 0x060B24B0, already in PS1 units; the Knuckles slot, src/weapon/w_001.c,
// follows the same)
#define MOD_BAL_BARE(hb, a, b)                                                 \
    { (hb) * 4, 1, 2, a, b }
extern s8 D_800B0188[];
static const ModBalanceField g_ModBalance[] = {
#include "../../../../src/dra/gen/us/mod_balance.h"
    MOD_BAL_BARE(4, 18, 23),  MOD_BAL_BARE(6, 12, 17),  MOD_BAL_BARE(8, 18, 28),
    MOD_BAL_BARE(10, 19, 30), MOD_BAL_BARE(11, 21, 30),
};

void CheckWeaponCombo(void);

static void ModApplyBalance(void) {
    const ModBalanceField* p;
    u8* d;
    u32 v;
    s32 changed = 0;

    for (p = g_ModBalance; p < &g_ModBalance[LEN(g_ModBalance)]; p++) {
        d = (p->enemy == 2 ? (u8*)D_800B0188
             : p->enemy    ? (u8*)g_EnemyDefs
                           : (u8*)g_EquipDefs) +
            p->off;
        v = MOD_PS1_BALANCE() ? p->ps1 : p->sat;
        if (p->size == 4) {
            changed |= *(u32*)d != v;
            *(u32*)d = v;
        } else if (p->size == 2) {
            changed |= *(u16*)d != (u16)v;
            *(u16*)d = v;
        } else {
            changed |= *d != (u8)v;
            *d = v;
        }
    }
    // the two-hand combo (Mablung + Woodland Shield) is cached when the
    // equipment changes; redo it when the balance swaps the tables under it
    // (a save of the other balance loaded without a reboot, a live flag poke)
    if (changed) {
        CheckWeaponCombo();
    }
}

// Saturn port: the menu text that follows the Dub row (PSP text with the PSP
// and JP dubs, the PS1's with the PS1 dub;
// mods/sotn-ultimate/tools/gen_menu_text.py). SEL leaves both versions of each
// string and its address in g_ModDub.text; the chosen one is written over the
// string every frame of play, as the balance above (each fits the PS1 string's
// bytes)
static void ModApplyDubText(void) {
    ModDubText* t = &g_ModDub.text;
    const u8* s;
    u8* d;
    s32 i, n, psp;

    if (t->magic != MOD_DUB_TEXT_MAGIC) {
        return;
    }
    psp = MOD_PSP_DUB_ON();
    for (i = 0; i < t->count; i++) {
        d = t->e[i].dst;
        s = t->e[i].src[psp];
        for (n = t->e[i].len; n; n--) {
            *d++ = *s++;
        }
    }
}
