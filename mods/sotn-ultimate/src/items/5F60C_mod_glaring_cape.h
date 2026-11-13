// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/5F60C.c (at file scope, in place)
// Saturn port: Glaring Cloth's cape flickers between two grey/white colour
// sets every frame (Saturn ALUCARD.PRG 0x060AF1AC, func_060AF1BC), the way
// Crystal cloak flickers to transparent. Kept const so it lives in rodata.
static const u16 g_ModGlaringCapeA[] = {0xC210, 0xA108, 0xFFFF, 0xF39C};
static const u16 g_ModGlaringCapeB[] = {0xA94A, 0x9084, 0xE318, 0xA94A};
static u_long* const g_ModGlaringCapePal[] = {
    MAKE_PAL_OP(PAL_UNK_OP3, 1),
    PAL_UNK_OP3_INFO(0x1006, 4),
    PAL_UNK_OP3_DATA(g_ModGlaringCapeA),
    PAL_UNK_OP3_DATA(g_ModGlaringCapeB),
    PAL_TERMINATE(),
};

// Rainbow Mantle: the same four cape colours, recomputed every frame from
// the game timer. func_800EA7CC recognises this descriptor and writes the
// colours itself (ModRainbowCape in 4A538.c); the data entry is a
// placeholder so the descriptor still has the PAL_UNK_OP3 shape.
u_long* const g_ModRainbowCapePal[] = {
    MAKE_PAL_OP(PAL_UNK_OP3, 1),
    PAL_UNK_OP3_INFO(0x1006, 4),
    PAL_UNK_OP3_DATA(g_ModGlaringCapeA),
    PAL_TERMINATE(),
};
