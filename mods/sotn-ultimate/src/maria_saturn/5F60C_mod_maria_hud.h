// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/5F60C.c (at file scope, in place)
// Saturn port: the Saturn's own HUD for Maria (GAME.PRG func_0607672C and
// func_06076A04, its compact status display; art = MAR_W.CHR package 5, put
// in F_GAME2's empty VRAM at tpage 0x18 u128-255 v136-231 by
// mods/sotn-ultimate/tools/gen_mar_hud.py). Eight sprites: the vine gauge
// holder, the HP (purple) and MP (blue) bars in its two tubes, three hearts
// digits in its green box, the subweapon icon in its vine ring and the MARIA
// plate, whose letters glow with her charge (MARIA.BIN copies its charge timer
// into g_Player.unk24). No boss gauge and no REST counter, as on the Saturn.
// Positions are the Saturn's, x * 4/5. The colours: HP bar and digits use
// palette 0x103 (= the Saturn's palette 0); 0x106-0x108 (Richter's boss
// gauge flash, which Maria never shows) are rewritten every frame with her
// plate (+ glow), MP bar and holder colours.
#define MAR_HUD_TPAGE 0x18
#define MAR_HUD_V 136
#define MAR_HUD_CLUT 0x106
// palettes 0x106 (the Saturn's palette 0), 0x107 (MP bar, colours 1-3:
// set every frame), 0x108 (the Saturn's palette 1)
static u16 g_MarHudPal[] = {
    0x0000, 0xFE9F, 0xFCDA, 0xCCF8, 0x8013, 0x801F, 0xA99F, 0xDEDF,
    0x8000, 0xBC00, 0x88CA, 0x952D, 0xAA16, 0xCF7F, 0xEFFF, 0xFFFF,
    0x0000, 0xFD8C, 0xFD8C, 0xFD8C, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x8C97, 0x8415, 0xFFFF, 0x98C6, 0x8CC2, 0x8000, 0xEFFF,
    0xCE73, 0xB38C, 0x8D43, 0x95C5, 0x9A28, 0x96A6, 0x9122, 0xB18C};
// the Saturn's palettes 3/4 (letters red / green) and 5/6 (blue letters on
// green, red on white): colours 9 (background) and 15 (letters)
static u16 g_MarHudGlow[][2] = {
    {0xBC00, 0x801F}, {0xBC00, 0x83E0}, {0x83E0, 0xFC00}, {0xFFFF, 0x801F}};
// the Saturn's palette 10, the full MP bar's colour ramp (red to white)
static u16 g_MarHudMpRamp[] = {
    0xB0A5, 0x843F, 0x8C7F, 0x94BF, 0x9CFF, 0xA53F, 0xAD7F, 0xB5BF,
    0xBDFF, 0xC63F, 0xCE7F, 0xD6BF, 0xDEFF, 0xE73F, 0xF7BF, 0xFD8C};
// ids 0x106-0x108 are one row of VRAM (func_800EAD7C)
static RECT g_MarHudClutRect = {
    (MAR_HUD_CLUT - 0x100) * COLORS_PER_PAL, 0xF0, COLORS_PER_PAL * 3, 1};
// the pause animation's sprite for each step of g_PlayerHud.unk24
// (StatusPause starts it at 1, it holds at 21, resumes at 51, ends at 71):
// the Saturn's sprites 0-4; 1-4 are Richter's PS1 copies of the same art in
// tpage 0x1B, {y, h, v}
static u8 g_MarHudPauseStep[] = {1, 6, 12, 17, 51, 57, 61, 66};
static u8 g_MarHudPauseFrame[] = {1, 2, 3, 4, 3, 2, 1, 0};
static u8 g_MarHudPauseSpr[][3] = {
    {21, 24, MAR_HUD_V}, {25, 16, 0}, {29, 8, 16}, {25, 16, 24}, {21, 24, 64}};

static void MarHudPalettes(void) {
    u16* clut;
    s32 i;
    s32 t;

    clut = &g_Clut[1][(MAR_HUD_CLUT - 0x100) * COLORS_PER_PAL];
    for (i = 0; i < LEN(g_MarHudPal); i++) {
        clut[i] = g_MarHudPal[i];
    }
    // the Saturn's glow: palette 3/4 above 100 frames of charge (every 8
    // frames), every 4 above 200, palette 5/6 above 300
    t = (s32)g_Player.unk24;
    if (t > 100) {
        i = (g_GameTimer >> (t > 200 ? 2 : 3)) & 1;
        if (t > 300) {
            i += 2;
        }
        clut[9] = g_MarHudGlow[i][0];
        clut[15] = g_MarHudGlow[i][1];
    }
    // the MP bar is one colour of palette 10, the Saturn's colour-table
    // offset: 15 (blue) normally; at full MP a 1-14 triangle wave of the
    // timer (red <-> white)
    t = 15;
    if (g_Status.mp == g_Status.mpMax) {
        t = g_Timer & 0x1F;
        if (t > 15) {
            t = 31 - t;
        }
        t++;
        if (t > 14) {
            t = 14;
        }
    }
    clut[17] = clut[18] = clut[19] = g_MarHudMpRamp[t];
    LoadImage((RECT*)&g_MarHudClutRect, (u_long*)clut);
}

