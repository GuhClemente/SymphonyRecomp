// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../../../../../src/st/st_update.h"

// Saturn port: as the Garden (mods/sotn-ultimate/src/garden/gdn/st_update.c),
// VRAM x 256-511, y 256-511 holds the Hell Garden's red mist and 3 pages of its
// tiles (mods/sotn-ultimate/tools/rgdn_assets.py), graphics bank
// MOD_BACKDROP_GFX = gfxBanks[0], which the stage loads when it starts. The
// mist is one quad behind the room's background, fixed on screen (the Saturn's
// third map layer, rotated with the rest of the stage), stored at half
// resolution and stretched over the screen; it is checked every frame and
// allocated again if the game freed it. Loading and save rooms (layer flags
// 0x200) keep their shared art. Each room loads its sprites (its entityGfxId);
// none of the Garden's zone streaming (yet).
#include "mod_backdrop.h"
#include "mod_sprite_zones.h"

#define MOD_BACKDROP_PRIORITY 0x10 // below the room background (0x20)

static s32 g_ModBackdropPrim = -1;

static Primitive* ModBackdropPrim(void) {
    Primitive* prim;
    s32 i = g_ModBackdropPrim;

    if (i >= 0 && i < MAX_PRIM_COUNT) {
        prim = &g_PrimBuf[i];
        if (prim->type == PRIM_GT4 && prim->tpage == MOD_BACKDROP_TPAGE &&
            prim->clut == MOD_BACKDROP_CLUT) {
            return prim;
        }
    }
    i = g_api.AllocPrimitives(PRIM_GT4, 1);
    if (i == -1) {
        return NULL;
    }
    g_ModBackdropPrim = i;
    // marked as the mist's at once: a loading or save room only hides it
    // (and sets nothing else), and an unmarked one failed the check above
    // every frame there, one primitive more each frame until none were left
    prim = &g_PrimBuf[i];
    prim->tpage = MOD_BACKDROP_TPAGE;
    prim->clut = MOD_BACKDROP_CLUT;
    return prim;
}

// The pause menu clears the sprites and the tile pages 12-15 and, before it
// fades back in, reloads the bank in D_80097904 (the room's, set when the room
// is entered): make it the room's sprites plus the pages
// (mods/sotn-ultimate/tools/rgdn_assets.py RELOAD_BANKS), or the pages alone
// (as the Garden, mods/sotn-ultimate/src/garden/gdn/st_update.c
// ModGdnReloadBank).
static void ModRgdnReloadBank(void) {
    s32 i;

    if (!D_80097904) {
        D_80097904 = MOD_PAGES_GFX;
        return;
    }
    for (i = 0; i < LEN(g_ModReloadBanks); i++) {
        if (D_80097904 == g_ModReloadBanks[i][0]) {
            D_80097904 = g_ModReloadBanks[i][1];
            return;
        }
    }
}

void ModRgdnUpdate(void) {
    Primitive* prim;

    Update();
    ModRgdnReloadBank();
    prim = ModBackdropPrim();
    if (prim == NULL) {
        return;
    }
    if (g_Tilemap.flags & 0x200) {
        prim->drawMode = DRAW_HIDE;
        return;
    }
    // the half-resolution mist over the whole screen
    prim->x0 = prim->x2 = 0;
    prim->x1 = prim->x3 = 256;
    prim->y0 = prim->y1 = 0;
    prim->y2 = prim->y3 = 240;
    prim->u0 = prim->u2 = 0;
    prim->u1 = prim->u3 = MOD_BACKDROP_W;
    prim->v0 = prim->v1 = 0;
    prim->v2 = prim->v3 = MOD_BACKDROP_H;
    prim->tpage = MOD_BACKDROP_TPAGE;
    prim->clut = MOD_BACKDROP_CLUT;
    prim->priority = MOD_BACKDROP_PRIORITY;
    prim->r0 = prim->g0 = prim->b0 = 0x80;
    prim->r1 = prim->g1 = prim->b1 = 0x80;
    prim->r2 = prim->g2 = prim->b2 = 0x80;
    prim->r3 = prim->g3 = prim->b3 = 0x80;
    prim->drawMode = DRAW_DEFAULT;
}
