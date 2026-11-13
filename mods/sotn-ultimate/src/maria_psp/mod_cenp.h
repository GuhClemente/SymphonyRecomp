// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port mod: the PS1 build of the PSP Maria boss (BIN/CENP.BIN,
// mods/sotn-ultimate/tools/build_cenp.sh). Included at the end of cen.h when
// the PSP boss sources are compiled for the PS1 (MOD_CENP_BOSS); glue in
// mod_cenp.c.
#ifndef MOD_CENP_H
#define MOD_CENP_H

// her PSP voice ids (+ MAR_SFX_OFFSET) -> the Saturn Maria clips that
// CEN's stage bank holds (src/st/cen/mod_cen_sfx.h); others pass through
s32 CenpSfx(s32 sfxId);
#define PlaySfx(id) PlaySfx(CenpSfx(id))
#define g_api_PlaySfx(id) g_api_PlaySfx(CenpSfx(id))
// the PSP's debug text
#define FntPrint(...) ((void)0)
// the PSP's E_BOSS_MARIA update; the PS1 EntityMaria (src/st/cen/e_maria.c)
// runs it through EntityMariaPsp (mod_cenp.c)
#define EntityMaria EntityMariaPspMain

#endif
