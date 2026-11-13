// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/ric/pl_setstep.c (at file scope, in place)
// Saturn 060A955C: a second jump in the air. The first sets unk44 0x21,
// the second 0x100; with 0x101 both are used.
void MarSetDoubleJump(void) {
    if (g_Player.unk44 & 1) {
        g_Player.unk44 = (g_Player.unk44 & ~0x20) | 0x100;
    } else {
        g_Player.unk44 = (g_Player.unk44 | 0x21) & ~2;
    }
    RicSetAnimation(D_8015550C);
    if (RicCheckFacing()) {
        RicSetSpeedX(FIX(1.25));
    }
    PLAYER.step = PL_S_JUMP;
    PLAYER.step_s = 0;
    PLAYER.velocityY = FIX(-4.75);
    if (g_Player.unk72) {
        PLAYER.velocityY = 0;
    }
}
