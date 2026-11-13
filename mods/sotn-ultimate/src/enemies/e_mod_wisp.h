// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Will-o'-Wisp (Saturn STAGE_16, the Cursed Prison, entities 0x28-0x2C:
 * func_060E7ED0 / 060E7FC4 / 060E86BC / 060E8AC0 / 060E887C, helper
 * func_060E7EA8) ported to a PS1 stage overlay (Saturn port mod, translated
 * from the SH-2; see saturn_wisp.c / notes.md). Include after the stage
 * header and the st_common helpers.
 *
 * Required from the including stage:
 *   enum entries, in Saturn id order (any values, need not be consecutive):
 *     E_MOD_WISP_SPAWNER (Saturn 0x28, the layout entity), E_MOD_WISP
 *     (0x29), E_MOD_WISP_FLAME (0x2A), E_MOD_WISP_AFTERIMAGE (0x2B),
 *     E_MOD_WISP_SPARKS (0x2C)
 *   MOD_WISP_ANIMSET   animSet of the converted Saturn bank 26 (0x060EEC7C;
 *                      frame table 0x060EEFBC, 41 frames, Saturn frame n =
 *                      PSX frame n, frames carry the block's cluts from
 *                      MOD_WISP_PAL in the Saturn palette order)
 *   MOD_WISP_UNK5A     (optional, default 0) unk5A / tpage of the animset
 *   MOD_SFX_WISP       the Saturn's 0x903 (mod_prs_sfx.h)
 *   enemy defs 418 (0x1A2, "Will O' Wisp") and 44 (0x2C, the PS1's own FIRE
 *   hitbox row, identical on both consoles); ENEMY_INTERACTABLE (5)
 *   MOD_WISP_HAS_PAL5  (optional, default 0) 1 if the palette block has a
 *                      6th palette (MOD_WISP_PAL + 5): the Saturn draws the
 *                      last death frame (33, part palette 1) with clutBase
 *                      + 4 = palette 5, one past the bank's five. With 0 the
 *                      port uses + 3 (palette 4, the darkest) for it.
 * Nothing is drawn with primitives from the bank's images (the sparks are
 * plain coloured tiles), so no image-part table is needed.
 *
 * Slots: the spawner (layout) puts its wisp in self+1; the wisp puts its
 * flame in self+1 (= spawner+2). Afterimages: AllocEntity 160-191; death
 * sparks: AllocEntity 224-255 (64 PRIM_TILE).
 *
 * Entry points:
 *   EntityModWispSpawner(Entity*)    <- E_MOD_WISP_SPAWNER (layout)
 *   EntityModWisp(Entity*)           <- E_MOD_WISP            (spawned)
 *   EntityModWispFlame(Entity*)      <- E_MOD_WISP_FLAME      (spawned)
 *   EntityModWispAfterimage(Entity*) <- E_MOD_WISP_AFTERIMAGE (spawned)
 *   EntityModWispSparks(Entity*)     <- E_MOD_WISP_SPARKS     (spawned)
 *
 * Drawing: the Saturn draws the wisp's sprite at a bobbing y (12 px above
 * its position, +-4 px) and moves the hitbox with it; the flame is drawn at
 * that same y. The PS1 has no separate draw position, so between updates the
 * wisp's posY holds the drawn y (the logical y is restored at the start of
 * each update). The wisp, its flame and its afterimages are additive (the
 * Saturn sprite class 5).
 */

#ifndef MOD_WISP_ANIMSET
#error "define MOD_WISP_ANIMSET before including e_mod_wisp.h"
#endif
#ifndef MOD_WISP_UNK5A
#define MOD_WISP_UNK5A 0
#endif
#ifndef MOD_WISP_HAS_PAL5
#define MOD_WISP_HAS_PAL5 0
#endif
// The frames carry the block's cluts; like the Saturn's clutBase offset, the
// entity palette is only the offset: 0 = normal, 2/3/4 = fading (palettes 2-4
// are darker copies), see the death, flame and afterimage code.
#define WISP_PAL 0
#define MOD_WISP_ENEMY 418      // Saturn TekiInit(self, 0x1A2)
#define MOD_WISP_FLAME_ENEMY 44 // Saturn TekiInit(self, 44): FIRE, atk 67/4

