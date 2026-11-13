// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port mod: weapon ID #60, the Saturn's Alucard Spear.
// Not on the PS1: built by mods/sotn-ultimate/tools/build_saturn_weapon.sh into
// BIN/WEAPON2.BIN. Translated from the Saturn's WEAPON0.PRG slot 60 (the
// WEAPON1.PRG copy differs only in addresses and its factory's hand bits):
// - EntityWeaponAttack, the two-handed swing (record 172, player anim 104),
//   is the Morningstar's code (w_003.c; the SH-2 is the same).
// - func_ptr_80170004: the back-forward special (record 224, MP 7, DRA
//   attack kind 32: a 5 px/frame dash, step_s 0x60, the new player anims
//   247/248 = the swing with a longer windup). Blueprint 54, entity 0xE1.
//   The swing's code, alive only while the player anim is 247/248.
// - func_ptr_80170008: the QCF special (record 225, MP 7, attack kind 33:
//   Alucard stands still holding the thrust for 40 frames, step_s 0x61).
//   Blueprint 60, entity 0xE2. The swing's code; at player pose 3 it spawns
//   blueprint 127 (entity 0xE3) once:
// - func_ptr_8017000C: sparks at the spear's tip while Alucard holds pose 4
//   (sprite frames 65-68, SFX 0x689 each time it moves).
// The weapon's data (frames, hitboxes, WeaponAnimation) and art are
// converted by mods/sotn-ultimate/tools/port_saturn_weapon.py (the Claymore
// family's frame layout, w_051, plus the sparks and the dash's after-images).
#define FUNC_04_VOID
#include "../../../../../src/weapon/weapon_private.h"
#include "sfx.h"
#include "mod/w_060_data.h"
#define g_Animset w_060_1

static s32 g_HandId = HAND_ID;

#include "../../../../../src/weapon/shared.h"

// the sparks (Saturn 0x060D6A80): sprite frames 65-68, looping
static AnimationFrame anim_spark[] = {
    POSE(3, 0x41, 0), POSE(3, 0x42, 0), POSE(3, 0x43, 0), POSE(3, 0x44, 0),
    POSE_LOOP(0)};

// The attack entities' shared setup (Saturn: CreateSpriteObject, then the
// same stores as w_003's step 0)
static void SpearInit(Entity* self, WeaponAnimation* anim) {
    SetSpriteBank1(g_Animset);
    self->animSet = ANIMSET_OVL(16);
    self->palette = PAL_UNK_110;
    self->unk5A = 0x64;
    if (g_HandId != 0) {
        self->animSet += 2;
        self->palette += 0x18;
        self->unk5A += 2;
    }
    self->palette += anim->palette;
    self->flags = FLAG_UNK_20000 | FLAG_POS_PLAYER_LOCKED;
    self->zPriority = PLAYER.zPriority - 2;
    self->blendMode = BLEND_TRANSP | BLEND_ADD;
    SetWeaponProperties(self, 0);
    self->step++;
}

static void SpearFollowPlayer(Entity* self) {
    self->drawFlags = PLAYER.drawFlags;
    self->scaleY = PLAYER.scaleY;
    self->rotPivotY = PLAYER.rotPivotY;
}

// Saturn 0x060D6128, the Morningstar's code
static void EntityWeaponAttack(Entity* self) {
    WeaponAnimation* anim;
    s8 animIndex;

    self->posX.val = PLAYER.posX.val;
    self->posY.val = PLAYER.posY.val;
    self->facingLeft = PLAYER.facingLeft;
    animIndex = (self->params >> 8) & 0x7f;
    anim = &w_060_anims_0[animIndex];

    if (PLAYER.ext.weapon.anim < anim->frameStart ||
        PLAYER.ext.weapon.anim >= (anim->frameStart + 7) ||
        g_Player.unk46 == 0) {
        DestroyEntity(self);
        return;
    }

    if (self->step == 0) {
        SpearInit(self, anim);
    }

    self->ext.weapon.anim = PLAYER.ext.weapon.anim - anim->frameStart;

    if ((PLAYER.poseTimer == 1) && (PLAYER.pose == anim->soundFrame)) {
        g_api.PlaySfx(anim->soundId);
    }

    if (g_api.UpdateUnarmedAnim(anim->frameProps, anim->frames) < 0) {
        DestroyEntity(self);
        return;
    }
    SpearFollowPlayer(self);
}

