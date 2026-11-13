// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/menu.c (at file scope, in place)
// Saturn port: the System screen's "Ultimate settings" row (Alucard games)
// and its popup with the Player Select's four Alucard options, drawn in the
// Sound box's window (mods/sotn-ultimate/src/menu/sysopt.c, after
// Alucard's sprites in BIN/ARC_F2.BIN). Three stubs in DRA's data
// (src/config_us.h) stand in for the retail calls below: they jump to its code
// when it is loaded (an Alucard game), else to the retail function (same
// arguments)
void ModSysOptBox(MenuContext* ctx, s32 cursorIdx, s32 x, s32 y, s32 w, s32 h,
                  s32 yGap, s32 bColorMode);
void ModSysOptInput(s32* nav, u8 nOptions, u32 type);
void ModSysOptSound(MenuContext* ctx);
#define MOD_SYSOPT_BOX ModSysOptBox
#define MOD_SYSOPT_INPUT ModSysOptInput
#define MOD_SYSOPT_SOUND ModSysOptSound
