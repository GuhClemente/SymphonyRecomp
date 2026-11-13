// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Human Face Tree ("Jinmenju", Saturn STAGE_15 entities 0x2E-0x33:
 * func_060E9E44 / 060EAAA8 / 060EACDC / 060EB2B8 / 060EB824 / 060EBA00)
 * ported to a PS1 stage overlay (Saturn port mod, translated from the SH-2;
 * see saturn_tree.c / notes.md). Include after the stage header and the
 * st_common helpers.
 *
 * Required from the including stage:
 *   MOD_TREE_ANIMSET   animSet of the converted Saturn bank 26 (0x060F6DC4;
 *                      frame table 0x060F7EFC, 89 frames, Saturn frame n =
 *                      PSX frame n, 6 palettes, frames carry their cluts)
 *   MOD_TREE_UNK5A     (optional, default 0) unk5A / tpage of the animset
 *   enum entries E_MOD_TREE (Saturn 0x2E, layout), E_MOD_TREE_FACE (0x2F),
 *   E_MOD_TREE_DROP (0x30), E_MOD_TREE_BLOOD (0x31), E_MOD_TREE_LEAF (0x32),
 *   E_MOD_TREE_BASE (0x33)
 *   enemy defs 406 (0x196, the tree) and 407 (0x197, a falling face)
 *
 * The tree occupies self .. self+9: self+1 = the trunk base (E_MOD_TREE_BASE),
 * self+2 .. self+9 = the 8 hanging faces (E_MOD_TREE_FACE). Falling faces use
 * slots 160-191, blood and leaves 224-255.
 *
 * Entry points:
 *   EntityModTree(Entity*)       <- E_MOD_TREE
 *   EntityModTreeFace(Entity*)   <- E_MOD_TREE_FACE   (spawned only)
 *   EntityModTreeDrop(Entity*)   <- E_MOD_TREE_DROP   (spawned only)
 *   EntityModTreeBlood(Entity*)  <- E_MOD_TREE_BLOOD  (spawned only)
 *   EntityModTreeLeaf(Entity*)   <- E_MOD_TREE_LEAF   (spawned only)
 *   EntityModTreeBase(Entity*)   <- E_MOD_TREE_BASE   (spawned only)
 *
 * Drawing: no draw offset (the Saturn syncs the sprites to the entity with
 * no offset every frame; the base frame ends at y = 0 on the floor, 30 px
 * below the tree's origin). The tree, the hanging faces, the falling faces
 * and the leaves rotate (ENTITY_ROTATE): the bank needs seam bleeding.
 */

#ifndef MOD_TREE_ANIMSET
#error "define MOD_TREE_ANIMSET before including e_mod_tree.h"
#endif
#ifndef MOD_TREE_UNK5A
#define MOD_TREE_UNK5A 0
#endif
// Saturn clutBase offsets (entity->palette = offset only): 0 = the tree,
// 1 = red (eyes; the blood), 2 = withering, 3 = dead (also the death leaves),
// 4 = unused, 5 = grey (stones of the trunk base, set by the frames)
#define TREE_PAL 0
#define TREE_PAL_WITHER 2
#define TREE_PAL_DEAD 3
#define MOD_TREE_ENEMY 406      // Saturn TekiInit(self, 0x196)
#define MOD_TREE_DROP_ENEMY 407 // Saturn TekiInit(self, 0x197)

// Leaves (frames 54-66): their parts use palette 3 (attr 0x0300). The Saturn
// draws the hit leaves with clutBase - 3 (-> palette 0, green) and the death
// leaves with the base (-> palette 3, withered). The PS1 cannot use a
// negative palette (bit 15 = PAL_UNK_FLAG, absolute clut), so:
// 1 (default): the converter writes frames 54-66 with part palette 3 - 3 = 0;
//    hit leaves use palette 0, death leaves palette 3.
// 0: frames converted as they are (part palette 3); hit leaves then need
//    MOD_TREE_PAL_ABS = the g_ClutIds index of the bank's palette 0.
#ifndef MOD_TREE_LEAF_FRAMES_PAL_SHIFTED
#define MOD_TREE_LEAF_FRAMES_PAL_SHIFTED 1
#endif
#if MOD_TREE_LEAF_FRAMES_PAL_SHIFTED
#define TREE_LEAF_PAL_HIT 0
#define TREE_LEAF_PAL_DEATH 3
#else
#ifndef MOD_TREE_PAL_ABS
#error "MOD_TREE_LEAF_FRAMES_PAL_SHIFTED 0 needs MOD_TREE_PAL_ABS"
#endif
#define TREE_LEAF_PAL_HIT (PAL_UNK_FLAG | MOD_TREE_PAL_ABS)
#define TREE_LEAF_PAL_DEATH 0
#endif

// z: the tree itself, its hanging faces and its trunk base stand behind
// Alucard (0x94) as on the Saturn, and behind dropped items (center - 0x14,
// 0x80; e_collect.h), which land in front of it: faces 0x7F, trunk 0x7D,
// base 0x78. What it throws and sheds, the falling faces and the leaves, flies
// in front, above room 0's flowers (fg 0xA0 on the PS1, see
// MOD_GDN_ENEMY_FRONT_Z in gdn.h)
#ifndef MOD_TREE_BODY_Z
#define MOD_TREE_BODY_Z 0x7F
#endif
#ifndef MOD_TREE_Z
#ifdef MOD_GDN_ENEMY_FRONT_Z
#define MOD_TREE_Z MOD_GDN_ENEMY_FRONT_Z
#else
#define MOD_TREE_Z 0xA8
#endif
#endif

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif
#ifndef SAT_X_INV
#define SAT_X_INV(v) ((v) * 5 / 4)
#endif
// Saturn ratan2 returns 0..0x1000, the PS1's -0x800..0x800
#define TREE_RATAN2(y, x) (ratan2(y, x) & 0xFFF)

// Sounds: all four are Saturn-only (> 0x8F5): PS1 stand-ins
#ifndef MOD_TREE_SFX_FACE_FALLS
#define MOD_TREE_SFX_FACE_FALLS MOD_SFX_TREE_FACE_FALL // Saturn 0x8FA
#endif
#ifndef MOD_TREE_SFX_SPLAT
#define MOD_TREE_SFX_SPLAT MOD_SFX_TREE_FACE_BURST // Saturn 0x8FB
#endif
#ifndef MOD_TREE_SFX_HURT
#define MOD_TREE_SFX_HURT MOD_SFX_TREE_HURT // Saturn 0x8FC
#endif
#ifndef MOD_TREE_SFX_DEATH
#define MOD_TREE_SFX_DEATH MOD_SFX_TREE_DEATH // Saturn 0x8FD
#endif

typedef enum {
    TREE_INIT = 0,
    TREE_IDLE = 1,  // watch the player, sway 1/256 of the frames
    TREE_ALERT = 2, // player within 191: shake 1/64, close (57) -> 4
    TREE_SHAKE = 3, // swing; at the 2nd reversal one face is thrown
    TREE_DROP = 4,  // drop every hanging face, regrow them all
    TREE_HIT = 5,
    TREE_DEATH = 16,
} TreeSteps;

typedef enum {
    TREE_DROP_INIT = 0,
    TREE_DROP_FALL = 1,
    TREE_DROP_HIT = 0x10,    // hit by the player
    TREE_DROP_LANDED = 0x20, // dropped straight and reached the floor
    TREE_DROP_TOUCH = 0x30,  // touched the player
} TreeDropSteps;

typedef struct {
    /* Saturn 0x78 */ Entity* base;   // self+1
    /* Saturn 0xB0 */ u8* anim;       // hit / death animation
    /* Saturn 0x7C */ s16 timer;      // death countdown
    /* Saturn 0x7E */ s16 angVel;     // rotate speed, 4.12
    /* Saturn 0x82 */ s16 lastAngVel; // last non-zero angVel
    /* Saturn 0x84 */ s16 faceCount;  // faces left to throw / regrowth slot
    /* Saturn 0x86 */ s16 regrowing;
    /* Saturn 0x88 */ s16 regrowDelay;
    /* Saturn 0xB6 */ s16 swaying;
} ET_ModTree;

typedef struct {
    /* Saturn 0x78 */ Entity* tree;
    /* Saturn 0x80 */ s16 lastTreeAngVel;
    /* Saturn 0xA2 */ s16 frame;
} ET_ModTreeFace;

typedef struct {
    /* Saturn 0xB0 */ u8* anim;
    /* Saturn 0x7C */ s16 timer;
} ET_ModTreeDrop;

typedef struct {
    /* Saturn 0x7C */ s16 phase;
    /* Saturn 0x8C */ s16 offX; // written, never read
} ET_ModTreeLeaf;

typedef struct {
    /* Saturn 0x9C */ Primitive* lines; // prim 10 (first line)
} ET_ModTreeBlood;

#define TREE_EXT(e) ((ET_ModTree*)&(e)->ext)
#define TREE_FACE_EXT(e) ((ET_ModTreeFace*)&(e)->ext)
#define TREE_DROP_EXT(e) ((ET_ModTreeDrop*)&(e)->ext)
#define TREE_LEAF_EXT(e) ((ET_ModTreeLeaf*)&(e)->ext)
#define TREE_BLOOD_EXT(e) ((ET_ModTreeBlood*)&(e)->ext)

// Saturn 0x060F6CD4.. animations, frames 1:1
static u8 tree_anim_hit_l[] = { // 0x060F6CD4, player on the left
    14, 12, 14, 13, 14, 14, 14, 15, 14, 14, 14, 13, 14, 12, 0xFF, 0};
static u8 tree_anim_hit_r[] = { // 0x060F6CE4, player on the right
    14, 16, 14, 17, 14, 18, 14, 19, 14, 18, 14, 17, 14, 16, 0xFF, 0};
static u8 tree_anim_death_l[] = { // 0x060F6CFC
    18, 12, 18, 13, 18, 14, 98, 15, 18, 20, 0xFF, 0};
static u8 tree_anim_death_r[] = { // 0x060F6D08
    18, 16, 18, 17, 18, 18, 98, 19, 18, 23, 0xFF, 0};
static u8 tree_anim_drop_a[] = { // 0x060F6D1C, falling face (frame 35)
    4, 69, 4, 70, 4, 71, 0xFF, 0};
static u8 tree_anim_drop_b[] = { // 0x060F6D24, falling face (frame 47)
    4, 72, 4, 73, 0xFF, 0};

// Saturn 0x060F6D4C: {max player dx, frame} (the face looks at the player)
static s16 tree_look[11][2] = {
    {SAT_X(-140), 1}, {SAT_X(-100), 2}, {SAT_X(-70), 3}, {SAT_X(-28), 4},
    {SAT_X(-10), 5},  {SAT_X(10), 6},   {SAT_X(28), 7},  {SAT_X(70), 8},
    {SAT_X(100), 9},  {SAT_X(140), 10}, {0x7FFF, 11},
};
// Saturn 0x060F6D98: hitbox {offX, offY, w, h} by the same index (Saturn
// space, SAT_X at runtime for offX and w)
static s8 tree_hitbox[11][4] = {
    {-22, -30, 14, 40}, {-22, -30, 14, 40}, {-14, -30, 10, 40},
    {-14, -30, 10, 40}, {-2, -30, 4, 40},   {0, -30, 4, 40},
    {2, -30, 4, 40},    {14, -30, 10, 40},  {14, -30, 10, 40},
    {22, -30, 14, 40},  {22, -30, 14, 40},
};
// Saturn 0x060F6D78: branch anchors {distance, angle from straight up}
static s16 tree_branches[8][2] = {
    {83, -368}, {82, -256}, {82, -160}, {81, -80},
    {80, 80},   {77, 176},  {80, 320},  {88, 384},
};
// Saturn 0x060F6DF0: {falling face frame, floor sensor y} per branch
static s16 tree_drop_frames[8][2] = {
    {47, 4}, {47, 4}, {47, 4}, {47, 4}, {47, 4}, {35, 7}, {35, 7}, {35, 7},
};
// Saturn 0x060F6E18: leaf {x, y} by frame - 0x36 (x only stored, y 0)
static s16 tree_leaf_offs[13] = {8, 14, 6, 9, 12, 15, 5, 9, 8, 9, 10, 11, 12};
// Saturn 0x060FB0EC (bss): the faces' random order, shared by all trees
static u8 tree_face_order[8];

static void TreeShuffleFaces(void) {
    u8 values[9]; // the shift below reads one past the end (as the Saturn)
    s32 i, idx;

    for (i = 0; i < 8; i++) {
        values[i] = i;
    }
    values[8] = 0;
    for (i = 0; i < 8; i++) {
        idx = (u8)((u8)Random() % (8 - i));
        tree_face_order[i] = values[idx];
        for (; idx <= 7; idx++) {
            values[idx] = values[idx + 1];
        }
    }
}

// Saturn func_060E99A4 (inlined in steps 1-4): returns the player's dx
static s16 TreeLookAtPlayer(Entity* self) {
    s16 dx;
    s32 i;

    dx = PLAYER.posX.i.hi - self->posX.i.hi;
    for (i = 0; dx > tree_look[i][0]; i++) {
    }
    self->animCurFrame = tree_look[i][1];
    self->hitboxOffX = SAT_X(tree_hitbox[i][0]);
    self->hitboxOffY = tree_hitbox[i][1];
    self->hitboxWidth = SAT_X(tree_hitbox[i][2]);
    self->hitboxHeight = tree_hitbox[i][3];
    return dx;
}

// Saturn func_060E9A14 (inlined): the faces hang from rotating anchors
static void TreePlaceBranches(Entity* self) {
    Entity* face;
    s32 i;

    face = self + 2;
    for (i = 0; i < 8; i++, face++) {
        face->posX.val = self->posX.val +
                         SAT_X(tree_branches[i][0] *
                               rsin(self->rotate + tree_branches[i][1]) * 16);
        face->posY.val =
            self->posY.val -
            tree_branches[i][0] * rcos(tree_branches[i][1] + self->rotate) * 16;
    }
}

// Saturn 0x060EA8BE / 0x060EA8D2 (func_060E9AAC's body): back to upright
static void TreeEaseRotate(Entity* self) {
    if (self->rotate > 0) {
        self->rotate = self->rotate - 1 - (self->rotate >> 3);
    } else {
        self->rotate = self->rotate + 1 + ((-self->rotate) >> 3);
    }
}

// Saturn 0x060EA90C..0x060EA962: one pendulum step (constant pull towards
// upright); true when the swing reversed
static bool TreeSwing(Entity* self, s16 accel) {
    bool flipped;

    if (self->rotate > 0) {
        TREE_EXT(self)->angVel -= accel;
    } else {
        TREE_EXT(self)->angVel += accel;
    }
    self->rotate += TREE_EXT(self)->angVel >> 12;
    flipped = (TREE_EXT(self)->lastAngVel * TREE_EXT(self)->angVel) < 0;
    if (TREE_EXT(self)->angVel) {
        TREE_EXT(self)->lastAngVel = TREE_EXT(self)->angVel;
    }
    return flipped;
}

// Saturn: 5 leaves (params 0) when hit, 32 (params 1) at death
static void TreeSpawnLeaves(Entity* self, s32 count, u16 params) {
    Entity* ent;
    s32 i;

    for (i = 0; i < count; i++) {
        ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
        if (ent == NULL) {
            continue;
        }
        CreateEntityFromEntity(E_MOD_TREE_LEAF, self, ent);
        ent->params = params;
        ent->posX.i.hi = self->posX.i.hi + SAT_X(((s32)(u8)Random() - 127) / 3);
        if (params) {
            ent->posY.i.hi = self->posY.i.hi - ((Random() & 15) | 0x50);
        } else {
            ent->posY.i.hi = self->posY.i.hi - 80;
        }
        if (Random() & 1) {
            ent->velocityX = SAT_X((Random() & 7) * 0x2000);
        } else {
            ent->velocityX = -SAT_X((Random() & 7) * 0x2000);
        }
        ent->velocityY = -(Random() & 63) * 0xC00;
        TREE_LEAF_EXT(ent)->phase = (u8)Random() << 4;
    }
}

// Saturn func_060E9D38: once faceCount is 0, regrow the 8 faces in a random
// order, one every 32 frames (blocks shaking and dropping meanwhile)
static void TreeRegrowFaces(Entity* self) {
    if (!TREE_EXT(self)->faceCount && !TREE_EXT(self)->regrowing) {
        TreeShuffleFaces();
        TREE_EXT(self)->regrowing = 1;
        TREE_EXT(self)->regrowDelay = 0;
    }
    if (TREE_EXT(self)->regrowing) {
        TREE_EXT(self)->regrowDelay++;
        if (TREE_EXT(self)->regrowDelay == 0x20) {
            self[2 + tree_face_order[TREE_EXT(self)->faceCount]].step = 3;
            TREE_EXT(self)->regrowDelay = 0;
            TREE_EXT(self)->faceCount++;
            if (TREE_EXT(self)->faceCount == 8) {
                TREE_EXT(self)->regrowing = 0;
            }
        }
    }
}

static void TreeDropAllFaces(Entity* self) {
    s32 i;

    for (i = 0; i < 8; i++) {
        if (self[2 + i].step == 1) {
            self[2 + i].step = 4;
        }
    }
}

// Saturn func_060E9AEC: straighten up, wither, drop the faces and the leaves
static void TreeDeathSequence(Entity* self) {
    switch (self->step_s) {
    case 2:
        if (self->rotate == 0) {
            self->step_s = 0;
        } else {
            TreeEaseRotate(self);
        }
        TREE_EXT(self)->timer = 0x80;
        break;
    case 0:
        TREE_EXT(self)->timer--;
        if (TREE_EXT(self)->timer == 0x3F) {
            self->palette = TREE_PAL_WITHER;
            TREE_EXT(self)->base->animCurFrame = 87;
        }
        if (TREE_EXT(self)->timer == 0x20) {
            self->palette = TREE_PAL_DEAD;
            TREE_EXT(self)->base->animCurFrame = 88;
        }
        if (TREE_EXT(self)->timer == 0) {
            TreeDropAllFaces(self);
            TreeSpawnLeaves(self, 32, 1);
            self->step_s = 16; // done: the dead tree stays
        }
        break;
    }
}

void EntityModTree(Entity* self) {
    u16 init[5];
    Entity* ent;
    s16 dist;
    s32 i;

    // Saturn 0x060E9E5E: hit
    if ((self->hitFlags & 3) && self->step != TREE_DEATH) {
        if (self->step != TREE_HIT) {
            TreeSpawnLeaves(self, 5, 0);
            if (PLAYER.posX.i.hi - self->posX.i.hi > 0) {
                TREE_EXT(self)->angVel = -0x4000; // leans away
                TREE_EXT(self)->lastAngVel = -0x2000;
                TREE_EXT(self)->anim = tree_anim_hit_r;
            } else {
                TREE_EXT(self)->angVel = 0x4000;
                TREE_EXT(self)->lastAngVel = 0x2000;
                TREE_EXT(self)->anim = tree_anim_hit_l;
            }
            self->step_s = 2; // (the Saturn's too: SetStep clears it)
        }
        SetStep(TREE_HIT); // in step 5: only restarts the hit animation
    }
    // Saturn 0x060E9F6A: killed
    if ((self->flags & FLAG_DEAD) && self->step != TREE_DEATH) {
        SetStep(TREE_DEATH);
        self->hitboxState = 0;
        if (PLAYER.posX.i.hi - self->posX.i.hi > 0) {
            TREE_EXT(self)->anim = tree_anim_death_r;
        } else {
            TREE_EXT(self)->anim = tree_anim_death_l;
        }
        self->step_s = 2;
    }

    switch (self->step) {
    case TREE_INIT:
        init[0] = MOD_TREE_ANIMSET;
        init[1] = 1;
        init[2] = MOD_TREE_UNK5A;
        init[3] = TREE_PAL;
        init[4] = MOD_TREE_ENEMY;
        InitializeEntity(init);                // step -> 1
        self->zPriority = MOD_TREE_BODY_Z - 2; // Saturn sprite z -= 2
        // Saturn sprite flags |= 0x40 every frame: always rotated
        self->drawFlags |= ENTITY_ROTATE;
        for (i = 0; i < 8; i++) {
            ent = self + 2 + i;
            CreateEntityFromEntity(E_MOD_TREE_FACE, self, ent);
            ent->animCurFrame = 0x1A + i;
            ent->rotate = (u8)Random();
            TREE_FACE_EXT(ent)->tree = self;
            ent->params = i;
        }
        ent = self + 1;
        TREE_EXT(self)->base = ent;
        CreateEntityFromEntity(E_MOD_TREE_BASE, self, ent);
        TreeShuffleFaces();
        TREE_EXT(self)->faceCount = 8;
        break;

    case TREE_IDLE:
        dist = TreeLookAtPlayer(self);
        if (dist < 0) {
            dist = -dist;
        }
        TreePlaceBranches(self);
        TreeRegrowFaces(self);
        if (dist <= SAT_X(191)) {
            SetStep(TREE_ALERT);
        }
        if ((u8)Random() == 0 && !TREE_EXT(self)->swaying) {
            TREE_EXT(self)->swaying = 1;
            if (PLAYER.posX.i.hi - self->posX.i.hi > 0) {
                TREE_EXT(self)->angVel = 0x2000;
                TREE_EXT(self)->lastAngVel = 0x1000;
            } else {
                TREE_EXT(self)->angVel = -0x2000;
                TREE_EXT(self)->lastAngVel = -0x1000;
            }
        }
        if (!TREE_EXT(self)->swaying) {
            break;
        }
        switch (self->step_s) {
        case 0:
            if (TreeSwing(self, 288)) {
                self->step_s = 1;
            }
            break;
        case 1:
            if (TreeSwing(self, 288)) {
                self->step_s = 3;
            }
            break;
        case 2:
            if (self->rotate == 0) {
                self->step_s = 0;
            } else {
                TreeEaseRotate(self);
            }
            break;
        case 3:
            if (self->rotate) {
                TreeEaseRotate(self);
                break;
            }
            SetStep(TREE_IDLE);
            self->step_s = 2;
            TREE_EXT(self)->swaying = 0;
            break;
        }
        break;

    case TREE_ALERT:
        dist = TreeLookAtPlayer(self);
        if (dist < 0) {
            dist = -dist;
        }
        TreePlaceBranches(self);
        TreeRegrowFaces(self);
        if (dist > SAT_X(191)) {
            SetStep(TREE_IDLE);
        }
        if (dist <= SAT_X(57) && !TREE_EXT(self)->regrowing) {
            SetStep(TREE_DROP);
        }
        if (((u8)Random() >> 2) == 0 && !TREE_EXT(self)->regrowing) {
            SetStep(TREE_SHAKE);
            if (PLAYER.posX.i.hi - self->posX.i.hi > 0) {
                TREE_EXT(self)->angVel = 0x3000;
                TREE_EXT(self)->lastAngVel = 0x1800;
            } else {
                TREE_EXT(self)->angVel = -0x3000;
                TREE_EXT(self)->lastAngVel = -0x1800;
            }
            self->step_s = 2;
        }
        break;

    case TREE_SHAKE:
        TreeLookAtPlayer(self);
        TreePlaceBranches(self);
        switch (self->step_s) {
        case 0:
            if (TreeSwing(self, 288)) {
                self->step_s = 1;
            }
            break;
        case 1: // stronger pull; the reversal throws a face
            if (TreeSwing(self, 576)) {
                self->step_s = 3;
                self[2 + tree_face_order[TREE_EXT(self)->faceCount - 1]].step =
                    2;
                TREE_EXT(self)->faceCount--;
            }
            break;
        case 2: // entered here: straighten up first
            if (self->rotate == 0) {
                self->step_s = 0;
            } else {
                TreeEaseRotate(self);
            }
            break;
        case 3:
            if (self->rotate) {
                TreeEaseRotate(self);
                break;
            }
            SetStep(TREE_ALERT);
            break;
        }
        break;

    case TREE_DROP:
        TreeLookAtPlayer(self);
        TreeDropAllFaces(self);
        TREE_EXT(self)->timer = 0x100; // never read
        TREE_EXT(self)->faceCount = 0; // -> regrow them all
        SetStep(TREE_ALERT);
        break;

    case TREE_HIT:
        AnimateEntity(TREE_EXT(self)->anim, self);
        if ((Random() & 1) && self->pose == 2 && self->poseTimer == 0) {
            g_api.PlaySfx(MOD_TREE_SFX_HURT);
        }
        TreePlaceBranches(self);
        switch (self->step_s) {
        case 0:
            if (TreeSwing(self, 288)) {
                self->step_s = 3;
            }
            break;
        case 1:
            break;
        case 2:
            if (self->rotate == 0) {
                self->step_s = 0;
            } else {
                TreeEaseRotate(self);
            }
            break;
        case 3:
            if (self->rotate) {
                TreeEaseRotate(self);
                break;
            }
            SetStep(TREE_ALERT);
            self->step_s = 2; // (as the Saturn; unused by step 2)
            break;
        }
        break;

    case TREE_DEATH:
        if (self->pose == 1 && self->poseTimer == 0) {
            g_api.PlaySfx(MOD_TREE_SFX_DEATH);
        }
        AnimateEntity(TREE_EXT(self)->anim, self);
        TreePlaceBranches(self);
        TreeDeathSequence(self);
        break;
    }
    // Saturn epilogue (sprite rotate = rotate) is implicit with ENTITY_ROTATE
}

// Saturn func_060EAAA8: a face hanging from a branch (self+2 .. self+9 of the
// tree, which positions it every frame)
void EntityModTreeFace(Entity* self) {
    u16 init[5];
    Entity* tree;
    Entity* ent;
    s16 rot;

    tree = TREE_FACE_EXT(self)->tree;
    switch (self->step) {
    case 0:
        init[0] = MOD_TREE_ANIMSET;
        init[1] = self->animCurFrame; // 0x1A + branch, set by the tree
        init[2] = MOD_TREE_UNK5A;
        init[3] = TREE_PAL;
        init[4] = ENEMY_INTERACTABLE;      // Saturn TekiInit(self, 5)
        InitializeEntity(init);            // step -> 1
        self->zPriority = MOD_TREE_BODY_Z; // on the tree, behind Alucard
        self->drawFlags |= ENTITY_ROTATE;
        TREE_FACE_EXT(self)->frame = self->animCurFrame;
        break;

    case 1: // swings with the tree's angular acceleration, settles back
        rot = self->rotate +
              ((TREE_EXT(tree)->angVel - TREE_FACE_EXT(self)->lastTreeAngVel) >>
               5);
        if (rot >= 0) {
            self->rotate = rot - 1;
        } else {
            self->rotate = rot + 1;
        }
        TREE_FACE_EXT(self)->lastTreeAngVel = TREE_EXT(tree)->angVel;
        break;

    case 2: // thrown at the player by a shake
    case 4: // dropped straight (player close, tree dead)
        ent = AllocEntity(&g_Entities[160], &g_Entities[192]);
        if (ent == NULL) {
            self->animCurFrame = TREE_FACE_EXT(self)->frame;
        } else {
            CreateEntityFromEntity(E_MOD_TREE_DROP, self, ent);
            ent->animCurFrame = tree_drop_frames[self->params][0];
            if (self->step == 2) {
                ent->params = 1;
                // reaches the player's x in 40 frames (PS1 dx: no SAT_X)
                ent->velocityX = (PLAYER.posX.val - self->posX.val) / 40;
                ent->velocityY = FIX(-1.5);
            } else {
                ent->params = 0;
                ent->velocityX = 0;
                ent->velocityY = 0;
            }
            self->animCurFrame = 0;
        }
        // Saturn: step 5 even when the allocation failed (the face then
        // stays visible but inert until regrown)
        self->step = 5;
        break;

    case 3: // regrown
        self->animCurFrame = TREE_FACE_EXT(self)->frame;
        self->step = 1;
        break;

    case 5: // empty branch
        break;
    }
}

// Saturn func_060EACDC: a falling face (enemy 0x197)
void EntityModTreeDrop(Entity* self) {
    u16 init[5];
    Collider col;
    Entity* ent;
    s16 sensorY;
    u8 hitFlags;

    hitFlags = self->hitFlags;
    if (hitFlags & 0x80) {
        if (self->step != TREE_DROP_TOUCH) {
            SetStep(TREE_DROP_TOUCH);
        }
    } else if (hitFlags || (self->flags & FLAG_DEAD)) {
        if (self->step != TREE_DROP_HIT && self->step != TREE_DROP_TOUCH) {
            SetStep(TREE_DROP_HIT);
        }
    }
    // Saturn reads the branch table with the face's own params (0/1): 4 px
    sensorY = tree_drop_frames[self->params][1];

    switch (self->step) {
    case TREE_DROP_INIT:
        init[0] = MOD_TREE_ANIMSET;
        init[1] = self->animCurFrame; // 35 or 47, set by the branch
        init[2] = MOD_TREE_UNK5A;
        init[3] = TREE_PAL;
        init[4] = MOD_TREE_DROP_ENEMY;
        InitializeEntity(init);       // step -> 1
        self->zPriority = MOD_TREE_Z; // TODO Z (Saturn: default enemy z)
        self->drawFlags |= ENTITY_ROTATE;
        self->flags |= FLAG_UNK_2000;
        self->hitboxWidth = SAT_X(5);
        self->hitboxHeight = 5;
        if (self->animCurFrame == 0x2F) {
            TREE_DROP_EXT(self)->anim = tree_anim_drop_b;
        } else {
            TREE_DROP_EXT(self)->anim = tree_anim_drop_a;
        }
        g_api.PlaySfx(MOD_TREE_SFX_FACE_FALLS);
        break;

    case TREE_DROP_FALL: // nose along the velocity
        MoveEntity();
        self->velocityY += FIX(0.1875);
        self->rotate =
            0x400 - TREE_RATAN2(self->velocityY, -SAT_X_INV(self->velocityX));
        g_api.CheckCollision(
            self->posX.i.hi, (s16)(self->posY.i.hi + sensorY), &col, 0);
        if (!(col.effects & EFFECT_SOLID)) {
            break;
        }
        if (self->params == 0) {
            SetStep(TREE_DROP_LANDED);
            break;
        }
        // thrown: bounce, harmless, until the bounce is slow
        self->posY.i.hi += col.unk18;
        self->velocityY = -self->velocityY / 2;
        self->velocityX = self->velocityX / 2;
        self->hitboxWidth = 0;
        self->hitboxHeight = 0;
        self->hitboxState = 0;
        if (-self->velocityY <= 0x6FFF) {
            DestroyEntity(self);
            return;
        }
        break;

    case TREE_DROP_HIT:
        switch (self->step_s) {
        case 0:
            self->velocityY = -self->velocityY / 2;
            self->step_s++;
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent != NULL) {
                CreateEntityFromEntity(E_MOD_TREE_BLOOD, self, ent);
                ent->velocityX = self->velocityX;
                g_api.PlaySfx(MOD_TREE_SFX_SPLAT);
            }
            break;
        case 1: // spin down
            MoveEntity();
            self->velocityY += FIX(0.1875);
            self->palette = TREE_PAL;
            if (self->velocityX > 0) {
                self->rotate -= 0x40;
            } else {
                self->rotate += 0x40;
            }
            g_api.CheckCollision(
                self->posX.i.hi, (s16)(self->posY.i.hi + sensorY), &col, 0);
            if (col.effects & EFFECT_SOLID) {
                TREE_DROP_EXT(self)->timer = 80;
                self->step_s++;
                g_api.PlaySfx(MOD_TREE_SFX_SPLAT);
            }
            AnimateEntity(TREE_DROP_EXT(self)->anim, self);
            break;
        case 2:
            if (--TREE_DROP_EXT(self)->timer == 0) {
                DestroyEntity(self);
                return;
            }
            break;
        }
        break;

    case TREE_DROP_LANDED:
        if (self->step_s == 0) {
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent != NULL) {
                CreateEntityFromEntity(E_MOD_TREE_BLOOD, self, ent);
                ent->params = 2;
                g_api.PlaySfx(MOD_TREE_SFX_SPLAT);
            }
            self->hitboxState = 0;
            self->step_s++;
        }
        if (!AnimateEntity(TREE_DROP_EXT(self)->anim, self)) {
            DestroyEntity(self);
            return;
        }
        break;

    case TREE_DROP_TOUCH:
        switch (self->step_s) {
        case 0:
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent != NULL) {
                CreateEntityFromEntity(E_MOD_TREE_BLOOD, self, ent);
                ent->velocityX = -self->velocityX / 4;
                ent->params = 1;
                g_api.PlaySfx(MOD_TREE_SFX_SPLAT);
            }
            TREE_DROP_EXT(self)->timer = 8;
            self->velocityX = 0;
            self->velocityY = 0;
            self->step_s++;
            self->hitboxState = 0;
            break;
        case 1:
            if (--TREE_DROP_EXT(self)->timer == 0) {
                self->step_s++;
            }
            break;
        case 2:
            MoveEntity();
            self->velocityY += FIX(0.1875);
            g_api.CheckCollision(
                self->posX.i.hi, (s16)(self->posY.i.hi + sensorY), &col, 0);
            if (col.effects & EFFECT_SOLID) {
                TREE_DROP_EXT(self)->timer = 80;
                self->step_s++;
            }
            break;
        case 3:
            if (--TREE_DROP_EXT(self)->timer == 0) {
                DestroyEntity(self);
                return;
            }
            break;
        }
        self->palette = TREE_PAL;
        AnimateEntity(TREE_DROP_EXT(self)->anim, self);
        break;
    }
}

