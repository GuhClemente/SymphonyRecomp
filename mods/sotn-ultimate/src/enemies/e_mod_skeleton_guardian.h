// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Skeleton Guardian (Saturn RSTAGE15 "Hell Garden" entities 0x38-0x3B:
 * func_060EB550 / 060EC174 / 060EC274 / 060EC374), ported to a PS1 stage
 * overlay (Saturn port mod, translated from the SH-2). Include after the stage
 * header and e_misc.h's EntityExplosionVariantsSpawner.
 *
 * A kneeling skeleton knight with a tall shield and a spear (TekiInit 0x1A9
 * "Skeleton Guardian"). It walks at the player and stabs; when hit while
 * walking or stabbing it hops back (no hitbox, red tint) into a guard stance
 * behind its shield; struck in that stance it slides back and lunges. From
 * afar it sometimes charges at the player.
 *
 * Required from the including stage:
 *   MOD_SKGUARDIAN_ANIMSET  animSet of the converted Saturn bank 28
 *                           (0x060F2E08, frames 0x060F33B4: 26 frames, Saturn
 *                           frame n = PSX frame n, 2 palettes)
 *   MOD_SKGUARDIAN_UNK5A    (optional, default 0)
 *   enum entries E_MOD_SKGUARDIAN (Saturn 0x38), E_MOD_SKGUARDIAN_SPEAR
 *   (0x39, self + 1), E_MOD_SKGUARDIAN_SHIELD (0x3A, self + 2),
 *   E_MOD_SKGUARDIAN_DEBRIS (0x3B) and E_EXPLOSION.
 *
 * Entry points:
 *   EntityModSkGuardian(Entity*)        <- E_MOD_SKGUARDIAN (layout entity)
 *   EntityModSkGuardianSpear(Entity*)   <- E_MOD_SKGUARDIAN_SPEAR
 *   EntityModSkGuardianShield(Entity*)  <- E_MOD_SKGUARDIAN_SHIELD
 *   EntityModSkBreederDebris(Entity*)   <- E_MOD_SKGUARDIAN_DEBRIS (the
 *       Skeleton Breeder's debris: the Saturn's 060EC374 is the same code,
 *       frames 0x13-0x1A)
 *
 * The Saturn draws the guardian 32 px below the entity (its epilogue; the
 * frames have the knee/feet at y = 0, the floor sensor is {0, 32}): frames
 * 1-18 are converted +32 (rgdn_assets.py), the debris 19-26 as they are.
 */

#ifndef MOD_SKGUARDIAN_ANIMSET
#error                                                                         \
    "define MOD_SKGUARDIAN_ANIMSET before including e_mod_skeleton_guardian.h"
#endif
#ifndef MOD_SKGUARDIAN_UNK5A
#define MOD_SKGUARDIAN_UNK5A 0
#endif

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif

// 1: the Saturn's tint (drawFlags OPACITY | MASK_G | MASK_B: the red channel
// eases toward 128, or 192 = reddish while it hops back and guards); 0: plain
// drawing. See notes.md (the Saturn's first ease starts from 0: a dark flash).
#ifndef MOD_SKGUARDIAN_TINT
#define MOD_SKGUARDIAN_TINT 1
#endif

// the debris: the breeder's, with the guardian's frames
#define MOD_SKBREEDER_DEBRIS_ONLY
#define MOD_SKBREEDER_ANIMSET MOD_SKGUARDIAN_ANIMSET
#define MOD_SKBREEDER_UNK5A MOD_SKGUARDIAN_UNK5A
#define MOD_SKBREEDER_DEBRIS_FRAME 0x13
#include "e_mod_skeleton_breeder.h"

// st_common.h (built in the stage's st_common.c), e_misc.h
s32 UnkCollisionFunc2(s16* sensors);
void EntityExplosionVariantsSpawner(
    Entity* self, u8 count, u8 params, s16 x, s16 y, u8 index, s16 xGap);

// The sounds are the PS1's own (same ids on both)
#define MOD_SKGUARDIAN_SFX_STAB SFX_WEAPON_SWISH_C    // 0x60C stab / charge
#define MOD_SKGUARDIAN_SFX_DEATH SFX_SKELETON_DEATH_B // 0x62A breaks up
#define MOD_SKGUARDIAN_SFX_STEP SFX_STOMP_HARD_E      // 0x64A charge steps

