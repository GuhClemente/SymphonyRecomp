// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Water Leaper (Saturn STAGE_15 func_060EBC24, entity 0x34) and its spawner
 * (func_060ECB7C, entity 0x35) ported to a PS1 stage overlay (Saturn port
 * mod, translated from the SH-2; see saturn_leaper.c / notes.md).
 * Include after the stage header (st_common helpers live in st_common.c).
 *
 * Required from the including stage:
 *   MOD_LEAPER_ANIMSET   animSet of the converted Saturn bank 27
 *                        (26 frames, 4 palettes; frames carry the cluts)
 *   MOD_LEAPER_UNK5A     (optional, default 0) unk5A / tpage of the animset
 *   enum entries E_MOD_LEAPER (Saturn 0x34), E_MOD_LEAPER_SPAWNER (Saturn
 *   0x35), E_SPLASH_WATER (Saturn 0x18) and E_SURFACING_WATER (Saturn 0x19)
 *   from src/st/water_effects.h (the Garden already has them).
 *   enemy def 413 (0x19D) for the Leaper.
 *
 * Entry points:
 *   EntityModLeaper(Entity*)         <- E_MOD_LEAPER (spawned only)
 *   EntityModLeaperSpawner(Entity*)  <- E_MOD_LEAPER_SPAWNER (layout, room 0)
 */

#ifndef MOD_LEAPER_ANIMSET
#error "define MOD_LEAPER_ANIMSET before including e_mod_leaper.h"
#endif
#ifndef MOD_LEAPER_UNK5A
#define MOD_LEAPER_UNK5A 0
#endif
// Saturn clutBase offset: the Leaper never changes it (one colour scheme;
// per-part palettes 0-3 come from the frames themselves)
#define LEAPER_PAL 0
#define MOD_LEAPER_ENEMY 413 // Saturn TekiInit(self, 0x19D)

#ifndef MOD_LEAPER_E_SPLASH
#define MOD_LEAPER_E_SPLASH E_SPLASH_WATER // Saturn 0x18 (func_060DD37C)
#endif
#ifndef MOD_LEAPER_E_SURFACING
#define MOD_LEAPER_E_SURFACING E_SURFACING_WATER // Saturn 0x19 (060DD898)
#endif

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif
#ifndef SAT_X_INV
#define SAT_X_INV(v) ((v) * 5 / 4)
#endif

// 1: aim (dive, leap) and tilt the sprite from Saturn-space X values
//    (dx * 5/4), then scale X speeds by 4/5: the PS1 path is the exact 4/5
//    horizontal image of the Saturn path.
// 0: use the plain PS1 dx / velocityX (geometrically true on PS1).
#ifndef MOD_LEAPER_AIM_SATURN_SPACE
#define MOD_LEAPER_AIM_SATURN_SPACE 1
#endif
#if MOD_LEAPER_AIM_SATURN_SPACE
#define LEAPER_SAT_DX(v) SAT_X_INV(v)
#else
#define LEAPER_SAT_DX(v) (v)
#endif

// Saturn 0x060F8240 water rect {1608, 532, 0, 440, 512, 7, 7, 0}: the
// Leaper uses its own copy (surface 440, one pixel above g_WaterXTbl's 441)
#define MOD_LEAPER_WATER_TOP 440
#define MOD_LEAPER_WATER_BOTTOM 512
// room X limits (Saturn 0x646/0x647/0x850 -> 1284/1285/1702)
#define LEAPER_MIN_X SAT_X(0x646)
#define LEAPER_MIN_X_SET SAT_X(0x647)
#define LEAPER_MAX_X SAT_X(0x850)

