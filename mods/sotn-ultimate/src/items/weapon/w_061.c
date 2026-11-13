// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port mod: weapon ID #61, the Saturn's Woodland Shield.
// Not on the PS1: built by mods/sotn-ultimate/tools/build_saturn_weapon.sh into
// BIN/WEAPON2.BIN. Art, palettes and sprite frames are the Saturn's,
// converted by mods/sotn-ultimate/tools/port_saturn_weapon.py
// (mods/sotn-ultimate/src/items/weapon/mod/w_061_data.h). The guard is the
// Leather shield's (w_008.c): the Saturn's code for it is the same, and so are
// its AnimationFrames and hitboxes. The Shield rod spell (special record 226,
// 30 MP) is translated from the Saturn's SH-2:
//   EntityWeaponShieldSpell  the shield grows, then a vine shoots out
//                            (blueprint 88 -> func_ptr_80170024)
//   func_ptr_80170024        the homing vine: a trail of 40 leaf quads; each
//                            hit spawns blueprint 78 -> func_ptr_80170010
//   func_ptr_80170010        a thorn burst stuck to the enemy hit (up to 3
//   hits)
// Horizontal Saturn screen quantities are scaled by 4/5 (Saturn 320 wide).
#include "../../../../../src/weapon/weapon_private.h"
#include "sfx.h"
#include "mod/w_061_data.h"
#define g_Animset w_061_1

// the Saturn record 226: Shield rod + Woodland Shield (equipment_mod.h)
#define SPELL_EQUIP_ID 226

static AnimationFrame anim_00[] = {POSE(2, 0x01, 1), POSE_LOOP(0)};
static AnimationFrame anim_01[] = {POSE(2, 0x02, 2), POSE_LOOP(0)};
static AnimationFrame anim_09[] = {
    POSE(2, 0x03, 3), POSE(2, 0x04, 3), POSE(2, 0x05, 3), POSE(2, 0x06, 3),
    POSE(2, 0x07, 3), POSE(2, 0x08, 3), POSE(2, 0x09, 4), POSE(2, 0x0A, 1),
    POSE(2, 0x0B, 1), POSE(2, 0x0C, 1), POSE_LOOP(0)};
static AnimationFrame anim_08[] = {
    POSE(3, 0x0D, 1),  POSE(3, 0x0E, 7),  POSE(3, 0x0F, 8),  POSE(3, 0x10, 9),
    POSE(3, 0x11, 10), POSE(3, 0x12, 11), POSE(3, 0x13, 12), POSE(3, 0x14, 13),
    POSE(3, 0x15, 14), POSE(3, 0x16, 13), POSE(3, 0x17, 12), POSE(3, 0x18, 11),
    POSE(3, 0x19, 10), POSE(3, 0x1A, 9),  POSE(3, 0x1B, 8),  POSE(3, 0x1C, 7),
    POSE_LOOP(0)};
static AnimationFrame anim_07[] = {
    POSE(1, 0x24, 1), POSE(1, 0x25, 1), POSE(1, 0x26, 1), POSE(1, 0x27, 1),
    POSE(1, 0x28, 1), POSE(1, 0x29, 1), POSE(1, 0x2A, 1), POSE(1, 0x2B, 1),
    POSE(1, 0x2C, 1), POSE(1, 0x2D, 1), POSE(1, 0x2E, 1), POSE(1, 0x2F, 1),
    POSE(1, 0x30, 1), POSE_LOOP(0)};
static AnimationFrame anim_06[] = {
    POSE(1, 0x2A, 1), POSE(1, 0x2B, 1), POSE(1, 0x2C, 1), POSE(1, 0x2D, 1),
    POSE(1, 0x2E, 1), POSE(1, 0x2F, 1), POSE(1, 0x30, 1), POSE_LOOP(0)};
static AnimationFrame anim_05[] = {
    POSE(2, 0x21, 1), POSE(2, 0x22, 1), POSE(2, 0x23, 1), POSE(2, 0x24, 1),
    POSE(2, 0x25, 1), POSE(2, 0x26, 1), POSE(2, 0x27, 1), POSE(2, 0x28, 1),
    POSE(2, 0x29, 1), POSE(2, 0x2A, 1), POSE(2, 0x2B, 1), POSE(2, 0x2C, 1),
    POSE(2, 0x2D, 1), POSE(2, 0x2E, 1), POSE(2, 0x2F, 1), POSE(2, 0x30, 1),
    POSE_LOOP(0)};