typedef enum {
    SKG_INIT = 0,
    SKG_WALK = 1,
    SKG_STAB = 3,
    SKG_GUARD = 6,  // behind the shield
    SKG_CHARGE = 8, // wind-up, run, brake
    SKG_HOP = 16,   // hit while walking/stabbing: hops back, no hitbox
    SKG_DEATH = 18,
} SkGuardianSteps;

// Ext fields (Saturn offsets), through SKG_EXT()
typedef struct {
    /* Saturn 0x78 */ u8 pad[0x14];
    /* Saturn 0x8C */ s16 timer;
    /* Saturn 0x8E */ s16 guardTimer;
    /* Saturn 0x90 */ s16 unused90;
    /* Saturn 0x92 */ s16 hitboxState; // saved at init
    /* Saturn 0x94 */ u8 tint;         // target of the red channel
    /* Saturn 0x95 */ u8 pad95[3];
    /* Saturn 0x98 */ u16 tintFixed; // red channel << 8, eased
} ET_ModSkGuardian;
#define SKG_EXT(e) ((ET_ModSkGuardian*)&(e)->ext)

// Saturn 0x060F2DB4.. animations (frames 1:1)
static u8 skg_anim_walk[] = { // 0x060F2DB4 (loops)
    0x0D, 0x01, 0x0D, 0x02, 0x0D, 0x03, 0x0D, 0x02, 0x00, 0x00};
static u8 skg_anim_stab[] = { // 0x060F2DBE
    0x15, 0x04, 0x01, 0x05, 0x02, 0x06, 0x10, 0x07, 0xFF, 0xFF};
static u8 skg_anim_windup[] = { // 0x060F2DC8, the charge's wind-up
    0x13, 0x08, 0x02, 0x09, 0x1A, 0x0A, 0x02, 0x0B, 0x03, 0x0C, 0x03, 0x0D,
    0x03, 0x0C, 0x03, 0x0D, 0x02, 0x0C, 0x02, 0x0D, 0x20, 0x0E, 0xFF, 0xFF};
static u8 skg_anim_run[] = { // 0x060F2DE0 (loops)
    0x07, 0x0F, 0x07, 0x10, 0x07, 0x11, 0x00, 0x00};
static u8 skg_anim_brake[] = { // 0x060F2DE8, back to the walk pose
    0x04, 0x12, 0x05, 0x0E, 0x05, 0x03, 0xFF, 0xFF};
static u8 skg_anim_hop[] = { // 0x060F2DF0, into the guard pose (18)
    0x05, 0x03, 0x05, 0x0E, 0x05, 0x12, 0xFF, 0xFF};

// Saturn 0x060F2DAC {{0,32},{8,0}}: UnkCollisionFunc2 (X scaled)
static s16 skg_sensors[] = {0, 32, SAT_X(8), 0};

// Saturn 0x060F2DF8 / 0x060F2E00: where the 8 pieces appear (X mirrored by
// facing); the same numbers as the breeder's
static s8 skg_debris_off_x[8] = {-3, -4, 8, 7, 0, -22, 8, 12};
static s8 skg_debris_off_y[8] = {-8, -5, -8, 22, 8, 19, -12, 11};

// {w, h, offX, offY} per frame 1..18, Saturn space (SAT_X at runtime)
// Saturn 0x060F2E34: the body
static s8 skg_body_hitbox[18][4] = {
    {7, 16, 0, 8}, {7, 16, 0, 8}, {7, 16, 0, 8}, {7, 16, 0, 8}, {7, 16, 0, 8},
    {7, 16, 0, 8}, {7, 16, 0, 8}, {7, 16, 0, 8}, {7, 16, 0, 8}, {4, 16, 0, 8},
    {4, 16, 0, 8}, {4, 16, 0, 8}, {4, 16, 0, 8}, {4, 16, 0, 8}, {7, 16, 0, 8},
    {7, 16, 0, 8}, {7, 16, 0, 8}, {7, 16, 0, 8},
};
// Saturn 0x060F2E7C: the spear (hurts the player)
static s8 skg_spear_hitbox[18][4] = {
    {6, 6, -24, 21},   {6, 6, -28, 20},   {6, 6, -31, 18},  {6, 6, 4, -34},
    {16, 6, -20, -32}, {8, 12, -38, -14}, {6, 12, -34, 20}, {6, 8, 8, -24},
    {6, 8, 8, -24},    {6, 8, 8, -24},    {8, 8, -16, -18}, {8, 4, -32, 6},
    {8, 4, -32, 4},    {8, 4, -32, 5},    {8, 4, -40, 5},   {8, 4, -41, 3},
    {8, 4, -40, 3},    {8, 4, -32, 4},
};
// Saturn 0x060F2EC4: the shield (takes the player's hits)
static s8 skg_shield_hitbox[18][4] = {
    {0, 0, 0, 0},    {0, 0, 0, 0},    {0, 0, 0, 0},    {0, 0, 0, 0},
    {0, 0, 0, 0},    {0, 0, 0, 0},    {0, 0, 0, 0},    {0, 0, 0, 0},
    {0, 0, -6, 0},   {4, 16, -6, 12}, {4, 16, -6, 12}, {4, 16, -6, 12},
    {4, 16, -6, 12}, {4, 16, -6, 12}, {5, 18, -16, 6}, {5, 18, -16, 6},
    {5, 18, -16, 6}, {4, 16, -8, 12},
};