static Primitive* MarHudSprite(
    Primitive* prim, s32 x, s32 y, s32 w, s32 h, s32 u, s32 v) {
    SetTexturedPrimRect(prim, x, y, w, h, u, v);
    prim->tpage = MAR_HUD_TPAGE;
    prim->clut = 0x103;
    prim->priority = 0x1F0;
    prim->drawMode = DRAW_ABSPOS;
    return prim->next;
}

static void MarHudInit(void) {
    D_801397FC = 0;
    D_80139008 = 0;
    D_8003C744 = 0;
    g_PlayerHud.unk28 = 0;
    g_PlayerHud.unk24 = 0;
    g_PlayerHud.primIndex1 = AllocPrimitivesReverse(PRIM_GT4, 8);
    MarHudUpdate();
}

static void MarHudUpdate(void) {
    Primitive* prim;
    Primitive* p;
    RicSubwpnIconParams* ptr;
    s32 i;
    s32 d;
    s32 n;

    // (the Saturn's compact HUD has no D_8003C744 boss / hide states)
    prim = &g_PrimBuf[g_PlayerHud.primIndex1];
    MarHudPalettes();
    // the shown HP follows by 5 a frame
    n = g_Status.hp - (s32)g_PlayerHud.displayHP;
    if (n > 5) {
        n = 5;
    }
    if (n < -5) {
        n = -5;
    }
    n += g_PlayerHud.displayHP;
    g_PlayerHud.displayHP = n;
    // the holder (under the rest)
    p = prim;
    prim = MarHudSprite(prim, 2, 24, 38, 96, 128, MAR_HUD_V);
    p->clut = MAR_HUD_CLUT + 2;
    p->priority = 0x1EF;
    // the bars rise from y 116 (the Saturn's bottom row) in its tubes
    n = n * 90 / g_Status.hpMax;
    p = prim;
    prim = MarHudSprite(prim, 6, 117 - n, 4, n, 240, MAR_HUD_V);
    if (n <= 0) {
        p->drawMode = DRAW_HIDE;
    }
    n = g_Status.mp * 57 / g_Status.mpMax;
    p = prim;
    prim = MarHudSprite(prim, 21, 117 - n, 3, n, 245, MAR_HUD_V);
    p->clut = MAR_HUD_CLUT + 1;
    // under the holder, whose glass highlight in this tube shows over it
    p->priority = 0x1EE;
    if (n <= 0) {
        p->drawMode = DRAW_HIDE;
    }
    // hearts, ones first: the tens only from 10, the hundreds from 100
    for (i = 0, n = g_Status.hearts; i < 3; i++, n /= 10) {
        p = prim;
        prim = MarHudSprite(
            prim, 27 - i * 7, 29, 7, 8, 168 + n % 10 * 7, MAR_HUD_V + 24);
        if (i && n == 0) {
            p->drawMode = DRAW_HIDE;
        }
    }
    // the subweapon icon, Richter's, in her vine ring (2 px lower)
    if (g_Status.subWeapon == 0) {
        prim->drawMode = DRAW_HIDE;
    } else {
        ptr = &g_ricSubwpnIcons[g_Status.subWeapon - 1];
        MarHudSprite(
            prim, ptr->x + 2, ptr->y + 24, ptr->w, ptr->h, ptr->u, ptr->v);
        prim->tpage = ptr->tpage;
        prim->clut = ptr->clut;
        if (prim->clut == 0x17F) {
            prim->drawMode |= (DRAW_TPAGE | DRAW_TRANSP);
        }
    }
    prim = prim->next;
    // the plate, or the pause animation's sprite
    n = g_PlayerHud.unk24;
    d = 0;
    for (i = 0; i < LEN(g_MarHudPauseStep); i++) {
        if (n >= g_MarHudPauseStep[i]) {
            d = g_MarHudPauseFrame[i];
        }
    }
    MarHudSprite(prim, 36, g_MarHudPauseSpr[d][0], 64, g_MarHudPauseSpr[d][1],
                 d ? 64 : 168, g_MarHudPauseSpr[d][2]);
    prim->clut = MAR_HUD_CLUT;
    prim->priority = 0x1EF;
    if (d) {
        prim->tpage = 0x1B;
    }
    if (n != 0 && n != 21) {
        g_PlayerHud.unk24 = n >= 71 ? 0 : n + 1;
    }
}
