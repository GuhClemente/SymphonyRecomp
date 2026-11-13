// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * The Gardener (Saturn STAGE_15 entities 0x3D-0x3F: func_060EE6C4 /
 * 060EEEE4 / 060EEE20), ported to a PS1 stage overlay (Saturn port mod,
 * translated from the SH-2). Include after the stage header and the
 * st_common helpers. See notes.md for the state list and uncertainties.
 *
 * A gardener with big hedge shears. It walks slowly toward the player
 * clicking its shears; at the end of each walk cycle it either keeps walking
 * (50%), or, when the player is far, winds up and dashes with the shears held
 * out (a separate hitbox entity), or, when the player is near, throws the
 * shears as a spinning boomerang.
 *
 * Required from the including stage:
 *   MOD_GARDENER_ANIMSET  animSet of the converted Saturn bank 30 (0x060F96D8,
 *                         frame table 0x060FA59C, 46 frames, Saturn frame n =
 *                         PSX frame n, converted WITHOUT a y shift)
 *   MOD_GARDENER_UNK5A    (optional, default 0) unk5A / tpage of the animset
 *   enum entries E_MOD_GARDENER (Saturn 0x3D), E_MOD_GARDENER_SHEARS (0x3E),
 *   E_MOD_GARDENER_HITBOX (0x3F), E_EXPLOSION (Saturn 2); the stage's
 *   EntityExplosionVariantsSpawner (e_misc.h, E_EXPLOSION_VARIANTS).
 *
 * Entry points:
 *   EntityModGardener(Entity*)        <- E_MOD_GARDENER (layout, params 0/1)
 *   EntityModGardenerShears(Entity*)  <- E_MOD_GARDENER_SHEARS (thrown)
 *   EntityModGardenerHitbox(Entity*)  <- E_MOD_GARDENER_HITBOX (self + 1)
 */

#ifndef MOD_GARDENER_ANIMSET
#error "define MOD_GARDENER_ANIMSET before including e_mod_gardener.h"
#endif
#ifndef MOD_GARDENER_UNK5A
#define MOD_GARDENER_UNK5A 0
#endif
// 4 palettes in the bank, chosen per sprite part (baked into the frames);
// the code never changes clutBase: palette offset 0.
#define GARDENER_PAL 0

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif

// st_common.h (built in the stage's st_common.c) / e_misc.h (e_misc.c)
s32 UnkCollisionFunc3(s16* sensors);
s32 UnkCollisionFunc2(s16* posX);
void EntityExplosionVariantsSpawner(
    Entity* self, u8 count, u8 params, s16 x, s16 y, u8 index, s16 xGap);

// Sounds. 0x60A, 0x60C, 0x616 and 0x6C7 are PS1 ids too; 0x90E and 0x90F
// are Saturn-only, PS1 sounds stand in.
#define MOD_GARDENER_SFX_SNIP MOD_SFX_GARDENER_SHEARS // Saturn 0x90F, click
#define MOD_GARDENER_SFX_DEATH MOD_SFX_GARDENER_DEATH // Saturn 0x90E, dies
#define MOD_GARDENER_SFX_DASH_A SFX_WHIP_TWIRL_SWISH  // 0x6C7, dash starts
#define MOD_GARDENER_SFX_DASH_B SFX_SCRAPE_B          // 0x616, dash starts
#define MOD_GARDENER_SFX_THROW SFX_WEAPON_SWISH_C     // 0x60C, throw
#define MOD_GARDENER_SFX_SPIN SFX_WEAPON_SWISH_A      // 0x60A, shears fly

typedef enum {
    GARDENER_INIT = 0,
    GARDENER_FALL = 1,   // drops to the floor
    GARDENER_WALK = 2,   // walks toward the player, decides at each cycle end
    GARDENER_WINDUP = 3, // raises the shears, then DASH
    GARDENER_DASH = 4,   // lunges with the shears forward (hitbox 0x3F)
    GARDENER_THROW = 5,  // throws the shears (0x3E)
    GARDENER_DEATH = 6,
} GardenerSteps;

