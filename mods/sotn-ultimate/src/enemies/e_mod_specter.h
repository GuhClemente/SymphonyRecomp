// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Specter (Saturn STAGE_16 entity 0x20, func_060E55B0 with its SetStep
 * func_060E558C inlined) ported to a PS1 stage overlay (Saturn port mod,
 * translated from the SH-2; see saturn_specter.c / notes.md). Include after
 * the stage header (prs.h) and the st_common helpers.
 *
 * A ghost that drifts up and down near the top of the screen until the
 * player comes within ~100 px, then winds up and chases the player for good
 * (2 px/frame, homing every 30 frames, kept on the screen), swinging when it
 * is close and facing the player. A hit knocks it back and makes it fade
 * out and in over 90 frames with no hitbox, then it drifts again. A Holy hit
 * leaves it at 1 HP. When it dies it is knocked back, writhes, then
 * stretches and squashes flat and vanishes.
 *
 * The Saturn never draws its sprite: it shows the frame's image as ONE
 * textured quad (a VDP1 distorted sprite) that leans into its motion and is
 * squashed when it dies. The port does the same with one PRIM_GT4; the
 * entity itself has animSet 0, so the engine draws nothing else.
 *
 * Required from the including stage:
 *   enum entry      E_MOD_SPECTER (Saturn 0x20; layout entity, no children)
 *   enemy def       417 (0x1A1, "Specter"; generated, enemydefs_mod.h)
 *   MOD_SPECTER_PAL the g_ClutIds index of bank 23's palette 0 (the 10
 *                   palettes kept in order as a block: 0 normal, 1 flicker,
 *                   2..9 the fade to black); mod_psx_sprites.h has it
 *   MOD_SFX_SPECTER_A / MOD_SFX_SPECTER_B (Saturn 0x900 / 0x901,
 *                   mod_prs_sfx.h)
 *   g_ModSpecterTex the texture of each frame, declared before this header:
 *       static const s16 g_ModSpecterTex[][5] = { {u, v, w, h, tpage}, ...};
 *     row n = Saturn/PSX frame n of bank 23 (frame table 0x060ECE10), rows
 *     0..24 at least (row 0 unused, all 0; frames 25-29 are never shown).
 *     Frame n (1-24) is one part showing Saturn image n - 1. From the PSX
 *     part the converter made of it (PsxSpriteSheet.frame(), one part):
 *       u = left + 128 * (quad & 1), v = top + 128 * (quad >> 1)
 *           (the texel of the image's top-left corner in its 256x256 4bpp
 *           texture page, quadrant offset included; quad = tileset & 3)
 *       w = width, h = height (the narrowed PSX size, 4/5 of the width)
 *       tpage = tileset >> 2 (the raw prim->tpage, e.g. 0x12 for VRAM x 128,
 *           y 256; 4bpp, blend bits 0)
 *     u + w must stay <= 255 and u >= 1 (the packer's QUAD_EDGE / free
 *     column 0 rules already guarantee it: a flipped quad reads u - 1).
 *   MOD_SPECTER_ANIMSET is NOT needed at run time (only its VRAM art and the
 *     table above): the frames are never drawn by the engine.
 * Optional:
 *   MOD_SPECTER_Z         primitive priority (default: the entity's z from
 *                         InitializeEntity; Saturn: absolute 98) TODO Z
 *   MOD_SPECTER_DRAWMODE  default additive (DRAW_TRANSP with the tpage's
 *                         blend mode 1). The Saturn ORs 0x5000 into the
 *                         colour word (most prims use 0x4000): in the
 *                         game's VDP2 sprite type 5 that is priority
 *                         register 5 (S5PRIN = 6), and VDP2 colour-
 *                         calculates sprite pixels of priority 6 (SPCCCS 1,
 *                         SPCCN 6, SPCCEN 1) in "add as is" mode (CCMD 1),
 *                         set at every stage load (src/saturn/game_3b.c):
 *                         the ghost is added to the background. The PS1
 *                         only blends STP texels: prs_assets.py gives bank
 *                         23's colours the STP bit. See inv/step14/notes.md.
 *
 * Slots: none spawned (no AllocEntity). Primitives: 1 PRIM_GT4 per Specter
 * (3 in room 1), freed by DestroyEntity (FLAG_HAS_PRIMS).
 *
 * Entry point: EntityModSpecter(Entity*) <- E_MOD_SPECTER
 */