// Sounds. Saturn 0x777/0x778 exist on PS1 (same ids, Diplocephalus names
// in sfx.h); 0x911-0x913 are Saturn-only and get PS1 stand-ins.
#ifndef MOD_LEAPER_SFX_LEAP
#define MOD_LEAPER_SFX_LEAP MOD_SFX_WATER_SPLASH // Saturn 0x777; spawner leap
#endif
#ifndef MOD_LEAPER_SFX_RIPPLE
#define MOD_LEAPER_SFX_RIPPLE MOD_SFX_WATER_SURFACE // Saturn 0x778; ripple
#endif
#define MOD_LEAPER_SFX_DEATH MOD_SFX_LEAPER_DEATH   // Saturn 0x911
#define MOD_LEAPER_SFX_JUMP_OUT MOD_SFX_LEAPER_LEAP // Saturn 0x912
#define MOD_LEAPER_SFX_FALL_IN MOD_SFX_LEAPER_FALL  // Saturn 0x913

s32 UnkCollisionFunc3(s16* sensors);
void SetSubStep(u8 step_s);

typedef enum {
    LEAPER_INIT = 0,
    LEAPER_SWIM_UP = 1,   // from the bottom until the head leaves the water
    LEAPER_SWIM = 3,      // near the player (|dx| <= 96 Saturn px)
    LEAPER_JUMP = 4,      // out of the water, decelerate, hover
    LEAPER_DIVE = 5,      // dive at the player's locked position
    LEAPER_FALL = 7,      // gave up the dive: falls back
    LEAPER_CRUISE = 8,    // far from the player; also after re-entering
    LEAPER_LEAP = 9,      // forced by the spawner: arc towards the player
    LEAPER_DEATH = 0x10,  // any hit kills it
    LEAPER_BOUNCE = 0x20, // touched the player out of the water
    LEAPER_CORPSE = 0x30, // not reachable from the Saturn code
} LeaperSteps;

typedef struct {
    /* Saturn 0x84 */ s32 prevX;     // pos before the move
    /* Saturn 0x88 */ s32 prevY;     // (water entry check)
    /* Saturn 0x8C */ s32 targetX;   // dive/leap target (step 4 end:
    /* Saturn 0x90 */ s32 targetY;   // PLAYER.posX/Y.val, never read)
    /* Saturn 0x94 */ s16 targetRot; // rotate eases towards it
    /* Saturn 0x9C */ u8 inWater;
    /* Saturn 0xA0 */ u8 timer;
    /* Saturn 0xA2 */ u8 counter; // swim counter / hover wobble phase
    /* pad         */ u8 pad;
    /* Saturn 0x18 */ u16 facing; // the Saturn entity's facingLeft (logic)
} ET_ModLeaper;
// self->facingLeft is the Saturn SPRITE flip (flags 0x10): func_060ECCE8
// copies the logical facing into it only at some points, and the dive
// check compares the two.
// The Saturn's ratan2 returns 0..0x1000, the PS1's -0x800..0x800 (same
// argument order): the Leaper's rotation targets and its "tilt until equal"
// step need the Saturn's range (with the PS1's, a negative target left the
// easing stuck short of it, so the Leaper never leapt)
#define LEAPER_RATAN2(y, x) (ratan2(y, x) & 0xFFF)
#define LEAPER_EXT(e) ((ET_ModLeaper*)&(e)->ext)
#define LEAPER_FACING(e) (LEAPER_EXT(e)->facing)

// Saturn 0x060F8250.. (frames 1:1 with Saturn bank 27)
static u8 leaper_anim_swim[] = { // 0x060F8250, steps 1 and 3
    4, 1, 3, 2, 4, 3, 4, 1, 3, 4, 10, 5, 0, 0};
static u8 leaper_anim_flail[] = { // 0x060F8260, steps 7, 0x20, 0x30
    6, 1, 9, 6, 6, 1, 9, 7, 0, 0};
static u8 leaper_anim_turn[] = { // 0x060F826C, step 8 turn around
    10, 1, 7, 8, 7, 9, 7, 10, 10, 11, 0xFF, 0};
static u8 leaper_anim_dive[] = { // 0x060F8288, step 5 (one frame only)
    3, 14, 2, 17, 3, 18, 3, 14, 2, 19, 3, 20, 0, 0};
