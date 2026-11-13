// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Skeleton Breeder and its Skeleton Beast (Saturn STAGE_15 entities
 * 0x40-0x43: func_060EF0C0 / 060EF5E0 / 060EF6E0 / 060EFF44), ported to a
 * PS1 stage overlay (Saturn port mod, translated from the SH-2). Include after
 * the stage header and the st_common helpers. See notes.md for the state list
 * and uncertainties.
 *
 * The decomp names the Saturn file entlead.c ("SkeletonLeader"), but this
 * entity calls TekiInit(0x1A4), whose def is "Skeleton Breeder"; its beast uses
 * 0x1A5 "Skeleton Beast" and the weapon hitbox 0x1A6.
 *
 * Required from the including stage:
 *   MOD_SKBREEDER_ANIMSET  animSet of the converted Saturn bank 31 (0x060FA7B8)
 *                          (38 frames, Saturn frame n = PSX frame n, 1 palette)
 *   MOD_SKBREEDER_UNK5A    (optional, default 0) unk5A / tpage of the animset
 *   enum entries E_MOD_SKBREEDER (Saturn 0x40), E_MOD_SKBREEDER_WEAPON (0x41),
 *   E_MOD_SKBREEDER_BEAST (0x42), E_MOD_SKBREEDER_DEBRIS (0x43) and
 *   E_EXPLOSION (Saturn 2).
 *
 * Entry points:
 *   EntityModSkBreeder(Entity*)        <- E_MOD_SKBREEDER (layout entity)
 *   EntityModSkBreederWeapon(Entity*)  <- E_MOD_SKBREEDER_WEAPON (self + 1)
 *   EntityModSkBreederBeast(Entity*)   <- E_MOD_SKBREEDER_BEAST (self + 2)
 *   EntityModSkBreederDebris(Entity*)  <- E_MOD_SKBREEDER_DEBRIS
 *
 * The Saturn draws the breeder's sprite 32 px and the beast's 16 px below the
 * entity (their epilogues; the frames have their feet at y = 0). The PS1
 * cannot offset an entity's drawing, so frames 13-31 must be converted +32 px
 * and frames 1-12 +16 px (gdn_assets.py shifted()); the debris frames 32-37
 * as they are. See notes.md.
 *
 * The Hell Garden's Skeleton Guardian (e_mod_skeleton_guardian.h) reuses the
 * debris (its Saturn RSTAGE15 func_060EC374 is this one with frames 0x13..):
 * with MOD_SKBREEDER_DEBRIS_ONLY defined only EntityModSkBreederDebris and its
 * helpers are compiled, MOD_SKBREEDER_DEBRIS_FRAME is the first piece's frame
 * (default 0x20) and MOD_SKBREEDER_ANIMSET / _UNK5A are the including
 * enemy's; it needs only E_EXPLOSION.
 */

#ifndef MOD_SKBREEDER_ANIMSET
#error "define MOD_SKBREEDER_ANIMSET before including e_mod_skeleton_breeder.h"
#endif
#ifndef MOD_SKBREEDER_UNK5A
#define MOD_SKBREEDER_UNK5A 0
#endif
// One palette in the bank and no clutBase changes: palette offset 0.
#define SKB_PAL 0

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif

// 1: the debris keeps the Saturn entity's ENTITY_OPACITY with opacity 0x70
// (slightly darker on the PS1); 0: plain drawing. See notes.md.
#ifndef MOD_SKBREEDER_DEBRIS_OPACITY
#define MOD_SKBREEDER_DEBRIS_OPACITY 1
#endif
#ifndef MOD_SKBREEDER_DEBRIS_FRAME
#define MOD_SKBREEDER_DEBRIS_FRAME 0x20
#endif

#ifndef MOD_SKBREEDER_DEBRIS_ONLY
// st_common.h (built in the stage's st_common.c)
s32 UnkCollisionFunc3(s16* sensors);
s32 UnkCollisionFunc2(s16* posX);

// All the sounds exist on the PS1 with the same ids.
#define MOD_SKBREEDER_SFX_SWING SFX_FAST_SWORD_SWISHES // 0x6BA, swing loop
#define MOD_SKBREEDER_SFX_THRUST SFX_WEAPON_STAB_B     // 0x62E, thrust
#define MOD_SKBREEDER_SFX_DEATH SFX_SKELETON_DEATH_C   // 0x62B, breaks up
#define MOD_SKBREEDER_SFX_STEP SFX_STOMP_HARD_E        // 0x64A, beast runs
#define MOD_SKBREEDER_SFX_BEAST_DIE                                            \
    SFX_SMALL_FLAME_IGNITE // 0x691, beast
                           // explodes

