// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Red Gargoyle (Saturn STAGE_15 func_060E6EA0 / 060E7564 / 060E7780)
 * ported to a PS1 stage overlay (Saturn port mod, translated from the SH-2).
 * Include after the stage header and st_common helpers. See notes.md for the
 * state list and uncertainties.
 *
 * Required from the including stage:
 *   MOD_GARGOYLE_ANIMSET  animSet of the converted Saturn bank 24
 *   (palette variants: entity palette 1 = +0, 2 = +9, 0 = +0x12)
 *   MOD_GARGOYLE_UNK5A    (optional, default 0) unk5A / tpage of the animset
 *   enum entries E_MOD_GARGOYLE (Saturn id 35), E_GARGOYLE_DEBRIS (Saturn 36),
 *   E_GARGOYLE_PEDESTAL (Saturn 37), and E_EXPLOSION (Saturn 2).
 *
 * Entry points:
 *   EntityModGargoyle(Entity*)          <- E_MOD_GARGOYLE, params 0/1/2
 *   EntityModGargoyleDebris(Entity*)    <- E_GARGOYLE_DEBRIS
 *   EntityModGargoylePedestal(Entity*)  <- E_GARGOYLE_PEDESTAL
 */

#ifndef MOD_GARGOYLE_ANIMSET
#error "define MOD_GARGOYLE_ANIMSET before including ps1_gargoyle.h"
#endif
// The converted frames carry the palette block's own cluts (MOD_GARGOYLE_PAL
// on), and the PS1 adds the entity's palette to them: like the Saturn's
// clutBase offset, the entity's palette is only the variant's offset.
#define GARG_PAL 0
#ifndef MOD_GARGOYLE_UNK5A
#define MOD_GARGOYLE_UNK5A 0
#endif

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#define SAT_X(v) ((v) * 4 / 5)
// PS1 -> Saturn space for horizontal deltas (used for the homing angle only)
#define SAT_X_INV(v) ((v) * 5 / 4)

// 1: compute the homing angle from a Saturn-space dx (dx * 5/4) and then
//    scale the resulting X velocity by 4/5. This makes the PS1 path the exact
//    4/5 horizontal image of the Saturn path (angle and turn rate behave the
//    same as on Saturn relative to the room).
// 0: aim with the plain PS1-space dx (geometrically "true" aim on PS1 with
//    an anisotropic X velocity scale; slightly different turning behaviour).
#ifndef MOD_GARGOYLE_AIM_SATURN_SPACE
#define MOD_GARGOYLE_AIM_SATURN_SPACE 1
#endif

// Sounds: the Saturn's 0x68C is the PS1's too; 0x8FE (claw swing) and 0x8FF
// (the shattered statue lands) are Saturn-only, so PS1 sounds stand in
#define MOD_GARGOYLE_SFX_FLAP SFX_WING_FLAP_A        // Saturn 0x68C, in flight
#define MOD_GARGOYLE_SFX_SWING MOD_SFX_GARGOYLE_CLAW // Saturn 0x8FE, claw
#define MOD_GARGOYLE_SFX_DEBRIS MOD_SFX_GARGOYLE_DEBRIS // Saturn 0x8FF

typedef enum {
    GARGOYLE_INIT = 0,
    GARGOYLE_DORMANT = 1, // waits for the player, then plays the wake anim
    GARGOYLE_FLY = 2,
    GARGOYLE_ATTACK = 3,
    GARGOYLE_TAUNT = 4,
    GARGOYLE_DEATH = 5,
} GargoyleSteps;

typedef enum {
    GARGOYLE_FLY_HOMING = 0,
    GARGOYLE_FLY_COAST = 1,
} GargoyleFlySubSteps;

