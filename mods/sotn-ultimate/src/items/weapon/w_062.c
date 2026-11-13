// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port mod: weapon ID #62, the Saturn's Astral Dagger.
// Not on the PS1: built by mods/sotn-ultimate/tools/build_saturn_weapon.sh into
// BIN/WEAPON2.BIN. Translated from the Saturn's WEAPON0.PRG slot 62:
// - EntityWeaponAttack, the stab, is the Combat knife's (w_032.c; the SH-2 is
//   the same code). Its data (frames, hitboxes, WeaponAnimation) and art are
//   converted by mods/sotn-ultimate/tools/port_saturn_weapon.py (the Combat
//   knife's frame layout, then 14 frames for the special's raised dagger).
// - The special (QCF + attack, item record 227, MP 4): DRA spawns
//   func_ptr_80170024 (blueprint 90, entity 0xE9), which shows the dagger
//   raised over Alucard's head and then spawns func_ptr_8017000C (blueprint
//   68, entity 0xE3): a dagger drawn as a rotating primitive that flies at
//   7 px/frame and homes on an enemy.
// The Saturn's screen is 320 pixels wide: its horizontal distances and
// speeds are 5/4 of the PS1's, so they are scaled by 4/5 here (the
// primitive's x radii, the homing distance, the dagger's hitbox width; the
// Saturn's own velocity * 5 / 4 is dropped).
#include "../../../../../src/weapon/weapon_private.h"
#include "sfx.h"
#include "mod/w_062_data.h"
#define g_Animset w_062_1

static s32 g_HandId = HAND_ID;

#include "../../../../../src/weapon/shared.h"

// the special's raised dagger (Saturn 0x060D6AA0): sprite frames 55-68
static AnimationFrame anim_raise[] = {
    POSE(3, 0x37, 0), POSE(3, 0x38, 0), POSE(3, 0x39, 0), POSE(3, 0x3A, 0),
    POSE(3, 0x3B, 0), POSE(4, 0x3C, 0), POSE(4, 0x3D, 0), POSE(4, 0x3E, 0),
    POSE(5, 0x3F, 0), POSE(5, 0x40, 0), POSE(5, 0x41, 0), POSE(7, 0x42, 0),
    POSE(7, 0x43, 0), POSE(8, 0x44, 0), POSE(9, 0x37, 0), POSE_END};

// Saturn 0x060D6128, the Combat knife's code
static void EntityWeaponAttack(Entity* self) {
    WeaponAnimation* anim;
    s16 animIndex;

    self->posX.val = PLAYER.posX.val;
    self->posY.val = PLAYER.posY.val;
    self->facingLeft = PLAYER.facingLeft;
    animIndex = (self->params >> 8) & 0x7F;
    anim = &w_062_anims_0[animIndex];

    if ((PLAYER.ext.player.anim < anim->frameStart ||
         PLAYER.ext.player.anim >= (anim->frameStart + 7) ||
         g_Player.unk46 == 0) &&
        self->step != 2) {
        self->step = 2;
        self->ext.timer.t = 4;
        self->animCurFrame = 0;
        self->hitboxState = 0;
    }
    if (self->step == 0) {
        SetSpriteBank1(g_Animset);
        self->animSet = ANIMSET_OVL(0x10);
        self->palette = PAL_UNK_110;
        self->unk5A = 0x64;
        if (g_HandId) {
            self->palette += 0x18;
            self->animSet += 2;
            self->unk5A = 0x66;
        }
        self->palette += anim->palette;
        self->flags = FLAG_POS_PLAYER_LOCKED | FLAG_UNK_20000;
        if (self->params & 0x7F00) {
            self->zPriority = PLAYER.zPriority - 2;
        } else {
            self->zPriority = PLAYER.zPriority + 2;
        }
        SetWeaponProperties(self, 0);
        self->step++;
    }
    if (self->step == 2) {
        self->ext.weapon.lifetime--;
        if (!self->ext.weapon.lifetime) {
            DestroyEntity(self);
        }
    } else {
        self->ext.weapon.anim = PLAYER.ext.player.anim - anim->frameStart;
        if ((PLAYER.poseTimer == 1) && (PLAYER.pose == anim->soundFrame)) {
            g_api.PlaySfx(anim->soundId);
            // (the Saturn's voice 0x709 is the PS1's 0x6EF)
            if (anim->soundId == SFX_WEAPON_SWISH_B) {
                g_api.PlaySfx(SFX_VO_ALU_ATTACK_B);
            }
        }
        if (g_api.UpdateUnarmedAnim(anim->frameProps, anim->frames) < 0) {
            self->step = 2;
            self->ext.weapon.lifetime = 4;
            self->animCurFrame = 0;
            self->hitboxState = 0;
        }
        if (self->ext.weapon.anim < 4 && PLAYER.pose == 6) {
            self->hitboxState = 4;
        }
        self->drawFlags = PLAYER.drawFlags;
        self->scaleY = PLAYER.scaleY;
        self->rotPivotY = PLAYER.rotPivotY;
    }
}