typedef enum {
    SKB_INIT = 0,
    SKB_WALK = 1,
    SKB_SWING = 3,  // weapon swinging loop
    SKB_THRUST = 4, // long low thrust when the player is close
    SKB_DEATH = 16,
} SkBreederSteps;

typedef enum {
    SKB_BEAST_INIT = 0,
    SKB_BEAST_FALL = 1,
    SKB_BEAST_IDLE = 5,
    SKB_BEAST_GROWL = 6,
    SKB_BEAST_7 = 7, // goes straight back to IDLE (never set by the code)
    SKB_BEAST_DASH = 8,
    SKB_BEAST_RETURN = 9, // runs back to its master
    SKB_BEAST_DEATH = 10,
} SkBreederBeastSteps;

#endif // !MOD_SKBREEDER_DEBRIS_ONLY

// Ext fields, accessed through SKB_EXT() (no change to the Ext union).
typedef struct {
    /* Saturn 0x8C */ s16 timer;
    /* Saturn 0x8E */ s16 counter; // breeder: swing frames; beast: cleared
    /* Saturn 0x1E */ s16 spin;    // the Saturn keeps the spin speed in
                                   // entity->rotate and adds it to the sprite's
} ET_ModSkBreeder;
#define SKB_EXT(e) ((ET_ModSkBreeder*)&(e)->ext)

#ifndef MOD_SKBREEDER_DEBRIS_ONLY
// Saturn 0x060FA77E.. animations, frames 1:1
static u8 skb_anim_walk[] = { // 0x060FA77E, breeder WALK (loops)
    0x06, 0x0E, 0x04, 0x0F, 0x04, 0x10, 0x06,
    0x11, 0x05, 0x12, 0x05, 0x13, 0x00, 0x00};
static u8 skb_anim_swing[] = { // 0x060FA78C, breeder SWING (loops)
    0x02, 0x14, 0x03, 0x15, 0x03, 0x16, 0x03,
    0x17, 0x03, 0x18, 0x02, 0x19, 0x00, 0x00};
static u8 skb_anim_thrust[] = { // 0x060FA79A, breeder THRUST
    0x02, 0x1A, 0x03, 0x1B, 0x03, 0x1C, 0x03,
    0x1D, 0x12, 0x1E, 0x12, 0x1F, 0xFF, 0xFF};
static u8 skb_anim_beast_idle[] = { // 0x060FA760, beast IDLE (loops)
    0x0A, 0x01, 0x0A, 0x08, 0x0A, 0x09, 0x2F, 0x01, 0x00, 0x00};
static u8 skb_anim_beast_growl[] = { // 0x060FA76A, beast GROWL (loops)
    0x01, 0x0A, 0x01, 0x0B, 0x00, 0x00};
static u8 skb_anim_beast_run[] = { // 0x060FA770, beast DASH/RETURN (loops)
    0x03, 0x01, 0x03, 0x02, 0x03, 0x03, 0x03,
    0x04, 0x03, 0x05, 0x03, 0x07, 0x00, 0x00};

// Collision sensors (X scaled, Y as is)
// Saturn 0x060FA758 {{0,32},{8,0}}: breeder, UnkCollisionFunc2
static s16 skb_sensors_walk[] = {0, 32, SAT_X(8), 0};
// Saturn 0x060FA810 {{0,16},{4,0}}: beast, UnkCollisionFunc2
static s16 skb_sensors_beast_walk[] = {0, 16, SAT_X(4), 0};
// Saturn 0x060FA818 {{0,16},{0,4},{8,-4},{-16,0}}: beast, UnkCollisionFunc3
static s16 skb_sensors_beast_fall[] = {
    0, 16, 0, 4, SAT_X(8), -4, SAT_X(-16), 0};

// Saturn 0x060FA7E4 (0x060FA794 + frame * 4): weapon hitbox {w, h, offX,
// offY} for the breeder's frames 0x14..0x1E, Saturn space (SAT_X at runtime)
static s8 skb_weapon_hitbox[11][4] = {
    {16, 3, 0, -16},  {16, 3, 0, -16},  {16, 3, 0, -16},  {16, 3, 0, -16},
    {16, 3, 0, -16},  {16, 3, 0, -16},  {16, 3, 0, -16},  {16, 2, 0, -16},
    {12, 12, -44, 2}, {20, 8, -48, 22}, {16, 2, -52, 28},
};