static u8 leaper_anim_death[] = { // 0x060F8298, step 0x10
    4, 21, 4, 22, 4, 23, 3, 24, 3, 23, 3, 24, 3, 25, 3, 24, 3, 25, 0xFF, 0};
static u8 leaper_anim_dash[] = { // 0x060F82AC, steps 4 and 9
    1, 1, 1, 2, 1, 3, 1, 1, 1, 4, 1, 5, 0, 0};
static u8 leaper_anim_cruise[] = { // 0x060F82BC, step 8
    10, 1, 5, 4, 12, 5, 5, 4, 0xFF, 0};

// Saturn 0x060F81BC: step 3 velocity by animCurFrame - 1 (facing right)
static Point32 leaper_swim_vel[] = {
    {SAT_X(FIX(0.25)), FIX(0.375)},   {SAT_X(FIX(0.375)), FIX(0.4375)},
    {SAT_X(FIX(1.25)), FIX(-0.4375)}, {SAT_X(FIX(1.375)), FIX(-0.5)},
    {SAT_X(FIX(1.5)), FIX(-0.5)},     {SAT_X(FIX(0.5)), FIX(-0.375)},
};
// Saturn 0x060F81EC: step 8 velocity by animCurFrame - 1 (facing right)
static Point32 leaper_cruise_vel[] = {
    {SAT_X(FIX(0.1875)), FIX(0.25)},   {SAT_X(FIX(0.25)), FIX(-0.375)},
    {SAT_X(FIX(0.625)), FIX(-0.375)},  {SAT_X(FIX(0.75)), FIX(-0.3125)},
    {SAT_X(FIX(0.8125)), FIX(-0.375)}, {SAT_X(FIX(0.25)), FIX(-0.3125)},
};
// Saturn 0x060F821C: UnkCollisionFunc3 sensors {0,16, 0,4, 4,-4, -8,0}
static s16 leaper_sensors[] = {0, 16, 0, 4, SAT_X(4), -4, SAT_X(-8), 0};
// Saturn 0x060F822C: jump-out speeds (negated)
static s32 leaper_jump_vy[] = {FIX(8.5), FIX(8), FIX(7.5), FIX(7), FIX(6.5)};

// Saturn 0x060ECCE8: sprite h-flip <- facingLeft
static void LeaperSyncFlip(Entity* self) {
    self->facingLeft = LEAPER_FACING(self);
}

static void LeaperEaseRotate(Entity* self) {
    if (self->rotate != LEAPER_EXT(self)->targetRot) {
        self->rotate += (LEAPER_EXT(self)->targetRot - self->rotate) >> 3;
    }
}

static void LeaperSavePos(Entity* self) {
    LEAPER_EXT(self)->prevX = self->posX.i.hi;
    LEAPER_EXT(self)->prevY = self->posY.i.hi;
}

// Saturn 0x060EBB2C: crossed (surface - yOff) going down -> back in water
static void LeaperCheckWaterEntry(Entity* self, s16 yOff) {
    Entity* splash;
    s32 splashY;

    splashY = (MOD_LEAPER_WATER_TOP - g_Tilemap.scrollY.i.hi) - yOff;
    if (self->posY.i.hi >= splashY && LEAPER_EXT(self)->prevY < splashY) {
        if (!LEAPER_EXT(self)->inWater) {
            splash = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (splash != NULL) {
                CreateEntityFromCurrentEntity(MOD_LEAPER_E_SPLASH, splash);
                splash->posY.i.hi =
                    MOD_LEAPER_WATER_TOP - g_Tilemap.scrollY.i.hi;
                splash->velocityY = self->velocityY;
            }
            g_api.PlaySfx(MOD_LEAPER_SFX_FALL_IN);
            LEAPER_EXT(self)->inWater = true;
        }
        LEAPER_EXT(self)->counter = 0;
        self->rotate &= 0xFFF;
        LEAPER_EXT(self)->targetRot = 0;
        SetStep(LEAPER_CRUISE);
    }
}

