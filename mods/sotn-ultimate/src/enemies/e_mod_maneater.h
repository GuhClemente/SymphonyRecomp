// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Man Eater ("Hitotorisou", Saturn STAGE_15 entities 0x3B-0x3C:
 * func_060EDF80 / func_060EE4B8), ported to a PS1 stage overlay (Saturn port
 * mod, translated from the SH-2; see saturn_maneater.c / notes.md). Include
 * after the stage header and the st_common helpers.
 *
 * A pod plant. It sits closed until the player comes within 175 (Saturn) px,
 * opens, then shuffles toward the player. Up close it bites: if the player is
 * in front of it and in reach (and not crouching, in mist/bat form, stoned,
 * invincible or in the axe armor), it swallows the player (GRAB_START), chews
 * (GRAB_DAMAGE, 48 damage), spits the player out (GRAB_NONE) and recovers.
 * While it holds the player it has no hitbox and draws in front of the
 * player. When killed it waits 3 frames and bursts into 19 pieces that fall
 * and pop into small explosions on the floor.
 *
 * Required from the including stage:
 *   MOD_MANEATER_ANIMSET  animSet of the converted Saturn bank 29 (0x060F8BFC,
 *                         frame table 0x060F9470, 59 frames, Saturn frame n =
 *                         PSX frame n, converted WITHOUT a y shift; frames
 *                         56-58 are unused and may be left empty)
 *   MOD_MANEATER_UNK5A    (optional, default 0) unk5A / tpage of the animset
 *   enum entries E_MOD_MANEATER (Saturn 0x3B, layout), E_MOD_MANEATER_DEBRIS
 *   (Saturn 0x3C), E_EXPLOSION (Saturn 2)
 *   enemy def 414 (0x19E, "Man Eater") and 2 (ENEMY_PARTICLE)
 *
 * Slots: the Man Eater uses only its own (layout) slot. The debris and its
 * explosions use AllocEntity 224..255.
 *
 * Entry points:
 *   EntityModManEater(Entity*)        <- E_MOD_MANEATER (layout, params unused)
 *   EntityModManEaterDebris(Entity*)  <- E_MOD_MANEATER_DEBRIS (spawned only)
 *
 * Drawing: no draw offset, no rotation (the Saturn sets its sprite rotation
 * flag but rotate is always 0), palette offset 0 (the frames carry their part
 * palettes 0-3).
 */

#ifndef MOD_MANEATER_ANIMSET
#error "define MOD_MANEATER_ANIMSET before including e_mod_maneater.h"
#endif
#ifndef MOD_MANEATER_UNK5A
#define MOD_MANEATER_UNK5A 0
#endif
// 6 palettes in the bank, chosen per sprite part (baked into the frames:
// the plant uses 0-2, the debris 3, the unused frames 57/58 use 4/5); the
// code never changes clutBase: palette offset 0.
#define MANEATER_PAL 0
#define MOD_MANEATER_ENEMY 0x19E    // Saturn TekiInit(self, 0x19E), 414
#define MOD_MANEATER_DEBRIS_ENEMY 2 // Saturn TekiInit(self, 2), ENEMY_PARTICLE

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif

// z. TODO Z: the Saturn uses the bank's default sprite z for the plant and
// the debris, and raises the plant's sprite z by 0x30 from the bite's 6th
// pose (the jaws closing) until it is back to walking, so the swallowed
// player is drawn inside it. The greenhouse fg (plants) is at 0x60 on the
// PS1, Alucard at 0x94: the plant walks behind Alucard (the InitializeEntity
// default, 0x94 - 0xC) and covers him while biting.
#ifndef MOD_MANEATER_Z
#define MOD_MANEATER_Z 0x88
#endif
#ifndef MOD_MANEATER_BITE_DZ
#define MOD_MANEATER_BITE_DZ 0x30 // Saturn: sprite z + 0x30 -> 0xB8
#endif
#ifndef MOD_MANEATER_DEBRIS_Z
#define MOD_MANEATER_DEBRIS_Z MOD_MANEATER_Z
#endif