// Saturn 0x060FA7A8 / 0x060FA7B0: where the 6 debris pieces appear (X is
// mirrored by facing). Two more entries follow in the data, unused.
static s8 skb_debris_off_x[6] = {-3, -4, 8, 7, 0, -22};
static s8 skb_debris_off_y[6] = {-8, -5, -8, 22, 8, 19};

#endif // !MOD_SKBREEDER_DEBRIS_ONLY

// Stand-in for the Saturn's MTH_GetRand(): only its low 17 bits are used.
static u32 SkbGetRand(void) {
    u32 r = Random();
    r = (r << 8) | Random();
    return (r << 8) | Random();
}

#ifndef MOD_SKBREEDER_DEBRIS_ONLY

static void SkbFacePlayer(Entity* self) {
    self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
}

void EntityModSkBreeder(Entity* self) {
    u16 init[5];
    Entity* ent;
    s32 i;
    u8 r;

    if ((self->flags & FLAG_DEAD) && self->step != SKB_DEATH) {
        SetStep(SKB_DEATH);
    }

    switch (self->step) {
    case SKB_INIT:
        init[0] = MOD_SKBREEDER_ANIMSET;
        init[1] = 0; // Saturn sets animCurFrame 0 (an empty frame)
        init[2] = MOD_SKBREEDER_UNK5A;
        init[3] = SKB_PAL;
        init[4] = 0x1A4; // "Skeleton Breeder"
        InitializeEntity(init);
        self->zPriority = MOD_GDN_ENEMY_FRONT_Z; // above the flowers
        // Saturn-space values written after TekiInit
        self->hitboxWidth = SAT_X(6);
        self->hitboxHeight = 16;
        self->hitboxOffX = SAT_X(-8);
        self->hitboxOffY = 8;
        SkbFacePlayer(self);
        // the weapon hitbox and the beast, in the next two slots
        CreateEntityFromEntity(E_MOD_SKBREEDER_WEAPON, self, self + 1);
        CreateEntityFromEntity(E_MOD_SKBREEDER_BEAST, self, self + 2);
        SetStep(SKB_WALK);
        self->velocityX = 0;
        self->velocityY = 0;
        self->animCurFrame = 0;
        break;

    case SKB_WALK:
        if ((g_Timer % 30) == 0) {
            SkbFacePlayer(self);
        }
        if (GetDistanceToPlayerX() <= SAT_X(79)) {
            r = Random();
            if ((r % 30) == 0) {
                self->step = SKB_SWING;
            }
        } else if (GetDistanceToPlayerX() <= SAT_X(127)) {
            r = Random();
            if ((r % 60) == 0) {
                self->step = SKB_SWING;
            }
        }
        if (self->step != SKB_WALK) {
            SkbFacePlayer(self);
            SetStep(self->step);
            self->velocityX = 0;
            self->velocityY = 0;
            break;
        }
        AnimateEntity(skb_anim_walk, self);
        if (self->facingLeft) {
            self->velocityX = SAT_X(FIX(0.375)); // Saturn 0x6000
        } else {
            self->velocityX = SAT_X(-FIX(0.375));
        }
        UnkCollisionFunc2(skb_sensors_walk);
        break;

    case SKB_SWING:
        switch (self->step_s) {
        case 0:
            r = Random();
            SKB_EXT(self)->timer = (r % 60) + 100;
            SKB_EXT(self)->counter = 0;
            self->step_s++;
            break;
        case 1:
            AnimateEntity(skb_anim_swing, self);
            // pose 5 with timer 0: the next AnimateEntity starts frame 0x19
            if (self->pose == 5 && self->poseTimer == 0) {
                PlaySfxPositional(MOD_SKBREEDER_SFX_SWING);
            }
            if (++SKB_EXT(self)->counter > 50) {
                if (GetDistanceToPlayerX() <= SAT_X(79)) {
                    SetStep(SKB_THRUST);
                    self->velocityX = 0;
                    self->velocityY = 0;
                    break;
                }
            }
            if (--SKB_EXT(self)->timer < 0) {
                SetStep(SKB_WALK);
                self->velocityX = 0;
                self->velocityY = 0;
            }
            break;
        }
        break;

    case SKB_THRUST:
        // pose 4 with timer 0: the next AnimateEntity starts frame 0x1E
        if (self->pose == 4 && self->poseTimer == 0) {
            PlaySfxPositional(MOD_SKBREEDER_SFX_THRUST);
        }
        if (!AnimateEntity(skb_anim_thrust, self)) {
            SetStep(SKB_WALK);
            self->velocityX = 0;
            self->velocityY = 0;
        }
        break;

    case SKB_DEATH:
        switch (self->step_s) {
        case 0:
            SkbFacePlayer(self);
            for (i = 0; i < 6; i++) {
                ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
                if (ent == NULL) {
                    break;
                }
                CreateEntityFromEntity(E_MOD_SKBREEDER_DEBRIS, self, ent);
                ent->facingLeft = self->facingLeft;
                ent->params = i;
                if (self->facingLeft) {
                    ent->posX.i.hi -= SAT_X(skb_debris_off_x[i]);
                } else {
                    ent->posX.i.hi += SAT_X(skb_debris_off_x[i]);
                }
                ent->posY.i.hi += skb_debris_off_y[i];
                // TODO Z (Saturn: the breeder's sprite zPriority; the debris
                // then draws at that - 2)
                ent->zPriority = self->zPriority;
            }
            PlaySfxPositional(MOD_SKBREEDER_SFX_DEATH);
            self->step_s++;
            break;
        case 1:
            DestroyEntity(self);
            return;
        }
        break;
    }
    // Saturn epilogue: sprite pos = entity pos + (0, 32); see the header.
}

