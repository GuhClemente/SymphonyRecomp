// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/4A538.c (at file scope, in place)
extern u_long* const g_ModRainbowCapePal[];

// Saturn port: Rainbow Mantle's cape colours, as Saturn func_060AF1BC
// computes them for item 0x34 every frame. Six waves of the game timer give
// the channel values; each pair of cape colours is a wave colour and the
// same at 3/4 brightness. Saturn and PS1 share the colour layout. v1.3: the
// Saturn's waves reach 32 at their peaks, which carries into the next
// channel (or out of blue) for ~15 frames, a hard colour flash; scaled to
// 0..31 here instead
#define MOD_WAVE(x) (((x) + 0x1000) * 31 >> 13)
#define MOD_RGB(r, g, b) (0x8000 + ((b) << 10) + ((g) << 5) + (r))
#define MOD_DIM(x) ((x) * 3 >> 2)
static void ModRainbowCape(u16* clut) {
    s32 t = g_GameTimer;
    s32 r0 = MOD_WAVE(rcos(t));
    s32 g0 = MOD_WAVE(rcos(t + 0x555));
    s32 b0 = MOD_WAVE(rsin(t + 0xAAA));
    s32 r1 = MOD_WAVE(rsin((t * 11 >> 3) + 0x1E0));
    s32 g1 = MOD_WAVE(rsin(t + 0xCC));
    s32 b1 = MOD_WAVE(rcos(t * 2));

    clut[0] = MOD_RGB(MOD_DIM(r1), MOD_DIM(g1), MOD_DIM(b1));
    clut[1] = MOD_RGB(r1, g1, b1);
    clut[2] = MOD_RGB(r0, g0, b0);
    clut[3] = MOD_RGB(MOD_DIM(r0), MOD_DIM(g0), MOD_DIM(b0));
}