// st_common.h (built in the stage's st_common.c)
s32 UnkCollisionFunc3(s16* sensors);
s32 UnkCollisionFunc2(s16* posX);

// Sounds: 0x90B, 0x90C and 0x90D are Saturn-only (the PS1 list ends at
// 0x8F5); PS1 plant sounds that GDN already plays stand in.
#define MOD_MANEATER_SFX_OPEN MOD_SFX_MANEATER_OPEN   // Saturn 0x90C wake/bite
#define MOD_MANEATER_SFX_CHOMP MOD_SFX_MANEATER_SNAP  // Saturn 0x90D jaws
#define MOD_MANEATER_SFX_BURST MOD_SFX_MANEATER_BURST // Saturn 0x90B bursts

// Saturn g_Player.status & 0x010401A3: the player cannot be swallowed
#define MANEATER_NO_GRAB_STATUS                                                \
    (PLAYER_STATUS_AXEARMOR | PLAYER_STATUS_DEAD | PLAYER_STATUS_INVINCIBLE |  \
     PLAYER_STATUS_STONE | PLAYER_STATUS_CROUCH | PLAYER_STATUS_MIST_FORM |    \
     PLAYER_STATUS_BAT_FORM)
#define MANEATER_GRAB_DAMAGE 48 // Saturn: g_Player.grabDamage = 48

typedef enum {
    MANEATER_INIT = 0,
    MANEATER_IDLE = 1,   // closed pod, waits for the player (|dx| <= 175)
    MANEATER_WAKE = 2,   // opens up
    MANEATER_WALK = 3,   // walks toward the player, bites at |dx| <= 31
    MANEATER_ATTACK = 4, // step_s: 0 bite, 1 chew, 2 spit, 3/4 recover
    MANEATER_DEATH = 5,  // bursts after 3 frames
} ManEaterSteps;

enum {
    MANEATER_BITE = 0,
    MANEATER_CHEW = 1,
    MANEATER_SPIT = 2,
    MANEATER_RECOVER = 3,
    MANEATER_RECOVER_MISS = 4,
};

// Ext fields, accessed through MANEATER_EXT() (no change to the Ext union)
typedef struct {
    /* Saturn 0x78 */ u16 savedHitboxState;
    /* Saturn 0x7C */ s16 timer; // set to 30 at the bite, never read
} ET_ModManEater;
#define MANEATER_EXT(e) ((ET_ModManEater*)&(e)->ext)

// Saturn 0x060F8C18.. animations, frames 1:1
static u8 maneater_anim_idle[] = { // 0x060F8C18, loops
    15, 1, 15, 2, 15, 3, 15, 2, 0, 0};
static u8 maneater_anim_wake[] = { // 0x060F8C22
    32, 2, 3, 4, 3, 5, 3, 6, 3, 7, 32, 8, 0xFF, 0xFF};
static u8 maneater_anim_walk[] = { // 0x060F8C30, loops
    11, 9, 11, 10, 11, 11, 11, 12, 11, 13, 11, 14, 11, 15, 11, 16, 0, 0};
static u8 maneater_anim_bite[] = { // 0x060F8C42
    5, 17, 6, 18, 7, 19, 8, 20, 4, 21, 3, 22, 2, 23, 1, 24, 8, 25, 0xFF, 0xFF};
static u8 maneater_anim_chew[] = { // 0x060F8C56
    7, 26, 7, 27, 7, 28, 7, 27, 7, 29, 7, 30, 7, 29, 0xFF, 0xFF};
static u8 maneater_anim_spit[] = { // 0x060F8C66
    23, 36, 3, 26, 2, 31, 1, 32, 15, 33, 0xFF, 0xFF};
static u8 maneater_anim_recover[] = { // 0x060F8C72
    48, 33, 7, 34, 7, 35, 0xFF, 0xFF};

