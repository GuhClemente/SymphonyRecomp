// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Ifrit (Saturn RSTAGE15 "Hell Garden" entities 0x23-0x2B: func_060E6878 and
 * its helpers 060E76C4 / 78B4 / 7AC4 / 7C3C / 7E60 / 823C / 854C / 88A0,
 * enemy def 0x192; the decomp calls it "Efreet"), ported to the Hell Garden
 * overlay (Saturn port mod, translated from the SH-2). Include after the
 * stage header and e_misc.h.
 *
 * A red fire genie floating on a flame tail, bobbing, with a flame in its
 * hair and one in its hand (two helper entities, self + 1 and self + 2,
 * that burn the player too). It turns to face the player, dashes at him
 * when he is far, backs off when he is near, and attacks:
 *   - near: a lunging punch;
 *   - mid range: it gathers embers, then breathes a stream of fireballs
 *     (the common opaque puff, growing as it flies);
 *   - far: it raises a blue flame and throws a fire ring that flies along
 *     the ground and erupts into a tall fire pillar under the player.
 * Hit while idle, dashing or backing off it backs off. It leaves fading red
 * afterimages. Dying it darkens, a fire pillar rises under it, it stretches
 * into the pillar and rises with it, and teal embers drift up.
 *
 * Required from the including stage:
 *   MOD_IFRIT_ANIMSET   animSet of the converted Saturn bank 24 (0x060F04E4,
 *                       frames 0x060F1648: PSX frame n = Saturn frame n,
 *                       drawn at the entity as the Saturn's; frame 69's
 *                       image, the fire pillar, kept whole)
 *   MOD_IFRIT_UNK5A     (optional, default 0)
 *   MOD_SFX_IFRIT_DEATH / _FIRE_BREATH / _FIRE_RING / _FIRE_PILLAR /
 *   _CHARGE             (Saturn 0x8F2 / 0x8F3 / 0x8F4 / 0x8F5 / 0x8F6)
 *   enum entries E_MOD_IFRIT (Saturn 0x23) and E_MOD_IFRIT_FLAME,
 *   _FLAME2, _AFTERIMAGE, _PUFF, _RING, _EMBERS, _PILLAR, _ASHES (0x24-0x2B)
 *
 * Position: as the Wraith's (mods/sotn-ultimate/src/enemies/e_mod_wraith.h),
 * the Saturn draws the sprite, places the hitbox and spawns the helpers at a
 * bobbing height (Saturn 0x98 = posY - 12 + 8 sin) while its posY stays on its
 * floor sensor; here posY is the logical position during the update and the
 * drawn one between updates, ext->bob the difference.
 *
 * Primitives: the Saturn's fire pillars are VDP1 scaled sprites of the
 * bank's image 70 (frame 69, palette 2) stretched over a rectangle: here
 * textured GT4s with frame 69's part (its tpage, clut and uv read from the
 * converted frame). Its dots (the gathering embers, the ashes) are 1-2 px
 * lines: here tiles, their state in the tiles' unused fields.
 */

#ifndef MOD_IFRIT_ANIMSET
#error "define MOD_IFRIT_ANIMSET before including e_mod_ifrit.h"
#endif
#ifndef MOD_IFRIT_UNK5A
#define MOD_IFRIT_UNK5A 0
#endif
// the Saturn's sprite flag 0x80, set on the afterimages after 8 ticks and on
// the fireballs once they no longer burn (unknown; taken as half
// transparency: the bank's colours carry the STP bit, rgdn_assets.py)
#ifndef MOD_IFRIT_FADE_BLEND
#define MOD_IFRIT_FADE_BLEND BLEND_TRANSP
#endif
// the death pillar's VDP1 mesh (every other frame, in front): taken as half
// transparency
#ifndef MOD_IFRIT_MESH
#define MOD_IFRIT_MESH DRAW_TRANSP
#endif
// the fireballs: the common opaque puff (Saturn 0.BIN's, its palette + 1;
// here the PS1's own, DRA's animset 14 as EntityExplosionPuffOpaque's)
#ifndef MOD_IFRIT_PUFF_PAL
#define MOD_IFRIT_PUFF_PAL 0xD0
#endif

// Saturn screen is 320 px wide, PS1 is 256: horizontal quantities * 4/5.
#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif

// st_common.h (built in the stage's st_common.c)
s32 UnkCollisionFunc2(s16* sensors);

typedef enum {
    IFRIT_INIT = 0,
    IFRIT_IDLE = 1,   // faces the player, picks an action
    IFRIT_ATTACK = 2, // picks an attack by distance
    IFRIT_DASH = 3,   // at a far player
    IFRIT_PUNCH = 4,
    IFRIT_BACKOFF = 5,
    IFRIT_BREATH = 6, // gathers embers, breathes fireballs
    IFRIT_RING = 8,   // throws the fire ring (the pillar)
    IFRIT_TURN = 16,
    IFRIT_HIT = 18, // hit while idle, dashing or backing off: = 5
    IFRIT_DEATH = 20,
} IfritSteps;

typedef struct {
    Primitive* prim; // Saturn 0x7C
    s32 t;           // Saturn 0x78: a fireball's spawn timer (set by the Ifrit)
    s16 timer;       // Saturn 0x84
    s16 timer2;      // Saturn 0x88
    s16 fxTimer;     // Saturn 0x90 / 0xA4
    s16 phase;       // Saturn 0x94: the bobbing
    s32 bob;         // Saturn 0x98 - posY: drawn height - logical posY
    s32 fx;          // Saturn 0x9C: the afterimages' and fireballs' ticks
    Entity* parent;  // Saturn 0xA0
    s32 scrollX;     // Saturn 0xA8 / 0xAC: the camera, last frame
    s32 scrollY;
    Entity* pillar; // Saturn 0xB4: the death pillar
    s16 scale;      // a fireball's sprite scale (Saturn u8, 0x40 = 1.0)
} ET_ModIfrit;
#define IFRIT_EXT(e) ((ET_ModIfrit*)&(e)->ext)

typedef struct {
    Primitive* prim;
    s16 timer; // Saturn 0x90
    s16 pad;
    // the death pillar's two rectangles: x relative to the entity (Saturn
    // units), y on screen
    s16 xa[2], xc[2], ya[2], yc[2];
} ET_ModIfritPillar;
#define PILLAR_EXT(e) ((ET_ModIfritPillar*)&(e)->ext)