static void SkgFacePlayer(Entity* self) {
    self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
}

static void SkgSetStep(Entity* self, u8 step) {
    self->velocityX = 0;
    self->velocityY = 0;
    SetStep(step); // pose, poseTimer, step_s = 0
}

static void SkgHitbox(Entity* self, s8 (*table)[4], s32 frame) {
    frame--;
    if ((u16)frame <= 17) {
        self->hitboxWidth = SAT_X(table[frame][0]);
        self->hitboxHeight = table[frame][1];
        self->hitboxOffX = SAT_X(table[frame][2]);
        self->hitboxOffY = table[frame][3];
    } else {
        self->hitboxWidth = 0;
        self->hitboxHeight = 0;
        self->hitboxOffX = 0;
        self->hitboxOffY = 0;
    }
}

// two dust puffs at the feet (Saturn SpawnDestructAnim = PS1
// EntityExplosionVariantsSpawner; X not mirrored by facing)
static void SkgDust(Entity* self) {
    s16 x;
    u8 v;

    x = (Random() & 3) | 8;
    v = (Random() & 3) + 1;
    EntityExplosionVariantsSpawner(self, 1, 1, SAT_X(x), 32, v, 0);
    x = -8 - (Random() & 3);
    v = (Random() & 3) + 1;
    EntityExplosionVariantsSpawner(self, 1, 1, SAT_X(x), 32, v, 0);
}