// Saturn func_060EF5E0: the breeder's weapon, an invisible hitbox entity.
void EntityModSkBreederWeapon(Entity* self) {
    u16 init[5];
    Entity* parent = self - 1;
    s32 idx;

    switch (self->step) {
    case 0:
        init[0] = 0; // no sprite
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = 0x1A6;
        InitializeEntity(init); // step -> 1
        /* fallthrough */
    case 1:
        idx = parent->animCurFrame - 0x14;
        if ((u16)idx <= 10) {
            self->hitboxWidth = SAT_X(skb_weapon_hitbox[idx][0]);
            self->hitboxHeight = skb_weapon_hitbox[idx][1];
            self->hitboxOffX = SAT_X(skb_weapon_hitbox[idx][2]);
            self->hitboxOffY = skb_weapon_hitbox[idx][3];
        } else {
            self->hitboxWidth = 0;
            self->hitboxHeight = 0;
            self->hitboxOffX = 0;
            self->hitboxOffY = 0;
        }
        self->facingLeft = parent->facingLeft;
        self->hitboxState = parent->hitboxState;
        self->posX.i.hi = parent->posX.i.hi;
        self->posY.i.hi = parent->posY.i.hi;
        if (parent->entityId != E_MOD_SKBREEDER) {
            DestroyEntity(self);
        }
        break;
    }
}