// a tile's state in its unused fields
#define IFRIT_DOT_X(p) (*(s32*)&(p)->r1)
#define IFRIT_DOT_Y(p) (*(s32*)&(p)->x1)
#define IFRIT_DOT_VY(p) (*(s32*)&(p)->x2)
#define IFRIT_DOT_T(p) (*(u16*)&(p)->u1)

// Saturn 0x060F03DE.., (duration, frame) as the PS1's
static u8 ifrit_anim_flame[] = { // 0x060F03DE (the hair flame, loops)
    0x05, 0x01, 0x05, 0x02, 0x05, 0x03, 0x05, 0x04, 0x00, 0x00};
static u8 ifrit_anim_flame_out[] = { // 0x060F03E8
    0x08, 0x01, 0x08, 0x05, 0x08, 0x06, 0x08, 0x07, 0x00, 0x00};
static u8 ifrit_anim_flame2[] = { // 0x060F03F2 (the hand flame, loops)
    0x05, 0x08, 0x05, 0x09, 0x05, 0x0A, 0x05, 0x0B, 0x00, 0x00};
static u8 ifrit_anim_flame2_out[] = { // 0x060F03FC
    0x08, 0x08, 0x08, 0x0C, 0x08, 0x0D, 0x08, 0x0E, 0x00, 0x00};
static u8 ifrit_anim_idle[] = { // 0x060F0406 (loops)
    0x1B, 0x0F, 0x0F, 0x10, 0x10, 0x11, 0x0F, 0x10, 0x00, 0x00};
static u8 ifrit_anim_turn[] = { // 0x060F0410
    0x08, 0x0F, 0x03, 0x16, 0x03, 0x17, 0x02, 0x18, 0x02, 0x19, 0x02,
    0x1A, 0x04, 0x1B, 0x02, 0x1C, 0x02, 0x1D, 0x08, 0x1E, 0xFF, 0xFF};
static u8 ifrit_anim_dash[] = { // 0x060F0426 (loops)
    0x02, 0x0F, 0x02, 0x12, 0x02, 0x13, 0x00, 0x00};
static u8 ifrit_anim_dash_end[] = { // 0x060F042E
    0x02, 0x13, 0x02, 0x12, 0x02, 0x0F, 0xFF, 0xFF};
static u8 ifrit_anim_back[] = { // 0x060F0436
    0x02, 0x0F, 0x02, 0x14, 0x02, 0x15, 0xFF, 0xFF};
static u8 ifrit_anim_back_end[] = { // 0x060F043E
    0x02, 0x15, 0x02, 0x14, 0x02, 0x0F, 0xFF, 0xFF};
static u8 ifrit_anim_raise[] = { // 0x060F0446 (raises the blue flame)
    0x18, 0x0F, 0x06, 0x1F, 0x06, 0x20, 0x05, 0x21, 0xFF, 0xFF};
static u8 ifrit_anim_hold[] = { // 0x060F0450 (loops)
    0x03, 0x22, 0x03, 0x23, 0x03, 0x24, 0x03, 0x25, 0x00, 0x00};
static u8 ifrit_anim_throw[] = { // 0x060F045A (the ring leaves at frame 42)
    0x04, 0x22, 0x03, 0x26, 0x03, 0x27, 0x02, 0x28, 0x02, 0x29,
    0x02, 0x2A, 0x03, 0x2B, 0x04, 0x2C, 0x04, 0x2D, 0x0A, 0x2E,
    0x04, 0x2F, 0x04, 0x30, 0x04, 0x31, 0x37, 0x0F, 0xFF, 0xFF};
static u8 ifrit_anim_charge[] = { // 0x060F0478 (gathers embers)
    0x08, 0x0F, 0x08, 0x32, 0x08, 0x34, 0x40, 0x33, 0x08, 0x34, 0x04,
    0x35, 0x03, 0x36, 0x03, 0x37, 0x03, 0x38, 0x03, 0x39, 0xFF, 0xFF};
static u8 ifrit_anim_breath[] = { // 0x060F048E (loops)
    0x03, 0x3A, 0x03, 0x3B, 0x00, 0x00};
static u8 ifrit_anim_breath_end[] = { // 0x060F0494
    0x03, 0x3C, 0x03, 0x3D, 0x03, 0x3E, 0x03,
    0x36, 0x06, 0x35, 0x18, 0x0F, 0xFF, 0xFF};
static u8 ifrit_anim_ring[] = { // 0x060F04A2 (loops)
    0x03, 0x3F, 0x03, 0x40, 0x03, 0x41, 0x00, 0x00};
static u8 ifrit_anim_punch[] = { // 0x060F04AA (lunges at frame 76)
    0x18, 0x0F, 0x04, 0x1F, 0x04, 0x20, 0x04, 0x47, 0x03, 0x48, 0x03, 0x49,
    0x03, 0x48, 0x03, 0x49, 0x03, 0x4A, 0x02, 0x4B, 0x03, 0x4C, 0x04, 0x4D,
    0x03, 0x4E, 0x04, 0x4F, 0x04, 0x30, 0x04, 0x31, 0x04, 0x0F, 0xFF, 0xFF};
static u8 ifrit_anim_death[] = { // 0x060F04CE (darkens)
    0x10, 0x0F, 0x05, 0x32, 0x0F, 0x42, 0x02, 0x43, 0x02, 0x42,
    0x02, 0x43, 0x02, 0x44, 0x02, 0x43, 0x2F, 0x44, 0xFF, 0xFF};
// Saturn 0x060F0774: the common opaque puff (the PS1's puff_anim_one)
static u8 ifrit_anim_puff[] = {
    0x03, 0x01, 0x03, 0x02, 0x03, 0x03, 0x03, 0x04, 0x03, 0x05,
    0x03, 0x06, 0x03, 0x07, 0x03, 0x08, 0x03, 0x09, 0x03, 0x0A,
    0x03, 0x0B, 0x03, 0x0C, 0x03, 0x0D, 0xFF, 0x00};

// Saturn 0x060F03C6: UnkCollisionFunc2 (the floor is 32 px below posY)
static s16 ifrit_sensors[] = {0, 32, SAT_X(8), 0};