// Ext fields. Accessed through GARG_EXT() so no change to the Ext union is
// needed (PS1 ext area is 0x40 bytes, this uses 0x14).
typedef struct {
    /* Saturn 0x78 */ s16 variant; // = params; selects speed/palette
    /* Saturn 0x7C */ s16 unk7C;   // cleared on wake, never read
    /* Saturn 0x7E */ s16 angle;   // homing angle
    /* pad         */ s16 pad;
    /* Saturn 0x80 */ s32 roomPosX;  // scrollX + posX at wake, never read
    /* Saturn 0x88 */ s16 hitPlayer; // ATTACK: latch, TAUNT: end counter
    /* Saturn 0x8A */ s16 timer;     // FLY: attack-check countdown
    /* Saturn 0xB0 */                // clutBase -> replaced by GARG_PAL
} ET_ModGargoyle;

typedef struct {
    /* Saturn 0x78 */ s16 variant; // copied from the gargoyle (same offset!)
    /* pad         */ s16 pad;
    /* Saturn 0x82 */ s16 explodeOffX; // frame[+6] of the fragment sprite
    /* Saturn 0x86 */ s16 explodeOffY; // frame[+8] of the fragment sprite
} ET_ModGargoyleDebris;

#define GARG_EXT(e) ((ET_ModGargoyle*)&(e)->ext)
#define GARG_DEBRIS_EXT(e) ((ET_ModGargoyleDebris*)&(e)->ext)

// Saturn 0x060F2F10.. (g_Stage15RedGargoyleAnimationData), frames 1:1
#if 0 // 0x060F2F10 "idle": not referenced by any of the three functions
static u8 garg_anim_idle[] = {0x62, 0x01, 0x62, 0x3A, 0x62, 0x3B, 0xFF, 0xFF};
#endif
static u8 garg_anim_wake[] = { // 0x060F2F18, DORMANT
    0x03, 0x02, 0x01, 0x03, 0x01, 0x02, 0x01, 0x03, 0x01, 0x04,
    0x01, 0x03, 0x2B, 0x04, 0x06, 0x05, 0x06, 0x06, 0x06, 0x07,
    0x06, 0x08, 0x29, 0x04, 0x07, 0x09, 0x1A, 0x0A, 0x02, 0x0B,
    0x03, 0x0C, 0x03, 0x0D, 0x04, 0x0E, 0xFF, 0xFF};
static u8 garg_anim_fly[] = { // 0x060F2F3E, FLY (loops)
    0x06, 0x10, 0x04, 0x11, 0x07, 0x12, 0x02, 0x13, 0x05, 0x14, 0x00, 0x00};
static u8 garg_anim_attack[] = { // 0x060F2F4A, ATTACK
    0x0B, 0x15, 0x13, 0x16, 0x02, 0x17, 0x02, 0x18, 0x05, 0x19, 0x0B,
    0x1A, 0x09, 0x1B, 0x07, 0x1C, 0x08, 0x1D, 0x0A, 0x1E, 0xFF, 0xFF};
static u8 garg_anim_taunt[] = { // 0x060F2F60, TAUNT
    0x07, 0x1F, 0x07, 0x20, 0x07, 0x21, 0x07, 0x22, 0x07, 0x23, 0x07,
    0x24, 0x07, 0x25, 0x07, 0x26, 0x07, 0x27, 0x07, 0x28, 0xFF, 0xFF};

// Saturn 0x060F2EF0: constant Y speed of the debris (FIX(2)).
// (0x060F2EEC, the X speed, is overwritten with a random value per fragment
// before use, so it is a local below.)
static s32 garg_debris_speed_y = FIX(2);

// Saturn s16 at (frame table 0x060F4018[n]) + 6 / + 8 for fragment frames
// 0x29..0x38 (first sprite part X/Y offset, see notes). Index = params-0x29.
static s16 garg_debris_explode_off[16][2] = {
    {3, 0}, {3, 0}, {3, 1},  {3, 1}, {4, 1},  {1, 0}, {3, 1}, {2, 2},
    {0, 1}, {2, 0}, {2, -1}, {5, 0}, {2, -2}, {1, 1}, {2, 5}, {3, 1},
};

