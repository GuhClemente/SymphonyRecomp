// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/71830.c (at file scope, in place)
// Saturn func_060A96A0
void PlayerStepGodspeedSprint(void) {
    s32 ret;

    if (g_Player.unk7A) {
        func_8010E6AC(false);
        return;
    }
    // Keep the tap window open so a quick re-tap restarts the sprint
    g_GodspeedTapTimer = 8;
    // Saturn animation 0xDF plays every walk cycle frame for 2 ticks; anim 13
    // alternates 3 and 2
    if (PLAYER.ext.player.anim == 13 && PLAYER.poseTimer > 2) {
        PLAYER.poseTimer = 2;
    }
    // Attacks, subweapons and items only know the PSX player steps, so let
    // them see a walk; whatever leaves the walk alone (no input, or an
    // attack that does not stop a walk, like Crissaegrim's: Saturn
    // func_060A69F4 handles step 19 as a walk) leaves the sprint alone
    PLAYER.step = Player_Walk;
    ret = func_8010FDF8(0x4301C);
    if (PLAYER.step == Player_Walk) {
        PLAYER.step = Player_FlameWhip;
    }
    if (ret == 0) {
        DecelerateX(FIX(0.15625));
        if (CheckMoveDirection() == 0) {
            func_8010E570(0);
        } else if (PLAYER.step_s == 0) {
            SetSpeedX(GODSPEED_SPEED);
        }
    }
}