// Saturn 0x060F0534: per frame {width, height, -offX, offY} (Saturn units)
static s8 ifrit_hitbox[82][4] = {
    {6, 14, -120, -96}, {8, 20, -14, -2}, {8, 20, -14, -2}, {8, 20, -14, -2},
    {8, 20, -14, -2},   {0, 0, 0, 0},     {0, 0, 0, 0},     {0, 0, 0, 0},
    {4, 4, 0, -6},      {4, 4, 0, -6},    {4, 4, 0, -6},    {4, 4, 0, -6},
    {0, 0, 0, 0},       {0, 0, 0, 0},     {0, 0, 0, 0},     {6, 24, 0, 6},
    {6, 24, 0, 6},      {6, 24, 0, 6},    {8, 22, 8, 6},    {8, 22, 8, 6},
    {6, 24, 2, 6},      {6, 24, -2, 6},   {6, 24, 0, 6},    {10, 22, 0, 6},
    {12, 20, 0, 6},     {18, 18, 0, 6},   {18, 18, 0, 6},   {12, 20, 0, 6},
    {10, 22, 0, 6},     {6, 24, 0, 6},    {6, 24, 0, 6},    {6, 24, 0, 6},
    {6, 24, 0, 6},      {6, 24, 0, 6},    {6, 24, 0, 6},    {6, 24, 0, 6},
    {6, 24, 0, 6},      {6, 24, 0, 6},    {6, 24, 0, 6},    {6, 24, 0, 6},
    {6, 24, 6, 6},      {6, 24, 8, 6},    {10, 24, 8, 6},   {10, 24, 8, 6},
    {10, 24, 8, 6},     {10, 24, 8, 6},   {10, 24, 8, 6},   {10, 24, 8, 6},
    {8, 24, 8, 6},      {8, 24, 0, 6},    {8, 24, -2, 6},   {10, 24, -2, 6},
    {10, 24, -2, 6},    {10, 22, 0, 6},   {12, 18, 10, 6},  {16, 18, 26, 6},
    {16, 18, 26, 6},    {16, 18, 26, 6},  {16, 18, 26, 6},  {16, 18, 26, 6},
    {16, 18, 26, 6},    {16, 18, 26, 6},  {16, 18, 26, 6},  {6, 14, -4, 0},
    {6, 14, -4, 0},     {6, 14, -4, 0},   {0, 0, 0, 0},     {0, 0, 0, 0},
    {0, 0, 0, 0},       {0, 0, 0, 0},     {0, 0, 0, 0},     {12, 24, 0, 0},
    {16, 24, 0, 0},     {20, 24, 0, 0},   {12, 28, 0, 0},   {12, 24, 8, 6},
    {32, 12, 20, 0},    {34, 12, 22, 0},  {20, 16, 8, 6},   {17, 18, 8, 6},
    {26, 12, 16, 4},    {0, 0, 0, 0},
};
// Saturn 0x060F0683: the hair flame's x per Ifrit frame
static s8 ifrit_flame_x[82] = {
    0, 0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 3, 6, 9, 12, 12, 9, 6, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0, 7, 7, 7, 7, 7, 7, 7, 7,
    0, 0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
// Saturn 0x060F06D3 / 0x060F0723: the hand flame's x / y per Ifrit frame
// (the tables overlap on the Saturn: kept as its bytes)
static s8 ifrit_flame2_x[82] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 1,
    1,  14, 13, 3,  -1, 0,  0,  0,  1,  0,  0,  2,  2,  2,  0,  0, 0,
    0,  0,  0,  0,  0,  0,  16, 16, 17, 18, 18, 17, 17, 17, 11, 2, -8,
    -9, -8, 3,  15, 37, 37, 37, 37, 37, 37, 37, 37, 0,  0,  0,  0, 0,
    0,  0,  0,  0,  0,  0,  0,  16, 17, 17, 17, 17, 17, 0};
static s8 ifrit_flame2_y[82] = {
    17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  1,  1,  0,
    1,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  0,  0,  0,  0,  0,  2,  2,
    2,  2, 2, 2, 2, 2, 3, 1, 2, 0, 1, 0, 6, 15, 15, 15, 15, 15, 15, 15, 15,
    0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2,  2,  2,  0,  0,  0};

static void IfritFacePlayer(Entity* self) {
    self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
}

static void IfritResetAnim(Entity* self) {
    self->pose = 0;
    self->poseTimer = 0;
}

static void IfritSetStep(Entity* self, u16 step) {
    self->pose = 0;
    self->poseTimer = 0;
    self->step = step;
    self->step_s = 0;
}

// forward (the way it faces) at v px/frame (v in Saturn units)
#define IFRIT_VX(self, v) ((self)->facingLeft ? SAT_X(v) : SAT_X(-(v)))

static void IfritMove(Entity* self) {
    self->posX.val += self->velocityX;
    self->posY.val += self->velocityY;
}

// PlaySfx, as the Saturn (not positional)
static void IfritSfx(s16 id) { g_api.PlaySfx(id); }

// the per-frame hitbox (Saturn units: x * 4/5)
static void IfritHitbox(Entity* self) {
    s8* hb = ifrit_hitbox[self->animCurFrame];

    self->hitboxWidth = SAT_X((u8)hb[0]);
    self->hitboxHeight = (u8)hb[1];
    self->hitboxOffX = SAT_X(-hb[2]);
    self->hitboxOffY = hb[3];
}

