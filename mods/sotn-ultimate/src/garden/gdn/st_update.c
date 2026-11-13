// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../../../../../src/st/st_update.h"

// Saturn port: VRAM x 256-511, y 256-511 holds the Garden's red mist and 3
// pages of its tiles (mods/sotn-ultimate/tools/gdn_assets.py); the game leaves
// it unused during play but the pause menu clears it (see ModGdnReloadBank).
// The red mist, behind the room's own background, fixed on screen, as the
// Saturn's third map layer: one quad from a texture page the game leaves
// unused, not a tile layer, since the renderer's tile budget (MAXSPRT16) is
// nearly used up by the room's two layers. It is stored at half resolution
// (MOD_BACKDROP_W x MOD_BACKDROP_H) and stretched over the screen, which
// leaves the rest of its page to sprites. The quad is checked every frame and
// allocated again if the game freed it. Loading and save rooms (layer flags
// 0x200) keep their shared art.
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

// Saturn port: room 0 has more enemies than the sprite pages hold. Its
// enemies stay loaded but for the Skeleton Breeders (west) and the Human Face
// Tree (east), which take turns in the same quadrants
// (mods/sotn-ultimate/tools/gdn_assets.py ZONE_WEST/EAST): past the empty
// middle of the room the other half's blocks are streamed in, decompressed a
// little each frame (the game's own loader does whole blocks at once, which
// stutters). Entering the room loads its graphics bank, the west's; entering at
// its east end loads the east's whole. The bank is also what the game reloads
// after the menu (D_80097904).
typedef struct {
    u8* data;
    s16 x, y; // VRAM
} ModZoneBlock;
#include "mod_zone_stream.h"

#define MOD_STREAM_NIBBLES 768 // decompressed per frame
#define MOD_STREAM_ROWS 8      // rows uploaded at a time

static struct {
    const ModZoneBlock* list;
    s16 count, block;
    u8* src;
    u8 srcHi; // the next nibble is the low one of *src
    u8 dict[8];
    s32 out; // nibbles written in the block
    u8 buf[2][MOD_STREAM_ROWS * 64];
    u8 page; // buffer being filled
} g_ModStream;

static s16 g_ModZone; // 0 outside room 0, 1 west, 2 east

static u8 ModStreamNibble(void) {
    u8 n;

    if (!g_ModStream.srcHi) {
        g_ModStream.srcHi = 1;
        return *g_ModStream.src >> 4;
    }
    g_ModStream.srcHi = 0;
    n = *g_ModStream.src & 0xF;
    g_ModStream.src++;
    return n;
}

static void ModStreamPut(u8 n) {
    RECT rect;
    u8* p;
    s32 o = g_ModStream.out % (MOD_STREAM_ROWS * 128);

    p = &g_ModStream.buf[g_ModStream.page][o >> 1];
    if (o & 1) {
        *p |= n << 4;
    } else {
        *p = n;
    }
    g_ModStream.out++;
    if (!(g_ModStream.out % (MOD_STREAM_ROWS * 128))) {
        rect.x = g_ModStream.list[g_ModStream.block].x;
        rect.y = g_ModStream.list[g_ModStream.block].y +
                 (g_ModStream.out / 128 - MOD_STREAM_ROWS);
        rect.w = 32;
        rect.h = MOD_STREAM_ROWS;
        LoadImage(&rect, (u_long*)g_ModStream.buf[g_ModStream.page]);
        g_ModStream.page ^= 1;
    }
}

static void ModStreamBlock(void) {
    s32 i;

    g_ModStream.src = g_ModStream.list[g_ModStream.block].data;
    for (i = 0; i < 8; i++) {
        g_ModStream.dict[i] = *g_ModStream.src++;
    }
    g_ModStream.srcHi = 0;
    g_ModStream.out = 0;
}

static void ModStreamStart(const ModZoneBlock* list, s32 count) {
    g_ModStream.list = list;
    g_ModStream.count = count;
    g_ModStream.block = 0;
    if (count) {
        ModStreamBlock();
    }
}

