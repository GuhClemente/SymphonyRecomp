// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port: the Saturn stages' candles (MOD_SATURN_CANDLES), shared by
// the Underground Garden and the Cursed Prison. The stage's e_breakable.c
// includes its mod_candle_glow.h (mods/sotn-ultimate/tools/*_assets.py) and
// defines g_ModCandleRemnantFrames, g_ModBreakableHitboxOffY and
// g_ModBreakableSfx per candle type before including this and ../e_breakable.h.
// Optional: MOD_CANDLE_GLOW_TYPES (a bit per candle type that glows; default
// all) and MOD_CANDLE_ANIMSET (the stand's animset; default ANIMSET_OVL(2)).
#ifndef MOD_CANDLE_ANIMSET
#define MOD_CANDLE_ANIMSET ANIMSET_OVL(2)
#endif

// The glow over a candle flame: g_ModCandleGlow per animation frame
static void ModCandleGlowInit(Entity* self) {
    Primitive* prim;

#ifdef MOD_CANDLE_GLOW_TYPES
    if (!(MOD_CANDLE_GLOW_TYPES & (1 << (self->params >> 12)))) {
        return;
    }
#endif
    self->primIndex = g_api.AllocPrimitives(PRIM_SPRT, 1);
    if (self->primIndex == -1) {
        return;
    }
    self->flags |= FLAG_HAS_PRIMS;
    prim = &g_PrimBuf[self->primIndex];
    prim->r0 = prim->g0 = prim->b0 = 0x80;
    prim->priority = self->zPriority - 1; // behind the flame, on the wall
    prim->drawMode = DRAW_HIDE;
}

static void ModCandleGlowUpdate(Entity* self) {
    Primitive* prim;
    const s16* glow;

    if (!(self->flags & FLAG_HAS_PRIMS) || self->animCurFrame < 1 ||
        self->animCurFrame > LEN(g_ModCandleGlow)) {
        return;
    }
    glow = g_ModCandleGlow[self->animCurFrame - 1];
    prim = &g_PrimBuf[self->primIndex];
    prim->x0 = self->posX.i.hi + glow[0];
    prim->y0 = self->posY.i.hi + glow[1];
    prim->u1 = glow[2];
    prim->v1 = glow[3];
    prim->u0 = glow[4];
    prim->v0 = glow[5];
    prim->clut = glow[6];
    prim->tpage = glow[7];
    prim->drawMode = DRAW_TPAGE2 | DRAW_TRANSP; // additive
}

static void ModCandleGlowFree(Entity* self) {
    if (self->flags & FLAG_HAS_PRIMS) {
        g_api.FreePrimitives(self->primIndex);
        self->flags &= ~FLAG_HAS_PRIMS;
    }
}

// The stand left on the wall when a candle breaks (Saturn entity 0x12),
// until the room is left: ANIMSET_OVL(2) frame g_ModCandleRemnantFrames[type]
// (params: the candle's)
void EntityModCandleRemnant(Entity* self) {
    if (!self->step) {
        InitializeEntity(g_EInitCommon);
        self->animSet = MOD_CANDLE_ANIMSET;
        self->animCurFrame = g_ModCandleRemnantFrames[self->params >> 12];
        self->unk5A = 0;
        self->palette = PAL_NONE;
        // one below the drops (EntityPrizeDrop: center - 20): this entity is
        // in slots 224+, drawn after the drops' 160+, so at the same z it
        // covered a coin bag or heart falling past it
        self->zPriority = g_unkGraphicsStruct.g_zEntityCenter - 21;
        self->flags &= ~FLAG_DESTROY_IF_OUT_OF_CAMERA;
        self->flags |= FLAG_KEEP_ALIVE_OFFCAMERA;
    }
}