// Saturn sprite class 5 = colour-code priority 5, which the game's VDP2 setup
// adds to the background (S5PRIN 6 = SPCCN, CCMD "add as is"; see
// inv/step14/notes.md): additive, as the PS1 NZ0 blue flame table's
// BLEND_TRANSP | BLEND_ADD. The bank's colours carry the STP bit for it
// (prs_assets.py; the PS1 blends only STP texels).
#ifndef MOD_WISP_BLEND
#define MOD_WISP_BLEND (BLEND_TRANSP | BLEND_ADD)
#endif
// the sparks (Saturn colour code priority 5, fading to black: additive)
#ifndef MOD_WISP_SPARK_DRAWMODE
#define MOD_WISP_SPARK_DRAWMODE                                                \
    (DRAW_TPAGE2 | DRAW_TPAGE | DRAW_UNK02 | DRAW_TRANSP)
#endif

// Sounds: 0x614 is the PS1's too; 0x903 is the Prison's own
#ifndef MOD_WISP_SFX_FLAME
#define MOD_WISP_SFX_FLAME SFX_ELECTRICITY // Saturn 0x614
#endif
#ifndef MOD_WISP_SFX_DEATH
#define MOD_WISP_SFX_DEATH MOD_SFX_WISP // Saturn 0x903
#endif

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif

typedef enum {
    WISP_INIT = 0,
    WISP_IDLE = 1,   // waits until the player is within 127 (Saturn) px
    WISP_APPEAR = 2, // fades in, harmless
    WISP_ATTACK = 3, // flies at the player, breathes a flame within 71 px
    WISP_HIT = 18,   // hit while idle or attacking: dies (no kill credit)
    WISP_DEAD = 20,  // killed: the same sequence
} WispSteps;

typedef struct {
    /* Saturn 0x84 */ s16 spawnCount;
    /* Saturn 0x9C */ s16 timer;
} ET_ModWispSpawner;

typedef struct {
    /* Saturn 0x98 */ s32 baseY;  // drawn (bobbing) y, 16.16
    /* PS1 only    */ s32 bobOff; // baseY - logical y at the last update
    /* Saturn 0x94 */ s16 phase;  // bob phase
    /* Saturn 0x9E */ s16 timer;
    /* Saturn 0xA4 */ s16 savedHitboxState;
    /* PS1 only    */ s16 drawn; // posY holds baseY (between updates)
    /* Saturn 0xA2 */            // clutBase -> WISP_PAL
} ET_ModWisp;

typedef struct {
    /* Saturn 0xA0 */ Entity* wisp; // read at init only (z)
    /* Saturn 0x9C */ s32 timer;
} ET_ModWispAfterimage;

typedef struct {
    /* Saturn 0x7C */ Primitive* prims;
    /* Saturn 0x90 */ s16 timer;
} ET_ModWispSparks;

#define WISP_SPAWNER_EXT(e) ((ET_ModWispSpawner*)&(e)->ext)
#define WISP_EXT(e) ((ET_ModWisp*)&(e)->ext)
#define WISP_AFTER_EXT(e) ((ET_ModWispAfterimage*)&(e)->ext)
#define WISP_SPARKS_EXT(e) ((ET_ModWispSparks*)&(e)->ext)

// Saturn 0x060EEBE2.. animations, frames 1:1
static u8 wisp_anim_idle[] = { // 0x060EEBE2
    5, 1, 5, 2, 5, 3, 5, 4, 5, 5, 5, 6, 0, 0};
static u8 wisp_anim_windup[] = { // 0x060EEBF0
    5, 1, 5, 7, 5, 8, 5, 9, 5, 10, 0xFF, 0};
static u8 wisp_anim_fly[] = { // 0x060EEBFC
    5, 11, 5, 12, 5, 13, 5, 14, 5, 15, 5, 16, 0, 0};
static u8 wisp_anim_flame[] = { // 0x060EEC0A
    3, 17, 3, 18, 2, 19, 3, 20, 3, 21, 3, 22, 2, 23, 0xFF, 0};
static u8 wisp_anim_die1[] = { // 0x060EEC1A
    2, 16, 2, 8, 2, 7, 2, 1, 2, 25, 0xFF, 0};
static u8 wisp_anim_die2[] = { // 0x060EEC26
    6, 25, 6, 26, 0, 0};