// DRA's DecompressData (src/dra/4AEA4.c), resumable between operations
static void ModStreamStep(void) {
    s32 budget = MOD_STREAM_NIBBLES;
    s32 op, ch, ch2, count;

    while (g_ModStream.block < g_ModStream.count && budget > 0) {
        op = ModStreamNibble();
        count = 0;
        switch (op) {
        case 0:
            ch = ModStreamNibble();
            ch2 = ModStreamNibble();
            for (count = ch * 16 + ch2 + 0x13; count; count--) {
                ModStreamPut(0);
            }
            budget -= ch * 16 + ch2 + 0x13;
            break;
        case 1:
            ModStreamPut(ModStreamNibble());
            budget--;
            break;
        case 2:
            ch = ModStreamNibble();
            ModStreamPut(ch);
            ModStreamPut(ch);
            budget -= 2;
            break;
        case 3:
            ModStreamPut(ModStreamNibble());
            ModStreamPut(ModStreamNibble());
            budget -= 2;
            break;
        case 4:
            ModStreamPut(ModStreamNibble());
            ModStreamPut(ModStreamNibble());
            ModStreamPut(ModStreamNibble());
            budget -= 3;
            break;
        case 5:
            ch = ModStreamNibble();
            ch2 = ModStreamNibble();
            for (count = ch2 + 3; count; count--) {
                ModStreamPut(ch);
            }
            budget -= ch2 + 3;
            break;
        case 6:
            ch = ModStreamNibble();
            for (count = ch + 3; count; count--) {
                ModStreamPut(0);
            }
            budget -= ch + 3;
            break;
        case 15:
            // the block is done (all 128 rows went up as they filled)
            g_ModStream.block++;
            if (g_ModStream.block < g_ModStream.count) {
                ModStreamBlock();
            }
            break;
        default: // 7-14: the block's dictionary
            ch = g_ModStream.dict[op - 7];
            switch (ch & 0xF0) {
            case 0x20:
                ModStreamPut(ch & 0xF);
                // fallthrough
            case 0x10:
                ModStreamPut(ch & 0xF);
                break;
            case 0x60:
                for (count = (ch & 0xF) + 3; count; count--) {
                    ModStreamPut(0);
                }
                break;
            }
            budget -= 2;
            break;
        }
    }
}

static void ModGdnZones(void) {
    s32 x;
    s32 zone;

    if (g_Tilemap.left != MOD_ZONE_ROOM_LEFT ||
        g_Tilemap.top != MOD_ZONE_ROOM_TOP) {
        g_ModZone = 0;
        g_ModStream.count = 0;
        return;
    }
    x = g_Tilemap.scrollX.i.hi + 128;
    if (!g_ModZone) {
        // just entered: the room's graphics bank (the west's) is loading
        g_ModZone = 1;
        g_ModStream.count = 0;
        if (x >= MOD_ZONE_EAST) {
            g_ModZone = 2;
            D_80097904 = MOD_ZONE_EAST_GFX;
            g_api.LoadGfxAsync(D_80097904);
        }
        return;
    }
    zone = g_ModZone;
    if (x >= MOD_ZONE_EAST) {
        zone = 2;
    } else if (x < MOD_ZONE_WEST) {
        zone = 1;
    }
    if (zone != g_ModZone) {
        g_ModZone = zone;
        // ModGdnReloadBank adds the pages for the menu's reload
        D_80097904 = zone == 2 ? MOD_ZONE_EAST_GFX : MOD_ZONE_WEST_GFX;
        if (zone == 2) {
            ModStreamStart(g_ModZoneEast, LEN(g_ModZoneEast));
        } else {
            ModStreamStart(g_ModZoneWest, LEN(g_ModZoneWest));
        }
    }
    ModStreamStep();
}

// Saturn port: the pause menu clears the sprites and the tile pages 12-15
// and, before it fades back in, reloads the bank in D_80097904 (the room's,
// set when the room is entered): make it the room's sprites plus the pages
// (mods/sotn-ultimate/tools/gdn_assets.py RELOAD_BANKS), so they come back
// behind the fade.
static void ModGdnReloadBank(void) {
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

void ModGdnUpdate(void) {
    Primitive* prim;

    Update();
    ModGdnZones();
    ModGdnReloadBank();
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