void EntityModIfrit(Entity* self) {
    u16 init[5];
    ET_ModIfrit* ext = IFRIT_EXT(self);
    Entity* ent;
    s32 drawY; // Saturn 0x98 (the previous frame's, then this one's)
    s16 dist;
    u8 r;

    self->posY.val -= ext->bob; // drawn -> logical
    drawY = self->posY.val + ext->bob;
    if (self->hitParams && (self->step & 1)) {
        IfritSetStep(self, IFRIT_HIT);
    }
    if ((self->flags & FLAG_DEAD) && self->step != IFRIT_DEATH) {
        IfritSetStep(self, IFRIT_DEATH);
    }

    switch (self->step) {
    case IFRIT_INIT:
        init[0] = MOD_IFRIT_ANIMSET;
        init[1] = 0;
        init[2] = MOD_IFRIT_UNK5A;
        init[3] = 0;     // the frames carry the bank's palettes
        init[4] = 0x192; // "Ifrit"
        InitializeEntity(init);
        self->drawFlags = ENTITY_OPACITY | ENTITY_MASK_B;
        self->opacity = 128;
        // its flames: in the hair (self + 1) and the hand (self + 2)
        DestroyEntity(self + 1);
        CreateEntityFromCurrentEntity(E_MOD_IFRIT_FLAME, self + 1);
        DestroyEntity(self + 2);
        CreateEntityFromCurrentEntity(E_MOD_IFRIT_FLAME2, self + 2);
        IfritFacePlayer(self);
        self->step = IFRIT_IDLE;
        goto epilogue;

    case IFRIT_IDLE:
        dist = GetDistanceToPlayerX();
        if (self->facingLeft != ((GetSideToPlayer() & 1) ^ 1)) {
            self->step = IFRIT_TURN;
        } else if (dist <= SAT_X(31)) {
            self->step = IFRIT_BACKOFF;
        } else if (dist <= SAT_X(63)) {
            self->step = (Random() & 1) ? IFRIT_BACKOFF : IFRIT_PUNCH;
        } else if (dist <= SAT_X(159)) {
            r = (Random() & 0xFF) % 60;
            if (r <= 1) {
                self->step = IFRIT_ATTACK;
            } else if (r == 2) {
                self->step = IFRIT_BACKOFF;
            }
        } else if ((Random() & 0xFF) % 30 == 0) {
            self->step = IFRIT_DASH;
        }
        if (self->step != IFRIT_IDLE) {
            IfritResetAnim(self);
            self->step_s = 0;
        } else {
            AnimateEntity(ifrit_anim_idle, self);
        }
        goto epilogue;

    case IFRIT_ATTACK:
        dist = GetDistanceToPlayerX();
        if (dist <= SAT_X(63)) {
            self->step = IFRIT_PUNCH;
        } else if (dist > SAT_X(128)) {
            self->step = IFRIT_RING;
        } else {
            self->step = IFRIT_BREATH;
        }
        goto epilogue;

    case IFRIT_DASH:
        switch (self->step_s) {
        case 0:
            if (AnimateEntity(ifrit_anim_dash, self)) {
                break;
            }
            self->animCurFrame = ifrit_anim_dash[5];
            self->velocityX = IFRIT_VX(self, FIX(4));
            ext->timer = (Random() & 0xFF) % 15 + 45;
            self->step_s++;
            break;
        case 1:
            // until near the player (or out of time), then slows down
            if (GetDistanceToPlayerX() > SAT_X(160)) {
                if (--ext->timer) {
                    break;
                }
            }
            self->velocityX /= 2;
            self->step_s++;
            break;
        case 2:
            if (AnimateEntity(ifrit_anim_dash_end, self)) {
                break;
            }
            self->velocityX /= 2;
            ext->timer = 20;
            self->step_s++;
            break;
        case 3:
            if (--ext->timer) {
                break;
            }
            IfritResetAnim(self);
            self->velocityX = 0;
            self->step = IFRIT_ATTACK;
            self->step_s = 0;
            break;
        }
        break;

    case IFRIT_BACKOFF:
    case IFRIT_HIT:
        switch (self->step_s) {
        case 0:
            if (AnimateEntity(ifrit_anim_back, self)) {
                break;
            }
            self->animCurFrame = ifrit_anim_back[5];
            self->velocityX = IFRIT_VX(self, FIX(-3));
            ext->timer = (Random() & 0xFF) % 15 + 15;
            if (self->step == IFRIT_HIT) {
                ext->timer = ext->timer * 2 / 3;
            }
            self->step_s++;
            break;
        case 1:
            if (--ext->timer) {
                break;
            }
            self->velocityX /= 2;
            self->step_s++;
            break;
        case 2:
            if (AnimateEntity(ifrit_anim_back_end, self)) {
                break;
            }
            self->velocityX /= 2;
            ext->timer = 15;
            self->step_s++;
            break;
        case 3:
            if (--ext->timer) {
                break;
            }
            IfritResetAnim(self);
            self->velocityX = 0;
            self->step = IFRIT_IDLE;
            self->step_s = 0;
            break;
        }
        break;

    case IFRIT_PUNCH:
        if (self->animCurFrame == 76) {
            self->velocityX = IFRIT_VX(self, FIX(4));
        }
        self->velocityX /= 2;
        if (!AnimateEntity(ifrit_anim_punch, self)) {
            IfritResetAnim(self);
            self->velocityX = 0;
            self->step = IFRIT_IDLE;
        }
        break;

    case IFRIT_BREATH:
        switch (self->step_s) {
        case 0:
            ext->timer2 = 0;
            self->step_s++;
            IfritSfx(MOD_SFX_IFRIT_CHARGE);
            /* fallthrough */
        case 1:
            // embers gather at its head (8 bursts, every 4 ticks)
            if (ext->timer2 <= 31 && !(ext->timer2 & 3)) {
                ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
                if (ent != NULL) {
                    CreateEntityFromEntity(E_MOD_IFRIT_EMBERS, self, ent);
                    if (self->facingLeft) {
                        ent->posX.i.hi -= SAT_X(4);
                    } else {
                        ent->posX.i.hi += SAT_X(4);
                    }
                    ent->posY.val = drawY - FIX(18);
                    ent->facingLeft = self->facingLeft;
                    IFRIT_EXT(ent)->parent = self;
                    ent->params = (ext->timer2 >> 3) & 1;
                }
            }
            ext->timer2++;
            if (AnimateEntity(ifrit_anim_charge, self)) {
                break;
            }
            IfritResetAnim(self);
            self->step_s++;
            ext->timer2 = 64;
            IfritSfx(MOD_SFX_IFRIT_FIRE_BREATH);
            break;
        case 2:
            // a fireball every other tick for 64 ticks
            if (ext->timer2 == 0) {
                IfritResetAnim(self);
                self->step_s++;
                break;
            }
            ext->timer2--;
            AnimateEntity(ifrit_anim_breath, self);
            if (!(g_Timer & 1)) {
                break;
            }
            ent = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (ent == NULL) {
                break;
            }
            CreateEntityFromEntity(E_MOD_IFRIT_PUFF, self, ent);
            if (self->facingLeft) {
                ent->posX.i.hi += SAT_X(48);
            } else {
                ent->posX.i.hi -= SAT_X(48);
            }
            ent->posY.val = drawY - FIX(2);
            ent->facingLeft = self->facingLeft;
            IFRIT_EXT(ent)->t = ext->timer2;
            break;
        case 3:
            if (!AnimateEntity(ifrit_anim_breath_end, self)) {
                IfritSetStep(self, IFRIT_IDLE);
            }
            break;
        }
        goto epilogue;

    case IFRIT_RING:
        switch (self->step_s) {
        case 0:
            if (!AnimateEntity(ifrit_anim_raise, self)) {
                IfritResetAnim(self);
                self->step_s++;
                ext->timer2 = 60;
            }
            break;
        case 1:
            if (--ext->timer2 == 0) {
                IfritResetAnim(self);
                self->step_s++;
                ext->timer2 = -1; // the Saturn's loop decrements once more
            }
            AnimateEntity(ifrit_anim_hold, self);
            break;
        case 2:
            if (!AnimateEntity(ifrit_anim_throw, self)) {
                IfritSetStep(self, IFRIT_IDLE);
            }
            if (self->animCurFrame == 42 && (g_Timer & 1)) {
                ent = AllocEntity(&g_Entities[160], &g_Entities[192]);
                if (ent != NULL) {
                    CreateEntityFromEntity(E_MOD_IFRIT_RING, self, ent);
                    if (self->facingLeft) {
                        ent->posX.i.hi += SAT_X(60);
                    } else {
                        ent->posX.i.hi -= SAT_X(60);
                    }
                    ent->posY.val = drawY - FIX(4);
                    ent->facingLeft = self->facingLeft;
                    IfritSfx(MOD_SFX_IFRIT_FIRE_RING);
                }
            }
            break;
        }
        goto epilogue;

    case IFRIT_TURN:
        if (!AnimateEntity(ifrit_anim_turn, self)) {
            IfritSetStep(self, IFRIT_IDLE);
            self->animCurFrame = ifrit_anim_turn[1];
        } else if (self->animCurFrame == 25 && self->step_s == 0) {
            // facing the screen: now the other way
            self->step_s = 1;
            self->facingLeft ^= 1;
        }
        goto epilogue;

    case IFRIT_DEATH:
        switch (self->step_s) {
        case 0:
            self->hitboxState = 0;
            self->velocityX = 0;
            self->velocityY = 0;
            ext->timer2 = 10;
            self->step_s++;
            break;
        case 1:
            if (--ext->timer2 > 0) {
                break;
            }
            ext->timer2 = 0;
            self->step_s++;
            break;
        case 2:
            // it darkens; its flames go out
            if (!AnimateEntity(ifrit_anim_death, self)) {
                self->step_s++;
                ext->timer2 = 0;
                IfritSfx(MOD_SFX_IFRIT_DEATH);
            }
            if (++ext->timer2 == 48) {
                (self + 1)->step++;
                (self + 2)->step++;
            }
            break;
        case 3:
            // a fire pillar rises under it
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent != NULL) {
                CreateEntityFromEntity(E_MOD_IFRIT_PILLAR, self, ent);
                ent->posY.i.hi += 32;
                if (self->facingLeft) {
                    ent->posX.i.hi += SAT_X(2);
                } else {
                    ent->posX.i.hi += SAT_X(6);
                }
                ent->zPriority = self->zPriority;
            }
            ext->pillar = ent;
            ext->timer2 = 30;
            self->step_s++;
            break;
        case 4:
            if (--ext->timer2 > 0) {
                break;
            }
            self->step_s++;
            ext->timer2 = 64;
            if (ext->pillar != NULL) { // (the Saturn doesn't check)
                ext->pillar->step++;
            }
            self->velocityY = -0x8000;
            self->velocityX = 0;
            break;
        case 5:
            // it stretches into the pillar, rising
            if (--ext->timer2 <= 8) {
                self->step_s++;
                if (ext->pillar != NULL) {
                    ext->pillar->step++;
                }
                ext->timer2 = 14;
                self->velocityY *= 2;
                self->animCurFrame = 0;
            }
            // the Saturn's sprite scale (0x40 = 1.0), x narrows, y grows
            self->drawFlags |= ENTITY_SCALEX | ENTITY_SCALEY;
            self->scaleX = (ext->timer2 & 0xFF) << 2;
            self->scaleY = ((-ext->timer2 - 128) & 0xFF) << 2;
            self->rotPivotX = 0;
            self->rotPivotY = 0;
            IfritMove(self);
            break;
        case 6:
            if (--ext->timer2 <= 0) {
                self->step_s++;
                ext->timer2 = 0;
            }
            IfritMove(self);
            break;
        case 7:
            // its ashes drift up
            ent = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (ent != NULL) {
                CreateEntityFromEntity(E_MOD_IFRIT_ASHES, self, ent);
                if (!self->facingLeft) {
                    ent->posX.i.hi += SAT_X(6);
                }
                ent->zPriority = self->zPriority;
            }
            DestroyEntity(self);
            return;
        }
        goto epilogue;

    default:
        goto epilogue;
    }
    // Saturn 0x060E6F02: the walking steps move on the floor
    UnkCollisionFunc2(ifrit_sensors);