void EntityModSkGuardian(Entity* self) {
    u16 init[5];
    Entity* ent;
    s32 i;
    s16 dist;
    s32 diff;
    u8 r;

    // hit while walking or stabbing: hop back
    if (self->hitParams && (self->step & 1)) {
        SkgSetStep(self, SKG_HOP);
    }
    if ((self->flags & FLAG_DEAD) && self->step != SKG_DEATH) {
        SkgSetStep(self, SKG_DEATH);
        self->opacity = 0x80;
    }

    switch (self->step) {
    case SKG_INIT:
        init[0] = MOD_SKGUARDIAN_ANIMSET;
        init[1] = 0;
        init[2] = MOD_SKGUARDIAN_UNK5A;
        init[3] = 0;     // frames carry their cluts (2 palettes)
        init[4] = 0x1A9; // "Skeleton Guardian"
        InitializeEntity(init);
        self->hitboxWidth = SAT_X(7);
        self->hitboxHeight = 16;
        self->hitboxOffX = 0;
        self->hitboxOffY = 8;
#if MOD_SKGUARDIAN_TINT
        self->drawFlags = ENTITY_OPACITY | ENTITY_MASK_G | ENTITY_MASK_B;
#endif
        self->opacity = 0x80;
        SKG_EXT(self)->tint = 0x80;
        SkgFacePlayer(self);
        CreateEntityFromEntity(E_MOD_SKGUARDIAN_SPEAR, self, self + 1);
        CreateEntityFromEntity(E_MOD_SKGUARDIAN_SHIELD, self, self + 2);
        SkgSetStep(self, SKG_WALK);
        SKG_EXT(self)->hitboxState = self->hitboxState;
        self->animCurFrame = 0;
        break;

    case SKG_WALK:
        if ((g_Timer % 30) == 0) {
            SkgFacePlayer(self);
        }
        dist = GetDistanceToPlayerX();
        if (dist <= SAT_X(47)) {
            r = Random();
            if ((r % 20) == 0) {
                self->step = SKG_STAB;
            }
        } else if (dist <= SAT_X(95)) {
            r = Random();
            if ((r % 60) == 0) {
                self->step = SKG_STAB;
            }
        } else if (dist <= SAT_X(255)) {
            r = Random();
            if ((r % 60) == 0) {
                self->step = SKG_CHARGE;
            }
        }
        if (self->step != SKG_WALK) {
            SkgFacePlayer(self);
            SkgSetStep(self, self->step);
            break;
        }
        AnimateEntity(skg_anim_walk, self);
        if (self->facingLeft) {
            self->velocityX = SAT_X(FIX(0.75)); // Saturn 0xC000
        } else {
            self->velocityX = SAT_X(-FIX(0.75));
        }
        UnkCollisionFunc2(skg_sensors);
        break;

    case SKG_STAB:
        // pose 1 with timer 0: frame 5 next
        if (self->pose == 1 && self->poseTimer == 0) {
            PlaySfxPositional(MOD_SKGUARDIAN_SFX_STAB);
        }
        if (!AnimateEntity(skg_anim_stab, self)) {
            SkgSetStep(self, SKG_WALK);
        }
        UnkCollisionFunc2(skg_sensors);
        break;

    case SKG_HOP:
        switch (self->step_s) {
        case 0:
            SkgFacePlayer(self);
            // away from the player
            if (self->facingLeft) {
                self->velocityX = SAT_X(-FIX(4));
            } else {
                self->velocityX = SAT_X(FIX(4));
            }
            self->hitboxState = 0;
            SKG_EXT(self)->timer = (Random() % 20) + 50;
            self->step_s++;
            SKG_EXT(self)->tint = 0xC0;
            break;
        case 1:
            self->velocityX = self->velocityX * 24 / 25;
            if ((SKG_EXT(self)->timer & 3) == 0) {
                SkgDust(self);
            }
            AnimateEntity(skg_anim_hop, self);
            if (--SKG_EXT(self)->timer < 0) {
                self->hitboxState = SKG_EXT(self)->hitboxState;
                SkgSetStep(self, SKG_GUARD);
            }
            break;
        }
        UnkCollisionFunc2(skg_sensors);
        break;

    case SKG_GUARD:
        // no animation: the hop left it in the guard pose (frame 18)
        switch (self->step_s) {
        case 0:
            SKG_EXT(self)->guardTimer = (Random() % 60) + 150;
            self->step_s++;
            /* fallthrough */
        case 1:
            if (--SKG_EXT(self)->guardTimer < 0) {
                SkgSetStep(self, SKG_WALK);
                SKG_EXT(self)->tint = 0x80;
            } else if (self->hitFlags & 3) {
                // struck behind the shield
                SkgFacePlayer(self);
                self->hitboxState = 0;
                self->step_s++;
            }
            break;
        case 2:
            // slide back
            if (self->facingLeft) {
                self->velocityX = SAT_X(-FIX(3));
            } else {
                self->velocityX = SAT_X(FIX(3));
            }
            SKG_EXT(self)->timer = (Random() % 20) + 50;
            self->step_s++;
            /* fallthrough */
        case 3:
            self->velocityX = self->velocityX * 32 / 33;
            if ((SKG_EXT(self)->timer & 3) == 0) {
                SkgDust(self);
            }
            if (--SKG_EXT(self)->timer < 0) {
                self->step_s++;
            }
            break;
        case 4:
            // lunge: the charge's run, toward the player
            self->hitboxState = SKG_EXT(self)->hitboxState;
            SkgSetStep(self, SKG_CHARGE);
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(5.25)); // Saturn 0x54000
            } else {
                self->velocityX = SAT_X(-FIX(5.25));
            }
            SKG_EXT(self)->timer = (Random() % 20) + 50;
            self->step_s = 1;
            SKG_EXT(self)->tint = 0x80;
            break;
        }
        UnkCollisionFunc2(skg_sensors);
        break;

    case SKG_CHARGE:
        switch (self->step_s) {
        case 0:
            // pose 5 with timer 0: frame 0x0C next
            if (self->pose == 5 && self->poseTimer == 0) {
                PlaySfxPositional(MOD_SKGUARDIAN_SFX_STAB);
            }
            if (AnimateEntity(skg_anim_windup, self)) {
                break;
            }
            self->pose = 0;
            self->poseTimer = 0;
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(6));
            } else {
                self->velocityX = SAT_X(-FIX(6));
            }
            SKG_EXT(self)->timer = (Random() % 20) + 50;
            self->step_s++;
            break;
        case 1:
            self->velocityX = self->velocityX * 32 / 33;
            AnimateEntity(skg_anim_run, self);
            if ((g_Timer % 7) == 0) {
                PlaySfxPositional(MOD_SKGUARDIAN_SFX_STEP);
            }
            if (--SKG_EXT(self)->timer < 0) {
                self->pose = 0;
                self->poseTimer = 0;
                SKG_EXT(self)->timer = 0;
                self->step_s++;
            }
            break;
        case 2:
            if ((g_Timer % 7) == 0) {
                PlaySfxPositional(MOD_SKGUARDIAN_SFX_STEP);
            }
            self->velocityX /= 2;
            if ((SKG_EXT(self)->timer & 3) == 0) {
                SkgDust(self);
            }
            SKG_EXT(self)->timer++;
            if (!AnimateEntity(skg_anim_brake, self)) {
                SkgSetStep(self, SKG_WALK);
            }
            break;
        }
        UnkCollisionFunc2(skg_sensors);
        break;

    case SKG_DEATH:
        switch (self->step_s) {
        case 0:
            SkgFacePlayer(self);
            for (i = 0; i < 8; i++) {
                ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
                if (ent == NULL) {
                    break;
                }
                CreateEntityFromEntity(E_MOD_SKGUARDIAN_DEBRIS, self, ent);
                ent->facingLeft = self->facingLeft;
                ent->params = i;
                if (self->facingLeft) {
                    ent->posX.i.hi -= SAT_X(skg_debris_off_x[i]);
                } else {
                    ent->posX.i.hi += SAT_X(skg_debris_off_x[i]);
                }
                ent->posY.i.hi += skg_debris_off_y[i];
                ent->zPriority = self->zPriority;
            }
            PlaySfxPositional(MOD_SKGUARDIAN_SFX_DEATH);
            self->step_s++;
            break;
        case 1:
            DestroyEntity(self);
            return;
        }
        break;
    }

    // Saturn epilogue: the red channel eases toward the target (1/32 per
    // frame; tintFixed starts at 0 on the Saturn too)
    if (self->opacity != SKG_EXT(self)->tint) {
        diff = (SKG_EXT(self)->tint << 8) - SKG_EXT(self)->tintFixed;
        SKG_EXT(self)->tintFixed += diff / 32;
        self->opacity = SKG_EXT(self)->tintFixed >> 8;
    }
    // the body's hitbox follows the frame
    SkgHitbox(self, skg_body_hitbox, self->animCurFrame);
    // sprite pos = entity pos + (0, 32): baked into the frames
}