static AnimationFrame anim_10[] = {
    POSE(2, 0x31, 1), POSE(2, 0x32, 1), POSE(2, 0x33, 1), POSE_END};
static AnimationFrame anim_11[] = {
    POSE(2, 0x34, 2), POSE(2, 0x35, 2), POSE(2, 0x36, 2), POSE_END};
static AnimationFrame anim_12[] = {
    POSE(2, 0x33, 1), POSE(2, 0x32, 1), POSE(2, 0x31, 1), POSE_END};
static AnimationFrame anim_13[] = {
    POSE(2, 0x36, 2), POSE(2, 0x35, 2), POSE(2, 0x34, 2), POSE_END};
static AnimationFrame anim_04[] = {
    POSE(4, 0x1D, 1), POSE(4, 0x1E, 1), POSE_LOOP(0)};
static AnimationFrame anim_02[] = {
    POSE(2, 0x1F, 6), POSE(2, 0x20, 6), POSE_LOOP(0)};
static AnimationFrame anim_03[] = {
    POSE(3, 0x37, 17), POSE(3, 0x38, 17), POSE(3, 0x38, 17), POSE(3, 0x38, 16),
    POSE(3, 0x38, 15), POSE(3, 0x39, 1),  POSE(3, 0x3A, 1),  POSE(3, 0x3B, 1),
    POSE(3, 0x3C, 1),  POSE(3, 0x3D, 1),  POSE(3, 0x01, 1),  POSE(3, 0x01, 1),
    POSE(3, 0x01, 1),  POSE(3, 0x01, 1),  POSE(3, 0x01, 1),  POSE_LOOP(0)};

// (the Saturn's, same as its and the PS1's Leather shield: not rescaled)
static s8 hitboxes[][4] = {
    {0, 0, 0, 0},    {14, -7, 4, 12},  {14, 13, 4, 12}, {-11, -7, 4, 12},
    {1, -5, 4, 12},  {-32, -31, 0, 0}, {22, -6, 4, 12}, {15, -6, 4, 12},
    {16, -5, 4, 12}, {17, -4, 4, 12},  {18, -3, 4, 12}, {19, -2, 4, 12},
    {20, -1, 4, 12}, {21, 0, 4, 12},   {22, 1, 4, 12},  {12, -5, 4, 12},
    {10, -3, 4, 12}, {8, -1, 4, 12}};

static AnimationFrame* anims[] = {
    anim_00, anim_01, anim_02, anim_03, anim_04, anim_05, anim_06,
    anim_07, anim_08, anim_09, anim_10, anim_11, anim_12, anim_13};

// the vine's leaf segments: Saturn images 10-15 (12x16 here), hand 0's v
static u8 vineUV[][2] = {
    {W_061_IMG10_U, W_061_IMG10_V}, {W_061_IMG11_U, W_061_IMG11_V},
    {W_061_IMG12_U, W_061_IMG12_V}, {W_061_IMG13_U, W_061_IMG13_V},
    {W_061_IMG14_U, W_061_IMG14_V}, {W_061_IMG15_U, W_061_IMG15_V}};

// func_ptr_80170024's ext (the Saturn's 0x78 / 0x7C)
typedef struct {
    /* 0x7C */ Entity* target;
    /* 0x80 */ Primitive* prim; // the trail's head
} ET_Vine;

// func_ptr_80170010's ext (the Saturn's 0x78 / 0x7A)
typedef struct {
    /* 0x7C */ s16 size;
    /* 0x7E */ s16 hits;
} ET_Thorn;

static s32 g_HandId = HAND_ID;

#include "../../../../../src/weapon/shared.h"

// textures a GT4 with a 4bpp image of this hand's half of the art page
static void SetPrimImage(Primitive* prim, s32 u, s32 v, s32 w, s32 h) {
    v += g_HandId << 7;
    prim->tpage = 0x19;
    prim->clut = PAL_UNK_111 + g_HandId * N_WEAPON_PAL; // palette 1
    prim->u0 = prim->u2 = u;
    prim->u1 = prim->u3 = u + w - 1;
    prim->v0 = prim->v1 = v;
    prim->v2 = prim->v3 = v + h - 1;
}

