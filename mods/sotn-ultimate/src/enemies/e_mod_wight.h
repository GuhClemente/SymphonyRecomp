// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Wight (Saturn RSTAGE16 "Soul Prison" entities 0x15-0x17: func_060E2578 /
 * 060E3244 / 060E3438, enemy def 0x1A7), ported to the Soul Prison overlay
 * (Saturn port mod, translated from the SH-2: the Saturn code was raw data
 * in asm/saturn). Include
 * after the stage header and e_misc.h (CheckColliderOffsets,
 * EntityExplosionVariantsSpawner).
 *
 * A green, shimmering spirit inhabiting a skeleton with a sword and a red
 * shield (the decomp's RSTAGE16 "Wight" label is on the Flying Zombie: this
 * is the real one). It shuffles slowly back and forth, slashes when the
 * player is near; hit while walking, the spirit is knocked out of the
 * skeleton for a moment (frames 39-58) and climbs back in. Dying, the
 * skeleton collapses and the spirit (frame 71's single image) is pulled up
 * out of it as a stretched polygon, then the body explodes.
 *
 * Required from the including stage:
 *   MOD_WIGHT_ANIMSET   animSet of the converted Saturn bank 18 (0x060EA3BC,
 *                       frames 0x060ECA7C: PSX frame n = Saturn frame n, all
 *                       drawn +32 as the Saturn's sprites are)
 *   MOD_WIGHT_UNK5A     (optional, default 0)
 *   MOD_WIGHT_SPIRIT_TEX_U/V/W/H/TPAGE/CLUT  frame 71's texture
 *   MOD_SFX_WIGHT_SLASH / MOD_SFX_WIGHT_DEATH  (Saturn 0x908 / 0x909)
 *   enum entries E_MOD_WIGHT (Saturn 0x15), E_MOD_WIGHT_SWORD (0x16, always
 *   self + 1), E_MOD_WIGHT_TRAIL (0x17, slots 160-191) and E_EXPLOSION.
 *
 * Entry points:
 *   EntityModWight(Entity*)       <- E_MOD_WIGHT (layout entity)
 *   EntityModWightSword(Entity*)  <- E_MOD_WIGHT_SWORD: the sword during a
 *                                    slash (frames 74-100), its hitbox
 *   EntityModWightTrail(Entity*)  <- E_MOD_WIGHT_TRAIL: the sword's fading
 *                                    afterimages
 * Primitives: 1 PRIM_GT4 per dying Wight (FLAG_HAS_PRIMS).
 */

#ifndef MOD_WIGHT_ANIMSET
#error "define MOD_WIGHT_ANIMSET before including e_mod_wight.h"
#endif
#ifndef MOD_WIGHT_UNK5A
#define MOD_WIGHT_UNK5A 0
#endif

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif
// rounded to nearest (the spirit polygon's corners: the art is 4/5 rounded)
#define WIGHT_SAT_XR(v) (((v) * 4 + ((v) < 0 ? -2 : 2)) / 5)

// the sword's afterimages: drawn opaque and darkened with drawFlags/opacity
// (the Saturn's gouraud shading: OPACITY, no mask, 96 fading by 6 a tick);
// from tick 4 the Saturn sets sprite flag 0x80 on them, the VDP1 MESH bit
// (zero.BIN 0x0600B4C4 ORs CMDPMOD 0x100: a checkerboard of the sprite's
// pixels, its 50% transparency), drawn here as a 50% blend (the bank's
// colours carry the STP bit for it, rprs_assets.py). (The sprite flag 0x40
// the trail also gets is the scale/rotate enable, not translucency.)
#ifndef MOD_WIGHT_TRAIL_BLEND
#define MOD_WIGHT_TRAIL_BLEND BLEND_TRANSP
#endif
// the spirit polygon: the Saturn's primitive draw mode 2 (= the PS1's
// DRAW_UNK02, opaque: AllocPrimitives(2) makes it a VDP1 distorted sprite
// with CMDPMOD 0x1488, no colour calculation, and its colours are the bank's
// RGB lookup table, which VDP2 doesn't blend either), 8 + 2 while hidden
#ifndef MOD_WIGHT_SPIRIT_DRAWMODE
#define MOD_WIGHT_SPIRIT_DRAWMODE DRAW_UNK02
#endif

// st_common.h (built in the stage's st_common.c), e_misc.h
s32 UnkCollisionFunc2(s16* sensors);
u8 CheckColliderOffsets(s16* arg0, u8 facing);
void EntityExplosionVariantsSpawner(
    Entity* self, u8 count, u8 params, s16 x, s16 y, u8 index, s16 xGap);

typedef enum {
    WIGHT_INIT = 0,
    WIGHT_TURN = 2, // turn around, walk on
    WIGHT_WALK = 3,
    WIGHT_SLASH = 4,
    WIGHT_HIT = 16, // hit while walking: the spirit knocked out and back
    WIGHT_DEATH = 18,
    // Saturn step 32: a frame viewer (pad L/R change the frame, prints
    // "CHARAL:%08X(%d)"), debug only: not ported
} WightSteps;

typedef struct {
    Primitive* prim; // Saturn 0x7C: the dying spirit
    s16 walkTimer;   // Saturn 0x94
    s16 deathTimer;  // Saturn 0x96
    s16 hitboxState; // Saturn 0xA4: saved while knocked back
    u8 backFacing;   // Saturn 0xAC: facingLeft ^ 1
    u8 pad;
    s16 baseX;      // the spirit's centre (PS1 screen x)
    s16 sx[4];      // its corners' x, Saturn space from baseX (TL TR BR BL)
    s32 trailTimer; // Saturn 0x9C (the afterimage)
    // MOD_WIGHT_PS1_GLOW / _SPIRIT: the Wight's glow and spirit entities,
    // and on those the Wight they belong to
    struct Entity* glow;
    struct Entity* spirit;
    struct Entity* owner;
} ET_ModWight;
#define WIGHT_EXT(e) ((ET_ModWight*)&(e)->ext)

// Saturn 0x060EA2BC.., (duration, frame) as the PS1's
static u8 wight_anim_walk[] = { // 0x060EA2BC (loops)
    0x0C, 0x01, 0x2A, 0x02, 0x0A, 0x03, 0x0A, 0x04, 0x0E, 0x05,
    0x0E, 0x06, 0x0C, 0x07, 0x2A, 0x08, 0x06, 0x09, 0x06, 0x0A,
    0x06, 0x0B, 0x0E, 0x0C, 0x0E, 0x0D, 0x00, 0x00};
static u8 wight_anim_slash[] = { // 0x060EA2D8 (the body, sword-less)
    0x05, 0x08, 0x05, 0x0E, 0x05, 0x0F, 0x05, 0x10, 0x07, 0x11, 0x09,
    0x12, 0x08, 0x13, 0x08, 0x14, 0x09, 0x15, 0x09, 0x16, 0x0B, 0x17,
    0x07, 0x18, 0x07, 0x19, 0x03, 0x1A, 0x02, 0x1B, 0x02, 0x48, 0x02,
    0x1C, 0x02, 0x49, 0x02, 0x1D, 0x02, 0x1E, 0x02, 0x1F, 0x02, 0x20,
    0x02, 0x21, 0x03, 0x22, 0x20, 0x21, 0x03, 0x23, 0x04, 0x24, 0x04,
    0x25, 0x05, 0x26, 0x05, 0x08, 0xFF, 0xFF};
static u8 wight_anim_sword[] = { // 0x060EA316 (the sword, same timing)
    0x05, 0x00, 0x05, 0x4A, 0x05, 0x4B, 0x05, 0x4C, 0x07, 0x4D, 0x09,
    0x4E, 0x08, 0x4F, 0x08, 0x50, 0x09, 0x51, 0x09, 0x52, 0x0B, 0x53,
    0x07, 0x54, 0x07, 0x55, 0x03, 0x56, 0x02, 0x57, 0x02, 0x58, 0x02,
    0x59, 0x02, 0x5A, 0x02, 0x5B, 0x02, 0x5C, 0x02, 0x5D, 0x02, 0x5E,
    0x02, 0x5F, 0x03, 0x60, 0x20, 0x5F, 0x03, 0x61, 0x04, 0x62, 0x04,
    0x63, 0x05, 0x64, 0x05, 0x00, 0xFF, 0xFF};
static u8 wight_anim_hit[] = { // 0x060EA354 (the spirit out and back)
    0x02, 0x0C, 0x02, 0x27, 0x02, 0x28, 0x02, 0x29, 0x02, 0x2A, 0x03, 0x2B,
    0x08, 0x2C, 0x04, 0x2D, 0x05, 0x2E, 0x06, 0x2F, 0x07, 0x30, 0x04, 0x31,
    0x05, 0x32, 0x05, 0x33, 0x05, 0x34, 0x12, 0x35, 0x07, 0x36, 0x05, 0x37,
    0x05, 0x38, 0x09, 0x39, 0x07, 0x3A, 0x40, 0x0C, 0xFF, 0xFF};
static u8 wight_anim_death[] = { // 0x060EA382 (the skeleton collapses)
    0x19, 0x0C, 0x03, 0x27, 0x03, 0x28, 0x03, 0x29, 0x18, 0x2A,
    0x06, 0x3B, 0x06, 0x3C, 0x06, 0x3D, 0x06, 0x3E, 0x06, 0x3F,
    0x06, 0x40, 0x06, 0x41, 0x06, 0x42, 0x19, 0x43, 0xFF, 0xFF};

// Saturn 0x060EA26A / 0x060EA270: CheckColliderOffsets (ahead / behind)
static s16 wight_wall_front[] = {SAT_X(24), 28, 0xFF};
static s16 wight_wall_dying[] = {SAT_X(32), 28, 0xFF};
// Saturn 0x060EA27C / 0x060EA284: UnkCollisionFunc2 (walk / dying)
static s16 wight_sensors[] = {0, 32, SAT_X(8), 0};
static s16 wight_sensors_dying[] = {0, 32, SAT_X(-8), 0};
// Saturn 0x060EA3AE: the palette cycle (offsets from the bank's palette)
static u8 wight_pal_cycle[] = {0, 2, 1, 2};
// Saturn 0x060EA3E0: the spirit's rectangle {left, top, width, height}
static u8 wight_spirit_rect[] = {16, 18, 32, 48};
// Saturn 0x060EA3E4 / 0x060EA414: the sword's hitbox per frame 77-100
// {offX, -offY} and {width, height} (Saturn space: SAT_X at runtime)
static s8 wight_sword_off[24][2] = {
    {15, -18},  {19, -16},  {21, -16},  {22, -8},   {27, 0},    {25, 1},
    {23, 1},    {21, 11},   {13, 20},   {2, 26},    {-7, 29},   {-20, 28},
    {-32, 20},  {-48, 14},  {-64, 0},   {-52, -8},  {-40, -20}, {-32, -22},
    {-28, -24}, {-26, -26}, {-24, -24}, {-22, -24}, {-16, -25}, {-16, -24},
};
static u8 wight_sword_size[24][2] = {
    {2, 10},  {2, 10},  {3, 10},  {4, 9},   {5, 8},   {5, 7},
    {6, 6},   {7, 6},   {8, 6},   {9, 8},   {10, 11}, {10, 15},
    {20, 18}, {24, 18}, {48, 16}, {28, 12}, {20, 10}, {16, 8},
    {12, 6},  {10, 5},  {8, 4},   {6, 6},   {4, 6},   {3, 7},
};

static void WightFacePlayer(Entity* self) {
    self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
}

static void WightSetStep(Entity* self, u16 step) {
    self->velocityX = 0;
    self->velocityY = 0;
    self->pose = 0;
    self->poseTimer = 0;
    self->step = step;
    self->step_s = 0;
}

// Saturn: (u32)(g_Timer * 0xAAAAAAAB >> 32) & 12 indexes a word table:
// floor(2t/3) / 4 & 3
static u8 WightPalCycle(void) {
    return wight_pal_cycle[((g_Timer * 2 / 3) >> 2) & 3];
}

// the spirit polygon: Saturn vertices TL TR BR BL -> PS1 0 1 3 2
static void WightSpiritX(Entity* self) {
    ET_ModWight* ext = WIGHT_EXT(self);
    Primitive* prim = ext->prim;

    prim->x0 = ext->baseX + WIGHT_SAT_XR(ext->sx[0]);
    prim->x1 = ext->baseX + WIGHT_SAT_XR(ext->sx[1]);
    prim->x3 = ext->baseX + WIGHT_SAT_XR(ext->sx[2]);
    prim->x2 = ext->baseX + WIGHT_SAT_XR(ext->sx[3]);
}

static void WightSpiritFlip(Entity* self) {
    Primitive* prim = WIGHT_EXT(self)->prim;

    if (self->facingLeft) {
        prim->u0 = prim->u2 =
            MOD_WIGHT_SPIRIT_TEX_U + MOD_WIGHT_SPIRIT_TEX_W - 1;
        prim->u1 = prim->u3 = MOD_WIGHT_SPIRIT_TEX_U - 1;
    } else {
        prim->u0 = prim->u2 = MOD_WIGHT_SPIRIT_TEX_U;
        prim->u1 = prim->u3 = MOD_WIGHT_SPIRIT_TEX_U + MOD_WIGHT_SPIRIT_TEX_W;
    }
}

// a hit wall flips the Wight (and the spirit's texture)
static void WightTurnBack(Entity* self, s32 withPrim) {
    self->velocityX = 0;
    self->facingLeft ^= 1;
    if (withPrim) {
        WightSpiritFlip(self);
    }
}

#if defined(MOD_WIGHT_PS1_SPIRIT) || defined(MOD_WIGHT_PS1_GLOW)
// PS1-style effects (include/game.h switches): E_MOD_WIGHT_TRAIL entities
// with these params, from the trail slots (160-191)
#define WIGHT_FX_GLOW 0x100
#define WIGHT_FX_SPIRIT 0x200
// the death spirit's frame: the standing pose (its feet at +33)
#define WIGHT_SPIRIT_FRAME 12
#define WIGHT_SPIRIT_FEET 33

static Entity* WightFx(Entity* self, u16 params) {
    Entity* fx = AllocEntity(&g_Entities[160], &g_Entities[192]);

    if (fx != NULL) {
        CreateEntityFromEntity(E_MOD_WIGHT_TRAIL, self, fx);
        fx->params = params;
        fx->facingLeft = self->facingLeft;
        WIGHT_EXT(fx)->owner = self;
    }
    return fx;
}
#endif

void EntityModWight(Entity* self) {
    u16 init[5];
    ET_ModWight* ext = WIGHT_EXT(self);
    Primitive* prim;
    Entity* ent;
    s16 primIndex;
    s16 dist;
    s16 x;
    u8 v;
    s32 i;

    if (self->hitParams && (self->step & 1)) {
        WightSetStep(self, WIGHT_HIT);
    }
    if ((self->flags & FLAG_DEAD) && self->step != WIGHT_DEATH) {
        WightSetStep(self, WIGHT_DEATH);
        self->hitboxState = 0;
    }

    switch (self->step) {
    case WIGHT_INIT:
        init[0] = MOD_WIGHT_ANIMSET;
        init[1] = 0;
        init[2] = MOD_WIGHT_UNK5A;
        init[3] = 0;     // the frames carry the palette block's cluts
        init[4] = 0x1A7; // "Wight"
        InitializeEntity(init);
        self->hitboxWidth = SAT_X(8);
        self->hitboxHeight = 16;
        self->hitboxOffY += 8;
        DestroyEntity(self + 1);
        CreateEntityFromCurrentEntity(E_MOD_WIGHT_SWORD, self + 1);
#if defined(MOD_WIGHT_PS1_GLOW)
        ext->glow = WightFx(self, WIGHT_FX_GLOW);
#endif
        WightFacePlayer(self);
        WightSetStep(self, WIGHT_WALK);
        self->animCurFrame = 0;
        break;

    case WIGHT_TURN:
        self->facingLeft ^= 1;
        WightSetStep(self, WIGHT_WALK);
        break;

    case WIGHT_WALK:
        dist = GetDistanceToPlayerX();
        if (self->step_s == 0) {
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(1.0 / 16));
            } else {
                self->velocityX = SAT_X(-FIX(1.0 / 16));
            }
            ext->walkTimer = 540;
            self->step_s++;
        }
        if (dist <= SAT_X(111) && ext->walkTimer <= 479 && (Random() & 1)) {
            WightSetStep(self, WIGHT_SLASH);
            break;
        }
        AnimateEntity(wight_anim_walk, self);
        UnkCollisionFunc2(wight_sensors);
        if (CheckColliderOffsets(wight_wall_front, self->facingLeft)) {
            WightSetStep(self, WIGHT_TURN);
            break;
        }
        ext->walkTimer--;
        if (dist <= SAT_X(143) && (ext->walkTimer % 180) == 0) {
            WightFacePlayer(self);
            self->step_s = 0;
        } else if (ext->walkTimer <= 0) {
            WightSetStep(self, WIGHT_TURN);
        }
        break;

    case WIGHT_SLASH:
        if (self->step_s == 0) {
            WightFacePlayer(self);
            self->step_s++;
        }
        if (self->pose == 14 && self->poseTimer == 0) {
            PlaySfxPositional(MOD_SFX_WIGHT_SLASH);
        }
        if (!AnimateEntity(wight_anim_slash, self)) {
            (self + 1)->params = 0;
            (self + 1)->pose = 0;
            (self + 1)->poseTimer = 0;
            WightSetStep(self, WIGHT_WALK);
        } else if ((self + 1)->params == 0) {
            (self + 1)->params = 1; // the sword swings
        }
        break;

    case WIGHT_HIT:
        if (self->step_s == 0) {
            WightFacePlayer(self);
            // away from the player
            if (self->facingLeft) {
                self->velocityX = SAT_X(-FIX(1.5));
            } else {
                self->velocityX = SAT_X(FIX(1.5));
            }
            ext->hitboxState = self->hitboxState;
            self->hitboxState = 0;
            self->step_s++;
            PlaySfxPositional(SFX_WHIP_TWIRL_SWISH); // 0x6C7
            PlaySfxPositional(SFX_SCRAPE_B);         // 0x616
        }
        if (self->animCurFrame > 48) {
            self->velocityX = self->velocityX * 15 / 16;
        }
        if (self->animCurFrame == 52) {
            self->velocityX = 0;
        }
        if (!AnimateEntity(wight_anim_hit, self)) {
            WightSetStep(self, WIGHT_WALK);
            self->hitboxState = ext->hitboxState;
            break;
        }
        UnkCollisionFunc2(wight_sensors);
        ext->backFacing = self->facingLeft ^ 1;
        if (self->velocityX != 0 &&
            CheckColliderOffsets(wight_wall_front, ext->backFacing)) {
            WightTurnBack(self, 0);
        }
        break;

    case WIGHT_DEATH:
        prim = ext->prim;
        switch (self->step_s) {
        case 0:
            WightFacePlayer(self);
            if (self->facingLeft) {
                self->velocityX = SAT_X(-FIX(1.5));
            } else {
                self->velocityX = SAT_X(FIX(1.5));
            }
#if defined(MOD_WIGHT_PS1_SPIRIT)
            ext->spirit = NULL;
            self->step_s++;
            break;
#endif
            primIndex = g_api.AllocPrimitives(PRIM_GT4, 1);
            if (primIndex == -1) {
                DestroyEntity(self);
                return;
            }
            self->flags |= FLAG_HAS_PRIMS;
            self->primIndex = primIndex;
            prim = ext->prim = &g_PrimBuf[primIndex];
            prim->type = PRIM_GT4;
            prim->r0 = prim->g0 = prim->b0 = 0x80;
            prim->r1 = prim->g1 = prim->b1 = 0x80;
            prim->r2 = prim->g2 = prim->b2 = 0x80;
            prim->r3 = prim->g3 = prim->b3 = 0x80;
            prim->tpage = MOD_WIGHT_SPIRIT_TEX_TPAGE;
            prim->v0 = prim->v1 = MOD_WIGHT_SPIRIT_TEX_V;
            prim->v2 = prim->v3 =
                MOD_WIGHT_SPIRIT_TEX_V + MOD_WIGHT_SPIRIT_TEX_H;
            prim->priority = self->zPriority - 2;
            prim->drawMode = MOD_WIGHT_SPIRIT_DRAWMODE | DRAW_HIDE;
            WightSpiritFlip(self);
            self->step_s++;
            /* fallthrough */
        case 1:
            if (self->animCurFrame <= 41 && self->animCurFrame != 12) {
                self->velocityX = self->velocityX * 9 / 10;
            }
            if (self->animCurFrame == 59) {
                self->velocityX = 0;
            }
            if (!AnimateEntity(wight_anim_death, self)) {
                self->step_s++;
#if !defined(MOD_WIGHT_PS1_SPIRIT)
                WightSpiritFlip(self);
#endif
                break;
            }
#if defined(MOD_WIGHT_PS1_SPIRIT)
            if (self->animCurFrame == 59 && ext->spirit == NULL) {
                // the spirit stands over the fallen skeleton
                ext->spirit = WightFx(self, WIGHT_FX_SPIRIT);
            }
#else
            if (self->animCurFrame == 59) {
                // the spirit appears over the fallen skeleton
                ext->baseX = self->posX.i.hi;
                ext->sx[0] = ext->sx[3] = -wight_spirit_rect[0];
                ext->sx[1] = ext->sx[2] =
                    -wight_spirit_rect[0] + wight_spirit_rect[2];
                WightSpiritX(self);
                prim->y0 = prim->y1 = self->posY.i.hi - wight_spirit_rect[1];
                prim->y2 = prim->y3 = prim->y0 + wight_spirit_rect[3];
                prim->drawMode = MOD_WIGHT_SPIRIT_DRAWMODE;
            }
#endif
            if (self->velocityX != 0 &&
                !(u8)UnkCollisionFunc2(wight_sensors_dying)) {
#if defined(MOD_WIGHT_PS1_SPIRIT)
                WightTurnBack(self, 0);
#else
                WightTurnBack(self, 1);
#endif
            }
            ext->backFacing = self->facingLeft ^ 1;
            if (self->velocityX != 0 &&
                CheckColliderOffsets(wight_wall_dying, ext->backFacing)) {
#if defined(MOD_WIGHT_PS1_SPIRIT)
                WightTurnBack(self, 0);
#else
                WightTurnBack(self, 1);
#endif
            }
            break;
        case 2:
#if defined(MOD_WIGHT_PS1_SPIRIT)
            // the spirit entity stretches itself on the same 26 ticks
            if (ext->deathTimer <= 25) {
                ext->deathTimer++;
            } else {
                self->step_s++;
            }
            break;
#endif
            // the spirit is pulled up: widens, then narrows to a line
            if (ext->deathTimer <= 11) {
                prim->y0 += 4;
                prim->y1 += 4;
                ext->sx[0] -= 2;
                ext->sx[1] += 2;
                ext->sx[3] -= 4;
                ext->sx[2] += 4;
            } else if (ext->deathTimer <= 17) {
                prim->y0 -= 6;
                prim->y1 -= 6;
                ext->sx[0] += 6;
                ext->sx[1] -= 6;
                ext->sx[3] += 8;
                ext->sx[2] -= 8;
            } else if (ext->deathTimer <= 25) {
                prim->y0 -= 12;
                prim->y1 -= 12;
                if (ext->sx[0] < ext->sx[1]) {
                    ext->sx[0] += 4;
                    ext->sx[1] -= 4;
                } else {
                    ext->sx[1] = ext->sx[0] + 1;
                }
                if (ext->sx[3] < ext->sx[2]) {
                    ext->sx[3] += 6;
                    ext->sx[2] -= 6;
                } else {
                    ext->sx[2] = ext->sx[3] + 1;
                }
            } else {
                prim->drawMode |= DRAW_HIDE;
                self->step_s++;
                break;
            }
            WightSpiritX(self);
            ext->deathTimer++;
            break;
        case 3:
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent != NULL) {
                CreateEntityFromEntity(E_EXPLOSION, self, ent);
                ent->params = 3;
                if (self->facingLeft) {
                    ent->posX.i.hi -= SAT_X(32);
                } else {
                    ent->posX.i.hi += SAT_X(32);
                }
                ent->posY.i.hi += 32;
            }
            ext->deathTimer = 30;
            self->step_s++;
            PlaySfxPositional(MOD_SFX_WIGHT_DEATH);
            break;
        case 4:
            if (--ext->deathTimer <= 0) {
                self->step_s++;
            }
            break;
        case 5:
            DestroyEntity(self);
            return;
        }
#if !defined(MOD_WIGHT_PS1_SPIRIT)
        // the spirit's colours cycle with the body's
        prim->clut = MOD_WIGHT_SPIRIT_TEX_CLUT + WightPalCycle();
#endif
        break;
    }

    // Saturn epilogue: dust at the feet while moving 1 px/frame or more
    if ((self->velocityX >= FIX(1) || self->velocityX <= -FIX(1)) &&
        (g_Timer & 1)) {
        x = Random() & 7;
        v = (Random() & 3) + 1;
        EntityExplosionVariantsSpawner(self, 1, 1, SAT_X(x), 32, v, 0);
        x = -8 | Random();
        v = (Random() & 3) + 1;
        EntityExplosionVariantsSpawner(self, 1, 1, SAT_X(x), 32, v, 0);
    }
    // the shimmer: the bank's palettes 0, 2, 1, 2 (MOD_WIGHT_PAL + offset)