epilogue:
    IfritHitbox(self);
    // the bobbing (frozen while breathing fire and dying), the drawn height
    drawY = self->posY.val + (rsin(ext->phase * 32) << 7) - FIX(12);
    if (self->step != IFRIT_BREATH && self->step != IFRIT_DEATH) {
        ext->phase++;
    }
    // the afterimages, every 6 frames
    if (self->step != IFRIT_DEATH && !((u32)g_Timer % 6)) {
        ent = AllocEntity(&g_Entities[160], &g_Entities[192]);
        if (ent != NULL) {
            DestroyEntity(ent);
            CreateEntityFromEntity(E_MOD_IFRIT_AFTERIMAGE, self, ent);
            ent->facingLeft = self->facingLeft;
            ent->animCurFrame = self->animCurFrame;
            IFRIT_EXT(ent)->parent = self;
            ent->posY.val = drawY;
        }
    }
    ext->bob = drawY - self->posY.val;
    self->posY.val = drawY; // drawn (and hit-tested) there
}

// Saturn func_060E76C4 (self + 1) / 060E78B4 (self + 2): the flames in its
// hair and hand, following its frame; they burn (def 44). They go out when
// the Ifrit dies.
static void IfritFlame(Entity* self, Entity* parent, u8* anim, u8* animOut) {
    u16 init[5];

    if (parent->entityId != E_MOD_IFRIT) {
        DestroyEntity(self);
        return;
    }
    switch (self->step) {
    case 0:
        init[0] = MOD_IFRIT_ANIMSET;
        init[1] = 0;
        init[2] = MOD_IFRIT_UNK5A;
        init[3] = 0;
        init[4] = 44;
        InitializeEntity(init);
        self->zPriority = (self - 1)->zPriority - 1;
        self->step = 1;
        self->flags |= FLAG_UNK_2000;
        self->flags &= ~FLAG_DESTROY_IF_OUT_OF_CAMERA;
        if (parent == self - 1) {
            self->attack = 35;
        }
        /* fallthrough */
    case 1:
        AnimateEntity(anim, self);
        break;
    case 2:
        IfritResetAnim(self);
        self->step++;
        /* fallthrough */
    case 3:
        if (!AnimateEntity(animOut, self)) {
            DestroyEntity(self);
            return;
        }
        break;
    }
    IfritHitbox(self);
    self->facingLeft = parent->facingLeft;
    self->hitboxState = parent->hitboxState;
    self->posX.i.hi = parent->posX.i.hi;
    // the Ifrit updates first: its posY is its drawn height (Saturn 0x98)
    self->posY.val = parent->posY.val;
}