#ifndef MOD_SPECTER_PAL
#error "define MOD_SPECTER_PAL (bank 23's palette block) before e_mod_specter.h"
#endif
#ifndef MOD_SPECTER_DRAWMODE
#define MOD_SPECTER_DRAWMODE (DRAW_TPAGE2 | DRAW_TPAGE | DRAW_TRANSP)
#endif
#define MOD_SPECTER_ENEMY 417 // Saturn TekiInit(self, 0x1A1)

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif
#ifndef SAT_X_INV
#define SAT_X_INV(v) ((v) * 5 / 4)
#endif

// Sounds: both Saturn-only, in the Prison's own bank
#define MOD_SPECTER_SFX_DEATH MOD_SFX_SPECTER_B // Saturn 0x901, killed
#define MOD_SPECTER_SFX_BURST MOD_SFX_SPECTER_A // Saturn 0x900, flattens

typedef enum {
    SPECTER_INIT = 0,
    SPECTER_DRIFT = 1,  // bob between topY and bottomY until the player is near
    SPECTER_WINDUP = 5, // one-shot animation, then chase
    SPECTER_CHASE = 7,  // home on the player for good
    SPECTER_HURT = 16,  // knocked back, fades out and in, no hitbox
    SPECTER_DEATH = 18,
} SpecterSteps;

// Ext fields (Saturn offsets), accessed through SPECTER_EXT() (24 bytes of
// the PS1 Ext)
typedef struct {
    /* Saturn 0x7C */ Primitive* prim;
    /* Saturn 0xA8 */ s32 targetVx; // PS1 units
    /* Saturn 0xAC */ s32 targetVy;
    /* Saturn 0x92 */ s16 swinging;
    /* Saturn 0x94 */ s16 unk94; // cleared, never read
    /* Saturn 0x96 */ s16 timer; // death: writhe time, then the squash
    /* Saturn 0x98 */ s16 fade;  // hurt: 90 .. -1
    /* Saturn 0xA4 */ s16 savedHitboxState;
    // Saturn 0xA2 (the sprite's clutBase, saved, never read): dropped
} ET_ModSpecter;
#define SPECTER_EXT(e) ((ET_ModSpecter*)&(e)->ext)

// Saturn bss 0x060EF11C-0x060EF12A: the drift constants, SHARED by every
// Specter (re-rolled whenever one enters SPECTER_DRIFT). Saturn units (the
// accel is scaled where it moves X).
static s32 g_ModSpecterRiseMax; // 0x060EF11C
static s32 g_ModSpecterFallMax; // 0x060EF120 (= RiseMax)
static s16 g_ModSpecterAccel;   // 0x060EF124
static s16 g_ModSpecterAccel2;  // 0x060EF126 (= Accel)
static s16 g_ModSpecterTopY;    // 0x060EF128, 72..87
static s16 g_ModSpecterBottomY; // 0x060EF12A, 104..119

// Saturn 0x060ECA8C.. (g_Stage16SpecterAnimationData), frames 1:1
static u8 specter_anim_windup[] = { // 0x060ECA8C
    0x06, 0x01, 0x05, 0x02, 0x04, 0x03, 0x04, 0x04, 0xFF, 0xFF};
static u8 specter_anim_chase[] = { // 0x060ECA96 (loops)
    0x04, 0x04, 0x04, 0x05, 0x04, 0x06, 0x04, 0x07, 0x00, 0x00};
static u8 specter_anim_hurt[] = { // 0x060ECAA0
    0x08, 0x07, 0x08, 0x08, 0xFF, 0xFF};
static u8 specter_anim_hurt_loop[] = { // 0x060ECAA6 (loops)
    0x08, 0x09, 0x08, 0x0A, 0x08, 0x0B, 0x08, 0x0C, 0x00, 0x00};
static u8 specter_anim_swing[] = { // 0x060ECACE
    0x06, 0x0D, 0x06, 0x0E, 0x06, 0x0F, 0x06, 0x10, 0x06,
    0x11, 0x06, 0x12, 0x06, 0x13, 0x06, 0x14, 0x06, 0x15,
    0x06, 0x16, 0x06, 0x17, 0x06, 0x18, 0xFF, 0xFF};