#if defined(MOD_WIGHT_PS1_GLOW)
    // (the copies without the spirit layer: the glow entity draws it)
    self->palette = MOD_WIGHT_BODY_PAL - MOD_WIGHT_PAL + WightPalCycle();
#else
    self->palette = WightPalCycle();
#endif
    // sprite pos = entity pos + (0, 32): baked into the frames
}

// Saturn func_060E3244: the sword (self + 1), drawn while it swings, its
// hitbox following the sword's frames 77-100
void EntityModWightSword(Entity* self) {
    u16 init[5];
    Entity* parent = self - 1;
    Entity* trail;
    u8 i;

    if (parent->entityId != E_MOD_WIGHT || parent->step == WIGHT_DEATH) {
        DestroyEntity(self);
        return;
    }
    if (self->step == 0) {
        init[0] = MOD_WIGHT_ANIMSET;
        init[1] = 0;
        init[2] = MOD_WIGHT_UNK5A;
        init[3] = 0;
        init[4] = 0x18D;
        InitializeEntity(init); // step -> 1
        self->attack /= 3;
        self->zPriority = parent->zPriority + 1;
    }
    self->posX.i.hi = parent->posX.i.hi;
    self->posY.i.hi = parent->posY.i.hi;
    self->facingLeft = parent->facingLeft;
    if (self->params) {
        AnimateEntity(wight_anim_sword, self);
        if (!(g_Timer & 1)) {
            trail = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (trail != NULL) {
                CreateEntityFromEntity(E_MOD_WIGHT_TRAIL, self, trail);
                trail->facingLeft = self->facingLeft;
                trail->animCurFrame = self->animCurFrame;
                trail->zPriority = self->zPriority;
            }
        }
    }
    i = self->animCurFrame - 77;
    if (i <= 23) {
        self->hitboxOffX = SAT_X(wight_sword_off[i][0]);
        self->hitboxOffY = -wight_sword_off[i][1];
        self->hitboxWidth = SAT_X(wight_sword_size[i][0]);
        self->hitboxHeight = wight_sword_size[i][1];
        self->hitboxState = parent->hitboxState;
    } else {
        self->hitboxState = 0;
    }
}