// Ext of the shears hitbox (0x3F), accessed through GARDENER_HITBOX_EXT().
typedef struct {
    /* Saturn 0x78 */ Entity* parent;
    /* Saturn 0x80 */ u16 savedHitboxState;
} ET_ModGardenerHitbox;
#define GARDENER_HITBOX_EXT(e) ((ET_ModGardenerHitbox*)&(e)->ext)

// Saturn 0x060F9640.. animations, frames 1:1 (none loops: all end with 0xFF)
static u8 gardener_anim_walk[] = { // 0x060F9640, WALK
    4,    0x01, 5,    0x02, 5,    0x03, 6,    0x04, 10,   0x05, 4,
    0x06, 5,    0x07, 5,    0x08, 6,    0x09, 10,   0x0A, 0xFF, 0x00};
static u8 gardener_anim_windup[] = { // 0x060F9658, WINDUP and DEATH
    5,    0x0C, 7,    0x0D, 7,    0x0E, 7,    0x0F, 7,    0x0E, 7,
    0x0D, 13,   0x0C, 3,    0x10, 2,    0x11, 1,    0x12, 0xFF, 0x00};
static u8 gardener_anim_dash[] = { // 0x060F9684, DASH
    2, 0x13, 2, 0x14, 2, 0x13, 2, 0x14, 2, 0x13, 24, 0x14, 3, 0x12, 0xFF, 0x00};
static u8 gardener_anim_throw[] = { // 0x060F9694, THROW
    6, 0x0B, 6,  0x27, 6,  0x28, 6,    0x29, 6,  0x2A, 6, 0x2B, 6,  0x2A,
    6, 0x2B, 6,  0x2A, 6,  0x2B, 6,    0x2C, 6,  0x15, 6, 0x16, 31, 0x17,
    4, 0x18, 11, 0x19, 3,  0x18, 3,    0x17, 3,  0x1A, 3, 0x1B, 3,  0x1C,
    3, 0x1D, 3,  0x1E, 5,  0x1F, 4,    0x20, 31, 0x21, 7, 0x22, 7,  0x23,
    7, 0x24, 7,  0x25, 10, 0x26, 0xFF, 0x00};

// Collision sensors (X scaled, Y as is)
// Saturn 0x060F9628 {{0,24},{0,4},{8,-4},{-16,0}}: UnkCollisionFunc3 (FALL)
static s16 gardener_sensors_fall[] = {0, 24, 0, 4, SAT_X(8), -4, SAT_X(-16), 0};
// Saturn 0x060F9638 {{0,24},{12,0}}: UnkCollisionFunc2 (WALK, DASH)
static s16 gardener_sensors_walk[] = {0, 24, SAT_X(12), 0};

// Saturn: (u16)(posX + 16) <= 0x164, i.e. on screen with a 16 px margin;
// the gardener's sounds only play then.
static s32 GardenerOnScreen(Entity* self) {
    return (u16)(self->posX.i.hi + SAT_X(16)) <= SAT_X(0x164);
}

static void GardenerFacePlayer(Entity* self) {
    self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
}