static void EntityWeaponAttack(Entity* self) {
    s32 anim1;
    s32 anim2;
    s32 attackButton;

    anim1 = 0;
    anim2 = 0;
    if (g_HandId) {
        attackButton = PAD_CIRCLE;
    } else {
        attackButton = PAD_SQUARE;
    }
    if (!(attackButton & g_Player.padPressed) && (self->step < 3)) {
        self->poseTimer = 0;
        self->pose = 0;
        self->step = 3;
    }
    if (self->step != 4) {
        self->posX.val = PLAYER.posX.val;
        self->posY.val = PLAYER.posY.val;
        self->facingLeft = PLAYER.facingLeft;
    }
    if ((g_Player.status & PLAYER_STATUS_UNK10000) && (self->step != 4)) {
        self->zPriority = PLAYER.zPriority + 2;
        self->step = 4;
        if (g_Player.vram_flag & TOUCHING_GROUND) {
            self->velocityX = PLAYER.velocityX;
        } else {
            self->velocityX = PLAYER.velocityX * 2;
        }
        self->velocityY = FIX(-3.5);
        self->ext.timer.t = 0x80;
        self->flags = FLAG_POS_CAMERA_LOCKED;
        self->animCurFrame = 0x3E;
    }
    if ((PLAYER.step == Player_Crouch) && (PLAYER.step_s != 2)) {
        anim2++;
    }
    switch (self->step) {
    case 0:
        SetSpriteBank1(g_Animset);
        if (g_HandId != 0) {
            self->animSet = ANIMSET_OVL(0x12);
            self->palette = PAL_UNK_128;
            self->unk5A = 0x66;
        } else {
            self->animSet = ANIMSET_OVL(0x10);
            self->palette = PAL_UNK_110;
            self->unk5A = 0x64;
        }
        self->flags = FLAG_POS_PLAYER_LOCKED | FLAG_UNK_20000;
        self->zPriority = PLAYER.zPriority - 2;
        g_Player.unk48 = 1;
        SetWeaponProperties(self, 0);
        self->step++;
        /* fallthrough */
    case 1:
        self->ext.shield.anim = anim2 + 10;
        if (self->poseTimer < 0) {
            self->step++;
        }
        break;
    case 2:
        switch (PLAYER.ext.player.anim) {
        case 9:
        case 10:
        case 11:
            anim1++;
            /* fallthrough */
        case 7:
            anim1++;
            /* fallthrough */
        case 8:
            anim1++;
            /* fallthrough */
        case 12:
            anim1++;
            /* fallthrough */
        case 13:
            anim1++;
            /* fallthrough */
        case 24:
        case 25:
            anim1++;
            /* fallthrough */
        case 14:
        case 15:
            anim1++;
            /* fallthrough */
        case 26:
            anim1 += 2;
            self->pose = PLAYER.pose;
            break;
        default:
            self->pose = 0;
            anim1 += anim2;
            break;
        }
        self->ext.shield.anim = anim1;
        self->poseTimer = 2;
        break;
    case 3:
        g_Player.unk48 = 0;
        self->ext.shield.anim = anim2 + 12;
        if (self->poseTimer < 0) {
            DestroyEntity(self);
            return;
        }
        break;
    case 4:
        self->hitboxState = 0;
        g_Player.unk48 = 0;
        self->drawFlags |= ENTITY_ROTATE;
        self->posY.val += self->velocityY;
        self->posX.val += self->velocityX;
        self->velocityY += FIX(20.0 / 128);
        self->rotate += ROT(11.25);
        if (--self->ext.timer.t < 0x10) {
            self->drawFlags |= ENTITY_BLINK;
        }
        if (--self->ext.timer.t == 0) {
            DestroyEntity(self);
            return;
        }
        break;
    }
    if (self->step != 4) {
        g_api.PlayAnimation(&hitboxes[0][0], anims);
    }
    self->drawFlags = PLAYER.drawFlags;
    self->scaleY = PLAYER.scaleY;
    self->rotPivotY = PLAYER.rotPivotY;
}

s32 func_ptr_80170004(Entity* self) {}

static void func_ptr_80170008(Entity* self) {}

static void func_ptr_8017000C(Entity* self) {}

