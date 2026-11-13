// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../../../../../src/st/e_room_fg.h"

// Saturn port: the water under room 0's bridge, as the Saturn's STAGE_15
// entities 0x15-0x1B. Alucard in the water (0x17), the splashes and drops
// (0x18-0x1B) are the Castle Entrance's (the same code and constants on the
// Saturn); the stage objects list (WRP's) links no extra files, so they are
// built here. Pools: x, width, slope, top, bottom, edge splash styles,
// current, as the Saturn's (STAGE_15 0x060F2044) with x and width x4/5.
#include "gdn.h"

u16 g_WaterSounds[] = {SFX_WATER_SPLASH_JUMP, SFX_WATER_SPLASH_MOVE};
s16 g_WaterXTbl[] = {1286, 426, 0, 441, 512, 7, 7, 0};

#include "../../../../../src/st/water_effects.h"

// The water body (Saturn entity 0x15), drawn as the Castle Entrance's water
// looks rather than the Saturn's (whose mesh polygon and surface shimmer
// work around its own transparency): an additive blue tint over the pool
// (NP3's EntityTransparentWater is additive too) with a thin bright surface
// line, between Alucard and the room's foreground. Flat quads: the Garden's
// texture pages are full. params: the pool, as EntityAlucardWaterEffect.
static const u8 g_ModWaterBands[][5] = {
    // top offset, height (0: to the bottom), r, g, b (additive); the moving
    // surface light is EntityModWaterSurface
    {0, 0, 0x14, 0x1C, 0x38},
};

void EntityModWaterBody(Entity* self) {
    Primitive* prim;
    s16* pool = &g_WaterXTbl[(self->params & 0xFF) * 8];
    s16 x0, x1, top, bottom, y;
    s32 i;

    if (!self->step) {
        InitializeEntity(g_EInitCommon);
        self->primIndex = g_api.AllocPrimitives(PRIM_G4, LEN(g_ModWaterBands));
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        // placed at the room's ends like the water effect, so it must
        // outlive its spawn point scrolling off camera
        self->flags &= ~FLAG_DESTROY_IF_OUT_OF_CAMERA;
        self->flags |= FLAG_HAS_PRIMS | FLAG_KEEP_ALIVE_OFFCAMERA;
        prim = &g_PrimBuf[self->primIndex];
        for (i = 0; prim != NULL; i++, prim = prim->next) {
            prim->r0 = prim->r1 = prim->r2 = prim->r3 = g_ModWaterBands[i][2];
            prim->g0 = prim->g1 = prim->g2 = prim->g3 = g_ModWaterBands[i][3];
            prim->b0 = prim->b1 = prim->b2 = prim->b3 = g_ModWaterBands[i][4];
            prim->priority = 0x9A; // above Alucard (0x94), below the fg (0xA0)
        }
    }
    x0 = pool[0] - g_Tilemap.scrollX.i.hi;
    x1 = x0 + pool[1];
    top = pool[3] - g_Tilemap.scrollY.i.hi;
    bottom = pool[4] - g_Tilemap.scrollY.i.hi;
    prim = &g_PrimBuf[self->primIndex];
    for (i = 0; prim != NULL; i++, prim = prim->next) {
        y = top + g_ModWaterBands[i][0];
        prim->x0 = prim->x2 = x0;
        prim->x1 = prim->x3 = x1;
        prim->y0 = prim->y1 = y;
        prim->y2 = prim->y3 =
            g_ModWaterBands[i][1] ? y + g_ModWaterBands[i][1] : bottom;
        prim->drawMode = DRAW_TPAGE2 | DRAW_TRANSP; // additive
    }
}

// The water's moving surface light (Saturn entity 0x16): 5 strip segments,
// 2px tall, along the pool's top edge, cycling frames 0, 1, 2, 1 every 10
// ticks as the Saturn (STAGE_15 0x060F2168). The strips live under the red
// mist in its texture page (mods/sotn-ultimate/src/garden/gdn/mod_shimmer.h),
// uploaded with it.
#include "mod_shimmer.h"

static const u8 g_ModShimmerFrames[] = {0, 1, 2, 1};

void EntityModWaterSurface(Entity* self) {
    Primitive* prim;
    s16* pool = &g_WaterXTbl[(self->params & 0xFF) * 8];
    const u8(*seg)[3];
    s16 x;
    s32 i;

    if (!self->step) {
        InitializeEntity(g_EInitCommon);
        self->primIndex =
            g_api.AllocPrimitives(PRIM_SPRT, LEN(g_ModShimmerSegs[0]));
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->flags &= ~FLAG_DESTROY_IF_OUT_OF_CAMERA;
        self->flags |= FLAG_HAS_PRIMS | FLAG_KEEP_ALIVE_OFFCAMERA;
        for (prim = &g_PrimBuf[self->primIndex]; prim; prim = prim->next) {
            prim->tpage = MOD_SHIMMER_TPAGE;
            prim->clut = MOD_SHIMMER_CLUT;
            prim->priority = 0x9C; // over the water body (0x9A)
            prim->r0 = prim->g0 = prim->b0 = 0x80;
        }
    }
    seg = g_ModShimmerSegs[g_ModShimmerFrames[(g_Timer / 10) % 4]];
    x = pool[0] - g_Tilemap.scrollX.i.hi;
    prim = &g_PrimBuf[self->primIndex];
    for (i = 0; prim; i++, prim = prim->next) {
        prim->x0 = x;
        prim->y0 = pool[3] - g_Tilemap.scrollY.i.hi;
        prim->u0 = seg[i][0];
        prim->v0 = seg[i][1];
        prim->u1 = seg[i][2];
        prim->v1 = 2;
        prim->drawMode = DRAW_DEFAULT;
        x += seg[i][2];
    }
}