void EntityModGardener(Entity* self) {
    u16 init[5];
    Entity* ent;
    s16 absX;
    s32 x, y;

    if ((self->flags & FLAG_DEAD) && self->step != GARDENER_DEATH) {
        SetStep(GARDENER_DEATH);
    }

    switch (self->step) {
    case GARDENER_INIT:
        init[0] = MOD_GARDENER_ANIMSET;
        init[1] = 1; // Saturn animCurFrame = 1
        init[2] = MOD_GARDENER_UNK5A;
        init[3] = GARDENER_PAL;
        init[4] = 0x195;        // "Gardener"
        InitializeEntity(init); // step -> 1
        // TODO Z (Saturn: no change, the bank's default sprite z)
        // Saturn-space values written after TekiInit
        self->hitboxWidth = SAT_X(8);
        self->hitboxHeight = 16;
        self->posY.i.hi -= 22;
        self->animCurFrame = 1;
        GardenerFacePlayer(self);
        // the shears' dash hitbox (0x3F) in the next slot
        CreateEntityFromCurrentEntity(E_MOD_GARDENER_HITBOX, self + 1);
        GARDENER_HITBOX_EXT(self + 1)->parent = self;
        break;

    case GARDENER_FALL:
        if (UnkCollisionFunc3(gardener_sensors_fall) & 1) {
            SetStep(GARDENER_WALK);
        }
        break;

    case GARDENER_WALK:
        switch (self->step_s) {
        case 0:
            self->hitboxWidth = SAT_X(8);
            self->hitboxHeight = 16;
            self->hitboxOffX = 0;
            GardenerFacePlayer(self);
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(0.5)); // Saturn 0x8000
            } else {
                self->velocityX = SAT_X(-FIX(0.5));
            }
            self->step_s = 1;
            /* fallthrough */
        case 1:
            // shears click at the start of frames 2 and 7, half of the time
            if (GardenerOnScreen(self) && (Random() & 1)) {
                if (self->pose == 1 && self->poseTimer == 0) {
                    g_api.PlaySfx(MOD_GARDENER_SFX_SNIP);
                }
                if (self->pose == 6 && self->poseTimer == 0) {
                    g_api.PlaySfx(MOD_GARDENER_SFX_SNIP);
                }
            }
            if (!AnimateEntity(gardener_anim_walk, self)) {
                GardenerFacePlayer(self);
                if (!(Random() & 1)) {
                    SetStep(GARDENER_WALK); // another walk cycle
                } else if (GetDistanceToPlayerX() > SAT_X(112)) {
                    // Saturn: SetStep(Random() & 1 ? 3 : 5), then SetStep(3)
                    // (always WINDUP; the Random() call is kept)
                    Random();
                    SetStep(GARDENER_WINDUP);
                } else {
                    SetStep(GARDENER_THROW);
                }
            }
            UnkCollisionFunc2(gardener_sensors_walk);
            break;
        }
        break;

    case GARDENER_WINDUP:
        switch (self->step_s) {
        case 0:
            self->velocityX = 0;
            self->velocityY = 0;
            self->hitboxWidth = SAT_X(10);
            self->hitboxHeight = 16;
            self->hitboxOffX = SAT_X(11);
            self->step_s++;
            /* fallthrough */
        case 1:
            if (!AnimateEntity(gardener_anim_windup, self)) {
                SetStep(GARDENER_DASH);
            }
            // pose 3 with timer 0: the shears are up (frame 0x0F next)
            if (GardenerOnScreen(self) && self->pose == 3 &&
                self->poseTimer == 0) {
                g_api.PlaySfx(MOD_GARDENER_SFX_SNIP);
            }
            break;
        }
        break;

    case GARDENER_DASH:
        switch (self->step_s) {
        case 0:
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(6));
            } else {
                self->velocityX = SAT_X(-FIX(6));
            }
            self->hitboxWidth = SAT_X(18);
            self->hitboxHeight = 16;
            self->hitboxOffX = SAT_X(-27);
            self->step_s++;
            if (GardenerOnScreen(self)) {
                g_api.PlaySfx(MOD_GARDENER_SFX_DASH_A);
                g_api.PlaySfx(MOD_GARDENER_SFX_DASH_B);
            }
            break;
        case 1:
            if (!AnimateEntity(gardener_anim_dash, self)) {
                SetStep(GARDENER_WALK);
            }
            // slows down by 0x3000 (Saturn) per frame, down to 0
            if (self->facingLeft) {
                self->velocityX -= SAT_X(0x3000);
                if (self->velocityX < 0) {
                    self->velocityX = 0;
                }
            } else {
                self->velocityX += SAT_X(0x3000);
                if (self->velocityX > 0) {
                    self->velocityX = 0;
                }
            }
            // dust at the feet every 3 frames (Saturn SpawnDestructAnim =
            // EntityExplosionVariantsSpawner; X not mirrored by facing)
            if ((g_Timer % 3) == 0) {
                x = Random() & 15;
                y = (Random() & 3) + 20;
                EntityExplosionVariantsSpawner(
                    self, 1, 1, SAT_X(x - 6), y, (Random() & 3) + 1, 0);
                EntityExplosionVariantsSpawner(
                    self, 1, 1, SAT_X(x + 6), y, (Random() & 3) + 1, 0);
            }
            UnkCollisionFunc2(gardener_sensors_walk);
            break;
        }
        break;

    case GARDENER_THROW:
        switch (self->step_s) {
        case 0:
            self->velocityX = 0;
            self->velocityY = 0;
            self->hitboxWidth = SAT_X(10);
            self->hitboxHeight = 18;
            self->hitboxOffX = SAT_X(10);
            self->step_s++;
            /* fallthrough */
        case 1:
            if (!AnimateEntity(gardener_anim_throw, self)) {
                SetStep(GARDENER_WALK);
            }
            // pose 0x15 with timer 0: frame 0x1D next
            if (GardenerOnScreen(self) && self->pose == 0x15 &&
                self->poseTimer == 0) {
                g_api.PlaySfx(MOD_GARDENER_SFX_THROW);
            }
            // pose 0x17 with timer 0: frame 0x1F next, the shears leave
            if (self->pose == 0x17 && self->poseTimer == 0) {
                ent = AllocEntity(&g_Entities[160], &g_Entities[192]);
                if (ent != NULL) {
                    CreateEntityFromEntity(E_MOD_GARDENER_SHEARS, self, ent);
                    ent->facingLeft = self->facingLeft;
                    if (ent->facingLeft) {
                        ent->posX.i.hi += SAT_X(16);
                    } else {
                        ent->posX.i.hi -= SAT_X(16);
                    }
                    ent->posY.i.hi -= 4;
                }
            }
            break;
        }
        break;

    case GARDENER_DEATH:
        switch (self->step_s) {
        case 0:
            self->step_s++;
            // Saturn: sprite flags |= 0x80 (effect not identified; ignored)
            if (GardenerOnScreen(self)) {
                g_api.PlaySfx(MOD_GARDENER_SFX_DEATH);
            }
            /* fallthrough */
        case 1:
            // ends after the 5th pose (33 frames) or at the anim end
            if (!AnimateEntity(gardener_anim_windup, self) ||
                (self->pose == 5 && self->poseTimer == 0)) {
                DestroyEntity(self);
                return;
            }
            if ((g_Timer & 7) == 0) {
                ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
                if (ent != NULL) {
                    CreateEntityFromEntity(E_EXPLOSION, self, ent);
                    ent->params = 2;
                    if (!self->facingLeft) {
                        ent->posX.i.hi = self->posX.i.hi + SAT_X(6);
                    } else {
                        ent->posX.i.hi = self->posX.i.hi - SAT_X(6);
                    }
                    ent->posY.i.hi = self->posY.i.hi + 18;
                }
            }
            break;
        }
        break;
    }

    // Saturn epilogue: keeps the gardener in its patch of the room (absolute
    // X; params 0: 0x90..0x22E, else 0x2D4..0x46C, Saturn units). Left limit
    // only while moving left, right limit otherwise.
    absX = self->posX.i.hi + g_Tilemap.scrollX.i.hi;
    if (self->params == 0) {
        if (self->velocityX < 0) {
            if (absX < SAT_X(0x90)) {
                self->posX.i.hi = SAT_X(0x90) - g_Tilemap.scrollX.i.hi;
            }
        } else if (absX > SAT_X(0x22E)) {
            self->posX.i.hi = SAT_X(0x22E) - g_Tilemap.scrollX.i.hi;
        }
    } else {
        if (self->velocityX < 0) {
            if (absX < SAT_X(0x2D4)) {
                self->posX.i.hi = SAT_X(0x2D4) - g_Tilemap.scrollX.i.hi;
            }
        } else if (absX > SAT_X(0x46C)) {
            self->posX.i.hi = SAT_X(0x46C) - g_Tilemap.scrollX.i.hi;
        }
    }
}