static u8 wisp_anim_die3[] = { // 0x060EEC2C
    2, 26, 3, 27, 4, 28, 4, 29, 4, 30, 5, 31, 5, 32, 5, 33, 0xFF, 0};
static u8 wisp_anim_appear[] = { // 0x060EEC3E
    5, 34, 4, 35, 4, 36, 4, 37, 4, 38, 0xFF, 0};

// the sparks' colours: the bank's palette 0, entries 0-15 (Saturn RGB555
// 0x98C8, 0xFFFF, 0xFBDA, ..., 0x8000), one step every 5 frames
#ifndef MOD_WISP_SPARK_COLORS
#define MOD_WISP_SPARK_COLORS                                                  \
    {0x40, 0x30, 0x30}, {0xF8, 0xF8, 0xF8}, {0xD0, 0xF0, 0xF0},                \
        {0xA8, 0xE0, 0xE8}, {0x80, 0xD8, 0xE8}, {0x50, 0xD0, 0xE0},            \
        {0x28, 0xC0, 0xD8}, {0x00, 0xB8, 0xD0}, {0x00, 0x98, 0xD8},            \
        {0x08, 0x78, 0xD0}, {0x08, 0x58, 0xD0}, {0x10, 0x38, 0xC8},            \
        {0x18, 0x18, 0xC8}, {0x10, 0x10, 0x80}, {0x08, 0x08, 0x40}, {          \
        0x00, 0x00, 0x00                                                       \
    }
#endif
static u8 wisp_spark_colors[16][3] = {MOD_WISP_SPARK_COLORS};

// Saturn func_060E7EA8 (inlined in func_060E7FC4): SetStep + stop
static void WispSetStep(Entity* self, u16 step) {
    self->velocityX = 0;
    self->velocityY = 0;
    self->pose = 0;
    self->poseTimer = 0;
    self->step = step;
    self->step_s = 0;
}

// Saturn MTH_GetRand() (32 bits) is only used modulo 0x6000 / 4095
static s32 WispRand16(void) {
    s32 hi;

    hi = Random() << 8;
    return hi | Random();
}

// Saturn func_060E7ED0: spawns a wisp in self+1 whenever that slot has none,
// every 300 frames (the first at once), 16 in all (the s16 counter keeps
// counting and wraps after ~9 minutes, as on the Saturn)
void EntityModWispSpawner(Entity* self) {
    u16 init[5];
    Entity* child;

    child = self + 1;
    if (self->step == 0) {
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = ENEMY_INTERACTABLE; // Saturn TekiInit(self, 5)
        InitializeEntity(init);       // step -> 1
        WISP_SPAWNER_EXT(self)->timer = 1000;
        WISP_SPAWNER_EXT(self)->spawnCount = 0;
    }
    if (child->entityId != E_MOD_WISP) {
        if (++WISP_SPAWNER_EXT(self)->timer > 300) {
            if (++WISP_SPAWNER_EXT(self)->spawnCount <= 16) {
                CreateEntityFromEntity(E_MOD_WISP, self, child);
                child->facingLeft = (GetSideToPlayer() & 1) ^ 1;
                WISP_SPAWNER_EXT(self)->timer = 0;
                child->posX.i.hi += SAT_X((s32)(Random() & 15) - 8);
                child->posY.i.hi += (s32)(Random() & 15) - 8;
                child->params = WISP_SPAWNER_EXT(self)->spawnCount;
            }
        }
    }
}