static void LeaperSpawnSplash(Entity* self) {
    Entity* ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
    if (ent != NULL) {
        CreateEntityFromEntity(MOD_LEAPER_E_SPLASH, self, ent);
        ent->posY.i.hi = MOD_LEAPER_WATER_TOP - g_Tilemap.scrollY.i.hi;
    }
}

void EntityModLeaper(Entity* self) {
    u16 init[5];
    Collider col;
    Entity* ent;
    s32 absY, absX;
    s32 dx, dy;
    s32 vxs;
    s32 angle;
    u8 hitFlags;
    u8 old;
    u8 rnd;

    // Saturn 0x060EBC3C
    hitFlags = self->hitFlags;
    if ((hitFlags & 0x80) && !LEAPER_EXT(self)->inWater &&
        self->step != LEAPER_BOUNCE) {
        SetStep(LEAPER_BOUNCE);
    }
    if ((hitFlags & 0x80) && LEAPER_EXT(self)->inWater) {
        if (self->step != LEAPER_SWIM) {
            self->rotate = 0;
            LEAPER_EXT(self)->targetRot = 0;
            SetStep(LEAPER_SWIM);
        }
    } else if (hitFlags || (self->flags & FLAG_DEAD)) {
        if (self->step != LEAPER_DEATH && self->step != LEAPER_BOUNCE) {
            SetStep(LEAPER_DEATH);
        }
    }

    switch (self->step) {
    case LEAPER_INIT:
        init[0] = MOD_LEAPER_ANIMSET;
        init[1] = 1;
        init[2] = MOD_LEAPER_UNK5A;
        init[3] = LEAPER_PAL;
        init[4] = MOD_LEAPER_ENEMY;
        InitializeEntity(init); // step -> 1
        // Saturn sprite flags |= 0x40 and sprite rotate = self->rotate in
        // the epilogue: the sprite always rotates
        self->drawFlags |= ENTITY_ROTATE;
        self->velocityY = FIX(-1);
        self->hitboxWidth = SAT_X(12); // Saturn-space value
        self->hitboxHeight = 10;
        self->hitboxOffY = 3;
        LEAPER_FACING(self) = (GetSideToPlayer() & 1) ^ 1;
        self->animCurFrame = 1;
        LeaperSyncFlip(self);
        LEAPER_EXT(self)->inWater = true;
        break;

    case LEAPER_SWIM_UP:
        MoveEntity();
        AnimateEntity(leaper_anim_swim, self);
        g_api.CheckCollision(
            self->posX.i.hi, (s16)(self->posY.i.hi - 24), &col, 0);
        if (!(col.effects & EFFECT_WATER)) {
            SetStep(LEAPER_CRUISE);
        }
        break;

    case LEAPER_SWIM:
        LeaperEaseRotate(self);
        self->velocityX = leaper_swim_vel[self->animCurFrame - 1].x;
        self->velocityY = leaper_swim_vel[self->animCurFrame - 1].y;
        if (!LEAPER_FACING(self)) {
            self->velocityX = -self->velocityX;
        }
        if (!AnimateEntity(leaper_anim_swim, self)) {
            if (Random() & 1) {
                LEAPER_FACING(self) = (GetSideToPlayer() & 1) ^ 1;
                LeaperSyncFlip(self);
            }
        }
        absY = (s16)(self->posY.i.hi + g_Tilemap.scrollY.i.hi);
        if (absY < MOD_LEAPER_WATER_TOP + 22) {
            self->velocityY = FIX(1.5);
        }
        if (absY > MOD_LEAPER_WATER_BOTTOM) {
            self->posY.i.hi =
                MOD_LEAPER_WATER_BOTTOM - g_Tilemap.scrollY.i.hi - 24;
        }
        MoveEntity();
        // ripple on the surface (absY is from before the move)
        if (self->pose == 3 && self->poseTimer == 1 &&
            absY < MOD_LEAPER_WATER_TOP + 30) {
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent != NULL) {
                g_api.PlaySfx(MOD_LEAPER_SFX_RIPPLE);
                CreateEntityFromEntity(MOD_LEAPER_E_SURFACING, self, ent);
                ent->posY.i.hi = MOD_LEAPER_WATER_TOP - g_Tilemap.scrollY.i.hi;
                if (LEAPER_FACING(self)) {
                    ent->posX.i.hi -= SAT_X(8);
                } else {
                    ent->posX.i.hi += SAT_X(8);
                }
                ent->velocityY = 0;
            }
        }
        if (GetDistanceToPlayerX() > SAT_X(96)) {
            LEAPER_EXT(self)->counter = 0;
            SetStep(LEAPER_CRUISE);
            break;
        }
        old = LEAPER_EXT(self)->counter++;
        if (old <= 32) {
            break;
        }
        LEAPER_EXT(self)->counter = 0;
        if (GetDistanceToPlayerX() <= SAT_X(47)) {
            break;
        }
        goto try_jump;

    case LEAPER_JUMP:
        switch (self->step_s) {
        case 0:
            rnd = Random();
            self->velocityY = -leaper_jump_vy[(u8)(rnd % 5)];
            self->step_s++;
            /* fallthrough */
        case 1: // rise until the head is out (velocityX kept)
            AnimateEntity(leaper_anim_dash, self);
            MoveEntity();
            absY = (s16)((s16)(self->posY.i.hi - 20) + g_Tilemap.scrollY.i.hi);
            if (absY >= MOD_LEAPER_WATER_TOP) {
                break;
            }
            g_api.PlaySfx(MOD_LEAPER_SFX_JUMP_OUT);
            LeaperSpawnSplash(self);
            LEAPER_EXT(self)->inWater = false;
            self->velocityX = 0;
            self->step_s++;
            break;
        case 2: // gravity until the jump slows down
            AnimateEntity(leaper_anim_dash, self);
            UnkCollisionFunc3(leaper_sensors);
            if (self->velocityY > FIX(-2)) {
                LEAPER_EXT(self)->timer = (Random() & 0x3F) | 0x80;
                self->step_s++;
            }
            break;
        case 3: // hover 129-192 frames: rise 0.25 px/f, tremble <= 0.5 px
            AnimateEntity(leaper_anim_dash, self);
            old = LEAPER_EXT(self)->counter++;
            dy = (s16)(rsin((u8)old << 9) * LEAPER_EXT(self)->timer * 32);
            self->posY.val = self->posY.val + FIX(-0.25) - dy;
            LEAPER_EXT(self)->timer--;
            if (LEAPER_EXT(self)->timer != 0xFF) {
                break;
            }
            LEAPER_EXT(self)->targetX = PLAYER.posX.val;
            LEAPER_EXT(self)->targetY = PLAYER.posY.val;
            SetStep(LEAPER_DIVE);
            break;
        }
        break;

    case LEAPER_DIVE:
        if (self->step_s == 0) {
            AnimateEntity(leaper_anim_dive, self);
            absX = PLAYER.posX.i.hi + g_Tilemap.scrollX.i.hi;
            LEAPER_FACING(self) = (GetSideToPlayer() & 1) ^ 1;
            // must not need to turn, the player must be at least 32 px
            // lower and over the pool (Saturn abs X 1639..2123)
            if (LEAPER_FACING(self) != self->facingLeft ||
                self->posY.i.hi + 32 - PLAYER.posY.i.hi > 0 ||
                (u32)(absX - SAT_X(0x667)) > SAT_X(0x1E4)) {
                SetStep(LEAPER_FALL);
                break;
            }
            LEAPER_EXT(self)->targetX = PLAYER.posX.i.hi;
            LEAPER_EXT(self)->targetY = PLAYER.posY.i.hi;
            self->step_s++;
            dx = LEAPER_SAT_DX(LEAPER_EXT(self)->targetX - self->posX.i.hi);
            dy = LEAPER_EXT(self)->targetY - self->posY.i.hi;
            angle = LEAPER_RATAN2(dy, dx);
            // Saturn vx = rcos / 4 * 5 (its own 5/4 widening), vy = rsin
            self->velocityX = SAT_X(rcos(angle) / 4 * 5);
            self->velocityY = rsin(angle);
            LEAPER_EXT(self)->targetRot = angle - 0x800;
            if (LEAPER_FACING(self)) {
                LEAPER_EXT(self)->targetRot = LEAPER_RATAN2(dy, -dx) - 0x800;
            }
            self->rotate = 0;
            break;
        }
        if (self->step_s != 1) {
            break;
        }
        // accelerate x 9/8 while |vx| <= 0x6FFFF (Saturn units)
        if ((u32)(self->velocityX + SAT_X(0x6FFFF)) <=
            (u32)(SAT_X(0x6FFFF) * 2)) {
            self->velocityX += self->velocityX >> 3;
            self->velocityY += self->velocityY >> 3;
        }
        LeaperEaseRotate(self);
        LeaperSavePos(self);
        MoveEntity();
        LeaperCheckWaterEntry(self, 8);
        break;

    case LEAPER_FALL:
        AnimateEntity(leaper_anim_flail, self);
        LeaperSavePos(self);
        FallEntity();
        MoveEntity();
        LeaperCheckWaterEntry(self, 8);
        break;

    case LEAPER_CRUISE:
        if (self->step_s == 0) {
            if (!AnimateEntity(leaper_anim_cruise, self)) {
                if (Random() & 3) {
                    SetSubStep(0);
                } else {
                    SetSubStep(1);
                }
            }
            self->velocityX = leaper_cruise_vel[self->animCurFrame - 1].x;
            self->velocityY = leaper_cruise_vel[self->animCurFrame - 1].y;
            if (!LEAPER_FACING(self)) {
                self->velocityX = -self->velocityX;
            }
            LeaperSyncFlip(self);
        } else if (self->step_s == 1) { // turn around
            self->velocityX = 0;
            self->velocityY = 0;
            if (!AnimateEntity(leaper_anim_turn, self)) {
                LEAPER_FACING(self) = LEAPER_FACING(self) ? 0 : 1;
                SetSubStep(0);
            }
        }
        LeaperEaseRotate(self);
        absY = (s16)(self->posY.i.hi + g_Tilemap.scrollY.i.hi);
        if (absY < MOD_LEAPER_WATER_TOP + 24) {
            self->velocityY = FIX(1.5);
        }
        if (absY > MOD_LEAPER_WATER_BOTTOM) {
            self->posY.i.hi =
                MOD_LEAPER_WATER_BOTTOM - g_Tilemap.scrollY.i.hi - 24;
        }
        MoveEntity();
        if (GetDistanceToPlayerX() <= SAT_X(95)) {
            LEAPER_EXT(self)->counter = 0;
            if (!(Random() & 7)) {
                SetStep(LEAPER_SWIM);
            }
        }
        old = LEAPER_EXT(self)->counter++;
        if (old <= 32) {
            break;
        }
        LEAPER_EXT(self)->counter = 0;
        if (GetDistanceToPlayerX() > SAT_X(127)) {
            break;
        }
    try_jump: // Saturn 0x060EC580: one chance in three
        rnd = Random();
        if ((u8)(rnd % 3) == 0) {
            SetStep(LEAPER_JUMP);
        }
        break;

    case LEAPER_LEAP:
        switch (self->step_s) {
        case 0:
            if (GetDistanceToPlayerX() <= SAT_X(47)) {
                self->rotate = 0;
                LEAPER_EXT(self)->targetRot = 0;
                SetStep(LEAPER_SWIM);
                break;
            }
            LEAPER_EXT(self)->targetX = PLAYER.posX.i.hi;
            LEAPER_EXT(self)->targetY = PLAYER.posY.i.hi;
            self->velocityY = ((Random() & 0xFF) << 8) + FIX(-9.5);
            dx = LEAPER_EXT(self)->targetX - self->posX.i.hi;
            LeaperSyncFlip(self);
            // Saturn vx = dx / 64 px per frame + (-0.5 .. +0.5)
#if MOD_LEAPER_AIM_SATURN_SPACE
            vxs = (SAT_X_INV(dx) << 10) - 0x8000 + ((Random() & 0xFF) << 8);
            self->velocityX = SAT_X(vxs);
#else
            vxs = (dx << 10) + SAT_X(-0x8000 + ((Random() & 0xFF) << 8));
            self->velocityX = vxs;
#endif
            angle = LEAPER_RATAN2(
                self->velocityY, LEAPER_FACING(self) ? -vxs : vxs);
            LEAPER_EXT(self)->targetRot = angle - 0x800;
            self->rotate = 0;
            self->step_s = 1;
            /* fallthrough */
        case 1: // tilt to the jump angle first (still under water)
            AnimateEntity(leaper_anim_dash, self);
            if (self->rotate != LEAPER_EXT(self)->targetRot) {
                self->rotate =
                    self->rotate + 1 +
                    ((LEAPER_EXT(self)->targetRot - self->rotate) >> 3);
            } else {
                self->step_s = 2;
            }
            break;
        case 2:
            AnimateEntity(leaper_anim_dash, self);
            MoveEntity();
            absY = (s16)((s16)(self->posY.i.hi - 20) + g_Tilemap.scrollY.i.hi);
            if (absY >= MOD_LEAPER_WATER_TOP) {
                break;
            }
            g_api.PlaySfx(MOD_LEAPER_SFX_LEAP);
            LeaperSpawnSplash(self);
            LEAPER_EXT(self)->inWater = false;
            self->step_s = 3;
            break;
        case 3: // ballistic arc, nose along the velocity
            AnimateEntity(leaper_anim_dash, self);
            if (LEAPER_FACING(self)) {
                self->rotate = LEAPER_RATAN2(
                    self->velocityY, -LEAPER_SAT_DX(self->velocityX));
            } else {
                self->rotate = LEAPER_RATAN2(
                    self->velocityY, LEAPER_SAT_DX(self->velocityX));
            }
            self->rotate -= 0x800;
            LeaperSavePos(self);
            self->velocityY += FIX(0.25);
            MoveEntity();
            LeaperCheckWaterEntry(self, 8);
            break;
        }
        break;

    case LEAPER_DEATH:
        AnimateEntity(leaper_anim_death, self);
        switch (self->step_s) {
        case 0: // knocked backwards
            if (self->velocityX > 0) {
                self->velocityX += SAT_X(FIX(-4));
            } else if (self->velocityX == 0 && LEAPER_FACING(self)) {
                self->velocityX += SAT_X(FIX(-4));
            } else {
                self->velocityX += SAT_X(FIX(4));
            }
            self->velocityY = -self->velocityY / 2;
            LEAPER_EXT(self)->timer = 80;
            self->hitboxState = 0;
            self->step_s++;
            g_api.PlaySfx(MOD_LEAPER_SFX_DEATH);
            /* fallthrough */
        case 1: // spin and fall for 81 frames
            MoveEntity();
            self->velocityY += FIX(0.1875);
            LEAPER_EXT(self)->timer--;
            if (LEAPER_EXT(self)->timer == 0xFF) {
                self->step_s++;
            }
            if (self->velocityX > 0) {
                self->rotate += 0xC0;
            } else {
                self->rotate -= 0xC0;
            }
            break;
        case 2:
            DestroyEntity(self);
            return;
        }
        break;

    case LEAPER_BOUNCE:
        // (frame 22 is set first, but AnimateEntity below overrides it)
        self->animCurFrame = 22;
        if (self->step_s == 0) {
            self->velocityX = -self->velocityX / 2;
            self->velocityY = -self->velocityY / 4 + FIX(-4);
            LEAPER_EXT(self)->targetRot = 0;
            self->step_s++;
        }
        if (self->step_s == 1) {
            if (self->velocityX > 0) {
                self->rotate -= 0x60;
            } else {
                self->rotate += 0x60;
            }
        }
        AnimateEntity(leaper_anim_flail, self);
        LeaperSavePos(self);
        FallEntity();
        MoveEntity();
        LeaperCheckWaterEntry(self, 8);
        break;

    case LEAPER_CORPSE: // unreachable on Saturn, kept for completeness
        switch (self->step_s) {
        case 0:
            self->hitboxState = 0;
            self->velocityX = self->velocityX / 4;
            self->velocityY = -self->velocityY / 2 + FIX(-1);
            LEAPER_EXT(self)->targetRot = 0x800;
            self->step_s++;
            break;
        case 1:
            LeaperEaseRotate(self);
            AnimateEntity(leaper_anim_flail, self);
            FallEntity();
            MoveEntity();
            g_api.CheckCollision(self->posX.i.hi, self->posY.i.hi, &col, 0);
            if (self->velocityY > 0 && (col.effects & EFFECT_SOLID)) {
                LEAPER_EXT(self)->timer = 32;
                SetSubStep(2);
            }
            break;
        case 2:
            LEAPER_EXT(self)->timer--;
            if (LEAPER_EXT(self)->timer == 0xFF) {
                DestroyEntity(self);
                return;
            }
            break;
        }
        self->animCurFrame = 22;
        break;
    }

    // Saturn epilogue (0x060ECB14): sprite rotate = self->rotate (implicit
    // on PS1 with ENTITY_ROTATE), then keep the Leaper over the pool
    absX = self->posX.i.hi + g_Tilemap.scrollX.i.hi;
    if (self->velocityX < 0) {
        if (absX <= LEAPER_MIN_X) {
            self->posX.i.hi = LEAPER_MIN_X_SET - g_Tilemap.scrollX.i.hi;
        }
    } else if (absX > LEAPER_MAX_X) {
        self->posX.i.hi = LEAPER_MAX_X - g_Tilemap.scrollX.i.hi;
    }
}