// Saturn func_060EEEE4: the thrown shears, a spinning boomerang. Starts at
// 6 px/f, slows by 0x1800 per frame and comes back after 64 frames while
// drifting up; destroyed off camera (enemy def 0xC4's flags).
void EntityModGardenerShears(Entity* self) {
    u16 init[5];

    switch (self->step) {
    case 0:
        init[0] = MOD_GARDENER_ANIMSET;
        init[1] = 45;
        init[2] = MOD_GARDENER_UNK5A;
        init[3] = GARDENER_PAL;
        init[4] = 0xC4;         // 0 HP, ATK 65, 4x4 (same record on both)
        InitializeEntity(init); // step -> 1
        self->flags |= FLAG_DESTROY_IF_OUT_OF_CAMERA | FLAG_UNK_2000;
        self->attack = 30;
        self->rotate = 0;
        self->drawFlags |= ENTITY_ROTATE; // Saturn sprite flags |= 0x40
        self->zPriority++;                // TODO Z (Saturn: its sprite z + 1)
        self->hitboxWidth = SAT_X(10);
        self->hitboxHeight = 10;
        if (self->facingLeft) {
            self->velocityX = SAT_X(FIX(6));
        } else {
            self->velocityX = SAT_X(-FIX(6));
        }
        self->velocityY = FIX(0.75);
        self->animCurFrame = 45;
        /* fallthrough */
    case 1:
        if ((g_Timer & 7) == 0) {
            PlaySfxPositional(MOD_GARDENER_SFX_SPIN);
        }
        self->rotate -= 0x100; // Saturn sprite rotate (see notes)
        MoveEntity();
        if (self->facingLeft) {
            self->velocityX -= SAT_X(0x1800);
        } else {
            self->velocityX += SAT_X(0x1800);
        }
        self->velocityY -= 0x700;
        break;
    }
}