// Saturn func_060E7FC4: the wisp (enemy 418)
void EntityModWisp(Entity* self) {
    u16 init[5];
    Entity* ent;
    ET_ModWisp* ext;

    ext = WISP_EXT(self);
    // PS1: back from the drawn y to the logical y (relative, so the camera
    // scroll applied to posY meanwhile is kept); baseY = the current drawn y
    if (ext->drawn) {
        ext->baseY = self->posY.val;
        self->posY.val -= ext->bobOff;
    }

    // Saturn 0x060E7FD0: hit while idle or attacking
    if (self->hitParams && (self->step & 1)) {
        WispSetStep(self, WISP_HIT);
        self->hitboxState = 0;
    }
    // Saturn 0x060E800E: killed
    if ((self->flags & FLAG_DEAD) && self->step != WISP_DEAD) {
        WispSetStep(self, WISP_DEAD);
        self->hitboxState = 0;
    }

    switch (self->step) {
    case WISP_INIT:
        init[0] = MOD_WISP_ANIMSET;
        init[1] = 0;
        init[2] = MOD_WISP_UNK5A;
        init[3] = WISP_PAL;
        init[4] = MOD_WISP_ENEMY;
        InitializeEntity(init); // TODO Z (Saturn: default enemy z)
        self->blendMode = MOD_WISP_BLEND;
        self->hitboxWidth = SAT_X(4);
        self->hitboxHeight = 4;
        self->hitboxOffX = 0;
        self->hitboxOffY = 0;
        self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
        WispSetStep(self, WISP_APPEAR);
        ext->savedHitboxState = self->hitboxState;
        self->hitboxState = 0;
        self->animCurFrame = 0;
        break;

    case WISP_APPEAR:
        if (!AnimateEntity(wisp_anim_appear, self)) {
            WispSetStep(self, WISP_IDLE);
            self->hitboxState = ext->savedHitboxState;
        }
        break;

    case WISP_IDLE:
        if (GetDistanceToPlayerX() <= SAT_X(127)) {
            WispSetStep(self, WISP_ATTACK);
            self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
            ext->timer = 0;
        }
        AnimateEntity(wisp_anim_idle, self);
        break;

    case WISP_ATTACK:
        switch (self->step_s) {
        case 0:
            ext->timer = 0;
            self->step_s++;
            // fallthrough
        case 1:
            if (!AnimateEntity(wisp_anim_windup, self)) {
                self->step_s++;
                self->pose = 0;
                self->poseTimer = 0;
            }
            break;
        case 2:
            if (GetDistanceToPlayerX() <= SAT_X(71)) {
                self->step_s++;
                ext->timer = 0;
            }
            break;
        case 3: // the flame, in self+1
            CreateEntityFromEntity(E_MOD_WISP_FLAME, self, self + 1);
            (self + 1)->facingLeft = self->facingLeft;
            ext->timer = 0;
            self->step_s++;
            g_api.PlaySfx(MOD_WISP_SFX_FLAME);
            break;
        case 4:
            if (++ext->timer > 32) {
                self->step_s++;
            }
            break;
        case 5:
            break;
        }
        // Saturn 0x060E8346: accelerate towards the side it faces (to 2 px
        // per frame on the Saturn), sink 1/4 px per frame
        MoveEntity();
        if (self->facingLeft) {
            if (self->velocityX <= SAT_X(0x1FFFF)) {
                self->velocityX += SAT_X(0x2222);
            }
        } else {
            if (self->velocityX > SAT_X(-0x20000)) {
                self->velocityX -= SAT_X(0x2222);
            }
        }
        self->posY.val += FIX(0.25);
        if (self->step_s > 1) {
            AnimateEntity(wisp_anim_fly, self);
        }
        // Saturn: x < -32 or x > 336 on a 320 px screen
        if ((u16)(self->posX.i.hi + SAT_X(32)) > SAT_X(368)) {
            DestroyEntity(self);
            return;
        }
        break;

    case WISP_HIT:
    case WISP_DEAD:
        switch (self->step_s) {
        case 0: // pushed back
            if (self->facingLeft) {
                self->velocityX = SAT_X(FIX(-3));
            } else {
                self->velocityX = SAT_X(FIX(3));
            }
            self->velocityY = -self->velocityY;
            self->step_s++;
            // fallthrough
        case 1:
            if (!AnimateEntity(wisp_anim_die1, self)) {
                self->step_s++;
                ext->timer = 0;
                self->pose = 0;
                self->poseTimer = 0;
            }
            break;
        case 2:
            AnimateEntity(wisp_anim_die2, self);
            if (++ext->timer > 18) {
                self->pose = 0;
                self->poseTimer = 0;
                ext->timer = 0;
                self->step_s++;
                g_api.PlaySfx(MOD_WISP_SFX_DEATH);
            }
            break;
        case 3: // bursts and fades out
            if (!AnimateEntity(wisp_anim_die3, self)) {
                DestroyEntity(self);
                return;
            }
            switch (self->animCurFrame) {
            case 30:
            case 31:
                self->palette = WISP_PAL + 2;
                break;
            case 32:
                self->palette = WISP_PAL + 3;
                break;
            case 33:
#if MOD_WISP_HAS_PAL5
                self->palette = WISP_PAL + 4; // part palette 1 -> 5
#else
                self->palette = WISP_PAL + 3; // (Saturn + 4, palette 5)
#endif
                break;
            default:
                self->palette = WISP_PAL;
                break;
            }
            if (++ext->timer == 0x11) {
                ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
                if (ent != NULL) {
                    CreateEntityFromEntity(E_MOD_WISP_SPARKS, self, ent);
                    ent->posY.val = ext->baseY;
                }
            }
            break;
        }
        // Saturn 0x060E8538: the push slows down by 0.1 px per frame
        MoveEntity();
        if (self->facingLeft) {
            if (self->velocityX < 0) {
                self->velocityX += SAT_X(0x1999);
            }
        } else {
            if (self->velocityX > 0) {
                self->velocityX -= SAT_X(0x1999);
            }
        }
        break;
    }

    // Saturn 0x060E859E: the sprite bobs +-4 px, 12 px above posY (once dead
    // the phase freezes and is read at half rate, a small jump as on the
    // Saturn); the hitbox follows the drawn sprite
    if (self->step == WISP_DEAD) {
        ext->baseY = self->posY.val + (rsin(ext->phase * 32) << 6) - FIX(12);
    } else {
        ext->baseY = self->posY.val + (rsin(ext->phase * 64) << 6) - FIX(12);
        ext->phase++;
    }
    // afterimages while attacking, every 6 frames
    if (self->step == WISP_ATTACK && !(g_Timer % 6)) {
        ent = AllocEntity(&g_Entities[160], &g_Entities[192]);
        if (ent != NULL) {
            CreateEntityFromEntity(E_MOD_WISP_AFTERIMAGE, self, ent);
            ent->facingLeft = self->facingLeft;
            ent->animCurFrame = self->animCurFrame;
            WISP_AFTER_EXT(ent)->wisp = self;
            ent->posY.val = ext->baseY;
        }
    }
    // PS1: draw at baseY (Saturn: sprite y = baseY, hitboxOffY = baseY - y)
    ext->bobOff = ext->baseY - self->posY.val;
    self->posY.val = ext->baseY;
    self->hitboxOffY = 0;
    ext->drawn = 1;
}