static void GargoyleSetPalette(Entity* self, s16 variant) {
    switch (variant) {
    case 1:
        self->palette = GARG_PAL;
        break;
    case 2:
        self->palette = GARG_PAL + 9;
        break;
    case 0:
        self->palette = GARG_PAL + 0x12;
        break;
    }
}

void EntityModGargoyle(Entity* self) {
    u16 init[5];
    Entity* ent;
    s32 angle;
    s32 dx;
    s32 speedX;
    s32 i;

    if ((self->flags & FLAG_DEAD) && self->step != GARGOYLE_DEATH) {
        SetStep(GARGOYLE_DEATH);
    }

    switch (self->step) {
    case GARGOYLE_INIT:
        init[0] = MOD_GARGOYLE_ANIMSET;
        init[1] = 2; // Saturn sets animCurFrame = 2 after TekiInit
        init[2] = MOD_GARGOYLE_UNK5A;
        init[3] = GARG_PAL;
        if (self->params == 0) {
            init[4] = 400;
        } else if (self->params == 1) {
            init[4] = 415;
        } else {
            init[4] = 416;
        }
        if (self->params <= 2) {
            InitializeEntity(init); // step -> 1
        } else {
            // Saturn never calls TekiInit for params > 2 (no hitbox)
            self->animSet = MOD_GARGOYLE_ANIMSET;
            self->unk5A = MOD_GARGOYLE_UNK5A;
            self->step = 1;
        }
        self->animCurFrame = 2;
        GARG_EXT(self)->variant = self->params;
        GargoyleSetPalette(self, GARG_EXT(self)->variant);

        // Pedestal in the next slot, at the original position.
        ent = self + 1;
        CreateEntityFromCurrentEntity(E_GARGOYLE_PEDESTAL, ent);
        GARG_EXT(ent)->variant = GARG_EXT(self)->variant;
        self->posY.i.hi -= 27;
        self->step = GARGOYLE_DORMANT;
        /* fallthrough */

    case GARGOYLE_DORMANT:
        if (GetDistanceToPlayerX() > SAT_X(0xDF)) {
            break;
        }
        if (AnimateEntity(garg_anim_wake, self)) {
            break;
        }
        SetStep(GARGOYLE_FLY);
        self->posY.i.hi -= 12;
        GARG_EXT(self)->roomPosX = g_Tilemap.scrollX.i.hi + self->posX.i.hi;
        self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
        GARG_EXT(self)->unk7C = 0;
        GARG_EXT(self)->timer = 16;
        break;

    case GARGOYLE_FLY:
        if ((g_Timer % 24) == 0) {
            PlaySfxPositional(MOD_GARGOYLE_SFX_FLAP);
        }
        AnimateEntity(garg_anim_fly, self);
        MoveEntity();

        switch (self->step_s) {
        case GARGOYLE_FLY_HOMING:
            dx = PLAYER.posX.i.hi - self->posX.i.hi;
#if MOD_GARGOYLE_AIM_SATURN_SPACE
            dx = SAT_X_INV(dx);
#endif
            angle = ratan2(PLAYER.posY.i.hi - self->posY.i.hi, dx);
            if (angle > 0x800) {
                angle -= 0x1000;
            }
            // Saturn speeds: variant 1 = 1 px/f, 0 = 2 px/f, 2 = 3 px/f;
            // turn rates 40 / 16 / 32 per frame. X velocity scaled 4/5.
            switch (GARG_EXT(self)->variant) {
            case 1:
                GARG_EXT(self)->angle =
                    LimitAngleChange(40, GARG_EXT(self)->angle, angle);
                self->velocityX = SAT_X(rcos(GARG_EXT(self)->angle) * 16);
                self->velocityY = rsin(GARG_EXT(self)->angle) * 16;
                break;
            case 0:
                GARG_EXT(self)->angle =
                    LimitAngleChange(16, GARG_EXT(self)->angle, angle);
                self->velocityX = SAT_X(rcos(GARG_EXT(self)->angle) * 32);
                self->velocityY = rsin(GARG_EXT(self)->angle) * 32;
                break;
            case 2:
                GARG_EXT(self)->angle =
                    LimitAngleChange(32, GARG_EXT(self)->angle, angle);
                self->velocityX = SAT_X(rcos(GARG_EXT(self)->angle) * 48);
                self->velocityY = rsin(GARG_EXT(self)->angle) * 48;
                break;
            }
            self->facingLeft = self->velocityX > 0 ? 1 : 0;
            // flying towards the player and close in X: stop steering
            if (self->facingLeft == ((GetSideToPlayer() & 1) ^ 1)) {
                if (GetDistanceToPlayerX() <= SAT_X(31)) {
                    self->step_s = GARGOYLE_FLY_COAST;
                }
            }
            break;
        case GARGOYLE_FLY_COAST:
            // keep the last velocity; resume homing once far again
            if (GetDistanceToPlayerY() > 64) {
                self->step_s = GARGOYLE_FLY_HOMING;
            } else if (GetDistanceToPlayerX() > SAT_X(64)) {
                self->step_s = GARGOYLE_FLY_HOMING;
            }
            break;
        }

        if (--GARG_EXT(self)->timer) {
            break;
        }
        GARG_EXT(self)->timer = 2;
        if (GetDistanceToPlayerY() > 15) {
            break;
        }
        if (GetDistanceToPlayerX() > SAT_X(31)) {
            break;
        }
        SetStep(GARGOYLE_ATTACK);
        self->velocityX = 0;
        self->velocityY = 0;
        GARG_EXT(self)->hitPlayer = 0;
        self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
        break;

    case GARGOYLE_ATTACK:
        // pose 3 with timer 0: the next AnimateEntity starts frame 0x18
        if (self->pose == 3 && self->poseTimer == 0) {
            g_api.PlaySfx(MOD_GARGOYLE_SFX_SWING);
        }
        if (!AnimateEntity(garg_anim_attack, self)) {
            if (GARG_EXT(self)->hitPlayer == 1) {
                SetStep(GARGOYLE_TAUNT);
                self->velocityX = 0;
                self->velocityY = 0;
                GARG_EXT(self)->hitPlayer = 0;
            } else {
                SetStep(GARGOYLE_FLY);
                GARG_EXT(self)->timer = 16;
                self->velocityX = 0;
                self->velocityY = 0;
            }
        }
        // Saturn g_Player.status (+0x3F8) & 0x10000
        if (g_Player.status & PLAYER_STATUS_UNK10000) {
            GARG_EXT(self)->hitPlayer = 1;
        }
        // Saturn values are Saturn-space (its TekiInit widens defs by 5/4)
        if ((u16)(self->animCurFrame - 0x18) <= 2) {
            self->hitboxWidth = SAT_X(22);
            self->hitboxHeight = 14;
            self->hitboxOffX = SAT_X(-16);
        } else {
            self->hitboxWidth = SAT_X(12);
            self->hitboxHeight = 14;
            self->hitboxOffX = SAT_X(-6);
        }
        self->hitboxOffY = 0;
        break;

    case GARGOYLE_TAUNT:
        if (!AnimateEntity(garg_anim_taunt, self)) {
            GARG_EXT(self)->hitPlayer++;
        }
        if (GARG_EXT(self)->hitPlayer > 1) {
            SetStep(GARGOYLE_FLY);
            self->velocityX = 0;
            self->velocityY = 0;
            GARG_EXT(self)->hitPlayer = 0;
            GARG_EXT(self)->timer = 16;
        }
        break;

    case GARGOYLE_DEATH:
        if (self->step_s <= 2) {
            self->step_s++;
            break;
        }
        if (self->step_s != 3) {
            break;
        }
        // 16 fragments in a 45-unit fan, away from the player and upwards
        angle = (GetSideToPlayer() & 1) ? 0xD00 : 0xB00;
        for (i = 0; i < 16; i++) {
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent != NULL) {
                CreateEntityFromEntity(E_GARGOYLE_DEBRIS, self, ent);
                ent->params = i + 0x29;
                GARG_DEBRIS_EXT(ent)->variant = GARG_EXT(self)->variant;
                speedX = (Random() & 0xF) << 14; // Saturn DAT_060F2EEC
                ent->velocityX = SAT_X((rcos(angle) * speedX) >> 12);
                ent->velocityY = (rsin(angle) * garg_debris_speed_y) >> 12;
            }
            if (GetSideToPlayer() & 1) {
                angle += 45;
            } else {
                angle -= 45;
            }
        }
        DestroyEntity(self);
        return;
    }
    // Saturn epilogue (sprite flags/rotate/flip) is implicit on PS1: the
    // entity is drawn from animSet/animCurFrame/facingLeft/palette.
}