// Saturn func_060EF6E0: the beast that stays with the breeder (self - 2).
void EntityModSkBreederBeast(Entity* self) {
    u16 init[5];
    Collider col;
    Entity* ent;
    Entity* master = self - 2;
    s32 dx;
    u8 r;

    // the breeder is gone (killed or despawned): explode
    if (master->entityId != E_MOD_SKBREEDER) {
        ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
        if (ent != NULL) {
            CreateEntityFromEntity(E_EXPLOSION, self, ent);
            ent->params = 0;
        }
        DestroyEntity(self);
        return;
    }
    if ((self->flags & FLAG_DEAD) && self->step != SKB_BEAST_DEATH) {
        SetStep(SKB_BEAST_DEATH);
        self->velocityX = 0;
        self->velocityY = 0;
    }

    switch (self->step) {
    case SKB_BEAST_INIT:
        init[0] = MOD_SKBREEDER_ANIMSET;
        init[1] = 1;
        init[2] = MOD_SKBREEDER_UNK5A;
        init[3] = SKB_PAL;
        init[4] = 0x1A5; // "Skeleton Beast"
        InitializeEntity(init);
        self->zPriority = MOD_GDN_ENEMY_FRONT_Z; // above the flowers
        self->hitboxWidth = SAT_X(9);
        self->hitboxHeight = 5;
        self->hitboxOffX = 0;
        self->hitboxOffY = 5;
        SkbFacePlayer(self);
        SetStep(SKB_BEAST_FALL);
        self->velocityX = 0;
        self->velocityY = 0;
        self->animCurFrame = 1;
        break;

    case SKB_BEAST_FALL:
        if (UnkCollisionFunc3(skb_sensors_beast_fall) & 1) {
            SetStep(SKB_BEAST_IDLE);
            self->velocityX = 0;
            self->velocityY = 0;
        }
        break;

    case SKB_BEAST_IDLE:
        if ((g_Timer % 30) == 0) {
            SkbFacePlayer(self);
        }
        if (GetDistanceToPlayerX() <= SAT_X(79)) {
            r = Random();
            r %= 60;
            if (r == 1) {
                self->step = SKB_BEAST_RETURN;
            } else if (r == 2) {
                self->step = SKB_BEAST_GROWL;
            }
        } else if (GetDistanceToPlayerX() <= SAT_X(0x9F)) {
            r = Random();
            if ((r % 60) == 0) {
                self->step = SKB_BEAST_GROWL;
            }
        } else {
            r = Random();
            if ((r % 60) == 0) {
                self->step = SKB_BEAST_RETURN;
            }
        }
        if (self->step != SKB_BEAST_IDLE) {
            SetStep(self->step);
            self->velocityX = 0;
            self->velocityY = 0;
            break;
        }
        AnimateEntity(skb_anim_beast_idle, self);
        break;

    case SKB_BEAST_GROWL:
        switch (self->step_s) {
        case 0:
            SkbFacePlayer(self);
            SKB_EXT(self)->timer = 90;
            SKB_EXT(self)->counter = 0;
            self->step_s++;
            break;
        case 1:
            AnimateEntity(skb_anim_beast_growl, self);
            if (GetDistanceToPlayerX() > SAT_X(0xB0)) {
                SetStep(SKB_BEAST_IDLE);
                self->velocityX = 0;
                self->velocityY = 0;
            } else if (--SKB_EXT(self)->timer < 0) {
                SetStep(SKB_BEAST_DASH);
                self->velocityX = 0;
                self->velocityY = 0;
            }
            break;
        }
        break;

    case SKB_BEAST_7:
        SetStep(SKB_BEAST_IDLE);
        self->velocityX = 0;
        self->velocityY = 0;
        break;

    case SKB_BEAST_DASH:
        switch (self->step_s) {
        case 0:
            r = Random();
            SKB_EXT(self)->timer = (r % 20) + 50;
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(7));
            } else {
                self->velocityX = SAT_X(-FIX(7));
            }
            self->step_s++;
            break;
        case 1:
            self->velocityX = self->velocityX * 32 / 33;
            if ((g_Timer & 7) == 0) {
                PlaySfxPositional(MOD_SKBREEDER_SFX_STEP);
            }
            AnimateEntity(skb_anim_beast_run, self);
            if (--SKB_EXT(self)->timer < 0) {
                SkbFacePlayer(self);
                SetStep(SKB_BEAST_IDLE);
                self->velocityX = 0;
                self->velocityY = 0;
            }
            break;
        }
        UnkCollisionFunc2(skb_sensors_beast_walk);
        break;

    case SKB_BEAST_RETURN:
        switch (self->step_s) {
        case 0:
            // face the breeder (Saturn compares the full fixed-point X)
            self->facingLeft = (self->posX.val > master->posX.val) ? 0 : 1;
            SKB_EXT(self)->timer = 120;
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(3.5));
            } else {
                self->velocityX = SAT_X(-FIX(3.5));
            }
            self->step_s++;
            /* fallthrough */
        case 1:
            dx = (s16)(master->posX.i.hi - self->posX.i.hi);
            if (dx < 0) {
                dx = -dx;
            }
            if (dx <= SAT_X(47)) {
                SKB_EXT(self)->timer = 30;
                self->step_s++;
            } else if (--SKB_EXT(self)->timer < 0) {
                SKB_EXT(self)->timer = 30;
                self->step_s++;
            }
            break;
        case 2:
            self->velocityX = self->velocityX * 16 / 17;
            if (--SKB_EXT(self)->timer < 0) {
                SkbFacePlayer(self);
                SetStep(SKB_BEAST_IDLE);
                self->velocityX = 0;
                self->velocityY = 0;
            }
            break;
        }
        // every frame of RETURN (also the frame it switches to IDLE)
        if ((g_Timer & 7) == 0) {
            PlaySfxPositional(MOD_SKBREEDER_SFX_STEP);
        }
        AnimateEntity(skb_anim_beast_run, self);
        UnkCollisionFunc2(skb_sensors_beast_walk);
        break;

    case SKB_BEAST_DEATH:
        switch (self->step_s) {
        case 0:
            self->hitboxState = 0;
            SkbFacePlayer(self);
            self->animCurFrame = 12;
            SKB_EXT(self)->timer = (Random() & 31) + 48;
            self->drawFlags |= ENTITY_ROTATE; // Saturn sprite flags |= 0x40
            SKB_EXT(self)->spin = 32;
            if (!self->facingLeft) {
                self->velocityX = SAT_X((s32)(SkbGetRand() & 0xFFFF) + FIX(2));
            } else {
                self->velocityX = SAT_X(-FIX(2) - (s32)(SkbGetRand() & 0xFFFF));
            }
            self->velocityY = -FIX(5) - (s32)(SkbGetRand() & 0x1FFFF);
            self->step_s++;
            break;
        case 1:
            g_api.CheckCollision(self->posX.i.hi, self->posY.i.hi, &col, 0);
            if (col.effects & EFFECT_SOLID) {
                SKB_EXT(self)->timer = 1;
            }
            if (--SKB_EXT(self)->timer) {
                self->rotate += SKB_EXT(self)->spin;
                FallEntity();
                MoveEntity();
                break;
            }
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent != NULL) {
                CreateEntityFromEntity(E_EXPLOSION, self, ent);
                ent->params = 0;
            }
            g_api.PlaySfx(MOD_SKBREEDER_SFX_BEAST_DIE);
            DestroyEntity(self);
            return;
        }
        break;
    }
    // Saturn epilogue: sprite pos = entity pos + (0, 16); see the header.
}