// Saturn geometry of frames 0..24, Saturn units: {half width, half height}
// of image f - 1 (records at 0x060ECE88, read as 0x060ECE84[f * 4]) and the
// centre {x, y} of the frame's part (frame + 6 / + 8).
static s8 specter_frame_geo[25][4] = {
    {0, 0, 0, 0},     {20, 30, 3, -29}, {20, 30, 2, -29}, {20, 31, 1, -28},
    {20, 31, 1, -28}, {20, 30, 2, -29}, {24, 30, 5, -29}, {24, 31, 5, -28},
    {20, 31, 1, -28}, {20, 30, 2, -29}, {24, 30, 5, -29}, {24, 31, 5, -28},
    {20, 31, 1, -28}, {24, 30, 5, -27}, {32, 29, 4, -27}, {32, 29, 4, -27},
    {32, 29, 5, -27}, {32, 29, 6, -27}, {32, 29, 7, -27}, {32, 29, 6, -27},
    {32, 29, 5, -27}, {32, 29, 5, -27}, {32, 29, 4, -27}, {28, 29, 0, -27},
    {24, 30, 5, -27},
};

static void SpecterSetStep(Entity* self, u16 step) { // func_060E558C
    self->pose = 0;
    self->poseTimer = 0;
    self->step = step;
    self->step_s = 0;
}

static void SpecterFacePlayer(Entity* self) {
    self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
}

static u32 SpecterGetRand(void) { // MTH_GetRand stand-in (low 14 bits used)
    u32 r = Random();
    return (r << 8) | Random();
}

// func_0607B0D0(self, PLAYER) in Saturn space, then the velocity in PS1 units
static void SpecterAim(Entity* self) {
    s32 dx = SAT_X_INV(PLAYER.posX.i.hi - self->posX.i.hi);
    s32 dy = PLAYER.posY.i.hi - self->posY.i.hi;
    u16 angle = ratan2(dy, dx) & 0xFFF;

    SPECTER_EXT(self)->targetVx = SAT_X(rcos(angle) * 32); // 2 px/frame
    SPECTER_EXT(self)->targetVy = rsin(angle) * 32;
}

