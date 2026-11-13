// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/d_E294.c (at file scope, in place)
// Saturn port: Alucard spear's back-forward special (equipment record 224)
// plays the Saturn's player anims 224/225 (ALUCARD.PRG 0x060B2392 /
// 0x060B23F2), here 247/248: the two-handed swing 104/105 with 11 more
// frames of windup (sprite 0xBA). Same sprite frames and hitboxes, built
// the way 104/105 are: the windup (sub-anim 29 of D_800B0594) then the
// swing's own follow-throughs (13 / 14).
static AnimationFrame g_ModSpearWindup[] = {
    POSE(1, 0xB9, 9), POSE(1, 0xB9, 9), POSE(1, 0xBA, 9), POSE(1, 0xBA, 9),
    POSE(1, 0xBA, 9), POSE(1, 0xBA, 9), POSE(1, 0xBA, 9), POSE(1, 0xBA, 9),
    POSE(1, 0xBA, 9), POSE(1, 0xBA, 9), POSE(1, 0xBA, 9), POSE(1, 0xBA, 9),
    POSE(1, 0xBA, 9), POSE(1, 0xBA, 9), POSE(1, 0xBA, 9), POSE(1, 0xBA, 9),
    POSE(1, 0xBA, 9), POSE(2, 0xBB, 9), POSE(2, 0xBB, 9), POSE(2, 0xBB, 9),
    POSE(4, 0xBC, 9), POSE(4, 0xBD, 9), POSE(4, 0xBE, 9), POSE(8, 0xBF, 9),
    POSE_END,
};
#define MOD_SUBANIM_SPEAR_WINDUP 29
static AnimationFrame g_ModSpearAnim247[] = {
    POSE_UNKNOWN(MOD_SUBANIM_SPEAR_WINDUP),
    POSE_UNKNOWN(0x0D),
    POSE_END,
};
static AnimationFrame g_ModSpearAnim248[] = {
    POSE_UNKNOWN(MOD_SUBANIM_SPEAR_WINDUP),
    POSE_UNKNOWN(0x0E),
    POSE_END,
};
