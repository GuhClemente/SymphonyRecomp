// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/cen/cen.h (at file scope, in place)
// the weapons' VRAM blocks (unk5A = the 128x128 4bpp block: tpage << 2 |
// quadrant; their sprite parts' tpage is 0): the PSP's, or on the PS1
// (BIN/CENP.BIN) the stage sprite pages 18/19 (x 0x80-0xFF, y 0x100-0x1FF,
// see src/st/cen_psp/e_maria_load.c). Prims take the page (+ quadrant uv).
#if defined(MOD_CENP_BOSS)
#define MAR_UNK5A_CRASH 0x48    // x 0x80 y 0x100
#define MAR_UNK5A_CARDINAL 0x49 // x 0xA0 y 0x100
#define MAR_UNK5A_OWL 0x4A      // x 0x80 y 0x180
#define MAR_UNK5A_CAT 0x4B      // x 0xA0 y 0x180
#define MAR_UNK5A_TURTLE 0x4C   // x 0xC0 y 0x100
#define MAR_UNK5A_DRAGON 0x4D   // x 0xE0 y 0x100
#define MAR_UNK5A_DOLL 0x4E     // x 0xC0 y 0x180
#else
#define MAR_UNK5A_TURTLE 0x50
#define MAR_UNK5A_CARDINAL 0x51
#define MAR_UNK5A_CAT 0x54
#define MAR_UNK5A_DRAGON 0x55
#define MAR_UNK5A_OWL 0x58
#define MAR_UNK5A_DOLL 0x59
#define MAR_UNK5A_CRASH 0x5C
#endif
#define MAR_TPAGE(unk5A) ((unk5A) >> 2)
// her body's player-format image slot (unk5A; the afterimages the next 3) and
// the g_PlayerDraw entry her code colours it with. On the PS1 (CENP.BIN)
// slot 0xC is DRA's (Alucard's effects; his mist samples that VRAM page) ->
// slot 8 as the Saturn fight's body (mod_cenp.c moves its rect clear of the
// dialogue's portraits outside the fight), afterimages 9-11
#if defined(MOD_CENP_BOSS)
#define MAR_UNK5A_BODY 8
#define MAR_PLDRAW_BODY 8
#else
#define MAR_UNK5A_BODY 0xC
#define MAR_PLDRAW_BODY 5
#endif