// Saturn 060D64E0: the thorn burst (blueprint 78, from the vine's hits). It
// sticks to the enemy it hit and hits it again, up to 3 times, for half the
// spell's attack; it pulses for 160 frames, then shrinks away.
static s32 func_ptr_80170010(Entity* self) {
    ET_Thorn* ext;
    Entity* target;
    Primitive* prim;
    s32 t;
    s16 hw;

    ext = (ET_Thorn*)&self->ext;
    switch (self->step) {
    case 0:
        self->primIndex = g_api.AllocPrimitives(PRIM_GT4, 1);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        prim = &g_PrimBuf[self->primIndex];
        prim->priority = 0xC1;
        SetPrimImage(
            prim, W_061_IMG3_U, W_061_IMG3_V, W_061_IMG3_W, W_061_IMG3_H);
        prim->drawMode = DRAW_UNK02;
        self->flags = FLAG_POS_CAMERA_LOCKED | FLAG_HAS_PRIMS;
        self->ext.weapon.equipId = SPELL_EQUIP_ID;
        SetWeaponProperties(self, 0);
        self->attack >>= 1;
        self->hitboxWidth = self->hitboxHeight = 6;
        ext->hits = 0;
        self->step++;
        /* fallthrough */
    case 1:
        target = self->unkB8;
        if (ext->hits) {
            self->posX.val = target->posX.val;
            self->posY.val = target->posY.val;
        }
        if (self->hitFlags) {
            self->hitboxWidth = self->hitboxHeight = 2;
            if (ext->hits > 2 || target->hitPoints == 0x7FFF ||
                (target->flags & FLAG_DEAD)) {
                self->step++;
            } else {
                ext->hits++;
            }
            self->hitFlags = 0;
        } else {
            t = g_Timer >> 3;
            if (t & 4) {
                ext->size = 23 - (t & 7);
            } else {
                ext->size = 19 + (t & 7);
            }
            if (++self->step_s > 0xA0) {
                self->step++;
                self->step_s = 0;
            }
        }
        break;
    default:
        if (--ext->size <= 0) {
            DestroyEntity(self);
            return;
        }
        break;
    }
    prim = &g_PrimBuf[self->primIndex];
    hw = ext->size * 4 / 5;
    prim->x0 = prim->x2 = self->posX.i.hi - hw;
    prim->x1 = prim->x3 = self->posX.i.hi + hw;
    prim->y0 = prim->y1 = self->posY.i.hi - ext->size;
    prim->y2 = prim->y3 = self->posY.i.hi + ext->size;
}

static s32 func_ptr_80170014(Entity* self) {}

// the Saturn's returns 8 (its code is the Leather shield's); unused by DRA
static int GetWeaponId(void) { return 61; }