s32 func_ptr_80170004(Entity* self) {}

static void func_ptr_80170008(Entity* self) {}

// Saturn 0x060D63D0: the special's homing dagger (blueprint 68). A GT4
// textured with the art's image 3 (the dagger pointing right), turned to
// self->rotate around the entity's position; it flies along that angle at
// 7 px/frame. Until it hits something or comes within 11 x 11 pixels of its
// target it turns straight to the target (func_80118B18 gives the angle; a
// negative result, the target gone or the dagger off screen, keeps the
// angle), then flies straight on until DRA destroys it off camera.
static void func_ptr_8017000C(Entity* self) {
    Primitive* prim;
    Entity* target;
    s16 angle;
    s32 dx;
    s32 dy;
    s16 posX;
    s16 posY;
    u8 v;

    switch (self->step) {
    case 0:
        self->primIndex = g_api.AllocPrimitives(PRIM_GT4, 1);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        v = 0;
        prim = &g_PrimBuf[self->primIndex];
        prim->tpage = 0x19;
        prim->clut = PAL_UNK_110;
        if (g_HandId) {
            v = 0x80;
            prim->clut += 0x18;
        }
        prim->u0 = prim->u2 = W_062_IMG3_U;
        prim->u1 = prim->u3 = W_062_IMG3_U + W_062_IMG3_W - 1;
        prim->v0 = prim->v1 = W_062_IMG3_V + v;
        prim->v2 = prim->v3 = W_062_IMG3_V + W_062_IMG3_H - 1 + v;
        prim->r0 = prim->g0 = prim->b0 = prim->r1 = prim->g1 = prim->b1 =
            prim->r2 = prim->g2 = prim->b2 = prim->r3 = prim->g3 = prim->b3 =
                0x80;
        prim->priority = PLAYER.zPriority + 2;
        prim->drawMode = DRAW_DEFAULT;
        self->posY.i.hi = PLAYER.posY.i.hi - 45;
        self->ext.weapon.unk82 = self->facingLeft;
        self->facingLeft = 0;
        self->ext.weapon.equipId = 227;
        SetWeaponProperties(self, 0);
        self->hitboxWidth = 4; // Saturn 6
        self->hitboxHeight = 6;
        self->flags = FLAG_POS_CAMERA_LOCKED | FLAG_HAS_PRIMS;
        self->ext.weapon.some_ent = g_api.func_80118970();
        g_api.PlaySfx(SFX_WEAPON_SWISH_B);
        self->step++;
        // fall through
    case 1:
        if (self->hitFlags) {
            self->step++;
            self->hitFlags = 0;
            break;
        }
        target = self->ext.weapon.some_ent;
        if (target != NULL) {
            dx = target->posX.i.hi - self->posX.i.hi;
            if (dx < 0) {
                dx = -dx;
            }
            dy = target->posY.i.hi - self->posY.i.hi;
            if (dy < 0) {
                dy = -dy;
            }
            // Saturn 14 x 11
            if (dx <= 11 && dy <= 11) {
                self->step++;
                break;
            }
        }
        angle = g_api.func_80118B18(self, target, self->ext.weapon.unk82);
        if (angle >= 0) {
            self->rotate = angle;
        }
        break;
    case 2:
        break;
    default:
        DestroyEntity(self);
        return;
    }

    self->velocityX = (rcos(self->rotate) * 0x70000) >> 12;
    self->posX.val += self->velocityX;
    self->velocityY = -((rsin(self->rotate) * 0x70000) >> 12);
    self->posY.val += self->velocityY;
    self->hitboxOffX = (self->velocityX >> 16) * 2;
    self->hitboxOffY = (self->velocityY >> 16) * 2;

    // the Saturn's distorted sprite corners A B C D (texture top-left,
    // top-right, bottom-right, bottom-left) are the GT4's 0 1 3 2; radii
    // 50, 115, 115, 45 >> 14 (x: 40, 92, 92, 36)
    prim = &g_PrimBuf[self->primIndex];
    posX = self->posX.i.hi;
    posY = self->posY.i.hi;
    prim->x0 = posX + ((rcos(self->rotate + 0x6EF) * 40) >> 14);
    prim->y0 = posY - ((rsin(self->rotate + 0x6EF) * 50) >> 14);
    prim->x1 = posX + ((rcos(self->rotate + 0x70) * 92) >> 14);
    prim->y1 = posY - ((rsin(self->rotate + 0x70) * 115) >> 14);
    prim->x3 = posX + ((rcos(self->rotate - 0x55) * 92) >> 14);
    prim->y3 = posY - ((rsin(self->rotate - 0x55) * 115) >> 14);
    prim->x2 = posX + ((rcos(self->rotate + 0x8D2) * 36) >> 14);
    prim->y2 = posY - ((rsin(self->rotate + 0x8D2) * 45) >> 14);
}

