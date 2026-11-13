// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/6E42C.c (at file scope, in place)
// Saturn func_060A5938
void GodspeedStartSprint(void) {
    if (g_Player.unk7A) {
        func_8010E6AC(false);
        return;
    }
    g_Player.unk44 = 0;
    SetPlayerStep(Player_FlameWhip);
    // Saturn sprint animation 0xDF is the PSX walk cycle (anim 13, frames
    // 0x29-0x38) at 2 ticks per frame; PlayerStepGodspeedSprint does the timing
    SetPlayerAnim(13);
    SetSpeedX(GODSPEED_SPEED);
    // Grounded with no vertical speed, so CheckFloor keeps snapping Alucard
    // to downhill slopes (a landing would otherwise leave him falling)
    PLAYER.velocityY = 0;
    CreateEntFactoryFromEntity(g_CurrentEntity, FACTORY(BP_1, 5), 0);
}