// Saturn func_060E86BC: the flame breath (enemy 44, attack / 4), self - 1 is
// the wisp. The Saturn keeps its y at the wisp's and draws it at the wisp's
// bob; the PS1 puts it at the bob and moves the hitbox back down.
void EntityModWispFlame(Entity* self) {
    u16 init[5];
    Entity* wisp;
    s16 y; // Saturn posY (the hitbox's reference)

    wisp = self - 1;
    if (wisp->step == WISP_DEAD) {
        DestroyEntity(self);
        return;
    }
    y = self->posY.i.hi;
    switch (self->step) {
    case 0:
        init[0] = MOD_WISP_ANIMSET;
        init[1] = 0;
        init[2] = MOD_WISP_UNK5A;
        init[3] = WISP_PAL;
        init[4] = MOD_WISP_FLAME_ENEMY;
        InitializeEntity(init); // step -> 1
        self->attack /= 4;
        self->blendMode = MOD_WISP_BLEND;
        self->zPriority = wisp->zPriority - 2; // TODO Z (Saturn: wisp z - 2)
        self->hitboxWidth = SAT_X(16);
        self->hitboxHeight = 16;
        self->hitboxOffX = SAT_X(-12);
        break;
    case 1:
        if (!AnimateEntity(wisp_anim_flame, self)) {
            DestroyEntity(self);
            return;
        }
        switch (self->animCurFrame) {
        case 17:
        case 23:
            self->palette = WISP_PAL + 3;
            break;
        case 18:
        case 22:
            self->palette = WISP_PAL + 2;
            break;
        default:
            self->palette = WISP_PAL;
            break;
        }
        // follows the wisp (its logical y: posY - bobOff while it is drawn)
        self->posX.i.hi = wisp->posX.i.hi;
        y = (wisp->posY.val - WISP_EXT(wisp)->bobOff) >> 16;
        if (!WISP_EXT(wisp)->drawn) {
            y = wisp->posY.i.hi;
        }
        break;
    }
    self->posY.val = WISP_EXT(wisp)->baseY;
    self->hitboxOffY = 8 + y - self->posY.i.hi;
}