#if defined(MOD_WIGHT_PS1_GLOW)
// the glow: the Wight's frame again, behind it, with only the spirit layer
// (indices 14/15) as additive yellows that pulse with the body's cycle
static void WightGlow(Entity* self) {
    Entity* owner = WIGHT_EXT(self)->owner;

    if (owner->entityId != E_MOD_WIGHT || WIGHT_EXT(owner)->glow != self) {
        DestroyEntity(self);
        return;
    }
    if (self->step == 0) {
        self->animSet = owner->animSet;
        self->unk5A = owner->unk5A;
        self->blendMode = BLEND_ADD;
        self->step = 1;
    }
    self->posX.val = owner->posX.val;
    self->posY.val = owner->posY.val;
    self->facingLeft = owner->facingLeft;
    self->animCurFrame = owner->animCurFrame;
    self->zPriority = owner->zPriority - 1;
    self->palette = MOD_WIGHT_GLOW_PAL - MOD_WIGHT_PAL + WightPalCycle();
}
#endif

#if defined(MOD_WIGHT_PS1_SPIRIT)
// the death spirit: the standing frame in yellow, additive, over the fallen
// skeleton; when the Wight's step_s 2 starts it does the Saturn polygon's
// 26 ticks with its scale (pivot at the feet): flattened wide, then pulled up
// into a thin line. sx[0] height, sx[1] top width, sx[2] bottom width
// (Saturn units, 48 and 32 at scale 1), deathTimer its tick.
static void WightSpirit(Entity* self) {
    ET_ModWight* ext = WIGHT_EXT(self);
    Entity* owner = ext->owner;
    s32 t;
    s32 w;

    if (owner->entityId != E_MOD_WIGHT || WIGHT_EXT(owner)->spirit != self) {
        DestroyEntity(self);
        return;
    }
    if (self->step == 0) {
        self->animSet = owner->animSet;
        self->unk5A = owner->unk5A;
        self->animCurFrame = WIGHT_SPIRIT_FRAME;
        self->zPriority = owner->zPriority - 2;
        self->blendMode = BLEND_ADD;
        self->drawFlags = ENTITY_SCALEX | ENTITY_SCALEY;
        self->scaleX = self->scaleY = 0x100;
        self->rotPivotY = WIGHT_SPIRIT_FEET;
        // a stage entity, so it scrolls with the room (it skips
        // InitializeEntity, which sets this from the EnemyDef)
        self->flags |= FLAG_POS_CAMERA_LOCKED;
        ext->sx[0] = 48;
        ext->sx[1] = ext->sx[2] = 32;
        ext->deathTimer = 0;
        self->step = 1;
    }
    self->palette = PAL_FLAG(MOD_WIGHT_SPIRIT_PAL + WightPalCycle());
    if (owner->step_s < 2) {
        return;
    }
    t = ext->deathTimer++;
    if (t > 25) {
        DestroyEntity(self);
        return;
    }
    if (t <= 11) {
        ext->sx[0] -= 4;
        ext->sx[1] += 4;
        ext->sx[2] += 8;
    } else if (t <= 17) {
        ext->sx[0] += 6;
        ext->sx[1] -= 12;
        ext->sx[2] -= 16;
    } else {
        ext->sx[0] += 12;
        ext->sx[1] = ext->sx[1] > 1 ? ext->sx[1] - 8 : 1;
        ext->sx[2] = ext->sx[2] > 1 ? ext->sx[2] - 12 : 1;
    }
    w = (MAX(ext->sx[1], 1) + MAX(ext->sx[2], 1)) / 2;
    self->scaleX = MAX(w * 0x100 / 32, 8);
    self->scaleY = MAX(ext->sx[0], 0) * 0x100 / 48;
}
#endif