// Collision sensors (X scaled, Y as is)
// Saturn 0x060F8BD8 {{0,0},{0,0}}: UnkCollisionFunc2 (WALK): the floor
// sensor is the origin itself, the frames end at y = -1
static s16 maneater_sensors_walk[] = {0, 0, 0, 0};
// Saturn 0x060F8BE8 {{0,8},{0,4},{8,-4},{-16,0}}: UnkCollisionFunc3 (debris)
static s16 maneater_sensors_debris[] = {
    0, 8, 0, 4, SAT_X(8), -4, SAT_X(-16), 0};

void EntityModManEater(Entity* self) {
    u16 init[5];
    Entity* ent;
    s16 absX;
    s32 i, v;

    if ((self->flags & FLAG_DEAD) && self->step != MANEATER_DEATH) {
        SetStep(MANEATER_DEATH);
    }

    switch (self->step) {
    case MANEATER_INIT:
        init[0] = MOD_MANEATER_ANIMSET;
        init[1] = 1;
        init[2] = MOD_MANEATER_UNK5A;
        init[3] = MANEATER_PAL;
        init[4] = MOD_MANEATER_ENEMY;
        InitializeEntity(init);           // step -> 1
        self->zPriority = MOD_MANEATER_Z; // TODO Z (Saturn: default sprite z)
        MANEATER_EXT(self)->savedHitboxState = self->hitboxState;
        self->hitboxOffY = -24;
        self->hitboxOffX = SAT_X(-8);
        self->hitboxWidth = SAT_X(18);
        self->hitboxHeight = 24;
        /* fallthrough */
    case MANEATER_IDLE:
        AnimateEntity(maneater_anim_idle, self);
        if (GetDistanceToPlayerX() <= SAT_X(175)) {
            self->pose = 0;
            self->poseTimer = 0;
            self->step = MANEATER_WAKE;
        }
        break;

    case MANEATER_WAKE:
        if (self->pose == 1 && self->poseTimer == 0) {
            g_api.PlaySfx(MOD_MANEATER_SFX_OPEN);
        }
        if (!AnimateEntity(maneater_anim_wake, self)) {
            self->pose = 0;
            self->poseTimer = 0;
            self->step = MANEATER_WALK;
        }
        break;

    case MANEATER_WALK:
        AnimateEntity(maneater_anim_walk, self);
        self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
        if (self->facingLeft) {
            self->velocityX = SAT_X(FIX(1));
        } else {
            self->velocityX = SAT_X(FIX(-1));
        }
        if (GetDistanceToPlayerX() <= SAT_X(31)) {
            self->pose = 0;
            self->poseTimer = 0;
            MANEATER_EXT(self)->timer = 30;
            self->step = MANEATER_ATTACK;
            self->step_s = MANEATER_BITE;
        }
        UnkCollisionFunc2(maneater_sensors_walk);
        break;

    case MANEATER_ATTACK:
        switch (self->step_s) {
        case MANEATER_BITE:
            if (self->pose == 1 && self->poseTimer == 0) {
                g_api.PlaySfx(MOD_MANEATER_SFX_OPEN);
            }
            if (self->pose == 4 && self->poseTimer == 0) {
                g_api.PlaySfx(MOD_MANEATER_SFX_CHOMP);
            }
            if (self->pose == 5 && self->poseTimer == 0) {
                // TODO Z (Saturn: sprite z + 0x30 until back to walking)
                self->zPriority += MOD_MANEATER_BITE_DZ;
            }
            if (AnimateEntity(maneater_anim_bite, self)) {
                break;
            }
            // the player must be in front of it and in reach
            if (!(g_Player.status & MANEATER_NO_GRAB_STATUS) &&
                GetDistanceToPlayerX() <= SAT_X(55) &&
                GetDistanceToPlayerY() <= 47 &&
                (GetSideToPlayer() & 1) != self->facingLeft) {
                g_Player.grabState = GRAB_START;
                g_Player.grabDamage = 0;
                g_Player.grabReactionEffect = GRAB_REACTION_NORMAL;
                self->hitboxState = 0;
                self->step_s = MANEATER_CHEW;
            } else {
                self->step_s = MANEATER_RECOVER_MISS;
                UnkCollisionFunc2(maneater_sensors_walk);
            }
            self->pose = 0;
            self->poseTimer = 0;
            break;

        case MANEATER_CHEW:
            if (!AnimateEntity(maneater_anim_chew, self)) {
                g_Player.grabDamage = MANEATER_GRAB_DAMAGE;
                g_Player.grabState = GRAB_DAMAGE;
                self->pose = 0;
                self->poseTimer = 0;
                self->step_s++;
            }
            break;

        case MANEATER_SPIT:
            if (!AnimateEntity(maneater_anim_spit, self)) {
                g_Player.grabState = GRAB_NONE;
                self->hitboxState = MANEATER_EXT(self)->savedHitboxState;
                self->pose = 0;
                self->poseTimer = 0;
                self->step_s++;
            }
            break;

        case MANEATER_RECOVER_MISS:
            self->hitboxState = MANEATER_EXT(self)->savedHitboxState;
            /* fallthrough */
        case MANEATER_RECOVER:
            if (!AnimateEntity(maneater_anim_recover, self)) {
                self->pose = 0;
                self->poseTimer = 0;
                self->step = MANEATER_WALK;
                self->step_s = 0;
                self->zPriority -= MOD_MANEATER_BITE_DZ; // TODO Z
            }
            break;
        }
        break;

    case MANEATER_DEATH:
        if (self->step_s <= 2) {
            self->step_s++;
            break;
        }
        if (self->step_s != 3) {
            break;
        }
        g_Player.grabState = GRAB_NONE;
        for (i = 0; i < 19; i++) {
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent == NULL) {
                continue;
            }
            CreateEntityFromEntity(E_MOD_MANEATER_DEBRIS, self, ent);
            ent->params = 0x25 + i; // frames 37..55
            // Saturn: ((rnd & 15) - 8) px / 5 * 3, then * 4/5 for the PS1
            v = ((Random() & 15) - 8) << 16;
            ent->velocityX = SAT_X(v / 5 * 3);
            v = ((Random() & 7) + 2) << 16;
            ent->velocityY = -v / 5 * 4;
        }
        g_api.PlaySfx(MOD_MANEATER_SFX_BURST);
        DestroyEntity(self);
        return;
    }

    // Saturn epilogue: keeps it in the room, absolute X 167..1112 (Saturn).
    // The left limit applies while moving left, the right one otherwise.
    // (On the Saturn this does not hold while walking, see notes.md.)
    absX = self->posX.i.hi + g_Tilemap.scrollX.i.hi;
    if (self->velocityX < 0) {
        if (absX < SAT_X(167)) {
            self->posX.i.hi = SAT_X(167) - g_Tilemap.scrollX.i.hi;
        }
    } else if (absX > SAT_X(1112)) {
        self->posX.i.hi = SAT_X(1112) - g_Tilemap.scrollX.i.hi;
    }
}

// Saturn func_060EE4B8: one of the 19 pieces of the burst pod (frame =
// params, 37..55). Falls with gravity and pops into a small explosion
// (E_EXPLOSION params 1) when it lands.
void EntityModManEaterDebris(Entity* self) {
    u16 init[5];
    Entity* ent;

    switch (self->step) {
    case 0:
        init[0] = MOD_MANEATER_ANIMSET;
        init[1] = 0;
        init[2] = MOD_MANEATER_UNK5A;
        init[3] = MANEATER_PAL;
        init[4] = MOD_MANEATER_DEBRIS_ENEMY;
        InitializeEntity(init);                  // step -> 1
        self->zPriority = MOD_MANEATER_DEBRIS_Z; // TODO Z (Saturn: default)
        self->animCurFrame = self->params;
        break;
    case 1:
        if (UnkCollisionFunc3(maneater_sensors_debris) & 1) {
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent != NULL) {
                CreateEntityFromEntity(E_EXPLOSION, self, ent);
                ent->params = 1;
            }
            DestroyEntity(self);
            return;
        }
        break;
    }
}