// Saturn 060D6764
static void EntityWeaponShieldSpell(Entity* self) {
    Primitive* prim;
    s16 x;
    s16 y;

    switch (self->step) {
    case 0:
        self->primIndex = g_api.AllocPrimitives(PRIM_GT4, 1);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        SetSpriteBank1(g_Animset);
        if (g_HandId != 0) {
            self->animSet = ANIMSET_OVL(0x12);
            self->palette = PAL_UNK_128;
            self->unk5A = 0x66;
            self->ext.shield.unk7C = 0;
        } else {
            self->animSet = ANIMSET_OVL(0x10);
            self->palette = PAL_UNK_110;
            self->unk5A = 0x64;
            self->ext.shield.unk7C = 0x80;
        }
        self->posY.i.hi -= 8;
        self->flags = FLAG_POS_CAMERA_LOCKED | FLAG_KEEP_ALIVE_OFFCAMERA |
                      FLAG_HAS_PRIMS | FLAG_UNK_10000;
        self->zPriority = PLAYER.zPriority - 2;
        self->facingLeft = PLAYER.facingLeft;
        self->animCurFrame = 0x3E;
        self->drawFlags = ENTITY_SCALEX | ENTITY_SCALEY;
        self->scaleX = self->scaleY = 0;
        // the glow, as every PS1 shield spell draws it
        prim = &g_PrimBuf[self->primIndex];
        prim->clut = PAL_UNK_19F;
        prim->tpage = 0x19;
        prim->u0 = prim->u2 = 0x80;
        prim->u1 = prim->u3 = 0x80 + 0x4F;
        prim->v0 = prim->v1 = self->ext.shield.unk7C + 0;
        prim->v2 = prim->v3 = self->ext.shield.unk7C + 0x4F;
        self->ext.shield.unk82 = 0;
        prim->r0 = prim->g0 = prim->b0 = prim->r1 = prim->g1 = prim->b1 =
            prim->r2 = prim->g2 = prim->b2 = prim->r3 = prim->g3 = prim->b3 =
                0x80;
        prim->priority = self->zPriority - 4;
        prim->drawMode =
            DRAW_UNK_40 | DRAW_TPAGE2 | DRAW_TPAGE | DRAW_COLORS | DRAW_TRANSP;
        SetSpeedX(FIX(-2));
        self->velocityY = 0;
        DestroyEntityWeapon(1);
        g_api.PlaySfx(SFX_ANIME_SWORD_C);
        g_unkGraphicsStruct.unk28 = 1;
        self->step++;
        break;
    case 1:
        DecelerateX(FIX(1.0 / 16));
        self->velocityY -= FIX(20.0 / 128);
        self->posX.val += self->velocityX;
        self->posY.val += self->velocityY;
        self->scaleX += 12;
        self->scaleY = self->scaleX;
        self->ext.shield.unk82 = self->scaleX * 40 / 256;
        if (self->scaleX >= 0x100) {
            self->ext.shield.unk82 = 40;
            self->scaleY = self->scaleX = 0x100;
            self->ext.shield.unk80 = 8;
            // the vine (func_ptr_80170024)
            g_api.CreateEntFactoryFromEntity(
                self, ((g_HandId + 1) << 14) + FACTORY(88, 0), 0);
            g_unkGraphicsStruct.unk28 = 0;
            self->step++;
        }
        break;
    case 2:
        if (--self->ext.shield.unk80 == 0) {
            self->ext.shield.unk80 = 0x10;
            self->step++;
        }
        break;
    case 3:
        prim = &g_PrimBuf[self->primIndex];
        if (!(self->ext.shield.unk80 & 1)) {
            prim->priority = 0x1BC;
        } else {
            prim->priority = PLAYER.zPriority - 4;
        }
        self->scaleX -= 0x10;
        if (self->scaleX <= 0) {
            self->scaleX = 0;
        }
        self->scaleY = self->scaleX;
        if (--self->ext.shield.unk80 == 0) {
            self->animCurFrame = 0;
            self->ext.shield.unk80 = 0x20;
            g_api.PlaySfx(SFX_TRANSFORM_3X);
            self->step++;
        }
        break;
    case 4:
        self->ext.shield.unk82 += 8;
        if (--self->ext.shield.unk80 == 0) {
            self->step++;
        }
        break;
    default:
        DestroyEntity(self);
        return;
    }
    // (the Saturn widens the glow by 5/4 for its 320-wide screen)
    x = self->posX.i.hi;
    y = self->posY.i.hi;
    prim = &g_PrimBuf[self->primIndex];
    if (prim->b3 > 8) {
        if (g_Timer & 1) {
            prim->drawMode |= DRAW_HIDE;
        } else {
            prim->drawMode &= ~DRAW_HIDE;
        }
    }
    prim->x0 = prim->x2 = x - self->ext.shield.unk82;
    prim->x1 = prim->x3 = x + self->ext.shield.unk82 - 1;
    prim->y0 = prim->y1 = y - self->ext.shield.unk82;
    prim->y2 = prim->y3 = y + self->ext.shield.unk82 - 1;
}