// Saturn func_060E3438: an afterimage of the swinging sword, drifting up and
// back while it fades
void EntityModWightTrail(Entity* self) {
    u16 init[5];
    ET_ModWight* ext = WIGHT_EXT(self);
    s16 frame;

#if defined(MOD_WIGHT_PS1_GLOW)
    if (self->params == WIGHT_FX_GLOW) {
        WightGlow(self);
        return;
    }
#endif
#if defined(MOD_WIGHT_PS1_SPIRIT)
    if (self->params == WIGHT_FX_SPIRIT) {
        WightSpirit(self);
        return;
    }
#endif

    switch (self->step) {
    case 0:
        frame = self->animCurFrame; // set by the sword
        init[0] = MOD_WIGHT_ANIMSET;
        init[1] = frame;
        init[2] = MOD_WIGHT_UNK5A;
        init[3] = 0;
        init[4] = 5;
        InitializeEntity(init); // step -> 1
        self->drawFlags = ENTITY_OPACITY;
        self->opacity = 96;
        self->blendMode = BLEND_NO; // opaque until the mesh (tick 4)
        ext->trailTimer = 0;
        self->zPriority -= 2;
        /* fallthrough */
    case 1:
        if (!(ext->trailTimer & 3)) {
            self->zPriority--;
        }
        if (self->facingLeft) {
            self->posX.val += SAT_X(-0x4000);
        } else {
            self->posX.val += SAT_X(0x4000);
        }
        self->posY.val -= 0xC000;
        if (ext->trailTimer <= 11) {
            self->opacity = 96 - ext->trailTimer * 6;
        } else {
            self->step++;
        }
        if (ext->trailTimer == 4) {
            // Saturn sprite flag 0x80: the VDP1 mesh
            self->blendMode = MOD_WIGHT_TRAIL_BLEND;
        }
        if (ext->trailTimer == 8) {
            self->drawFlags |= ENTITY_BLINK;
        }
        ext->trailTimer++;
        break;
    case 2:
        DestroyEntity(self);
        break;
    }
}
