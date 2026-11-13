// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/sel/2C048.c (at file scope, in place)
// Saturn port: the sound mode's main menu button (MOD_MENU_SOUND), a 5th
// one in the retail buttons' style
// (mods/sotn-ultimate/tools/gen_sel_soundmode.py: their template, a capital S
// block and "ound mode" cut from their words). The staircase is tightened so
// five fit with the retail right margin (x 351): x step 0x34 (retail 0x40), y
// step 0x20 (retail 0x28), from (0x10, 0x1C). The button lives at VRAM x 960 y
// 96 (4bpp tpage 0xF, u 0 v 96): F_SEL fills x 512-895 and the character select
// (x 896-959 at y 96+) never reaches it; uploaded with the menu's UI
// (InitMainMenuUI).
#define MOD_RO __attribute__((section(".rodata")))
#include "../../../../src/st/sel/mod_sel_soundmode_gfx.h"
#define MOD_MENU_BTN_X(i) (0x10 + ((i) - 1) * 0x34)
#define MOD_MENU_BTN_Y(i) (0x1C + ((i) - 1) * 0x20)
#define MOD_SOUND_BTN_TPAGE 0xF // 4bpp, x 960 y 0
#define MOD_SOUND_BTN_V 96
static void ModSoundBtnUpload(void) {
    RECT rect;

    rect.x = MOD_SOUND_BTN_TPAGE * 64;
    rect.y = MOD_SOUND_BTN_V;
    rect.w = MOD_SOUND_BTN_W / 4;
    rect.h = MOD_SOUND_BTN_H;
    LoadImage(&rect, (u_long*)g_ModSoundBtnPix);
}