void EntityModIfritFlame(Entity* self) {
    Entity* parent = self - 1;
    s32 x;

    IfritFlame(self, parent, ifrit_anim_flame, ifrit_anim_flame_out);
    if (!self->entityId) {
        return;
    }
    x = SAT_X(ifrit_flame_x[parent->animCurFrame]);
    self->posX.i.hi += self->facingLeft ? x : -x;
}

void EntityModIfritFlame2(Entity* self) {
    Entity* parent = self - 2;
    s32 x;

    IfritFlame(self, parent, ifrit_anim_flame2, ifrit_anim_flame2_out);
    if (!self->entityId) {
        return;
    }
    self->posY.val -= FIX(21);
    x = SAT_X(ifrit_flame2_x[parent->animCurFrame] - 1);
    self->posX.i.hi += self->facingLeft ? x : -x;
    self->posY.i.hi += ifrit_flame2_y[parent->animCurFrame];
}

// Saturn func_060E7AC4: an afterimage, fading to red where it was left
void EntityModIfritAfterimage(Entity* self) {
    u16 init[5];
    ET_ModIfrit* ext = IFRIT_EXT(self);
    s16 frame;

    switch (self->step) {
    case 0:
        frame = self->animCurFrame; // set by the Ifrit
        init[0] = MOD_IFRIT_ANIMSET;
        init[1] = frame;
        init[2] = MOD_IFRIT_UNK5A;
        init[3] = 0;
        init[4] = 5;
        InitializeEntity(init); // step -> 1
        self->animCurFrame = frame;
        self->drawFlags = ENTITY_OPACITY | ENTITY_MASK_R;
        self->opacity = 96;
        ext->fx = 0;
        self->zPriority = ext->parent->zPriority - 2;
        break;
    case 1:
        if (!(++ext->fx & 3)) {
            self->zPriority--;
        }
        if (ext->fx > 24) {
            self->step++;
        } else {
            self->opacity = 96 - ext->fx * 4;
        }
        if (ext->fx == 8) {
            self->blendMode = MOD_IFRIT_FADE_BLEND; // Saturn sprite flag 0x80
        }
        if (ext->fx == 16) {
            self->drawFlags |= ENTITY_BLINK;
        }
        break;
    default:
        DestroyEntity(self);
        break;
    }
}

// Saturn func_060E7C3C: a fireball of its breath, the common opaque puff,
// growing as it flies; it burns (def 44) for its first 29-32 ticks
void EntityModIfritPuff(Entity* self) {
    u16 init[5];
    ET_ModIfrit* ext = IFRIT_EXT(self);

    if (self->step == 0) {
        init[0] = ANIMSET_DRA(14);
        init[1] = 0;
        init[2] = 0x79;
        init[3] = MOD_IFRIT_PUFF_PAL;
        init[4] = 44;
        InitializeEntity(init); // step -> 1
        self->flags |= FLAG_UNK_2000;
        IfritResetAnim(self);
        self->drawFlags = ENTITY_OPACITY | ENTITY_SCALEX | ENTITY_SCALEY;
        ext->fx = Random() & 3;
        // the later ones fall (ext->t: the Ifrit's countdown, 64 to 1)
        self->velocityY = FIX(3) - (ext->t << 12);
        self->velocityY += rand() & 0x3FFF;
        self->velocityX = IFRIT_VX(self, FIX(4));
        ext->scale = 16; // a quarter (Saturn 0x40 = 1.0)
        self->scaleX = self->scaleY = ext->scale << 2;
        self->rotPivotX = 0;
        self->rotPivotY = 0;
        self->hitboxWidth = 0;
        self->hitboxHeight = 0;
        self->opacity = (rsin(g_Timer * 128) >> 6) - 64;
        return;
    }
    self->velocityY -= 0x1800;
    IfritMove(self);
    ext->scale += 3;
    self->scaleX = self->scaleY = ext->scale << 2;
    if (ext->fx <= 31) {
        self->hitboxWidth = SAT_X(ext->scale / 5);
        self->hitboxHeight = ext->scale / 5;
    }
    if (!AnimateEntity(ifrit_anim_puff, self)) {
        DestroyEntity(self);
        return;
    }
    if (++ext->fx == 32) {
        self->blendMode = MOD_IFRIT_FADE_BLEND; // Saturn sprite flag 0x80
        self->hitboxState = 0;
        self->hitboxWidth = 0;
        self->hitboxHeight = 0;
    }
}

// a GT4 drawing the fire pillar's image (frame 69's part) whole
static void IfritPillarPrim(Primitive* prim, u16 priority) {
    s16* part;
    s32 tp;
    s32 du, dv;

    part =
        ((s16**)((void**)g_api.o.spriteBanks)[MOD_IFRIT_ANIMSET & 0x7FFF])[69];
    part++; // the part count
    tp = part[6] + MOD_IFRIT_UNK5A;
    du = (tp & 1) ? 0x80 : 0;
    dv = (tp & 2) ? 0x80 : 0;
    prim->type = PRIM_GT4;
    prim->tpage = tp >> 2;
    prim->clut = part[5];
    prim->u0 = prim->u2 = part[7] + du;
    prim->u1 = prim->u3 = part[9] + du;
    prim->v0 = prim->v1 = part[8] + dv;
    prim->v2 = prim->v3 = part[10] + dv;
    prim->r0 = prim->g0 = prim->b0 = 0x80;
    prim->r1 = prim->g1 = prim->b1 = 0x80;
    prim->r2 = prim->g2 = prim->b2 = 0x80;
    prim->r3 = prim->g3 = prim->b3 = 0x80;
    prim->priority = priority;
    prim->drawMode = DRAW_DEFAULT;
}

// a Saturn scaled sprite: the image over the rectangle A (top-left) - C
// (bottom-right); C above A turns it upside down, as on the Saturn
static void IfritRect(Primitive* prim, s16 xa, s16 ya, s16 xc, s16 yc) {
    prim->x0 = prim->x2 = xa;
    prim->x1 = prim->x3 = xc;
    prim->y0 = prim->y1 = ya;
    prim->y2 = prim->y3 = yc;
}