// Saturn 060D6B74: the vine (blueprint 88, from the spell). Heads for an
// enemy (turning 0x70 a frame) at 16 pixels a frame, laying a leaf quad
// each frame (40 at most); off screen or out of quads, its tail is erased a
// quad a frame. Every hit spawns a thorn burst on the enemy.
static void func_ptr_80170024(Entity* self) {
    ET_Vine* ext;
    Primitive* prim;
    Primitive* next;
    Entity* target;
    s32 i;
    s32 savedX;
    s32 savedY;
    s16 angle;

    ext = (ET_Vine*)&self->ext;
    switch (self->step) {
    case 0:
        self->primIndex = g_api.AllocPrimitives(PRIM_GT4, 40);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->flags = FLAG_POS_CAMERA_LOCKED | FLAG_HAS_PRIMS;
        prim = &g_PrimBuf[self->primIndex];
        ext->prim = prim;
        for (i = 0; prim != NULL; prim = prim->next) {
            prim->x0 = prim->x1 = prim->x2 = prim->x3 = self->posX.i.hi;
            prim->y0 = prim->y1 = prim->y2 = prim->y3 = self->posY.i.hi;
            prim->priority = PLAYER.zPriority + 4;
            SetPrimImage(
                prim, vineUV[i][0], vineUV[i][1], W_061_IMG10_W, W_061_IMG10_H);
            prim->drawMode = DRAW_HIDE | DRAW_UNK02;
            if (++i > 5) {
                i = 0;
            }
        }
        self->ext.weapon.equipId = SPELL_EQUIP_ID;
        SetWeaponProperties(self, 0);
        self->hitboxWidth = self->hitboxHeight = 6;
        ext->target = g_api.func_80118970();
        angle = g_api.func_80118B18(self, ext->target, self->facingLeft);
        if (angle >= 0) {
            self->rotate = angle - 0x200 + (rand() & 0x3FF);
        }
        self->step++;
        break;
    case 1:
        angle = g_api.func_80118B18(self, ext->target, self->facingLeft);
        if (angle >= 0) {
            if (self->rotate >= angle) {
                self->rotate -= 0x70;
            } else {
                self->rotate += 0x70;
            }
        }
        prim = ext->prim;
        next = prim->next;
        ext->prim = next;
        // (the Saturn's x bound is 0x180, for its 320-wide screen)
        if ((u16)(self->posX.i.hi + 32) > 0x140 ||
            (u16)(self->posY.i.hi + 32) > 0x130 || next == NULL) {
            self->step++;
            break;
        }
        next->drawMode &= ~DRAW_HIDE;
        // the new quad starts where the last one ended (the Saturn's
        // distorted sprite is A B C D clockwise; a GT4 is 0 1 2 3 = A B D C)
        next->x0 = prim->x2;
        next->y0 = prim->y2;
        next->x1 = prim->x3;
        next->y1 = prim->y3;
        self->posX.val += rcos(self->rotate) * 0x400 / 5;
        self->posY.i.hi -= (rsin(self->rotate) * 16) >> 12;
        angle = self->rotate - 0x400;
        next->x2 = self->posX.i.hi + ((rcos(angle) * 32 / 5) >> 12);
        next->y2 = self->posY.i.hi - ((rsin(angle) * 8) >> 12);
        angle = self->rotate + 0x400;
        next->x3 = self->posX.i.hi + ((rcos(angle) * 32 / 5) >> 12);
        next->y3 = self->posY.i.hi - ((rsin(angle) * 8) >> 12);
        break;
    case 2:
        prim = &g_PrimBuf[self->primIndex];
        prim->drawMode |= DRAW_HIDE;
        ext->prim = prim->next;
        self->step++;
        break;
    case 3:
        prim = ext->prim;
        if (prim == NULL || (prim->drawMode & DRAW_HIDE)) {
            DestroyEntity(self);
            return;
        }
        prim->drawMode |= DRAW_HIDE;
        ext->prim = prim->next;
        break;
    default:
        DestroyEntity(self);
        return;
    }
    if (self->hitFlags) {
        target = self->unkB8;
        if (target->hitPoints != 0x7FFF && !(target->flags & FLAG_DEAD)) {
            savedX = self->posX.val;
            savedY = self->posY.val;
            self->posX.i.hi = (rand() & 15) - 7 + target->posX.i.hi;
            self->posY.i.hi = (rand() & 15) - 7 + target->posY.i.hi;
            // a thorn burst (func_ptr_80170010)
            g_api.CreateEntFactoryFromEntity(
                self, ((g_HandId + 1) << 14) + FACTORY(78, 0), 0);
            self->posX.val = savedX;
            self->posY.val = savedY;
        }
        self->hitFlags = 0;
    }
}

static void func_ptr_80170028(Entity* self) {}

static void WeaponUnused2C(void) {}

static void WeaponUnused30(void) {}

static void WeaponUnused34(void) {}

static void WeaponUnused38(void) {}

static void WeaponUnused3C(void) {}
