// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Wraith (Saturn RSTAGE16 "Soul Prison" entities 0x18-0x1B: func_060E3608 /
 * 060E4664 / 060E4804 / 060E4AFC, enemy def 0x1A8), ported to the Soul
 * Prison overlay (Saturn port mod, translated from the SH-2). Include after the
 * stage header and e_misc.h.
 *
 * A hooded ghost that bobs in the air. It lies in wait (not
 * drawn) until the player comes near, then fades in blinking. Floating, it
 * drifts at random, and now and then dashes at a far player, backs off from
 * a near one, or swoops at him to grab: caught, the player is held while
 * the Wraith drains his soul (sparks flow from him into it, 35 damage),
 * then thrown off. It leaves fading red afterimages wherever it flies.
 * Dying, it shrieks, collapses into a wisp (frame 45) that shrinks spinning
 * while sparks swirl into it, and vanishes.
 *
 * Required from the including stage:
 *   MOD_WRAITH_ANIMSET  animSet of the converted Saturn bank 0x060ECFA0
 *                       (frames 0x060ED788: PSX frame n = Saturn frame n,
 *                       all drawn +32 as the Saturn's sprite is)
 *   MOD_WRAITH_UNK5A    (optional, default 0)
 *   MOD_SFX_WRAITH_GRAB / _DRAIN / _DEATH / _VANISH (Saturn 0x904-0x907)
 *   enum entries E_MOD_WRAITH (Saturn 0x18), E_MOD_WRAITH_AFTERIMAGE
 *   (0x19), E_MOD_WRAITH_DRAIN (0x1A) and E_MOD_WRAITH_WISPS (0x1B), the
 *   helpers in slots 160-191.
 *
 * Entry points:
 *   EntityModWraith(Entity*)            <- E_MOD_WRAITH (layout entity)
 *   EntityModWraithAfterimage(Entity*)  <- E_MOD_WRAITH_AFTERIMAGE
 *   EntityModWraithDrain(Entity*)       <- E_MOD_WRAITH_DRAIN: 32 sparks
 *                                          pulled from the held player
 *   EntityModWraithWisps(Entity*)       <- E_MOD_WRAITH_WISPS: 12 sparks
 *                                          swirling into the dying Wraith
 * Primitives: PRIM_TILE sparks, 32 per drain, 12 per wisps entity
 * (FLAG_HAS_PRIMS). The Saturn allocates them in pairs (a coloured
 * polygon + a hidden one holding the spark's position and speed); here one
 * 2x2 tile each, its unused fields holding that state.
 *
 * Position: the Saturn draws the sprite (and places the hitbox and the
 * helpers) at a bobbing height (Saturn 0x98 = posY - 12 + 8 sin) while its
 * posY stays put; here posY is the logical position during the update and
 * the drawn one between updates (the engine draws and hit-tests there):
 * ext->bob keeps the difference (an offset, so that the engine's camera
 * shifts of posY carry over).
 */

#ifndef MOD_WRAITH_ANIMSET
#error "define MOD_WRAITH_ANIMSET before including e_mod_wraith.h"
#endif
#ifndef MOD_WRAITH_UNK5A
#define MOD_WRAITH_UNK5A 0
#endif

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif
// PS1 -> Saturn space (the sparks steer in Saturn space)
#define WRAITH_SAT_FROM_PS1(v) ((v) * 5 / 4)

// The Wraith and its afterimages are drawn opaque, tinted with
// drawFlags/opacity (the Saturn's gouraud shading; body: OPACITY | MASK_G,
// 144; afterimages: OPACITY | MASK_R, 128 fading). The sprite flag 0x40 the
// Saturn sets on them is the scale/rotate enable (src/saturn/
// update_entity_sprite.h), not translucency; the bank's colours are an RGB
// lookup table, which VDP2 doesn't blend (only colour-code priority 5 is,
// e.g. the Specter's and the Wisp's). From tick 8 the afterimages get
// sprite flag 0x80, the VDP1 MESH bit (a checkerboard: 50% transparency),
// drawn here as a 50% blend (the bank's colours carry the STP bit for it,
// rprs_assets.py).
#ifndef MOD_WRAITH_MESH_BLEND
#define MOD_WRAITH_MESH_BLEND BLEND_TRANSP
#endif

// st_common.h (built in the stage's st_common.c)
s32 GetDistanceToPlayerY(void);

typedef enum {
    WRAITH_INIT = 0,
    WRAITH_FLOAT = 1, // drifts at random, picks an action
    WRAITH_GRAB = 2,  // swoops at the player, holds him, drains him
    WRAITH_DASH = 3,  // dashes at a far player
    WRAITH_RECOVER = 4,
    WRAITH_BACKOFF = 5,
    WRAITH_WAIT = 6, // hidden until the player comes near, then appears
    WRAITH_HIT = 18, // hit while floating, dashing or backing off: = 5
    WRAITH_DEATH = 20,
} WraithSteps;

typedef struct {
    Primitive* prim; // Saturn 0x7C: the helpers' sparks
    s16 timer;       // Saturn 0x80
    s16 timer2;      // Saturn 0x88
    s16 phase;       // Saturn 0x8C: the bobbing
    s16 fxTimer;     // Saturn 0x90: the helpers' timer
    u8 unk87;        // Saturn 0x87 (cleared when a grab misses; unused)
    u8 pad;
    s16 hitboxState; // Saturn 0xA4: saved while waiting
    s32 bob;         // Saturn 0x98 - posY: drawn height - logical posY
    Entity* parent;  // Saturn 0xA0: the afterimage's Wraith
    s32 targetVx;    // Saturn 0xA8
    s32 targetVy;    // Saturn 0xAC
} ET_ModWraith;
#define WRAITH_EXT(e) ((ET_ModWraith*)&(e)->ext)

// a spark's state in its tile's unused fields (the Saturn's hidden polygon:
// position +4/+8, speed +12/+16)
#define WRAITH_SPARK_X(p) (*(s32*)&(p)->r1)
#define WRAITH_SPARK_Y(p) (*(s32*)&(p)->x1)
#define WRAITH_SPARK_VX(p) (*(s32*)&(p)->x2)
#define WRAITH_SPARK_VY(p) (*(s32*)&(p)->x3)

// Saturn 0x060ECECC.., (duration, frame) as the PS1's
static u8 wraith_anim_appear[] = { // 0x060ECECC
    0x18, 0x01, 0x03, 0x02, 0x03, 0x03, 0x03, 0x04, 0x03, 0x05,
    0x03, 0x06, 0x06, 0x07, 0x07, 0x08, 0x05, 0x09, 0x06, 0x0A,
    0x10, 0x0B, 0x02, 0x0C, 0x02, 0x0D, 0x02, 0x0E, 0x02, 0x0F,
    0x02, 0x10, 0x02, 0x11, 0x04, 0x12, 0x06, 0x13, 0xFF, 0xFF};
static u8 wraith_anim_float[] = { // 0x060ECEF4 (loops)
    0x04, 0x13, 0x04, 0x14, 0x04, 0x15, 0x04, 0x16, 0x00, 0x00};
static u8 wraith_anim_reach[] = { // 0x060ECEFE (gathers, reaches out)
    0x02, 0x13, 0x02, 0x12, 0x02, 0x11, 0x02, 0x10, 0x02, 0x0F, 0x02, 0x0E,
    0x02, 0x0D, 0x02, 0x0C, 0x02, 0x0B, 0x04, 0x0A, 0x08, 0x0B, 0x03, 0x0C,
    0x02, 0x0D, 0x02, 0x17, 0x03, 0x18, 0x03, 0x19, 0xFF, 0xFF};
static u8 wraith_anim_drain[] = { // 0x060ECF20 (loops)
    0x06, 0x1C, 0x06, 0x1D, 0x00, 0x00};
static u8 wraith_anim_release[] = { // 0x060ECF26
    0x04, 0x1C, 0x02, 0x1B, 0x02, 0x1A, 0x02, 0x19, 0x02, 0x18, 0x01,
    0x17, 0x01, 0x0C, 0x03, 0x0B, 0x07, 0x0A, 0x18, 0x0B, 0xFF, 0xFF};
static u8 wraith_anim_swoop[] = { // 0x060ECF3C (loops)
    0x05, 0x29, 0x05, 0x2A, 0x05, 0x2B, 0x05, 0x2C, 0x00, 0x00};
static u8 wraith_anim_seize[] = { // 0x060ECF46
    0x05, 0x1B, 0x05, 0x1C, 0xFF, 0xFF};
static u8 wraith_anim_hit[] = { // 0x060ECF4C
    0x04, 0x1E, 0x04, 0x26, 0x04, 0x27, 0x10, 0x28, 0x04,
    0x27, 0x04, 0x26, 0x04, 0x1E, 0x04, 0x13, 0xFF, 0xFF};
static u8 wraith_anim_death[] = { // 0x060ECF5E
    0x10, 0x13, 0x04, 0x1E, 0x04, 0x1F, 0x05, 0x20, 0x05,
    0x21, 0x18, 0x22, 0x02, 0x23, 0x01, 0x24, 0xFF, 0xFF};
static u8 wraith_anim_death_end[] = { // 0x060ECF6A (the tail of the above)
    0x02, 0x23, 0x01, 0x24, 0xFF, 0xFF};
static u8 wraith_anim_recover[] = { // 0x060ECF70
    0x03, 0x0C, 0x03, 0x0D, 0x03, 0x0E, 0x03, 0x0F, 0x03,
    0x10, 0x03, 0x11, 0x03, 0x12, 0x08, 0x13, 0xFF, 0xFF};
static u8 wraith_anim_withdraw[] = { // 0x060ECF82 (after a missed grab)
    0x03, 0x19, 0x03, 0x18, 0x03, 0x17, 0x03, 0x0D, 0x03, 0x0C,
    0x03, 0x0B, 0x03, 0x0C, 0x03, 0x0D, 0x03, 0x0E, 0x03, 0x0F,
    0x03, 0x10, 0x03, 0x11, 0x03, 0x12, 0x08, 0x13, 0xFF, 0xFF};

// Saturn DAT_060EFCDC: the drift's acceleration (one for all Wraiths)
static s16 g_ModWraithAccel;

static void WraithFacePlayer(Entity* self) {
    self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
}

static void WraithSetStep(Entity* self, u16 step) {
    self->pose = 0;
    self->poseTimer = 0;
    self->step = step;
    self->step_s = 0;
}

// v -= v / 2^n, rounded toward zero (the Saturn's shifts)
static s32 WraithDecel(s32 v, s32 n) {
    if (v < 0) {
        return v - ((v + (1 << n) - 1) >> n);
    }
    return v - (v >> n);
}

static void WraithMove(Entity* self) {
    self->posX.val += self->velocityX;
    self->posY.val += self->velocityY;
}

// PlaySfx, as the Saturn (not positional)
static void WraithSfx(s16 id) { g_api.PlaySfx(id); }

void EntityModWraith(Entity* self) {
    u16 init[5];
    ET_ModWraith* ext = WRAITH_EXT(self);
    Entity* ent;
    s32 dx;
    s32 dy;
    s32 v;
    s32 drawY; // Saturn 0x98 (the previous frame's, then this one's)
    u8 rnd;

    self->posY.val -= ext->bob; // drawn -> logical
    drawY = self->posY.val + ext->bob;
    if (self->hitParams && (self->step & 1)) {
        WraithSetStep(self, WRAITH_HIT);
    }
    if ((self->flags & FLAG_DEAD) && self->step != WRAITH_DEATH) {
        WraithSetStep(self, WRAITH_DEATH);
        self->hitboxState = 0;
        self->hitboxWidth = 0;
        self->hitboxHeight = 0;
    }

    switch (self->step) {
    case WRAITH_INIT:
        init[0] = MOD_WRAITH_ANIMSET;
        init[1] = 0;
        init[2] = MOD_WRAITH_UNK5A;
        init[3] = 0;     // the frames carry their cluts
        init[4] = 0x1A8; // "Wraith"
        InitializeEntity(init);
        self->blendMode = BLEND_NO;
        self->drawFlags = ENTITY_OPACITY | ENTITY_MASK_G;
        self->opacity = 144;
        self->hitboxWidth = SAT_X(8);
        self->hitboxHeight = 24;
        self->hitboxOffX = 0;
        self->hitboxOffY = 4;
        self->zPriority += 32;
        WraithFacePlayer(self);
        WraithSetStep(self, WRAITH_WAIT);
        break;

    case WRAITH_WAIT:
        switch (self->step_s) {
        case 0:
            ext->hitboxState = self->hitboxState;
            self->step_s++;
            /* fallthrough */
        case 1:
            dx = GetDistanceToPlayerX();
            dy = GetDistanceToPlayerY();
            if (dy <= 95 && dx <= SAT_X(127)) {
                self->step_s++;
            }
            break;
        case 2:
            self->drawFlags |= ENTITY_BLINK;
            ext->timer = 24;
            self->step_s++;
            /* fallthrough */
        case 3:
            if (!AnimateEntity(wraith_anim_appear, self)) {
                self->hitboxState = ext->hitboxState;
                WraithSetStep(self, WRAITH_FLOAT);
            }
            if (--ext->timer == 0) {
                self->drawFlags &= ~ENTITY_BLINK;
            }
            break;
        }
        break;

    case WRAITH_FLOAT:
        dx = GetDistanceToPlayerX();
        dy = GetDistanceToPlayerY();
        if (dx <= SAT_X(47)) {
            self->step = WRAITH_BACKOFF;
        } else if (dx <= SAT_X(95) && dy <= 47) {
            rnd = (u8)Random() % 60;
            if (rnd <= 1) {
                self->step = WRAITH_GRAB;
            } else if (rnd == 2) {
                self->step = WRAITH_BACKOFF;
            }
        } else if (dx <= SAT_X(159) && dy <= 79) {
            rnd = (u8)Random() % 60;
            if (rnd <= 1) {
                self->step = WRAITH_GRAB;
            }
        } else if (dx <= SAT_X(127)) {
            if ((u8)Random() % 60 == 0) {
                self->step = WRAITH_BACKOFF;
            }
        } else if (!(Random() & 3)) {
            self->step = WRAITH_DASH;
        }
        if (self->step != WRAITH_FLOAT) {
            self->pose = 0;
            self->poseTimer = 0;
            self->step_s = 0;
        } else {
            AnimateEntity(wraith_anim_float, self);
        }
        // every 64 frames a new drift, toward the player
        if (!(++ext->timer2 & 63)) {
            ext->targetVx = SAT_X(((rand() & 0x3FFF) - 0x2000) << 4);
            ext->targetVy = ((rand() & 0x3FFF) - 0x2000) << 3;
            g_ModWraithAccel = (rand() & 0x1FF) + 0x700;
            WraithFacePlayer(self);
        }
        if (ext->targetVx > self->velocityX) {
            self->velocityX += SAT_X(g_ModWraithAccel);
        } else if (ext->targetVx < self->velocityX) {
            self->velocityX -= SAT_X(g_ModWraithAccel);
        }
        if (ext->targetVy > self->velocityY) {
            self->velocityY += g_ModWraithAccel;
        } else if (ext->targetVy < self->velocityY) {
            self->velocityY -= g_ModWraithAccel;
        }
        if ((ext->targetVx >> 11) == (self->velocityX >> 11)) {
            self->velocityX = ext->targetVx;
        }
        if ((ext->targetVy >> 11) == (self->velocityY >> 11)) {
            self->velocityY = ext->targetVy;
        }
        WraithMove(self);
        break;

    case WRAITH_DASH:
        switch (self->step_s) {
        case 0:
            WraithFacePlayer(self);
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(4));
            } else {
                self->velocityX = SAT_X(FIX(-4));
            }
            self->velocityY = 0;
            ext->timer2 = 90;
            self->step_s++;
            break;
        case 1:
            self->velocityX = WraithDecel(self->velocityX, 6);
            WraithMove(self);
            v = PLAYER.posY.val - self->posY.val;
            if (v < 0) {
                v += 63;
            }
            self->posY.val += v >> 6;
            dx = GetDistanceToPlayerX();
            if (--ext->timer2 >= 0 && dx > SAT_X(127)) {
                break;
            }
            ext->timer2 = 0;
            WraithSetStep(self, WRAITH_FLOAT);
            break;
        }
        break;

    case WRAITH_BACKOFF:
    case WRAITH_HIT:
        switch (self->step_s) {
        case 0:
            WraithFacePlayer(self);
            // away from the player
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(-4));
            } else {
                self->velocityX = SAT_X(FIX(4));
            }
            self->velocityY = 0;
            ext->timer2 = 60;
            self->step_s++;
            break;
        case 1:
            self->velocityX = WraithDecel(self->velocityX, 6);
            AnimateEntity(wraith_anim_hit, self);
            v = PLAYER.posY.val - self->posY.val;
            if (v < 0) {
                v += 63;
            }
            self->posY.val += v >> 6;
            WraithMove(self);
            if (--ext->timer2 >= 0) {
                break;
            }
            ext->timer2 = 0;
            WraithSetStep(self, WRAITH_FLOAT);
            break;
        }
        break;

    case WRAITH_GRAB:
        switch (self->step_s) {
        case 0:
            if (AnimateEntity(wraith_anim_reach, self)) {
                break;
            }
            self->pose = 0;
            self->poseTimer = 0;
            dx = GetDistanceToPlayerX();
            dy = GetDistanceToPlayerY();
            if (dx <= SAT_X(47) || dy > 96) {
                WraithSetStep(self, WRAITH_RECOVER);
                break;
            }
            ext->timer = 120;
            self->step_s++;
            break;
        case 1:
            // swoops at the player, faster and faster
            if (!(ext->timer % 30)) {
                WraithFacePlayer(self);
            }
            v = ext->timer / 8;
            v = v ? FIX(4) / v : 0; // the Saturn's division by 0 gives 0
            if (self->facingLeft) {
                self->posX.val += SAT_X(FIX(1) + v);
            } else {
                self->posX.val -= SAT_X(FIX(1) + v);
            }
            self->posY.val += (PLAYER.posY.val - self->posY.val) / 24;
            dx = PLAYER.posX.i.hi - self->posX.i.hi;
            dy = PLAYER.posY.i.hi - self->posY.i.hi;
            if (!self->facingLeft) {
                dx = -dx;
            }
            if (!(g_Player.status &
                  (PLAYER_STATUS_AXEARMOR | PLAYER_STATUS_DEAD |
                   PLAYER_STATUS_INVINCIBLE | PLAYER_STATUS_STONE |
                   PLAYER_STATUS_CROUCH | PLAYER_STATUS_MIST_FORM |
                   PLAYER_STATUS_BAT_FORM)) &&
                (u32)(dy + 31) <= 38 &&
                (u32)(dx - SAT_X(33)) <= SAT_X(63) - SAT_X(33)) {
                // caught
                g_Player.grabState = GRAB_START;
                g_Player.grabDamage = 0;
                g_Player.grabReactionEffect = GRAB_REACTION_NORMAL;
                self->hitboxState = 0;
                self->velocityX = 0;
                self->velocityY = 0;
                self->step_s++;
                break;
            }
            if (!g_Player.grabState && --ext->timer == 0) {
                ext->unk87 = 0;
                self->pose = 0;
                self->poseTimer = 0;
                self->step = WRAITH_RECOVER;
                self->step_s = 1;
            }
            AnimateEntity(wraith_anim_swoop, self);
            break;
        case 2:
            // holds the player at arm's length
            if (self->facingLeft) {
                self->posX.i.hi = PLAYER.posX.i.hi - SAT_X(56);
            } else {
                self->posX.i.hi = PLAYER.posX.i.hi + SAT_X(56);
            }
            self->posY.i.hi = PLAYER.posY.i.hi;
            self->pose = 0;
            self->poseTimer = 0;
            self->step_s++;
            WraithSfx(MOD_SFX_WRAITH_GRAB);
            /* fallthrough */
        case 3:
            if (AnimateEntity(wraith_anim_seize, self)) {
                break;
            }
            g_Player.grabDamage = 35;
            g_Player.grabState = GRAB_DAMAGE;
            ext->timer = 90;
            self->pose = 0;
            self->poseTimer = 0;
            self->step_s++;
            break;
        case 4:
            // drains the player's soul
            AnimateEntity(wraith_anim_drain, self);
            if (--ext->timer == 0) {
                self->step_s++;
                break;
            }
            if (ext->timer & 15) {
                break;
            }
            ent = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (ent == NULL) {
                break;
            }
            CreateEntityFromEntity(E_MOD_WRAITH_DRAIN, self, ent);
            ent->facingLeft = self->facingLeft;
            ent->posY.val = drawY;
            ent->zPriority = self->zPriority;
            WraithSfx(MOD_SFX_WRAITH_DRAIN);
            break;
        case 5:
            if (!AnimateEntity(wraith_anim_release, self)) {
                self->step_s++;
            }
            break;
        case 6:
            // throws the player off, recoils
            g_Player.grabState = GRAB_NONE;
            self->hitboxState = 3;
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(-3));
            } else {
                self->velocityX = SAT_X(FIX(3));
            }
            self->velocityY = FIX(-2);
            ext->timer = 38;
            self->step_s++;
            /* fallthrough */
        case 7:
            WraithMove(self);
            self->velocityX = WraithDecel(self->velocityX, 6);
            self->velocityY = WraithDecel(self->velocityY, 5);
            if (--ext->timer == 0) {
                WraithSetStep(self, WRAITH_FLOAT);
            }
            break;
        }
        break;

    case WRAITH_RECOVER:
        switch (self->step_s) {
        case 0:
            if (!AnimateEntity(wraith_anim_recover, self)) {
                WraithSetStep(self, WRAITH_FLOAT);
            }
            break;
        case 1:
            if (!AnimateEntity(wraith_anim_withdraw, self)) {
                WraithSetStep(self, WRAITH_FLOAT);
            }
            break;
        }
        self->velocityX = WraithDecel(self->velocityX, 6);
        self->velocityY = WraithDecel(self->velocityY, 6);
        WraithMove(self);
        break;

    case WRAITH_DEATH:
        switch (self->step_s) {
        case 0:
            if (g_Player.grabState) {
                g_Player.grabState = GRAB_NONE;
            }
            WraithFacePlayer(self);
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(-4));
            } else {
                self->velocityX = SAT_X(FIX(4));
            }
            self->velocityY = 0;
            ext->timer2 = 90;
            self->step_s++;
            WraithSfx(MOD_SFX_WRAITH_DEATH);
            break;
        case 1:
            self->velocityX = WraithDecel(self->velocityX, 4);
            WraithMove(self);
            if (AnimateEntity(wraith_anim_death, self)) {
                break;
            }
            self->step_s++;
            self->velocityX = 0;
            self->velocityY = 0;
            self->pose = 0;
            self->poseTimer = 0;
            ext->timer2 = 224;
            break;
        case 2:
            ext->timer2--;
            if (AnimateEntity(wraith_anim_death_end, self)) {
                break;
            }
            WraithSfx(MOD_SFX_WRAITH_VANISH);
            self->step_s++;
            // the wisp (frame 45), its origin moved to its centre
            self->animCurFrame = 45;
            if (self->facingLeft) {
                self->posX.i.hi -= SAT_X(5);
            } else {
                self->posX.i.hi += SAT_X(5);
            }
            self->posY.i.hi -= 30;
            if (self->facingLeft) {
                self->posX.i.hi += SAT_X(4);
            } else {
                self->posX.i.hi -= SAT_X(4);
            }
            break;
        case 3:
            // shrinks, spinning
            if (--ext->timer2 <= 63) {
                DestroyEntity(self);
                return;
            }
            if (ext->timer2 == 84) {
                self->drawFlags |= ENTITY_BLINK;
            }
            // the Saturn's sprite scale 0x40 = 1.0, rotation as the PS1's
            self->drawFlags |= ENTITY_SCALEX | ENTITY_SCALEY | ENTITY_ROTATE;
            self->scaleX = self->scaleY = (ext->timer2 / 3) << 2;
            self->rotate = (-ext->timer2 * ext->timer2) & 0xFFF;
            self->rotPivotX = 0;
            self->rotPivotY = 32; // the Saturn sprite's origin
            break;
        }
        // sparks swirl into it
        if (ext->timer2 > 170 && !(ext->timer2 & 7)) {
            ent = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (ent != NULL) {
                CreateEntityFromEntity(E_MOD_WRAITH_WISPS, self, ent);
                ent->facingLeft = self->facingLeft;
                ent->posY.val = drawY;
                ent->zPriority = self->zPriority;
                if (self->animCurFrame == 45) {
                    ent->posY.i.hi += 31;
                    if (self->facingLeft) {
                        ent->posX.i.hi += SAT_X(4);
                    } else {
                        ent->posX.i.hi -= SAT_X(4);
                    }
                } else {
                    ent->posY.i.hi++;
                }
            }
        }
        break;
    }

    // Saturn epilogue: the bobbing (frozen while appearing, holding the
    // player and dying), the drawn height, the hitbox follows it
    drawY = self->posY.val + (rsin(ext->phase * 32) << 7) - FIX(12);
    if (!((self->step == WRAITH_GRAB && self->step_s > 1) ||
          self->step == WRAITH_DEATH || self->step == WRAITH_WAIT)) {
        ext->phase++;
    }
    // the afterimages, every 6 frames while it is about
    if (self->step != WRAITH_DEATH && self->step != WRAITH_WAIT &&
        !((u32)g_Timer % 6)) {
        ent = AllocEntity(&g_Entities[160], &g_Entities[192]);
        if (ent != NULL) {
            CreateEntityFromEntity(E_MOD_WRAITH_AFTERIMAGE, self, ent);
            ent->facingLeft = self->facingLeft;
            ent->animCurFrame = self->animCurFrame;
            WRAITH_EXT(ent)->parent = self;
            ent->posY.val = drawY; // the frames are +32
        }
    }
    ext->bob = drawY - self->posY.val;
    self->posY.val = drawY; // drawn (and hit-tested) there
    self->hitboxOffY = 4;
}

