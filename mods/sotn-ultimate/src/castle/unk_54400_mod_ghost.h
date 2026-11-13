// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/no1/unk_54400.c (at file scope, in place)
// mod: the skull's art is not in room 13's graphics. A room's objGfxId N
// loads gfxBanks[N - 1] (DRA LoadGfxAsync(objGfxId + 0x7FFF)): the tower
// (objGfxId 3) loads bank 2, which puts other art at the skull's texture
// page (tpage 0x50 = page 20, quarter 0 = VRAM 256,256), so the skull drew
// garbage. Bank 3 (its art) is loaded only by rooms 4 and 11. Bank 3's
// 128x128 block for VRAM 256,256 (the 4 skull poses + the sparks) is queued
// to 352,256 instead (page 21, quarter 1 -> unk5A 0x55): a rect that only
// bank 3 writes in NO1 (not bank 2, not DRA, not the familiars at 704,256),
// so nothing in room 13 uses it. The pause menu reuses entity VRAM (on exit
// DRA reloads only the room bank), so it is queued again whenever the game
// was paused (g_Timer ran while g_GameTimer did not), and at every spawn.
#define GHOST_VRAM_XY ((0x160 << 16) | 0x100) // 352,256 (x << 16 | y)
#define GHOST_SRC_XY ((0x100 << 16) | 0x100)  // bank 3's block at 256,256
#define GHOST_UNK5A 0x55
// DRA's g_GfxLoad (config/symbols.us.txt); not in the stage link's symbols
#define GHOST_GFX_LOAD ((GfxLoad*)0x80072FA0)
static GfxEntry g_GhostGfx[2];
static u32 g_GhostGfxStamp;

static void GhostLoadGfx(bool force) {
    GfxEntry* src;
    GfxLoad* load;
    u32 stamp;
    s32 i;

    stamp = g_Timer - g_GameTimer;
    if (!force && stamp == g_GhostGfxStamp) {
        return;
    }
    for (i = 0, load = GHOST_GFX_LOAD; i < 16; i++, load++) {
        if (load->kind == GFX_BANK_COMPRESSED && load->next == g_GhostGfx) {
            g_GhostGfxStamp = stamp; // already pending
            return;
        }
    }
    src = g_api.o.gfxBanks[3]->entries;
    while (LOWU(src->xy) != GHOST_SRC_XY) {
        if (LOWU(src->xy) == -1) {
            return;
        }
        src++;
    }
    g_GhostGfx[0].xy = (u_long*)GHOST_VRAM_XY;
    g_GhostGfx[0].wh = src->wh;
    g_GhostGfx[0].data = src->data;
    g_GhostGfx[1].xy = GFX_TERMINATE();
    for (i = 0, load = GHOST_GFX_LOAD; i < 16; i++, load++) {
        if (load->kind == GFX_BANK_NONE) {
            load->kind = GFX_BANK_COMPRESSED;
            load->unk6 = 0;
            load->unk8 = 0;
            load->next = g_GhostGfx;
            g_GhostGfxStamp = stamp;
            return;
        }
    }
    // queue full: try again next frame
}