// Saturn 0x060D62C8: the back-forward special. The swing's code, but alive
// only while the player anim is the special's (frameStart or + 1, the ledge
// variant) and without the g_Player.unk46 check
static void func_ptr_80170004(Entity* self) {
    WeaponAnimation* anim;
    s8 animIndex;

    self->posX.val = PLAYER.posX.val;
    self->posY.val = PLAYER.posY.val;
    self->facingLeft = PLAYER.facingLeft;
    animIndex = (self->params >> 8) & 0x7f;
    anim = &w_060_anims_0[animIndex];

    if (PLAYER.ext.weapon.anim != anim->frameStart &&
        PLAYER.ext.weapon.anim != anim->frameStart + 1) {
        DestroyEntity(self);
        return;
    }

    if (self->step == 0) {
        SpearInit(self, anim);
    }

    self->ext.weapon.anim = PLAYER.ext.weapon.anim - anim->frameStart;

    if ((PLAYER.poseTimer == 1) && (PLAYER.pose == anim->soundFrame)) {
        g_api.PlaySfx(anim->soundId);
    }

    if (g_api.UpdateUnarmedAnim(anim->frameProps, anim->frames) < 0) {
        DestroyEntity(self);
        return;
    }
    SpearFollowPlayer(self);
}

// Saturn 0x060D6464: the QCF special. The swing's code; at player pose 3 it
// spawns the sparks once (Saturn factory 0x107F hand 0 / 0x207F hand 1 =
// blueprint 127; step_s counts it)
static void func_ptr_80170008(Entity* self) {
    WeaponAnimation* anim;
    s8 animIndex;

    self->posX.val = PLAYER.posX.val;
    self->posY.val = PLAYER.posY.val;
    self->facingLeft = PLAYER.facingLeft;
    animIndex = (self->params >> 8) & 0x7f;
    anim = &w_060_anims_0[animIndex];

    if (PLAYER.ext.weapon.anim < anim->frameStart ||
        PLAYER.ext.weapon.anim >= (anim->frameStart + 7) ||
        g_Player.unk46 == 0) {
        DestroyEntity(self);
        return;
    }

    if (self->step == 0) {
        SpearInit(self, anim);
    }

    self->ext.weapon.anim = PLAYER.ext.weapon.anim - anim->frameStart;

    if ((PLAYER.poseTimer == 1) && (PLAYER.pose == anim->soundFrame)) {
        g_api.PlaySfx(anim->soundId);
    }

    if (self->step_s == 0 && PLAYER.pose == 3) {
        g_api.CreateEntFactoryFromEntity(self, WFACTORY(127, 0), 0);
        self->step_s++;
    }

    if (g_api.UpdateUnarmedAnim(anim->frameProps, anim->frames) < 0) {
        DestroyEntity(self);
        return;
    }
    SpearFollowPlayer(self);
}

// Saturn 0x060D6630: the sparks. Alive while Alucard holds pose 4 of the
// two-handed swing (player anim 104/105, the Saturn's 0x60/0x61), which the
// QCF special's step_s 0x61 holds for 40 frames. Each new spark frame jumps
// to a random spot within 8 px (x scaled 4/5) of Alucard's position and
// plays SFX 0x689.
static void func_ptr_8017000C(Entity* self) {
    if ((PLAYER.ext.weapon.anim != 0x68 && PLAYER.ext.weapon.anim != 0x69) ||
        PLAYER.pose != 4) {
        DestroyEntity(self);
        return;
    }

    if (self->step == 0) {
        SetSpriteBank1(g_Animset);
        self->animSet = ANIMSET_OVL(16);
        self->palette = PAL_UNK_110;
        self->unk5A = 0x64;
        if (g_HandId != 0) {
            self->animSet += 2;
            self->palette += 0x18;
            self->unk5A += 2;
        }
        self->palette += w_060_anims_0[2].palette;
        self->blendMode = BLEND_TRANSP | BLEND_ADD;
        self->zPriority = PLAYER.zPriority + 1;
        self->anim = anim_spark;
        self->flags = FLAG_UNK_100000 | FLAG_POS_PLAYER_LOCKED | FLAG_UNK_20000;
        self->step++;
    }

    if (self->poseTimer == 1) {
        self->posX.i.hi = PLAYER.posX.i.hi - 6 + ((rand() >> 3) & 15) * 4 / 5;
        self->posY.i.hi = PLAYER.posY.i.hi - 8 + ((rand() >> 10) & 15);
        g_api.PlaySfx(0x689);
    }
}

static s32 func_ptr_80170010(Entity* self) {}

static s32 func_ptr_80170014(Entity* self) {}

static int GetWeaponId(void) { return 60; }

static void EntityWeaponShieldSpell(Entity* self) {}

static void func_ptr_80170024(Entity* self) {}

static void func_ptr_80170028(Entity* self) {}

static void WeaponUnused2C(void) {}

static void WeaponUnused30(void) {}

static void WeaponUnused34(void) {}

static void WeaponUnused38(void) {}

static void WeaponUnused3C(void) {}