// Saturn 0x060E5F90: the textured quad of animCurFrame
static void SpecterDraw(Entity* self) {
    Primitive* prim = SPECTER_EXT(self)->prim;
    const s16* tex;
    s8* geo;
    s32 f;
    s32 x;
    s32 y;
    s32 l;
    s32 r;
    s32 t;
    s32 b;
    s32 dx;
    s32 dy;
    s32 a;
    s32 sx;
    s32 sy;

    f = self->animCurFrame;
    if (f < 1 || f > 24 || f >= LEN(g_ModSpecterTex)) {
        prim->drawMode = DRAW_HIDE;
        return;
    }
    tex = g_ModSpecterTex[f];
    geo = specter_frame_geo[f];
    x = self->posX.i.hi;
    y = self->posY.i.hi;
    // Saturn corners relative to the entity (sic: the right / bottom edge
    // adds the part offset, and the 32, twice; see notes.md). VDP1 corners
    // are inclusive, hence the + 1 on the right / bottom.
    if (self->facingLeft) {
        l = -geo[0] - geo[2];
        r = l + geo[0] * 2 - geo[2];
    } else {
        l = -geo[0] + geo[2];
        r = l + geo[0] * 2 + geo[2];
    }
    t = -geo[1] + geo[3] + 32;
    b = t + geo[1] * 2 + geo[3] + 32;
    prim->x0 = prim->x2 = x + SAT_X(l);
    prim->x1 = prim->x3 = x + SAT_X(r + 1);
    prim->y0 = prim->y1 = y + t;
    prim->y2 = prim->y3 = y + b + 1;

    // the palette (the hurt fade sets it in its step; the dying one keeps
    // the last): 0 for 2 frames, 1 for 2 frames (0x060ECB20)
    if (self->step != SPECTER_HURT && self->step != SPECTER_DEATH) {
        prim->clut = MOD_SPECTER_PAL + ((g_Timer & 2) ? 1 : 0);
    }

    // Saturn 0x060E6120: lean into the motion (PS1 velocity -> PS1 px)
    dx = (s16)(self->velocityX >> 13);
    dy = (s16)(self->velocityY >> 13);
    if (self->velocityX > 0) {
        prim->x0 += dx;
        prim->x1 += dx * 2;
        self->hitboxWidth = dx / 2 + SAT_X(16);
        self->hitboxOffX = -dx;
    } else {
        prim->x0 += dx * 2;
        prim->x1 += dx;
        self->hitboxWidth = SAT_X(16) - dx / 2;
        self->hitboxOffX = dx;
    }
    if (self->velocityY < 0) {
        prim->y0 += dy * 2;
        prim->y1 += dy * 2;
        self->hitboxHeight = 12 - dy / 2;
        self->hitboxOffY = dy / 2;
    } else {
        prim->y0 += dy;
        prim->y1 += dy;
        prim->y2 += dy;
        prim->y3 += dy;
        self->hitboxHeight = 12; // (hitboxOffY kept, as on the Saturn)
    }

    // the texture; facingLeft = the Saturn's horizontal flip bit (the PS1
    // sprite renderer's flipped u convention: u - 1)
    if (self->facingLeft) {
        prim->u0 = prim->u2 = tex[0] + tex[2] - 1;
        prim->u1 = prim->u3 = tex[0] - 1;
    } else {
        prim->u0 = prim->u2 = tex[0];
        prim->u1 = prim->u3 = tex[0] + tex[2];
    }
    prim->v0 = prim->v1 = tex[1];
    prim->v2 = prim->v3 = tex[1] + tex[3];
    prim->tpage = tex[4];
    prim->drawMode = MOD_SPECTER_DRAWMODE;

    // Saturn 0x060E624C: the death squash (step_s 3), 24 frames
    if (self->step == SPECTER_DEATH && self->step_s == 3) {
        a = (s16)(SPECTER_EXT(self)->timer * 128);
        if (a <= 0x7FF) {
            sx = (s16)(rsin(a) >> 6);       // 0 .. 64 .. 0
            sy = (s16)(rsin(a >> 1) / 192); // 0 .. 21
        } else if (a <= 0xBFF) {
            sx = (s16)(rsin(a) / 160);             // 0 .. -25
            sy = (s16)((rsin(a >> 1) >> 4) - 232); // 24 .. -51
        } else {
            DestroyEntity(self);
            return;
        }
        sx = SAT_X(sx);
        prim->x0 -= sx;
        prim->x1 += sx;
        prim->x2 -= sx;
        prim->x3 += sx;
        prim->y0 += sy;
        prim->y1 += sy;
        prim->y2 -= sy;
        prim->y3 -= sy;
        if (++SPECTER_EXT(self)->timer > 180) { // never reached
            DestroyEntity(self);
        }
    }
}