static s32 func_ptr_80170010(Entity* self) {}

static s32 func_ptr_80170014(Entity* self) {}

// (the Saturn's returns 32, the Combat knife's)
static int GetWeaponId(void) { return 62; }

static void EntityWeaponShieldSpell(Entity* self) {}

// Saturn 0x060D67BC: the special's start (blueprint 90). Shows the dagger
// raised over Alucard's head (sprite frames 55-68), keeps him invincible
// meanwhile and, when that animation ends, throws the homing dagger.
static void func_ptr_80170024(Entity* self) {
    switch (self->step) {
    case 0:
        SetSpriteBank1(g_Animset);
        self->animSet = ANIMSET_OVL(0x10);
        self->palette = PAL_UNK_110;
        self->unk5A = 0x64;
        if (g_HandId) {
            self->palette += 0x18;
            self->animSet += 2;
            self->unk5A = 0x66;
        }
        self->zPriority = PLAYER.zPriority + 2;
        self->flags = FLAG_UNK_100000;
        self->anim = anim_raise;
        g_api.PlaySfx(SFX_TRANSFORM);
        self->step++;
        break;
    case 1:
        if (self->poseTimer < 0) {
            g_api.CreateEntFactoryFromEntity(
                self, ((g_HandId + 1) << 14) + FACTORY(68, 0), 0);
            self->step++;
        }
        break;
    default:
        DestroyEntity(self);
        return;
    }
    g_api.func_8010E168(1, 45);
}

static void func_ptr_80170028(Entity* self) {}

static void WeaponUnused2C(void) {}

static void WeaponUnused30(void) {}

static void WeaponUnused34(void) {}

static void WeaponUnused38(void) {}

static void WeaponUnused3C(void) {}