// Saturn func_060E4664: an afterimage, drifting back and down while it
// fades to red
void EntityModWraithAfterimage(Entity* self) {
    u16 init[5];
    ET_ModWraith* ext = WRAITH_EXT(self);
    Entity* parent;
    s16 frame;

    switch (self->step) {
    case 0:
        frame = self->animCurFrame; // set by the Wraith
        parent = ext->parent;
        init[0] = MOD_WRAITH_ANIMSET;
        init[1] = frame;
        init[2] = MOD_WRAITH_UNK5A;
        init[3] = 0;
        init[4] = 5;
        InitializeEntity(init); // step -> 1
        self->animCurFrame = frame;
        self->blendMode = BLEND_NO; // opaque until the mesh (tick 8)
        self->drawFlags = ENTITY_OPACITY | ENTITY_MASK_R;
        self->opacity = 128;
        ext->fxTimer = 0;
        self->zPriority = parent->zPriority - 2;
        break;
    case 1:
        if (!(++ext->fxTimer & 3)) {
            self->zPriority--;
        }
        if (self->facingLeft) {
            self->posX.val += SAT_X(-0x2000);
        } else {
            self->posX.val += SAT_X(0x2000);
        }
        self->posY.val += FIX(0.5);
        if (ext->fxTimer > 24) {
            self->step++;
        } else {
            self->opacity = 128 - ext->fxTimer * 4;
        }
        if (ext->fxTimer == 8) {
            // Saturn sprite flag 0x80: the VDP1 mesh
            self->blendMode = MOD_WRAITH_MESH_BLEND;
        }
        if (ext->fxTimer == 16) {
            self->drawFlags |= ENTITY_BLINK;
        }
        break;
    default:
        DestroyEntity(self);
        break;
    }
}