void EntityModSpecter(Entity* self) {
    u16 init[5];
    Primitive* prim;
    s32 dx;
    s32 dy;
    s32 fade;
    s32 pal;
    s16 primIndex;

    // Saturn 0x060E55C4: hit while drifting, winding up or chasing
    if (self->hitParams && (self->step & 1)) {
        SpecterSetStep(self, SPECTER_HURT);
    }
    // 0x060E55F8: a Holy hit (5 invincibility frames, by the element table
    // of the hit detection, the same on the PS1) leaves it at 1 HP
    if (self->step != SPECTER_DEATH && self->nFramesInvincibility == 5) {
        self->hitPoints = 1;
    }
    // 0x060E561A: killed
    if ((self->flags & FLAG_DEAD) && self->step != SPECTER_DEATH) {
        SpecterSetStep(self, SPECTER_DEATH);
        self->hitboxState = 0;
    }

    switch (self->step) {
    case SPECTER_INIT:
        init[0] = 0; // no sprite: the quad is the Specter (see the top)
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = MOD_SPECTER_ENEMY;
        InitializeEntity(init);
        self->hitboxWidth = SAT_X(16);
        self->hitboxHeight = 16;
        SpecterFacePlayer(self);
        SpecterSetStep(self, SPECTER_DRIFT);
        primIndex = g_api.AllocPrimitives(PRIM_GT4, 1);
        if (primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->primIndex = primIndex;
        self->flags |= FLAG_HAS_PRIMS;
        prim = SPECTER_EXT(self)->prim = &g_PrimBuf[primIndex];
        prim->type = PRIM_GT4;
        prim->r0 = prim->g0 = prim->b0 = 0x80;
        prim->r1 = prim->g1 = prim->b1 = 0x80;
        prim->r2 = prim->g2 = prim->b2 = 0x80;
        prim->r3 = prim->g3 = prim->b3 = 0x80;
#ifdef MOD_SPECTER_Z
        prim->priority = MOD_SPECTER_Z;
#else
        prim->priority = self->zPriority; // TODO Z (Saturn: absolute 98)
#endif
        prim->clut = MOD_SPECTER_PAL;
        prim->drawMode = MOD_SPECTER_DRAWMODE;
        self->animCurFrame = 1; // read by SpecterDraw only (animSet 0)
        break;

    case SPECTER_DRIFT: // Saturn 0x060E5818
        dx = GetDistanceToPlayerX();
        switch (self->step_s) {
        case 0: // new shared drift constants
            g_ModSpecterAccel2 = g_ModSpecterAccel =
                (SpecterGetRand() & 0x1FF) + 0x700;
            g_ModSpecterFallMax = g_ModSpecterRiseMax =
                (SpecterGetRand() & 0x3FFF) + 0xE000;
            g_ModSpecterTopY = (Random() & 0xF) + 0x48;
            g_ModSpecterBottomY = (Random() & 0xF) + 0x68;
            self->step_s++;
            // fallthrough
        case 1: // rising
            if (self->posY.i.hi < g_ModSpecterTopY) {
                self->velocityY += g_ModSpecterAccel2;
            } else if (self->velocityY > -g_ModSpecterRiseMax) {
                self->velocityY -= g_ModSpecterAccel;
            }
            if (self->velocityY > 0) {
                self->velocityY = 0;
                self->step_s = 2;
                g_ModSpecterBottomY = (Random() & 0xF) + 0x68;
            }
            break;
        case 2: // sinking
            if (self->posY.i.hi > g_ModSpecterBottomY) {
                self->velocityY -= g_ModSpecterAccel;
            } else if (self->velocityY < g_ModSpecterFallMax) {
                self->velocityY += g_ModSpecterAccel2;
            }
            if (self->velocityY < 0) {
                self->velocityY = 0;
                self->step_s = 1;
                g_ModSpecterTopY = (Random() & 0xF) + 0x48;
            }
            break;
        }
        MoveEntity();
        if (dx <= SAT_X(127) && !(g_Timer % 60)) {
            SpecterSetStep(self, SPECTER_WINDUP);
        }
        if (!(g_Timer % 30)) {
            SpecterFacePlayer(self);
        }
        self->velocityX = self->velocityX * 15 / 16;
        break;

    case SPECTER_WINDUP: // Saturn 0x060E59EC
        self->velocityY = self->velocityY * 15 / 16;
        if (!AnimateEntity(specter_anim_windup, self)) {
            SpecterSetStep(self, SPECTER_CHASE);
        }
        break;

    case SPECTER_CHASE: // Saturn 0x060E5A32
        dx = GetDistanceToPlayerX();
        dy = GetDistanceToPlayerY();
        switch (self->step_s) {
        case 0:
            SPECTER_EXT(self)->swinging = 0;
            SPECTER_EXT(self)->unk94 = 0;
            SpecterAim(self);
            self->facingLeft = self->velocityX > 0 ? 1 : 0;
            self->step_s++;
            // fallthrough
        case 1:
            if (!(g_Timer % 30)) {
                SpecterAim(self);
            }
            if (SPECTER_EXT(self)->targetVx > self->velocityX) {
                self->velocityX += SAT_X(g_ModSpecterAccel);
            } else if (SPECTER_EXT(self)->targetVx < self->velocityX) {
                self->velocityX -= SAT_X(g_ModSpecterAccel);
            }
            if (SPECTER_EXT(self)->targetVy > self->velocityY) {
                self->velocityY += g_ModSpecterAccel;
            } else if (SPECTER_EXT(self)->targetVy < self->velocityY) {
                self->velocityY -= g_ModSpecterAccel;
            }
            if ((SPECTER_EXT(self)->targetVx >> 11) ==
                (self->velocityX >> 11)) {
                self->velocityX = SPECTER_EXT(self)->targetVx;
            }
            if ((SPECTER_EXT(self)->targetVy >> 11) ==
                (self->velocityY >> 11)) {
                self->velocityY = SPECTER_EXT(self)->targetVy;
            }
            self->facingLeft = self->velocityX > 0 ? 1 : 0;
            // stay on the screen (Saturn 320 x 240)
            if (self->posY.i.hi < 0) {
                self->velocityY += g_ModSpecterAccel;
            } else if (self->posY.i.hi > 240) {
                self->velocityY -= g_ModSpecterAccel;
            }
            if (self->posX.i.hi < 0) {
                self->velocityX += SAT_X(g_ModSpecterAccel);
            } else if (self->posX.i.hi > SAT_X(320)) {
                self->velocityX -= SAT_X(g_ModSpecterAccel);
            }
            break;
        }
        MoveEntity();
        // close, and facing the player: swing
        if (!SPECTER_EXT(self)->swinging && dx <= SAT_X(47) && dy <= 31 &&
            self->facingLeft == ((GetSideToPlayer() & 1) ^ 1)) {
            self->pose = 0;
            self->poseTimer = 0;
            SPECTER_EXT(self)->swinging = 1;
        }
        if (SPECTER_EXT(self)->swinging) {
            if (!AnimateEntity(specter_anim_swing, self)) {
                self->pose = 0;
                self->poseTimer = 0;
                SPECTER_EXT(self)->swinging = 0;
            }
        } else {
            AnimateEntity(specter_anim_chase, self);
        }
        break;

    case SPECTER_HURT: // Saturn 0x060E5C9E
        switch (self->step_s) {
        case 0:
            SpecterFacePlayer(self);
            self->velocityX =
                self->facingLeft ? SAT_X(-FIX(1.5)) : SAT_X(FIX(1.5));
            SPECTER_EXT(self)->savedHitboxState = self->hitboxState;
            self->hitboxState = 0;
            SPECTER_EXT(self)->fade = 90;
            self->step_s++;
            // fallthrough
        case 1:
            if (!AnimateEntity(specter_anim_hurt, self)) {
                self->pose = 0;
                self->poseTimer = 0;
                self->step_s++;
            }
            break;
        case 2:
            AnimateEntity(specter_anim_hurt_loop, self);
            break;
        }
        // Saturn 0x060E5D4E: fade out (2 .. 9 = black), then back in
        fade = SPECTER_EXT(self)->fade;
        if (fade > 79) {
            pal = 2;
        } else if (fade > 69) {
            pal = 3;
        } else if (fade > 59) {
            pal = 4;
        } else if (fade > 49) {
            pal = 5;
        } else if (fade > 39) {
            pal = 6;
        } else if (fade > 29) {
            pal = 7;
        } else if (fade > 19) {
            pal = 8;
        } else if (fade > 9) {
            pal = 9;
        } else if (fade > 1) {
            pal = fade;
        } else {
            pal = 0;
        }
        SPECTER_EXT(self)->prim->clut = MOD_SPECTER_PAL + pal;
        if (--SPECTER_EXT(self)->fade < 0) {
            SpecterSetStep(self, SPECTER_DRIFT);
            self->hitboxState = SPECTER_EXT(self)->savedHitboxState;
        }
        self->velocityX -= self->velocityX / 60;
        self->velocityY -= self->velocityY / 60;
        MoveEntity();
        break;

    case SPECTER_DEATH: // Saturn 0x060E5E6E
        switch (self->step_s) {
        case 0:
            SpecterFacePlayer(self);
            self->velocityX =
                self->facingLeft ? SAT_X(-FIX(1.5)) : SAT_X(FIX(1.5));
            self->step_s++;
            g_api.PlaySfx(MOD_SPECTER_SFX_DEATH);
            // fallthrough
        case 1:
            if (!AnimateEntity(specter_anim_hurt, self)) {
                self->pose = 0;
                self->poseTimer = 0;
                SPECTER_EXT(self)->timer = 0;
                self->step_s++;
            }
            break;
        case 2:
            AnimateEntity(specter_anim_hurt_loop, self);
            if (++SPECTER_EXT(self)->timer > 50) {
                SPECTER_EXT(self)->timer = 0;
                self->step_s++;
                self->velocityX = 0;
                self->velocityY = 0;
                g_api.PlaySfx(MOD_SPECTER_SFX_BURST);
            }
            break;
        }
        self->velocityX -= self->velocityX / 32;
        self->velocityY -= self->velocityY / 16;
        MoveEntity();
        break;
    }

    if (self->flags & FLAG_HAS_PRIMS) {
        SpecterDraw(self);
    }
}