// Saturn func_060EEE20: the shears' hitbox, live only while the gardener
// (always self - 1, via ext.parent) dashes.
void EntityModGardenerHitbox(Entity* self) {
    u16 init[5];
    Entity* parent;

    switch (self->step) {
    case 0:
        init[0] = 0; // no sprite
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = 0x23;         // invulnerable sub-hitbox, ATK 24
        InitializeEntity(init); // step -> 1
        GARDENER_HITBOX_EXT(self)->savedHitboxState = self->hitboxState;
        /* fallthrough */
    case 1:
        parent = GARDENER_HITBOX_EXT(self)->parent;
        switch (parent->step) {
        case GARDENER_DASH:
            self->hitboxState = GARDENER_HITBOX_EXT(self)->savedHitboxState;
            self->posX.i.hi = parent->posX.i.hi;
            self->posY.i.hi = parent->posY.i.hi - 5;
            self->facingLeft = parent->facingLeft;
            if (self->facingLeft) {
                self->posX.i.hi += SAT_X(0x30);
            } else {
                self->posX.i.hi -= SAT_X(0x30);
            }
            self->hitboxWidth = SAT_X(0x18);
            self->hitboxHeight = 4;
            break;
        case GARDENER_DEATH:
            DestroyEntity(self);
            break;
        default:
            self->hitboxState = 0;
            break;
        }
        break;
    }
}