static Primitive* IfritPrims(Entity* self, u8 type, s32 count) {
    s16 primIndex;

    primIndex = g_api.AllocPrimitives(type, count);
    if (primIndex == -1) {
        DestroyEntity(self);
        return NULL;
    }
    self->flags |= FLAG_HAS_PRIMS;
    self->primIndex = primIndex;
    return &g_PrimBuf[primIndex];
}

// Saturn func_060E7E60: the fire ring. It flies along until it is over the
// player, then erupts into a fire pillar (two quads, up and down from it)
// that grows, stops burning at 32 ticks and narrows away
void EntityModIfritRing(Entity* self) {
    u16 init[5];
    ET_ModIfrit* ext = IFRIT_EXT(self);
    Primitive* prim;
    Primitive* prim2;
    s16 x, y, t, w;

    switch (self->step) {
    case 0:
        init[0] = MOD_IFRIT_ANIMSET;
        init[1] = 0;
        init[2] = MOD_IFRIT_UNK5A;
        init[3] = 0;
        init[4] = 44;
        InitializeEntity(init);
        self->attack *= 2;
        self->step = 1;
        self->flags |= FLAG_UNK_2000;
        IfritResetAnim(self);
        self->velocityX = IFRIT_VX(self, FIX(6));
        self->hitboxWidth = SAT_X(5);
        self->hitboxHeight = 12;
        self->animCurFrame = 0;
        break;
    case 1:
        AnimateEntity(ifrit_anim_ring, self);
        IfritMove(self);
        if (GetDistanceToPlayerX() <= SAT_X(7)) {
            self->step++;
        }
        break;
    case 2:
        prim = IfritPrims(self, PRIM_GT4, 2);
        if (prim == NULL) {
            return;
        }
        ext->prim = prim;
        IfritPillarPrim(prim, self->zPriority);
        IfritPillarPrim(prim->next, self->zPriority);
        // the lower half is the image flipped, so both halves meet at their
        // bases and read as one pillar (the user saw two stacked flames with
        // the same orientation)
        // (v2 is one past the image's last row: start the flipped half on
        // that last row, or it shows a seam of the next image's texels)
        prim->next->v0 = prim->next->v1 = prim->v2 - 1;
        prim->next->v2 = prim->next->v3 = prim->v0;
        IfritRect(prim, self->posX.i.hi, self->posY.i.hi, self->posX.i.hi,
                  self->posY.i.hi);
        IfritRect(prim->next, self->posX.i.hi, self->posY.i.hi, self->posX.i.hi,
                  self->posY.i.hi);
        ext->fxTimer = 0;
        self->step++;
        self->animCurFrame = 0;
        IfritSfx(MOD_SFX_IFRIT_FIRE_PILLAR);
        /* fallthrough */
    case 3:
        prim = ext->prim;
        prim2 = prim->next;
        t = ext->fxTimer;
        x = self->posX.i.hi;
        y = self->posY.i.hi;
        if (t <= 16) {
            w = SAT_X(t);
            prim->x0 = prim->x2 = prim2->x0 = prim2->x2 = x - w;
            prim->x1 = prim->x3 = prim2->x1 = prim2->x3 = x + w;
            self->hitboxWidth = w;
        }
        if (t <= 31) {
            prim->y0 = prim->y1 = y - t * 8;
            prim->y2 = prim->y3 = y;
            prim2->y0 = prim2->y1 = y;
            prim2->y2 = prim2->y3 = y + t * 8;
            self->hitboxHeight = t * 7;
        } else if (t == 32) {
            self->hitboxWidth = 0;
            self->hitboxHeight = 0;
            self->hitboxState = 0;
        } else {
            w = SAT_X((40 - t) * 2);
            prim->x0 = prim->x2 = prim2->x0 = prim2->x2 = x - w;
            prim->x1 = prim->x3 = prim2->x1 = prim2->x3 = x + w;
        }
        if (++ext->fxTimer > 39) {
            self->step++;
        }
        break;
    default:
        DestroyEntity(self);
        break;
    }
}

// Saturn func_060E823C: 10 embers drawn into the Ifrit's head while it
// gathers its breath, dull yellow to white; two bursts blink in turn
void EntityModIfritEmbers(Entity* self) {
    ET_ModIfrit* ext = IFRIT_EXT(self);
    Entity* parent = ext->parent;
    Primitive* prim;
    s32 dcx, dcy;
    s32 t, h, c;

    if ((parent->flags & FLAG_DEAD) || parent->entityId != E_MOD_IFRIT) {
        DestroyEntity(self);
        return;
    }
    switch (self->step) {
    case 0:
        prim = IfritPrims(self, PRIM_TILE, 10);
        if (prim == NULL) {
            return;
        }
        self->flags |= FLAG_UNK_2000 | FLAG_POS_CAMERA_LOCKED;
        ext->prim = prim;
        for (; prim != NULL; prim = prim->next) {
            if (parent->facingLeft) {
                IFRIT_DOT_X(prim) =
                    (self->posX.i.hi + SAT_X((Random() & 0xFF) >> 1) + SAT_X(8))
                    << 16;
            } else {
                IFRIT_DOT_X(prim) =
                    (self->posX.i.hi - SAT_X((Random() & 0xFF) >> 1) - SAT_X(8))
                    << 16;
            }
            IFRIT_DOT_Y(prim) =
                (self->posY.i.hi + ((Random() & 0xFF) >> 1) - 64) << 16;
            prim->type = PRIM_TILE;
            prim->u0 = prim->v0 = 1;
            prim->x0 = IFRIT_DOT_X(prim) >> 16;
            prim->y0 = IFRIT_DOT_Y(prim) >> 16;
            prim->r0 = prim->g0 = prim->b0 = 0;
            prim->drawMode = DRAW_HIDE; // drawn from the next frame
            prim->priority = parent->zPriority - 2;
        }
        self->velocityX = SAT_X(0x4000);
        ext->fxTimer = 0;
        self->step++;
        break;
    case 1:
        dcx = ext->scrollX - g_Tilemap.scrollX.val;
        dcy = ext->scrollY - g_Tilemap.scrollY.val;
        self->posX.val = parent->posX.val;
        self->posY.val = parent->posY.val - FIX(18); // its drawn height
        IfritMove(self);
        t = ext->fxTimer;
        h = t / 2;
        // Saturn RGB555 (h << 10) + (h / 2 + 16 << 5) + 16 + h / 2
        c = h / 2 + 16;
        for (prim = ext->prim; prim != NULL; prim = prim->next) {
            IFRIT_DOT_X(prim) += dcx;
            IFRIT_DOT_X(prim) += (self->posX.val - IFRIT_DOT_X(prim)) / 20;
            prim->x0 = IFRIT_DOT_X(prim) >> 16;
            IFRIT_DOT_Y(prim) += dcy;
            IFRIT_DOT_Y(prim) += (self->posY.val - IFRIT_DOT_Y(prim)) / 16;
            prim->y0 = IFRIT_DOT_Y(prim) >> 16;
            prim->v0 = t > 32 ? 2 : 1;
            prim->r0 = c * 8;
            prim->g0 = c * 8;
            prim->b0 = h * 8;
            if (self->params ? (t & 1) : !(t & 1)) {
                prim->drawMode |= DRAW_HIDE;
            } else {
                prim->drawMode = DRAW_DEFAULT;
            }
        }
        if (++ext->fxTimer > 63) {
            self->step++;
        }
        break;
    default:
        DestroyEntity(self);
        return;
    }
    ext->scrollX = g_Tilemap.scrollX.val;
    ext->scrollY = g_Tilemap.scrollY.val;
}

