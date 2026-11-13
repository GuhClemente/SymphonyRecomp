// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../../../../../src/st/e_room_fg.h"

#include "rgdn.h"

// Saturn port: the Cloaked Knight (Saturn RSTAGE15 0x34, RNZ1's PS1 code)
#include "../../../../../src/st/e_cloaked_knight.h"

// Saturn port: the ceiling water of room 0, the Garden's pool turned over
// (RSTAGE15 entities 0x15-0x1C; see inv/step14/notes.md).
// Alucard in the water (0x17, func_060DC580) and the splashes and drops it
// throws (0x18-0x1C) are the Saturn's port of the PS1's own inverted-castle
// water (water_effects_rev.h, RNO3's): the same code, the pool mirrored by
// the room's size, gravity reversed, the head (not the feet) tested, the
// current pushing the other way; the Saturn only rescaled its edge
// constants to its 320 px screen (17 for the PS1's 14 etc.), which the PS1
// code has in PS1 pixels. Pools: as the Garden's (RSTAGE15 0x060EE52C holds
// STAGE_15's table: x 1608, width 532, slope 0, top 441, bottom 512, edge
// splash styles 7/7, current 0), x and width x4/5; the code mirrors them.
u16 g_WaterSounds[] = {SFX_WATER_SPLASH_JUMP, SFX_WATER_SPLASH_MOVE};
s16 g_WaterXTbl[] = {1286, 426, 0, 441, 512, 7, 7, 0};

#include "../../../../../src/st/water_effects_rev.h"

// The water body (Saturn 0x15, func_060DE268): one flat polygon over the
// mirrored pool, colour RGB555 (6, 8, 12) (0xB106) with the VDP1 MESH bit
// (CMDPMOD 0x05C0: every other pixel, i.e. 50% of it over the background),
// clipped to the screen, priority 0x9A (over Alucard, under the foreground).
// Here a 50% blended G4 (the PS1's equivalent of the mesh), the colour x
// 255/31. params: the pool.
void EntityModCeilingWater(Entity* self) {
    Primitive* prim;
    s16* pool = &g_WaterXTbl[(self->params & 0xFF) * 8];
    s16 left, right, top, bottom;

    if (!self->step) {
        InitializeEntity(g_EInitCommon);
        self->primIndex = g_api.AllocPrimitives(PRIM_G4, 1);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        // placed at the room's ends like the water effect, so it must
        // outlive its spawn point scrolling off camera
        self->flags &= ~FLAG_DESTROY_IF_OUT_OF_CAMERA;
        self->flags |= FLAG_HAS_PRIMS | FLAG_KEEP_ALIVE_OFFCAMERA;
        prim = &g_PrimBuf[self->primIndex];
        prim->r0 = prim->r1 = prim->r2 = prim->r3 = 6 * 255 / 31;
        prim->g0 = prim->g1 = prim->g2 = prim->g3 = 8 * 255 / 31;
        prim->b0 = prim->b1 = prim->b2 = prim->b3 = 12 * 255 / 31;
        prim->priority = 0x9A;
        prim->drawMode = DRAW_HIDE;
    }
    prim = &g_PrimBuf[self->primIndex];
    right = g_Tilemap.width - pool[0] - g_Tilemap.scrollX.i.hi;
    left = right - pool[1];
    bottom = g_Tilemap.height - pool[3] - g_Tilemap.scrollY.i.hi;
    top = g_Tilemap.height - pool[4] - g_Tilemap.scrollY.i.hi;
    if (right < 0 || left > 256 || top > 240 || bottom < 0) {
        prim->drawMode = DRAW_HIDE;
        return;
    }
    if (left < 0) {
        left = 0;
    }
    if (right > 256) {
        right = 256;
    }
    if (top < 0) {
        top = 0;
    }
    if (bottom > 240) {
        bottom = 240;
    }
    // (the Saturn's corners are inclusive: right - 1, bottom - 1)
    prim->x0 = prim->x2 = left;
    prim->x1 = prim->x3 = right;
    prim->y0 = prim->y1 = top;
    prim->y2 = prim->y3 = bottom;
    prim->drawMode = DRAW_TRANSP; // the mesh: 50%
}

// The water's surface light (Saturn 0x16, func_060DE40C / 060DE4CC /
// 060DE560): 5 strips 2 px tall laid right to left from the mirrored pool's
// right edge (each one's right end on the previous one's left end), just
// under the water (y = room height - the pool's top), frames 0, 1, 2, 1
// every 10 ticks (anim 0x060EE650), drawn flipped both ways (CMDCTRL 0x30:
// stored rotated here, mods/sotn-ultimate/tools/gdn_assets.py SHIMMER_FLIP),
// opaque, at priority 0x9C. A strip past the screen's right is hidden, and once
// one would start left of the screen it and the rest are; with the surface off
// screen vertically nothing changes. Segment advances: the Saturn's 128 x 4
// and 20 (0x060EE628) x4/5; each strip is drawn at its image's full width.
#include "mod_shimmer.h"

static const u8 g_ModShimmerAdvance[] = {102, 102, 102, 102, 16};

void EntityModCeilingShimmer(Entity* self) {
    Primitive* prim;
    s16* pool = &g_WaterXTbl[(self->params & 0xFF) * 8];
    const u8(*seg)[3];
    s16 x, y;
    s32 i;

    if (!self->step) {
        InitializeEntity(g_EInitCommon);
        self->primIndex = g_api.AllocPrimitives(PRIM_SPRT, 5);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->flags &= ~FLAG_DESTROY_IF_OUT_OF_CAMERA;
        self->flags |= FLAG_HAS_PRIMS | FLAG_KEEP_ALIVE_OFFCAMERA;
        for (prim = &g_PrimBuf[self->primIndex]; prim; prim = prim->next) {
            prim->tpage = MOD_SHIMMER_TPAGE;
            prim->clut = MOD_SHIMMER_CLUT;
            prim->priority = 0x9C;
            prim->r0 = prim->g0 = prim->b0 = 0x80;
            prim->v1 = 2;
            prim->drawMode = DRAW_HIDE;
        }
        self->pose = 0;
        self->poseTimer = 10;
    } else if (--self->poseTimer <= 0) {
        self->pose = (self->pose + 1) & 3;
        self->poseTimer = 10;
    }
    seg = g_ModShimmerSegs[self->pose == 3 ? 1 : self->pose];
    x = g_Tilemap.width - pool[0] - g_Tilemap.scrollX.i.hi;
    y = g_Tilemap.height - pool[3] - g_Tilemap.scrollY.i.hi;
    if ((u16)y > 239) {
        return;
    }
    prim = &g_PrimBuf[self->primIndex];
    for (i = 0; prim; i++, prim = prim->next) {
        if (x < 0) {
            break;
        }
        x = x + 1 - g_ModShimmerAdvance[i];
        prim->x0 = x;
        if (x > 255) {
            prim->drawMode = DRAW_HIDE;
            continue;
        }
        prim->u0 = seg[i][0];
        prim->v0 = seg[i][1];
        prim->u1 = seg[i][2];
        prim->y0 = y;
        prim->drawMode = DRAW_DEFAULT;
    }
    for (; prim; prim = prim->next) {
        prim->drawMode = DRAW_HIDE;
    }
}
