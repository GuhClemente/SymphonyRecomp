// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/sel/2C048.c (at file scope, in place)
// Saturn port: the sound mode's menu entry (cursor MOD_MENU_SOUND), its
// button (see ModSoundBtnUpload) after the four retail ones down the
// staircase, with the buttons' CLUTs: 0x203 under the cursor, 0x200 not.
// ModGothic: the menu's 16x16 gothic font (DrawString16x16's sheet: a-z
// from u 0x80 v 0xB0, 8 a row; space at 0xE0/0xF0), the sound mode title.
#define MOD_MENU_SOUND 5
#define Upd_Eng_ModSound 0x14
static void ModGothic(const char* s, s32 x, s32 y, s32 clut) {
    s32 i;

    for (; *s; s++, x += 16) {
        i = *s == ' ' ? 38 : *s - 'a'; // 38: u 0xE0, v 0xF0
        func_801B27A8(x, y, 16, 16, 0x80 + (i & 7) * 16, 0xB0 + (i >> 3) * 16,
                      clut, 12, 1, 0x40);
    }
}