#endif // !MOD_SKBREEDER_DEBRIS_ONLY

// Saturn func_060EFF44: a piece of the breeder, thrown up spinning; explodes
// when it touches the ground or after 48-79 frames.
void EntityModSkBreederDebris(Entity* self) {
    u16 init[5];
    Collider col;
    Entity* ent;

    switch (self->step) {
    case 0:
        init[0] = MOD_SKBREEDER_ANIMSET;
        init[1] = self->params + MOD_SKBREEDER_DEBRIS_FRAME; // 0x20..0x25
        init[2] = MOD_SKBREEDER_UNK5A;
        init[3] = SKB_PAL;
        init[4] = ENEMY_INTERACTABLE; // Saturn TekiInit(self, 5)
        InitializeEntity(init);       // step -> 1; keeps the zPriority
        self->hitboxState = 0;
        self->hitboxWidth = 0;
        self->hitboxHeight = 0;
        self->animCurFrame = self->params + MOD_SKBREEDER_DEBRIS_FRAME;
        self->zPriority -= 2; // TODO Z (Saturn sprite z = entity z - 2)
#if MOD_SKBREEDER_DEBRIS_OPACITY
        self->drawFlags = ENTITY_ROTATE | ENTITY_OPACITY;
        self->opacity = 0x70;
#else
        self->drawFlags = ENTITY_ROTATE;
#endif
        SKB_EXT(self)->timer = (Random() & 31) + 48;
        SKB_EXT(self)->spin = (Random() & 63) + 32;
        if (!self->facingLeft) {
            self->velocityX = SAT_X((s32)(SkbGetRand() & 0x1FFFF) + FIX(1));
        } else {
            self->velocityX = SAT_X(-FIX(1) - (s32)(SkbGetRand() & 0x1FFFF));
        }
        self->velocityY = -FIX(3) - (s32)(SkbGetRand() % FIX(3));
        break;

    case 1:
        g_api.CheckCollision(self->posX.i.hi, self->posY.i.hi, &col, 0);
        if (col.effects & EFFECT_SOLID) {
            SKB_EXT(self)->timer = 1;
        }
        if (--SKB_EXT(self)->timer) {
            self->rotate += SKB_EXT(self)->spin;
            FallEntity();
            MoveEntity();
            break;
        }
        ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
        if (ent != NULL) {
            CreateEntityFromEntity(E_EXPLOSION, self, ent);
            ent->params = 0;
        }
        DestroyEntity(self);
        return;
    }
}