void EntityModGargoyleDebris(Entity* self) {
    u16 init[5];
    Collider col;
    Entity* ent;
    s32 x, y;
    s32 idx;

    switch (self->step) {
    case 0:
        init[0] = MOD_GARGOYLE_ANIMSET;
        init[1] = self->params; // fragment frame 0x29..0x38
        init[2] = MOD_GARGOYLE_UNK5A;
        init[3] = GARG_PAL;
        init[4] = ENEMY_PARTICLE; // Saturn TekiInit(self, 2)
        InitializeEntity(init);   // step -> 1
        GargoyleSetPalette(self, GARG_DEBRIS_EXT(self)->variant);
        self->animCurFrame = self->params;
        idx = self->params - 0x29;
        if (idx >= 0 && idx < 16) {
            GARG_DEBRIS_EXT(self)->explodeOffX =
                SAT_X(garg_debris_explode_off[idx][0]);
            GARG_DEBRIS_EXT(self)->explodeOffY =
                garg_debris_explode_off[idx][1];
        }
        break;

    case 1:
        MoveEntity();
        self->velocityY += FIX(0.125); // Saturn 0x2000
        x = self->posX.i.hi;
        y = self->posY.i.hi;
        if (self->params == 0x29) {
            y += 11;
        }
        g_api.CheckCollision(x, y, &col, 0);
        if (col.effects & EFFECT_SOLID) {
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent != NULL) {
                CreateEntityFromEntity(E_EXPLOSION, self, ent);
                ent->posX.i.hi += GARG_DEBRIS_EXT(self)->explodeOffX;
                ent->posY.i.hi += GARG_DEBRIS_EXT(self)->explodeOffY;
                ent->params = 0;
                if (self->params == 0x29) {
                    g_api.PlaySfx(MOD_GARGOYLE_SFX_DEBRIS);
                }
            }
            DestroyEntity(self);
            return;
        }
        if (self->posY.i.hi > 0x110) {
            DestroyEntity(self);
            return;
        }
        break;
    }
}

// Saturn func_060E7780: the statue/pedestal left where the gargoyle sat.
void EntityModGargoylePedestal(Entity* self) {
    u16 init[5];

    if (self->step == 0) {
        init[0] = MOD_GARGOYLE_ANIMSET;
        init[1] = 0;
        init[2] = MOD_GARGOYLE_UNK5A;
        init[3] = GARG_PAL;
        init[4] = ENEMY_INTERACTABLE; // Saturn TekiInit(self, 5)
        InitializeEntity(init);       // step -> 1
        switch (GARG_EXT(self)->variant) {
        case 1:
            self->animCurFrame = 1;
            self->palette = GARG_PAL;
            break;
        case 2:
            self->animCurFrame = 0x3A;
            self->palette = GARG_PAL + 9;
            break;
        case 0:
            self->animCurFrame = 0x3B;
            self->palette = GARG_PAL + 0x12;
            break;
        }
        // behind the gargoyle (the Saturn's zPriority - 2; InitializeEntity's
        // default is center - 0xC) and behind what drops on it (e_collect.h:
        // center - 0x14), not in front of a coin bag landing there
        self->zPriority = g_unkGraphicsStruct.g_zEntityCenter - 0x16;
    }
}