static s32 WraithSparks(Entity* self, s32 count, u16 priority) {
    Primitive* prim;
    s16 primIndex;

    primIndex = g_api.AllocPrimitives(PRIM_TILE, count);
    if (primIndex == -1) {
        DestroyEntity(self);
        return 0;
    }
    self->flags |= FLAG_HAS_PRIMS;
    self->primIndex = primIndex;
    prim = &g_PrimBuf[primIndex];
    WRAITH_EXT(self)->prim = prim;
    while (prim != NULL) {
        prim->type = PRIM_TILE;
        prim->u0 = prim->v0 = 2;
        prim->priority = priority;
        prim->drawMode = DRAW_DEFAULT;
        prim = prim->next;
    }
    return 1;
}

// RGB555 -> the tile's colour
static void WraithSparkColour(Primitive* prim, s32 r, s32 g, s32 b) {
    prim->r0 = r * 8;
    prim->g0 = g * 8;
    prim->b0 = b * 8;
}

// Saturn func_060E4804: the soul drained from the held player, 32 sparks
// bursting from in front of the Wraith and pulled into it, blue to white
void EntityModWraithDrain(Entity* self) {
    u16 init[5];
    ET_ModWraith* ext = WRAITH_EXT(self);
    Primitive* prim;
    s32 speed;
    s32 angle;
    s32 t;
    s32 r, g, b;
    s32 dx, dy;

    switch (self->step) {
    case 0:
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = 5;
        InitializeEntity(init); // step -> 1
        self->step = 0;
        if (!WraithSparks(self, 32, 0x80)) {
            return;
        }
        for (prim = ext->prim; prim != NULL; prim = prim->next) {
            WraithSparkColour(prim, 8, 0, 0);
            Random();
            speed = (rand() & 0x7FFF) + 0x8000;
            angle = rand() % 0xFFF;
            WRAITH_SPARK_VX(prim) = SAT_X((rcos(angle) * speed) >> 13);
            WRAITH_SPARK_VY(prim) = ((rsin(angle) * speed) >> 12) - 0x12000;
            if (self->facingLeft) {
                WRAITH_SPARK_X(prim) = self->posX.val + SAT_X(FIX(61));
            } else {
                WRAITH_SPARK_X(prim) = self->posX.val - SAT_X(FIX(64));
            }
            WRAITH_SPARK_Y(prim) = self->posY.val;
            prim->x0 = WRAITH_SPARK_X(prim) >> 16;
            prim->y0 = WRAITH_SPARK_Y(prim) >> 16;
        }
        ext->fxTimer = 0;
        self->step++;
        /* fallthrough */
    case 1:
        t = ++ext->fxTimer;
        b = t / 2 + 16;
        g = t / 3 + 16;
        if (g > 31) {
            g = 31;
        }
        if (b > 31) {
            b = 31;
        }
        r = b == 31 ? t / 2 : 0;
        if (r > 31) {
            r = 31;
        }
        for (prim = ext->prim; prim != NULL; prim = prim->next) {
            if (prim->drawMode & DRAW_HIDE) {
                continue;
            }
            dx = self->posX.val - WRAITH_SPARK_X(prim);
            dy = self->posY.val - WRAITH_SPARK_Y(prim);
            if ((u32)(dx + 15) <= 30 && (u32)(dy + 15) <= 30) {
                prim->drawMode |= DRAW_HIDE; // arrived
            }
            // steered toward the Wraith (in Saturn space)
            angle = ratan2(dy >> 8, WRAITH_SAT_FROM_PS1(dx >> 8));
            WRAITH_SPARK_VX(prim) += SAT_X((rcos(angle) * (t / 5)) / 8);
            WRAITH_SPARK_VY(prim) += (rsin(angle) * (t / 5)) / 4;
            WRAITH_SPARK_X(prim) += WRAITH_SPARK_VX(prim);
            WRAITH_SPARK_Y(prim) += WRAITH_SPARK_VY(prim);
            prim->x0 = WRAITH_SPARK_X(prim) >> 16;
            prim->y0 = WRAITH_SPARK_Y(prim) >> 16;
            WraithSparkColour(prim, r, g, b);
        }
        if (ext->fxTimer > 58) {
            DestroyEntity(self);
        }
        break;
    }
}