// Saturn func_060EC174: the spear, an invisible hitbox entity in self + 1
void EntityModSkGuardianSpear(Entity* self) {
    u16 init[5];
    Entity* parent = self - 1;

    switch (self->step) {
    case 0:
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = 0x18D;
        InitializeEntity(init); // step -> 1
        self->attack = 40;
        /* fallthrough */
    case 1:
        SkgHitbox(self, skg_spear_hitbox, parent->animCurFrame);
        self->facingLeft = parent->facingLeft;
        self->hitboxState = parent->hitboxState;
        self->posX.i.hi = parent->posX.i.hi;
        self->posY.i.hi = parent->posY.i.hi;
        if (parent->entityId != E_MOD_SKGUARDIAN) {
            DestroyEntity(self);
        }
        break;
    }
}

// Saturn func_060EC274: the shield, an invisible hitbox entity in self + 2
// that the player's attacks hit (hitboxState 2) instead of the guardian
void EntityModSkGuardianShield(Entity* self) {
    u16 init[5];
    Entity* parent = self - 2;

    switch (self->step) {
    case 0:
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = 0x18D;
        InitializeEntity(init); // step -> 1
        self->attack = 40;
        self->hitboxState = 2;
        /* fallthrough */
    case 1:
        SkgHitbox(self, skg_shield_hitbox, parent->animCurFrame);
        self->facingLeft = parent->facingLeft;
        self->posX.i.hi = parent->posX.i.hi;
        self->posY.i.hi = parent->posY.i.hi;
        if (parent->entityId != E_MOD_SKGUARDIAN) {
            DestroyEntity(self);
        }
        break;
    }
}
