// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Copper Armor (Saturn RSTAGE15 "Hell Garden" entities 0x21/0x22:
 * func_060E5804 / func_060E6750, enemy def 0x191), ported to the Hell Garden
 * overlay (Saturn port mod, translated from the SH-2: the Saturn code was
 * raw data in asm/saturn).
 * Include after the stage header and e_misc.h (CheckColliderOffsets,
 * EntityExplosionVariantsSpawner).
 *
 * A golden-copper armoured swordsman (the decomp's RSTAGE15 "CopperArmor"
 * label is on the Cloaked Knight: this is the real one). It faces the
 * player, walks up to him, slashes (single slash, a long thrust, a
 * three-hit combo, combos ending in a lunging thrust), jumps over walls,
 * leaps at or away from the player when he is very close. Hit while idle or
 * walking it reels back. Dying, it falls apart into 8 pieces (frames 53-60)
 * that sink, burst and blink out.
 *
 * Required from the including stage:
 *   MOD_COPPER_ARMOR_ANIMSET  animSet of the converted Saturn bank 23
 *                             (0x060EF3F4, frames 0x060F0240: PSX frame n =
 *                             Saturn frame n, drawn at the entity as the
 *                             Saturn's)
 *   MOD_COPPER_ARMOR_UNK5A    (optional, default 0)
 *   MOD_SFX_COPPER_ARMOR_DEATH (Saturn 0x8F0)
 *   enum entries E_MOD_COPPER_ARMOR (Saturn 0x21, also its death pieces, in
 *   slots 160-191), E_MOD_COPPER_ARMOR_HITBOX (0x22, always self + 1) and
 *   E_EXPLOSION.
 *
 * Entry points:
 *   EntityModCopperArmor(Entity*)        <- E_MOD_COPPER_ARMOR
 *   EntityModCopperArmorHitbox(Entity*)  <- E_MOD_COPPER_ARMOR_HITBOX: the
 *                                           sword's hitbox, per body frame
 */

#ifndef MOD_COPPER_ARMOR_ANIMSET
#error "define MOD_COPPER_ARMOR_ANIMSET before including e_mod_copper_armor.h"
#endif
#ifndef MOD_COPPER_ARMOR_UNK5A
#define MOD_COPPER_ARMOR_UNK5A 0
#endif
// 1: the Saturn's tint (drawFlags OPACITY | MASK_B, opacity 112 + its speed
// in 1/16 px, up to 176: red and green follow the speed, blue stays 128)
#ifndef MOD_COPPER_ARMOR_TINT
#define MOD_COPPER_ARMOR_TINT 1
#endif

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif

// st_common.h (built in the stage's st_common.c), e_misc.h
s32 UnkCollisionFunc2(s16* sensors);
s32 UnkCollisionFunc3(s16* sensors);
u8 CheckColliderOffsets(s16* arg0, u8 facing);
void EntityExplosionVariantsSpawner(
    Entity* self, u8 count, u8 params, s16 x, s16 y, u8 index, s16 xGap);

typedef enum {
    COPPER_INIT = 0,
    COPPER_IDLE = 1, // faces the player, picks an action
    COPPER_JUMP = 2,
    COPPER_WALK = 3,
    COPPER_SLASH = 4,
    COPPER_THRUST = 6,
    COPPER_COMBO = 8,         // slash, slash, lunge
    COPPER_COMBO_THRUST = 10, // slash, slash, long thrust
    COPPER_FEINT = 12,        // shuffles back (random repeats), then slashes
    COPPER_FEINT_THRUST = 14, // the same, then the long thrust
    COPPER_HIT = 16,          // hit while idle or walking: reels back
    COPPER_DEATH = 18,
    COPPER_PIECE = 20, // a death piece (ext.timer set by the dying one)
} CopperArmorSteps;

typedef struct {
    // Saturn 0x78: the jump's direction flip (main), the life time (pieces)
    s16 timer;
    s16 walkTimer; // Saturn 0x94
} ET_ModCopperArmor;
#define COPPER_EXT(e) ((ET_ModCopperArmor*)&(e)->ext)

// Saturn 0x060EF304.., (duration, frame) as the PS1's
static u8 copper_anim_slash[] = { // 0x060EF304
    0x02, 0x01, 0x02, 0x02, 0x02, 0x03, 0x08, 0x04, 0x10, 0x05, 0x18, 0x06,
    0x08, 0x07, 0x03, 0x08, 0x03, 0x09, 0x03, 0x0A, 0x1E, 0x0B, 0x00, 0x00};
static u8 copper_anim_walk[] = { // 0x060EF31C
    0x12, 0x0C, 0x06, 0x0D, 0x10, 0x0E, 0x05, 0x0F, 0x00, 0x00};
static u8 copper_anim_idle[] = { // 0x060EF328
    0x17, 0x10, 0x0D, 0x11, 0x0D, 0x12, 0x09, 0x13, 0x0D, 0x14, 0x00, 0x00};
static u8 copper_anim_jump[] = { // 0x060EF334
    0x02, 0x15, 0x03, 0x16, 0x03, 0x17, 0x03,
    0x18, 0x03, 0x19, 0x03, 0x1A, 0xFF, 0x00};
static u8 copper_anim_land[] = { // 0x060EF344
    0x04, 0x1B, 0x04, 0x1C, 0x06, 0x1D, 0x08, 0x1E, 0x18, 0x1F, 0xFF, 0x00};
static u8 copper_anim_hit[] = { // 0x060EF350
    0x06, 0x20, 0x0A, 0x21, 0x14, 0x22, 0x16,
    0x23, 0x07, 0x25, 0x18, 0x26, 0x00, 0x00};
static u8 copper_anim_thrust[] = { // 0x060EF360
    0x02, 0x27, 0x03, 0x28, 0x04, 0x29, 0x06, 0x2A, 0x1C, 0x2B,
    0x06, 0x2C, 0x03, 0x2D, 0x01, 0x2E, 0x18, 0x2F, 0x03, 0x30,
    0x03, 0x31, 0x04, 0x32, 0x08, 0x33, 0x0A, 0x34, 0x00, 0x00};
static u8 copper_anim_combo1[] = { // 0x060EF380
    0x02, 0x01, 0x02, 0x02, 0x04, 0x03, 0x02, 0x04, 0x10, 0x05, 0x20, 0x06,
    0x04, 0x07, 0x02, 0x08, 0x02, 0x09, 0x02, 0x0A, 0x02, 0x0B, 0x00, 0x00};
static u8 copper_anim_combo2[] = { // 0x060EF398
    0x02, 0x04, 0x04, 0x05, 0x04, 0x06, 0x04, 0x07, 0x04,
    0x08, 0x04, 0x09, 0x04, 0x0A, 0x04, 0x0B, 0x00, 0x00};
static u8 copper_anim_lunge[] = { // 0x060EF3AC
    0x02, 0x04, 0x06, 0x05, 0x06, 0x06, 0x06, 0x07, 0x04,
    0x08, 0x04, 0x09, 0x06, 0x0A, 0x18, 0x0B, 0x00, 0x00};
static u8 copper_anim_feint[] = { // 0x060EF3C0
    0x02, 0x04, 0x02, 0x05, 0x02, 0x06, 0x02, 0x07, 0x02,
    0x08, 0x02, 0x09, 0x02, 0x0A, 0x02, 0x0B, 0x00, 0x00};
static u8 copper_anim_long_thrust[] = { // 0x060EF3D4
    0x02, 0x27, 0x03, 0x28, 0x04, 0x29, 0x06, 0x2A, 0x1C, 0x2B,
    0x06, 0x2C, 0x03, 0x2D, 0x01, 0x2E, 0x30, 0x2F, 0x03, 0x30,
    0x03, 0x31, 0x04, 0x32, 0x08, 0x33, 0x0A, 0x34, 0x00, 0x00};

// Saturn 0x060EF2B8: CheckColliderOffsets, the ground under a falling jump
static s16 copper_land_probe[] = {0, 8, 0xFF};
// Saturn 0x060EF2BE: CheckColliderOffsets, a wall ahead while walking
static s16 copper_wall[] = {SAT_X(24), -4, SAT_X(24), -24, 0xFF};
// Saturn 0x060EF2CA: UnkCollisionFunc2
static s16 copper_sensors[] = {0, 0, SAT_X(8), 0};
// Saturn 0x060EF2D2: UnkCollisionFunc3 (the jump)
static s16 copper_sensors_air[] = {
    0, 0, 0, -28, SAT_X(12), -36, SAT_X(-24), -32};
// Saturn 0x060EF2E2: each death piece's burst {x, y}. The Saturn scales x by
// 5/4 at runtime (a PS1-space table): x is used as it is here.
static s16 copper_piece_burst[8][2] = {
    {8, 24}, {-4, 24}, {0, 8}, {8, -4}, {0, 0}, {-16, -2}, {0, -4}, {0, -16},
};
// Saturn 0x060EF410 / 0x060EF4E4: the sword's hitbox per body frame 0-52,
// {offX, -offY - 32} and {width, height}. PS1-space values (the Saturn
// scales x and the width by 5/4 at runtime): used as they are here.
static s8 copper_sword_off[53][2] = {
    {0, 0},    {-17, 1}, {-22, 24}, {-14, 22}, {0, 0},    {0, 0},    {0, 0},
    {-42, 1},  {-40, 4}, {-37, 2},  {-32, 2},  {-16, -8}, {0, 0},    {0, 0},
    {0, 0},    {0, 0},   {-17, -4}, {-18, -3}, {-18, -2}, {-18, -3}, {-18, -3},
    {0, 0},    {0, 0},   {0, 0},    {-16, 24}, {-19, 21}, {-25, 4},  {-24, 2},
    {-17, 0},  {0, 0},   {0, 0},    {0, 0},    {0, 0},    {0, 0},    {0, 0},
    {0, 0},    {0, 0},   {0, 0},    {0, 0},    {0, 0},    {0, 0},    {0, 0},
    {-12, 15}, {-6, 13}, {-10, 21}, {-19, 17}, {-53, -8}, {-55, -6}, {-19, 18},
    {-11, 20}, {-7, 13}, {0, 0},    {0, 0},
};
static u8 copper_sword_size[53][2] = {
    {0, 0},  {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {16, 4}, {8, 1},
    {8, 1},  {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  {0, 0},
    {0, 0},  {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {3, 6}, {1, 8},  {8, 1},
    {8, 1},  {5, 4}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},  {0, 0},
    {0, 0},  {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {3, 8}, {3, 8},  {3, 8},
    {1, 10}, {8, 1}, {8, 1}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
};

static void CopperFacePlayer(Entity* self) {
    self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
}

static void CopperResetAnim(Entity* self) {
    self->pose = 0;
    self->poseTimer = 0;
}

// forward (the way it faces) at v px/frame (v in Saturn units)
#define COPPER_VX(self, v) ((self)->facingLeft ? SAT_X(v) : SAT_X(-(v)))

// Saturn func_060E56F4: the jump (the crouch, the flight, the landing).
// flip: jump the other way. Returns 1 when it is over.
static u8 CopperJump(Entity* self, u8 flip) {
    switch (self->step_s) {
    case 0:
        if (self->facingLeft) {
            self->velocityX = -self->velocityX;
        }
        if (flip) {
            self->velocityX = -self->velocityX;
        }
        self->step_s++;
        AnimateEntity(copper_anim_jump, self);
        return 0;
    case 1:
        AnimateEntity(copper_anim_jump, self);
        if (self->animCurFrame == 24) {
            self->step_s++;
        }
        return 0;
    case 2:
        AnimateEntity(copper_anim_jump, self);
        if (self->velocityY >= 0 &&
            CheckColliderOffsets(copper_land_probe, 0)) {
            self->poseTimer = 0;
            self->pose = 0;
            self->step_s++;
        }
        return (u8)UnkCollisionFunc3(copper_sensors_air);
    case 3:
        UnkCollisionFunc3(copper_sensors_air);
        return AnimateEntity(copper_anim_land, self) == 0;
    }
    return 0;
}

void EntityModCopperArmor(Entity* self) {
    u16 init[5];
    ET_ModCopperArmor* ext = COPPER_EXT(self);
    Entity* ent;
    s32 speed;
    s16 dist;
    s16 x;
    u8 r;
    s32 i;

    if (self->hitParams && (self->step & 1)) {
        CopperResetAnim(self);
        self->step = COPPER_HIT;
    }
    if (self->flags & FLAG_DEAD) {
        self->step = COPPER_DEATH;
    }

    switch (self->step) {
    case COPPER_INIT:
        init[0] = MOD_COPPER_ARMOR_ANIMSET;
        init[1] = 0;
        init[2] = MOD_COPPER_ARMOR_UNK5A;
        init[3] = 0;     // the frames carry the bank's palette
        init[4] = 0x191; // "Copper Armor"
        InitializeEntity(init);
#if MOD_COPPER_ARMOR_TINT
        self->drawFlags = ENTITY_OPACITY | ENTITY_MASK_B;
#endif
        if (ext->timer) {
            // a death piece
            self->animCurFrame = self->params + 53;
            self->hitboxState = 0;
            self->step = COPPER_PIECE;
            self->hitboxOffY = -28;
            break;
        }
        self->hitboxOffY = -28;
        DestroyEntity(self + 1);
        CreateEntityFromCurrentEntity(E_MOD_COPPER_ARMOR_HITBOX, self + 1);
        CopperFacePlayer(self);
        self->step = COPPER_JUMP;
        break;

    case COPPER_IDLE:
        CopperFacePlayer(self);
        dist = GetDistanceToPlayerX();
        if (dist <= SAT_X(31) || (dist <= SAT_X(111) && !(Random() & 63))) {
            // leap: over the player, or away when he faces away
            self->velocityX = SAT_X(FIX(-2.0));
            self->velocityY = FIX(-4.5);
            self->step_s = 0;
            ext->timer = PLAYER.facingLeft ^ self->facingLeft;
            self->step = COPPER_JUMP;
        } else if (dist <= SAT_X(79)) {
            r = (Random() & 0xFF) % 45;
            if (r <= 1) {
                self->step = COPPER_SLASH;
            } else if (r == 2) {
                self->step = COPPER_THRUST;
            }
        } else if (dist <= SAT_X(95)) {
            r = (Random() & 0xFF) % 45;
            if (r == 0) {
                self->step = COPPER_SLASH;
            } else if (r <= 2) {
                self->step = COPPER_THRUST;
            }
        } else if (dist <= SAT_X(111)) {
            switch ((Random() & 0xFF) % 90) {
            case 0:
                self->step = COPPER_COMBO;
                break;
            case 1:
                self->step = COPPER_COMBO_THRUST;
                break;
            case 2:
                self->step = COPPER_FEINT;
                break;
            case 3:
                self->step = COPPER_FEINT_THRUST;
                break;
            }
        } else if (!(Random() & 7)) {
            // jump at the player
            self->velocityX = SAT_X(FIX(-3.0));
            self->velocityY = FIX(-4.5);
            self->step_s = 0;
            ext->timer = 0;
            self->step = COPPER_JUMP;
        } else {
            self->velocityX = COPPER_VX(self, 0x6400);
            ext->walkTimer = 30;
            self->step = COPPER_WALK;
        }
        if (self->step == COPPER_IDLE) {
            AnimateEntity(copper_anim_idle, self);
        } else {
            CopperResetAnim(self);
        }
        break;

    case COPPER_JUMP:
        if (CopperJump(self, (u8)ext->timer)) {
            CopperResetAnim(self);
            self->step_s = 0;
            ext->timer = 0;
            self->step = COPPER_IDLE;
        }
        break;

    case COPPER_WALK:
        AnimateEntity(copper_anim_walk, self);
        UnkCollisionFunc2(copper_sensors);
        if (CheckColliderOffsets(copper_wall, self->facingLeft)) {
            // jump over the wall
            CopperResetAnim(self);
            self->velocityX = SAT_X(FIX(-2.0));
            self->velocityY = FIX(-4.5);
            self->step_s = 0;
            ext->timer = 0;
            self->step = COPPER_JUMP;
        }
        if (ext->walkTimer) {
            ext->walkTimer--;
        } else if (GetDistanceToPlayerX() <= SAT_X(144)) {
            CopperResetAnim(self);
            self->step = COPPER_IDLE;
        }
        break;

    case COPPER_SLASH:
        if (self->pose == 6 && self->poseTimer == 0) {
            PlaySfxPositional(SFX_WEAPON_SWISH_C); // 0x60C
        }
        if (self->animCurFrame == 7) {
            self->velocityX = COPPER_VX(self, FIX(4.0));
        }
        self->velocityX /= 2;
        if (!AnimateEntity(copper_anim_slash, self)) {
            CopperResetAnim(self);
            self->velocityX = 0;
            self->step = COPPER_IDLE;
        }
        UnkCollisionFunc2(copper_sensors);
        break;

    case COPPER_THRUST:
        if (self->pose == 8 && self->poseTimer == 0) {
            PlaySfxPositional(SFX_WEAPON_SWISH_B); // 0x60B
        }
        if (self->animCurFrame == 46) {
            self->velocityX = COPPER_VX(self, FIX(8.0));
        }
        self->velocityX = self->velocityX * 6 / 7;
        if (!AnimateEntity(copper_anim_thrust, self)) {
            CopperResetAnim(self);
            self->velocityX = 0;
            self->step = COPPER_IDLE;
        }
        UnkCollisionFunc2(copper_sensors);
        break;

    case COPPER_COMBO:
    case COPPER_COMBO_THRUST:
        switch (self->step_s) {
        case 0:
            self->step_s++;
            /* fallthrough */
        case 1:
            if (self->pose == 6 && self->poseTimer == 0) {
                PlaySfxPositional(SFX_WEAPON_SWISH_C);
            }
            if (self->animCurFrame == 7) {
                self->velocityX = COPPER_VX(self, FIX(4.0));
            }
            self->velocityX /= 2;
            if (!AnimateEntity(copper_anim_combo1, self)) {
                CopperResetAnim(self);
                self->step_s++;
            }
            break;
        case 2:
            if (self->pose == 3 && self->poseTimer == 0) {
                PlaySfxPositional(SFX_WEAPON_SWISH_C);
            }
            if (self->animCurFrame == 7) {
                self->velocityX = COPPER_VX(self, FIX(4.0));
            }
            self->velocityX /= 2;
            if (!AnimateEntity(copper_anim_combo2, self)) {
                CopperResetAnim(self);
                self->step_s++;
            }
            break;
        case 3:
            if (self->step == COPPER_COMBO) {
                // the lunge
                if (self->pose == 3 && self->poseTimer == 0) {
                    PlaySfxPositional(SFX_WEAPON_SWISH_C);
                }
                if (self->animCurFrame == 4) {
                    CopperFacePlayer(self);
                } else if (self->animCurFrame == 7) {
                    self->velocityX = COPPER_VX(self, FIX(6.0));
                }
                self->velocityX = self->velocityX * 4 / 5;
                i = AnimateEntity(copper_anim_lunge, self);
            } else {
                // the long thrust
                if (self->pose == 8 && self->poseTimer == 0) {
                    PlaySfxPositional(SFX_WEAPON_SWISH_B);
                }
                if (self->animCurFrame == 42) {
                    CopperFacePlayer(self);
                } else if (self->animCurFrame == 46) {
                    self->velocityX = COPPER_VX(self, FIX(16.0));
                }
                self->velocityX = self->velocityX * 5 / 6;
                i = AnimateEntity(copper_anim_long_thrust, self);
            }
            if (!i) {
                CopperResetAnim(self);
                self->velocityX = 0;
                self->step_s = 0;
                self->step = COPPER_IDLE;
            }
            break;
        default:
            goto epilogue; // no collision (Saturn: straight to the end)
        }
        UnkCollisionFunc2(copper_sensors);
        break;

    case COPPER_FEINT:
    case COPPER_FEINT_THRUST:
        switch (self->step_s) {
        case 0:
            self->step_s = 1;
            /* fallthrough */
        case 1:
            // a shuffle back, repeated until Random() & 3 == 0
            if (self->pose == 3 && self->poseTimer == 0) {
                PlaySfxPositional(SFX_WEAPON_SWISH_C);
            }
            if (self->animCurFrame == 4) {
                CopperFacePlayer(self);
            }
            if (self->animCurFrame == 7) {
                if (self->step == COPPER_FEINT) {
                    self->velocityX = COPPER_VX(self, -FIX(1.0));
                } else {
                    self->velocityX = COPPER_VX(self, -FIX(1.5));
                }
            }
            self->velocityX /= 2;
            if (!AnimateEntity(copper_anim_feint, self) && !(Random() & 3)) {
                CopperResetAnim(self);
                self->step_s++;
            }
            break;
        case 2:
            if (self->step == COPPER_FEINT) {
                if (self->pose == 6 && self->poseTimer == 0) {
                    PlaySfxPositional(SFX_WEAPON_SWISH_C);
                }
                if (self->animCurFrame == 4) {
                    CopperFacePlayer(self);
                } else if (self->animCurFrame == 7) {
                    self->velocityX = COPPER_VX(self, FIX(4.0));
                }
                self->velocityX = self->velocityX * 5 / 6;
                i = AnimateEntity(copper_anim_slash, self);
            } else {
                if (self->pose == 8 && self->poseTimer == 0) {
                    PlaySfxPositional(SFX_WEAPON_SWISH_B);
                }
                if (self->animCurFrame == 42) {
                    CopperFacePlayer(self);
                } else if (self->animCurFrame == 46) {
                    self->velocityX = COPPER_VX(self, FIX(8.0));
                }
                self->velocityX = self->velocityX * 5 / 6;
                i = AnimateEntity(copper_anim_thrust, self);
            }
            if (!i) {
                CopperResetAnim(self);
                self->velocityX = 0;
                self->step_s = 0;
                self->step = COPPER_IDLE;
            }
            break;
        default:
            goto epilogue;
        }
        UnkCollisionFunc2(copper_sensors);
        break;

    case COPPER_HIT:
        if (!AnimateEntity(copper_anim_hit, self)) {
            CopperResetAnim(self);
            self->step = COPPER_IDLE;
        }
        UnkCollisionFunc2(copper_sensors);
        // pushed back
        self->velocityX = COPPER_VX(self, -0xA000);
        if (self->animCurFrame > 34) {
            self->velocityX = 0;
        }
        break;

    case COPPER_DEATH:
        // it falls apart: 8 pieces (frames 53-60), each lasting a little
        // longer than the one before
        g_api.PlaySfx(MOD_SFX_COPPER_ARMOR_DEATH);
        for (i = 0; i < 8; i++) {
            ent = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (ent != NULL) {
                DestroyEntity(ent);
                CreateEntityFromEntity(E_MOD_COPPER_ARMOR, self, ent);
                ent->params = i;
                COPPER_EXT(ent)->timer = (i & 3) ? i * 16 : i * 16 + 16;
                COPPER_EXT(ent)->timer += 12;
                ent->facingLeft = self->facingLeft;
            }
        }
        DestroyEntity(self);
        return;

    case COPPER_PIECE:
        // sinks a pixel every 4 ticks, bursts at 10, blinks the last 16
        if (!(ext->timer & 3)) {
            self->posY.i.hi++;
        }
        if (ext->timer == 10) {
            ent = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (ent != NULL) {
                DestroyEntity(ent);
                CreateEntityFromEntity(E_EXPLOSION, self, ent);
                ent->params = 2;
                ent->posX.i.hi += copper_piece_burst[self->params][0];
                ent->posY.i.hi += copper_piece_burst[self->params][1] - 32;
            }
        }
        if (ext->timer-- < 0) {
            DestroyEntity(self);
            return;
        }
        if (ext->timer <= 15) {
            self->drawFlags |= ENTITY_BLINK;
        }
        break;
    }

epilogue:
    // Saturn epilogue: the tint follows the speed; dust at the feet while it
    // moves 1 px/frame or more (not jumping)
    speed = self->velocityX;
    if (speed < 0) {
        speed = -speed;
    }
    speed >>= 12;
    if (speed > 64) {
        speed = 64;
    }
#if MOD_COPPER_ARMOR_TINT
    self->opacity = speed + 112;
#endif
    if ((speed >> 4) && self->step != COPPER_JUMP && (g_Timer & 1)) {
        if (self->facingLeft) {
            x = (Random() & 3) | 16;
            r = (Random() & 3) + 1;
            EntityExplosionVariantsSpawner(self, 1, 1, SAT_X(x), 0, r, 0);
            x = (Random() & 7) | -24;
            r = (Random() & 3) + 1;
            EntityExplosionVariantsSpawner(self, 1, 1, SAT_X(x), 0, r, 0);
        } else {
            x = -16 - (Random() & 3);
            r = (Random() & 3) + 1;
            EntityExplosionVariantsSpawner(self, 1, 1, SAT_X(x), 0, r, 0);
            x = 24 - (Random() & 7);
            r = (Random() & 3) + 1;
            EntityExplosionVariantsSpawner(self, 1, 1, SAT_X(x), 0, r, 0);
        }
    }
}

// Saturn func_060E6750: the sword's hitbox (self + 1), following the body's
// frame (copper_sword_off / _size)
void EntityModCopperArmorHitbox(Entity* self) {
    u16 init[5];
    Entity* parent = self - 1;
    u8 i;

    if (self->step == 0) {
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = 0x18D;
        InitializeEntity(init); // step -> 1
        self->attack = 55;
    }
    i = parent->animCurFrame;
    if (i > 52) {
        i = 0;
    }
    self->hitboxOffX = copper_sword_off[i][0];
    self->hitboxOffY = -copper_sword_off[i][1] - 32;
    self->hitboxWidth = copper_sword_size[i][0];
    self->hitboxHeight = copper_sword_size[i][1];
    self->facingLeft = parent->facingLeft;
    self->hitboxState = parent->hitboxState;
    self->posX.i.hi = parent->posX.i.hi;
    self->posY.i.hi = parent->posY.i.hi;
    if (parent->entityId != E_MOD_COPPER_ARMOR) {
        DestroyEntity(self);
    }
}