// Saturn func_060E4AFC: 12 sparks bursting from the dying Wraith and
// swirling back into it, white fading through purple and blue to black
void EntityModWraithWisps(Entity* self) {
    u16 init[5];
    ET_ModWraith* ext = WRAITH_EXT(self);
    Primitive* prim;
    s32 speed;
    s32 angle;
    s32 t;
    s32 r, g, b;
    s32 dx, dy;
    u16 z;

    switch (self->step) {
    case 0:
        z = self->zPriority; // the Wraith's (set by it)
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = 5;
        InitializeEntity(init); // step -> 1
        self->step = 0;
        // the Saturn: the Wraith's sprite z - 1
        if (!WraithSparks(self, 12, z - 1)) {
            return;
        }
        for (prim = ext->prim; prim != NULL; prim = prim->next) {
            WraithSparkColour(prim, 0, 0, 0);
            speed = (rand() & 0x7FFF) + 0x8000;
            angle = rand() & 0xFFF;
            WRAITH_SPARK_VX(prim) = SAT_X(rcos(angle) * speed / 6 / 128);
            WRAITH_SPARK_VY(prim) = rsin(angle) * speed / 6 / 128;
            WRAITH_SPARK_X(prim) = self->posX.val + SAT_X(rcos(angle) << 7);
            WRAITH_SPARK_Y(prim) = self->posY.val + (rsin(angle) << 7);
            prim->x0 = (WRAITH_SPARK_X(prim) >> 16) - 1;
            prim->y0 = (WRAITH_SPARK_Y(prim) >> 16) - 1;
        }
        ext->fxTimer = 0;
        self->step++;
        /* fallthrough */
    case 1:
        t = ++ext->fxTimer;
        g = 31 - t;
        r = 31 - t / 2;
        b = 47 - t / 2;
        if (r < 0) {
            r = 0;
        }
        if (r > 31) {
            r = 31;
        }
        if (g < 0) {
            g = 0;
        }
        if (g > 31) {
            g = 31;
        }
        if (b < 0) {
            b = 0;
        }
        if (b > 31) {
            b = 31;
        }
        for (prim = ext->prim; prim != NULL; prim = prim->next) {
            if (prim->drawMode & DRAW_HIDE) {
                continue;
            }
            dx = WRAITH_SAT_FROM_PS1(
                self->posX.i.hi - (s16)(WRAITH_SPARK_X(prim) >> 16));
            dy = self->posY.i.hi - (s16)(WRAITH_SPARK_Y(prim) >> 16);
            if (dx * dx + dy * dy <= 15) {
                prim->drawMode |= DRAW_HIDE; // arrived
                continue;
            }
            // pushed around it (+-56 degrees), then pulled in
            angle = ratan2(dy, dx);
            if (self->facingLeft) {
                angle += 0x280;
            } else {
                angle -= 0x280;
            }
            WRAITH_SPARK_X(prim) += SAT_X(rcos(angle) * t);
            WRAITH_SPARK_Y(prim) += rsin(angle) * t;
            dx = WRAITH_SAT_FROM_PS1(
                self->posX.i.hi - (s16)(WRAITH_SPARK_X(prim) >> 16));
            dy = self->posY.i.hi - (s16)(WRAITH_SPARK_Y(prim) >> 16);
            angle = ratan2(dy, dx);
            WRAITH_SPARK_VX(prim) += SAT_X(rcos(angle));
            WRAITH_SPARK_VY(prim) += rsin(angle);
            WRAITH_SPARK_X(prim) += WRAITH_SPARK_VX(prim);
            WRAITH_SPARK_Y(prim) += WRAITH_SPARK_VY(prim);
            prim->x0 = (WRAITH_SPARK_X(prim) >> 16) - 1;
            prim->y0 = (WRAITH_SPARK_Y(prim) >> 16) - 1;
            WraithSparkColour(prim, r, g, b);
        }
        if (ext->fxTimer > 95) {
            DestroyEntity(self);
        }
        break;
    }
}