// Saturn func_060E8AC0: an afterimage of the attacking wisp, fades through
// palettes 2-4 and recedes for 30 frames
void EntityModWispAfterimage(Entity* self) {
    u16 init[5];

    switch (self->step) {
    case 0:
        init[0] = MOD_WISP_ANIMSET;
        init[1] = self->animCurFrame; // the wisp's, set at creation
        init[2] = MOD_WISP_UNK5A;
        init[3] = WISP_PAL;
        init[4] = ENEMY_INTERACTABLE; // Saturn TekiInit(self, 5)
        InitializeEntity(init);       // step -> 1
        self->blendMode = MOD_WISP_BLEND;
        WISP_AFTER_EXT(self)->timer = 0;
        // TODO Z (Saturn: wisp z - 2, then -1 every 4 frames)
        self->zPriority = WISP_AFTER_EXT(self)->wisp->zPriority - 2;
        break;
    case 1:
        WISP_AFTER_EXT(self)->timer++;
        if (!(WISP_AFTER_EXT(self)->timer & 3)) {
            self->zPriority--;
        }
        if (WISP_AFTER_EXT(self)->timer <= 9) {
            self->palette = WISP_PAL + 2;
        } else if (WISP_AFTER_EXT(self)->timer <= 19) {
            self->palette = WISP_PAL + 3;
        } else if (WISP_AFTER_EXT(self)->timer > 29) {
            DestroyEntity(self);
            return;
        } else {
            self->palette = WISP_PAL + 4;
        }
        break;
    }
}

// Saturn func_060E887C: 64 sparks bursting out of the dying wisp at 2-2.375
// (Saturn) px per frame for 78 frames, 1x2 px, fading white - cyan - blue -
// black. Per prim: x2/y2 = 8.8 speed (Saturn 16.16 in x2:y2 / x3:y3).
void EntityModWispSparks(Entity* self) {
    u16 init[5];
    Primitive* prim;
    s32 primIndex;
    s32 speed;
    s32 angle;
    s32 vx;
    s16 t;
    u8* color;

    switch (self->step) {
    case 0:
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = ENEMY_INTERACTABLE; // Saturn TekiInit(self, 5)
        InitializeEntity(init);       // step -> 1
        primIndex = g_api.AllocPrimitives(PRIM_TILE, 64);
        if (primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->flags |= FLAG_HAS_PRIMS;
        self->primIndex = primIndex;
        prim = &g_PrimBuf[primIndex];
        WISP_SPARKS_EXT(self)->prims = prim;
        while (prim != NULL) {
            speed = WispRand16() % 0x6000 + FIX(2);
            angle = WispRand16() % 4095;
            vx = ((rcos(angle) * speed) >> 12) * 6 / 5;
            prim->x2 = SAT_X(vx) >> 8;
            prim->y2 = ((rsin(angle) * speed) >> 12) >> 8;
            prim->type = PRIM_TILE;
            prim->u0 = 1;
            prim->v0 = 2;
            prim->x0 = self->posX.i.hi;
            prim->y0 = self->posY.i.hi;
            prim->priority = self->zPriority; // TODO Z (Saturn: unset)
            prim->drawMode = MOD_WISP_SPARK_DRAWMODE;
            prim = prim->next;
        }
        WISP_SPARKS_EXT(self)->timer = 0;
        // fallthrough (as the Saturn)
    case 1:
        t = ++WISP_SPARKS_EXT(self)->timer;
        color = wisp_spark_colors[(t / 5) & 15];
        prim = WISP_SPARKS_EXT(self)->prims;
        while (prim != NULL) {
            prim->x0 = self->posX.i.hi + ((prim->x2 * t) >> 8);
            prim->y0 = self->posY.i.hi + ((prim->y2 * t) >> 8);
            prim->r0 = color[0];
            prim->g0 = color[1];
            prim->b0 = color[2];
            prim = prim->next;
        }
        if (t > 78) {
            DestroyEntity(self);
        }
        break;
    }
}