// Saturn 0x060ECB7C: EntityMermanSpawner (e_merman2.h) with a Leaper, the
// Saturn pool range, slots 160-167, and a random "leap now" order.
void EntityModLeaperSpawner(Entity* self) {
    u16 init[5];
    Entity* ent;
    s16 xVar;
    s16 yVar;

    if (!self->step) {
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = ENEMY_SPAWNER; // Saturn TekiInit(self, 4)
        InitializeEntity(init);
        self->flags |= FLAG_UNK_2000;
    }
    if (g_Timer & 0x3F) {
        return;
    }
    yVar = g_Tilemap.scrollY.i.hi + PLAYER.posY.i.hi;
    if (yVar < 256) {
        return;
    }
    xVar = g_Tilemap.scrollX.i.hi + PLAYER.posX.i.hi;
    xVar += (Random() & 0xFF) - 0x80; // Saturn: ((rnd & 0xFF) - 0x80) * 5/4
    // Saturn room X 0x646..0x81B (1606..2075)
    if ((u16)(xVar - SAT_X(0x646)) > SAT_X(0x1D5)) {
        return;
    }
    yVar = 496;
    ent = AllocEntity(&g_Entities[160], &g_Entities[168]);
    if (ent != NULL) {
        CreateEntityFromCurrentEntity(E_MOD_LEAPER, ent);
        ent->posX.i.hi = xVar - g_Tilemap.scrollX.i.hi;
        ent->posY.i.hi = yVar - g_Tilemap.scrollY.i.hi;
    }
    if (Random() & 1) {
        return;
    }
    for (ent = &g_Entities[160]; ent < &g_Entities[168]; ent++) {
        if (ent->entityId == E_MOD_LEAPER &&
            (ent->step == LEAPER_SWIM || ent->step == LEAPER_CRUISE)) {
            ent->step = LEAPER_LEAP;
            ent->step_s = 0;
            ent->pose = 0;
            ent->poseTimer = 0;
        }
    }
}