static void IfritPillarDraw(Entity* self) {
    ET_ModIfritPillar* ext = PILLAR_EXT(self);
    Primitive* prim = ext->prim;
    s32 i;

    for (i = 0; i < 2; i++, prim = prim->next) {
        IfritRect(prim, self->posX.i.hi + SAT_X(ext->xa[i]), ext->ya[i],
                  self->posX.i.hi + SAT_X(ext->xc[i]), ext->yc[i]);
        // the Saturn's mesh every other frame, in front of the Ifrit; behind
        // it on the others
        if (g_Timer & 1) {
            prim->priority = self->zPriority + 3;
            prim->drawMode |= MOD_IFRIT_MESH;
        } else {
            prim->priority = self->zPriority - 1;
            prim->drawMode &= ~MOD_IFRIT_MESH;
        }
    }
}

// Saturn func_060E854C: the death pillar, under the dying Ifrit (which
// moves it on through steps 2 and 5): a wide pillar rises 12 px a tick,
// narrows while the Ifrit stretches into it, then lifts off with a narrow
// one (upside down) and narrows away. Fixed on screen.
void EntityModIfritPillar(Entity* self) {
    ET_ModIfritPillar* ext = PILLAR_EXT(self);
    Primitive* prim;
    s32 i;

    switch (self->step) {
    case 0:
        prim = IfritPrims(self, PRIM_GT4, 2);
        if (prim == NULL) {
            return;
        }
        self->flags |= FLAG_UNK_2000 | FLAG_POS_CAMERA_LOCKED;
        ext->prim = prim;
        IfritPillarPrim(prim, self->zPriority);
        IfritPillarPrim(prim->next, self->zPriority);
        ext->xa[0] = -48;
        ext->xc[0] = 48;
        ext->xa[1] = -16;
        ext->xc[1] = 16;
        for (i = 0; i < 2; i++) {
            ext->ya[i] = ext->yc[i] = self->posY.i.hi;
        }
        ext->timer = 0;
        self->step++;
        break;
    case 1:
        if (ext->timer <= 31) {
            ext->ya[0] -= 12;
            ext->timer++;
        }
        break;
    case 2:
        ext->timer = 64;
        self->step++;
        /* fallthrough */
    case 3:
        if (ext->timer > 0) {
            if (ext->timer & 1) {
                ext->xa[0]++;
                ext->xc[0]--;
            }
            ext->timer--;
        }
        break;
    case 4:
        ext->timer = 0;
        self->step++;
        /* fallthrough */
    case 5:
        if (ext->timer <= 15) {
            ext->yc[0] -= 24;
            ext->yc[1] -= 24;
            ext->timer++;
            break;
        }
        self->step++;
        /* fallthrough */
    case 6:
        ext->ya[0] -= 24;
        ext->yc[0] -= 24;
        ext->yc[1] -= 24;
        ext->ya[1] -= 24;
        if (ext->xa[0] >= ext->xc[0]) {
            DestroyEntity(self);
            return;
        }
        ext->xa[0]++;
        ext->xc[0]--;
        ext->xa[1]++;
        ext->xc[1]--;
        break;
    }
    IfritPillarDraw(self);
}

// Saturn func_060E88A0: 32 ashes (teal 2 px lines) drifting up where the
// Ifrit vanished, blinking out after 70 ticks; all gone when the first
// reaches 120
void EntityModIfritAshes(Entity* self) {
    ET_ModIfrit* ext = IFRIT_EXT(self);
    Primitive* prim;
    s32 dcy;

    switch (self->step) {
    case 0:
        prim = IfritPrims(self, PRIM_TILE, 32);
        if (prim == NULL) {
            return;
        }
        ext->prim = prim;
        for (; prim != NULL; prim = prim->next) {
            prim->type = PRIM_TILE;
            prim->priority = self->zPriority - 1;
            // Saturn RGB555 0xD280 | (Random() & 7)
            prim->r0 = (Random() & 7) * 8;
            prim->g0 = 20 * 8;
            prim->b0 = 20 * 8;
            prim->drawMode = DRAW_DEFAULT;
            prim->x0 = self->posX.i.hi + SAT_X((Random() & 15) - 8);
            prim->y0 = self->posY.i.hi + (Random() & 63) - 32;
            prim->u0 = 1;
            prim->v0 = 2;
            IFRIT_DOT_VY(prim) = ((Random() & 7) + 1) * 0x8000;
            IFRIT_DOT_Y(prim) = prim->y0 << 16;
            IFRIT_DOT_T(prim) = Random() & 31;
        }
        self->step++;
        break;
    case 1:
        // (the Saturn follows the camera's y only)
        dcy = ext->scrollY - g_Tilemap.scrollY.val;
        for (prim = ext->prim; prim != NULL; prim = prim->next) {
            IFRIT_DOT_T(prim)++;
            if (IFRIT_DOT_T(prim) > 70) {
                if (IFRIT_DOT_T(prim) & 1) {
                    prim->drawMode |= DRAW_HIDE;
                } else {
                    prim->drawMode = DRAW_DEFAULT;
                }
            }
            IFRIT_DOT_Y(prim) += dcy;
            IFRIT_DOT_Y(prim) -= IFRIT_DOT_VY(prim);
            prim->y0 = IFRIT_DOT_Y(prim) >> 16;
            if (IFRIT_DOT_T(prim) > 120) {
                DestroyEntity(self);
                return;
            }
        }
        break;
    }
    ext->scrollY = g_Tilemap.scrollY.val;
}