// Saturn func_060EB2B8: blood spray, 30 primitives (Saturn: 10 sprite prims
// with bank images 0-9 in palette 1, then 10 line + 3x3 polygon pairs).
// PS1: the 10 sprite drops are small red tiles (see notes.md); positions are
// screen space, x speeds scaled by 4/5. Per prim storage as the Saturn's:
// x2/y2 = 8.8 speed (y up), y3 = gravity; drops: y1 = start y; lines: x3 =
// life.
#ifndef MOD_TREE_BLOOD_DROP_W
#define MOD_TREE_BLOOD_DROP_W 3
#endif
#ifndef MOD_TREE_BLOOD_DROP_H
#define MOD_TREE_BLOOD_DROP_H 2
#endif

static void TreeBloodPlaceHead(Primitive* line) {
    Primitive* head = line->next;

    head->x0 = line->x1 - 1;
    head->y0 = line->y1 - 1;
}

void EntityModTreeBlood(Entity* self) {
    Primitive* prim;
    Primitive* head;
    s32 primIndex;
    s32 i;
    bool visible;

    switch (self->step) {
    case 0:
        primIndex = g_api.AllocPrimitives(PRIM_TILE, 30);
        if (primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->flags |= FLAG_UNK_2000 | FLAG_HAS_PRIMS;
        self->primIndex = primIndex;
        prim = &g_PrimBuf[primIndex];
        for (i = 0; i < 10; i++) { // drops
            prim->type = PRIM_TILE;
            prim->u0 = MOD_TREE_BLOOD_DROP_W;
            prim->v0 = MOD_TREE_BLOOD_DROP_H;
            prim->r0 = 0x90; // Saturn palette 1 (red), colour 5
            prim->g0 = 0x40;
            prim->b0 = 0x18;
            prim->x0 = self->posX.i.hi;
            prim->y0 = prim->y1 = self->posY.i.hi;
            prim->priority = self->zPriority; // TODO Z (Saturn z - 0x40)
            prim->drawMode = DRAW_DEFAULT;    // Saturn drawMode 2
            prim->x2 =
                (self->velocityX >> 8) + SAT_X((s32)(Random() & 0x7FF) - 0x400);
            prim->y2 = (self->velocityY >> 8) + (Random() & 0x1FF) + 0x80;
            if (self->params == 1) {
                prim->x2 /= 2;
            }
            prim->y3 = 32;
            prim = prim->next;
        }
        TREE_BLOOD_EXT(self)->lines = prim;
        for (i = 0; i < 10; i++) { // streaks: a line and its 3x3 head
            prim->type = PRIM_LINE_G2;
            prim->r0 = prim->r1 = 0xC0; // Saturn 0xA118
            prim->g0 = prim->g1 = 0x40;
            prim->b0 = prim->b1 = 0x40;
            prim->priority = self->zPriority; // TODO Z (Saturn z - 0x40)
            prim->x0 = prim->x1 = self->posX.i.hi;
            prim->y0 = prim->y1 = self->posY.i.hi;
            prim->x2 =
                (self->velocityX >> 8) + SAT_X((s32)(Random() & 0x3FF) - 0x200);
            prim->y2 = (u8)Random() + 0x40;
            prim->x3 = 16; // life
            prim->y3 = 32;
            if (self->velocityX == 0) {
                prim->x2 *= 2;
                prim->y2 += 0x200;
                prim->x3 += 8;
            }
            if (self->params == 1) {
                prim->x2 /= 2;
            }
            prim->drawMode = DRAW_DEFAULT;
            head = prim->next;
            head->type = PRIM_TILE;
            head->u0 = 3;
            head->v0 = 3;
            head->r0 = 0xE0; // Saturn 0x909C
            head->g0 = 0x20;
            head->b0 = 0x20;
            head->priority = self->zPriority; // TODO Z (Saturn z - 0x40)
            head->drawMode = DRAW_DEFAULT;
            TreeBloodPlaceHead(prim);
            prim = head->next;
        }
        self->step++;
        break;

    case 1:
        prim = &g_PrimBuf[self->primIndex];
        for (i = 0; i < 10; i++) {
            prim->x0 += prim->x2 >> 8;
            prim->y2 -= prim->y3;
            prim->y0 -= prim->y2 >> 8;
            if (prim->y0 > prim->y1) {
                prim->drawMode = DRAW_HIDE;
            }
            prim = prim->next;
        }
        prim = TREE_BLOOD_EXT(self)->lines;
        for (i = 0; i < 10; i++) {
            head = prim->next;
            prim->x0 = (prim->x0 + prim->x1) / 2; // the tail follows
            prim->y0 = (prim->y0 + prim->y1) / 2;
            prim->x1 += prim->x2 >> 8;
            prim->y2 -= prim->y3;
            prim->y1 -= prim->y2 >> 8;
            if (prim->x3-- == 0) {
                if (head->drawMode == DRAW_HIDE) {
                    prim->drawMode = DRAW_HIDE;
                } else {
                    head->drawMode = DRAW_HIDE;
                    prim->x3 = 5;
                }
            }
            TreeBloodPlaceHead(prim);
            prim = head->next;
        }
        visible = false;
        prim = &g_PrimBuf[self->primIndex];
        for (i = 0; i < 30; i++) {
            if (prim->drawMode != DRAW_HIDE) {
                visible = true;
                break;
            }
            prim = prim->next;
        }
        if (!visible) {
            DestroyEntity(self);
        }
        break;
    }
}

// Saturn func_060EB824 (init func_060EB728): a leaf, params 0 = shaken off
// by a hit (green), 1 = shed at death (withered)
void EntityModTreeLeaf(Entity* self) {
    u16 init[5];
    Collider col;
    u16 frame;

    switch (self->step) {
    case 0:
        frame = (u8)Random() % 13 + 0x36;
        init[0] = MOD_TREE_ANIMSET;
        init[1] = frame;
        init[2] = MOD_TREE_UNK5A;
        init[3] = self->params ? TREE_LEAF_PAL_DEATH : TREE_LEAF_PAL_HIT;
        init[4] = ENEMY_INTERACTABLE; // Saturn TekiInit(self, 5)
        InitializeEntity(init);       // step -> 1
        self->zPriority = MOD_TREE_Z; // TODO Z (Saturn: default enemy z)
        self->drawFlags |= ENTITY_ROTATE;
        self->rotate = (u8)Random() << 4;
        if (Random() & 1) {
            TREE_LEAF_EXT(self)->offX = -tree_leaf_offs[frame - 0x36];
        } else {
            TREE_LEAF_EXT(self)->offX = tree_leaf_offs[frame - 0x36];
        }
        break;

    case 1: // thrown up
        MoveEntity();
        self->velocityY += FIX(0.125);
        if (self->velocityY > 0) {
            self->velocityY = 0;
            self->flags |= FLAG_DESTROY_IF_OUT_OF_CAMERA |
                           FLAG_DESTROY_IF_BARELY_OUT_OF_CAMERA;
            self->step++;
        }
        break;

    case 2: // flutter down (the Saturn never caps the fall speed)
        MoveEntity();
        self->velocityY += 0x400; // FIX(1/64)
        self->velocityX = SAT_X(rsin(TREE_LEAF_EXT(self)->phase) << 5);
        TREE_LEAF_EXT(self)->phase += 0x20;
        if (self->velocityX > 0) {
            self->rotate -= 0x40;
        } else {
            self->rotate += 0x40;
        }
        // Saturn adds 3 to the 16.16 y (3/65536 px): no offset
        g_api.CheckCollision(self->posX.i.hi, self->posY.i.hi, &col, 0);
        if (col.effects & EFFECT_SOLID) {
            DestroyEntity(self);
            return;
        }
        break;
    }
}

// Saturn func_060EBA00: the trunk base / roots (self+1 of the tree), 30 px
// below the tree, on the floor; frames 68 (alive), 87, 88 (set by the tree)
void EntityModTreeBase(Entity* self) {
    u16 init[5];

    if (self->step == 0) {
        init[0] = MOD_TREE_ANIMSET;
        init[1] = 68;
        init[2] = MOD_TREE_UNK5A;
        init[3] = TREE_PAL;
        init[4] = ENEMY_INTERACTABLE; // Saturn TekiInit(self, 5)
        InitializeEntity(init);       // step -> 1
        // TODO Z (Saturn sprite z -= 8: behind the tree, still above the
        // flowers at 0xA0)
        self->zPriority = MOD_TREE_BODY_Z - 7;
        self->posY.i.hi += 30;
    }
}
