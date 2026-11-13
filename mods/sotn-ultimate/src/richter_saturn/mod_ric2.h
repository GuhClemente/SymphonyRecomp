// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port: the boss Richter's Saturn coat (MOD_BOSS_RIC_SATURN, BO6's
// BIN/BO6RIC2.BIN) on the kneeling Richter of the scene after Shaft's orb
// (func_us_801AD320). TOP draws him from its own two Richter images
// (richter_sprites[16] and [17], RIC's sprites 16 and 17) and its copy of
// PAL_RICHTER (PAL_BULK 0x230-0x23F = RIC 0x120-0x12F).
// g_ModTopRic2 is filled after the link by
// mods/sotn-ultimate/tools/build_topric2.py from RIC2.BIN (the coat is made
// after the stages are built): RIC2's sprites 16 and 17 (same sizes as the
// PS1's, overwritten in place), then {count, {clut id, pad, 16 colours} x
// count} for the RIC2 palettes in 0x230-0x23F. Zero (not filled): nothing
// changes.
#define MOD_TOP_RIC2_WORDS 231
u32 g_ModTopRic2[MOD_TOP_RIC2_WORDS] __attribute__((section(".rodata"))) = {0};

static void ModTopRic2Apply(void) {
    u8* src = (u8*)g_ModTopRic2;
    u8* dst;
    u16* pal;
    RECT rect;
    s32 i;
    s32 n;
    u16 id;

    if (!MOD_BOSS_RIC_SATURN() || !src[0]) {
        return;
    }
    for (i = 16; i < 18; i++) {
        dst = richter_sprites[i];
        for (n = 4 + dst[0] * dst[1] / 2; n > 0; n--) {
            *dst++ = *src++;
        }
    }
    n = *(u32*)src;
    pal = (u16*)(src + 4);
    for (; n > 0; n--) {
        id = pal[0];
        pal += 2;
        for (i = 0; i < COLORS_PER_PAL; i++) {
            ((u16*)g_Clut)[id * COLORS_PER_PAL + i] = pal[i];
        }
        rect.x = (g_ClutIds[id] & 0x3F) << 4;
        rect.y = g_ClutIds[id] >> 6;
        rect.w = COLORS_PER_PAL;
        rect.h = 1;
        LoadImage(&rect, (u_long*)pal);
        pal += COLORS_PER_PAL;
    }
}
