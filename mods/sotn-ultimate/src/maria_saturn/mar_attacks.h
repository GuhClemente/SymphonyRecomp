// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port: the Saturn Maria's attacks (MARIA.PRG), included by
// pl_blueprints.c when MOD_MARIA_SAT is defined (MARIA.BIN only).
//
//   Square        her shot (060A9990 -> blueprint 79, entity 69 060B9404),
//                 5 MP. In the air holding Down it flies diagonally down.
//   hold Square   charges (060A5AD4): Maria flashes from 100 frames; release
//                 fires level 1 (>100 frames, 10 MP: a growing fireball,
//                 060B9B88), level 2 (>200, 30 MP: an energy ball with a
//                 trail, 060B9D88) or level 3 (>300, 60 MP: a laser to the
//                 screen edge with two sparks, 060B96C0 / 060B99A8). At 370
//                 frames a guard appears in front of her (060BD150/060BD244).
//   Circle        her kick (060A9BC4 -> blueprint 80, entity 70 060AD0E8);
//                 while running, the slide kick.
//   Down + Cross  after both jumps: the dive kick (060A6B90 step_s 0x70,
//                 blueprint 81, entity 71 060AD248); bounces on a hit.
// Horizontal positions, speeds, hitbox x / width are the Saturn's at 4/5.
// Her effect art: mods/sotn-ultimate/tools/gen_mar_assets.py gen_fx (mar_fx.h).
#include "mar_fx.h"

#define MAR_PAL_SHOT 0x140 // mar_fx_pal[0] (Richter's whip palettes)
#define MAR_PAL_BALL 0x138 // + frame clut 1-3 -> 0x139-0x13B

typedef struct {
    s16 attack;
    u8 hitboxWidth;
    u8 hitboxHeight;
    s8 hitboxOffX;
    s8 hitboxOffY;
    u16 attackElement;
    u16 hitboxState;
    u8 nFramesInvincibility;
    u8 stunFrames;
    u16 hitEffect;
    u16 entityRoomIndex;
} MarAttackDef;

// Saturn MARIA.PRG combat configs (060ABFF0 applies them)
static MarAttackDef mar_def_shot = {
    15, 13, 8, 0, 0, 0x20, 2, 16, 4, 0x81, 0x20}; // 060C7A98
static MarAttackDef mar_def_charge1 = {
    30, 3, 8, 0, 0, 0x8000, 2, 16, 4, 0x81, 0x20}; // 060C7AE0
static MarAttackDef mar_def_charge2 = {
    40, 7, 9, 0, 0, 0x20, 2, 16, 4, 0x181, 8}; // 060C7AF0
static MarAttackDef mar_def_laser = {
    45, 13, 8, 0, 0, 0x20, 2, 16, 4, 0x81, 0x20}; // 060C7B00
static MarAttackDef mar_def_kick[] = {
    // 060C21AC: standing, crouching, rising, falling
    {30, 13, 5, 22, -3, 0x20, 2, 0x1A, 4, 1, 8},
    {30, 13, 6, 18, 23, 0x20, 2, 0x1A, 4, 1, 8},
    {30, 13, 5, 21, 6, 0x20, 2, 0x1A, 4, 1, 8},
    {30, 10, 5, 16, 20, 0x20, 2, 0x1A, 4, 1, 8},
};
static u8 mar_kick_poses[][2] = {{3, 8}, {3, 5}, {3, 7}, {2, 5}}; // 060C21EC
static MarAttackDef mar_def_dive = {
    30, 6, 8, 21, 26, 0x20, 2, 8, 4, 1, 8}; // 060C21FC
static MarAttackDef mar_def_charged = {
    255, 6, 16, 18, -4, 0, 4, 6, 0, 1, 1}; // 060BD150's
// MP per attack: shot, charge levels 1-3 (060C1980)
static u8 mar_mp_cost[] = {5, 10, 30, 60};
// where the shots start (060C7AA8): standing, crouching, air, air + Down;
// then where the shot waits for her arm
static s8 mar_shot_offsets[][2] = {
    {24, -11}, {24, 7}, {27, -16}, {30, 3},
    {13, -11}, {13, 7}, {13, -16}, {13, -8},
};
static AnimationFrame mar_anim_shot[] = {
    POSE(4, 1, 0), POSE(4, 2, 0), POSE(4, 3, 0), POSE(4, 4, 0), POSE_LOOP(0)};
static AnimationFrame mar_anim_shot_down[] = {
    POSE(4, 5, 0), POSE(4, 6, 0), POSE(4, 7, 0), POSE(4, 8, 0), POSE_LOOP(0)};
static AnimationFrame mar_anim_ball[] = {
    POSE(2, 9, 0),  POSE(1, 16, 0), POSE(2, 10, 0), POSE(1, 16, 0),
    POSE(2, 11, 0), POSE(1, 16, 0), POSE(2, 12, 0), POSE_LOOP(0)};

static s16 mar_charge;     // Saturn g_Player.unk3FC
static u16* mar_pal_shown; // the colours in PAL_RICHTER's VRAM now

static void MarSetAttack(Entity* e, MarAttackDef* d) {
    e->attack = d->attack;
    e->hitboxWidth = d->hitboxWidth;
    e->hitboxHeight = d->hitboxHeight;
    e->hitboxOffX = d->hitboxOffX;
    e->hitboxOffY = d->hitboxOffY;
    e->attackElement = d->attackElement;
    e->hitboxState = d->hitboxState;
    e->nFramesInvincibility = d->nFramesInvincibility;
    e->stunFrames = d->stunFrames;
    e->hitEffect = d->hitEffect;
    e->entityRoomIndex = d->entityRoomIndex;
    e->attack = g_api.func_800FD664(e->attack);
    func_8015F9F0(e);
}

// 060AAA2C
static s32 MarChargeLevel(void) {
    if (mar_charge > 300) {
        return 3;
    }
    if (mar_charge > 200) {
        return 2;
    }
    if (mar_charge > 100) {
        return 1;
    }
    return 0;
}

static void MarShowPal(u16* pal) {
    RECT rect;

    if (mar_pal_shown == pal) {
        return;
    }
    mar_pal_shown = pal;
    rect.x = (g_ClutIds[PAL_RICHTER] & 0x3F) << 4;
    rect.y = g_ClutIds[PAL_RICHTER] >> 6;
    rect.w = COLORS_PER_PAL;
    rect.h = 1;
    LoadImage(&rect, (u_long*)pal);
}

// the Saturn frame's palette bank: her lighter colours (bank 1) on the
// frames whose flag says so (the release of a shot, some special poses);
// DRA ignores a frame's palette offset for her PAL_FLAG palette
static u16* MarFramePal(void) {
    s32 f = PLAYER.animCurFrame & 0x7FFF;

    if ((u16)PLAYER.animSet == ANIMSET_OVL(0x10) && f < MAR_FRAME_COUNT &&
        mar_frame_light[f >> 3] & (1 << (f & 7))) {
        return mar_pal_light;
    }
    return mar_pal;
}

// Saturn 060A5AD4, from RicMain: charging while Square is held (the Saturn
// copies a palette into her CRAM bank and sets PLAYER.palette; here the
// colours go into PAL_RICHTER's VRAM, g_Clut keeps hers), otherwise the
// frame's own palette (MarFramePal). Also her MP regeneration (the Saturn's
// Richter/Maria HUD: 1 MP every 16 frames).
void MarCharge(s32 allowed) {
    static u8 seq[] = {0, 1, 2, 3, 4, 3, 2, 1};
    s32 lvl;
    u16* pal = MarFramePal();

    if (!(g_Status.D_80097BF8 & 1) && !(g_Timer & 0xF)) {
        g_Status.mp++;
        if (g_Status.mp > g_Status.mpMax) {
            g_Status.mp = g_Status.mpMax;
        }
    }
    if ((g_Player.padPressed & PAD_SQUARE) && allowed) {
        lvl = MarChargeLevel();
        if (lvl) {
            // Saturn: palette (5 * level + seq) of 060C16D0; seq 0 = hers
            pal = mar_charge_pal[lvl * 5 + seq[(g_Timer >> 1) & 7] - 5];
            DisableAfterImage(1, 4);
        }
        if (mar_charge == 370) {
            RicCreateEntFactoryFromEntity(g_CurrentEntity, BP_MAR_CHARGED, 0);
        }
        if (mar_charge < 400) {
            mar_charge++;
        }
    } else {
        mar_charge = 0;
    }
    MarShowPal(pal);
}

// Saturn 060AAA68's landing (RicCheckInput CHECK_GROUND): by unk46, the
// attack she lands in; a hard landing crouches her with dust
void MarLand(void) {
    switch (g_Player.unk46) {
    case 0:
    default:
        if (g_Player.unk44 & 8) {
            RicSetCrouch(3, PLAYER.velocityX);
        } else if (PLAYER.velocityY > FIX(6.875)) {
            RicSetCrouch(1, PLAYER.velocityX >> 1);
        } else {
            if (!(g_Player.padPressed & (PAD_LEFT | PAD_RIGHT))) {
                g_api.PlaySfx(SFX_STOMP_SOFT_B);
                RicSetStand(0);
            } else if ((g_Player.unk44 & 0x10) && !g_Player.unk7A) {
                RicSetRun();
            } else {
                RicSetWalk(0);
            }
            return;
        }
        g_api.PlaySfx(SFX_STOMP_SOFT_A);
        break;
    case 1:
    case 2:
        if (PLAYER.velocityY > FIX(6.875)) {
            RicSetCrouch(1, 0);
            g_api.PlaySfx(
                g_Player.unk46 == 1 ? SFX_STOMP_HARD_B : SFX_STOMP_SOFT_A);
        } else {
            RicSetStand(0);
            if (g_Player.unk46 == 2) {
                g_Player.unk46 = 0;
                return;
            }
            g_api.PlaySfx(SFX_STOMP_SOFT_B);
        }
        g_Player.unk46 = 0;
        break;
    case 3:
        if (PLAYER.velocityY > FIX(6.875) || (g_Player.unk44 & 8)) {
            g_api.PlaySfx(SFX_STOMP_SOFT_A);
            RicCreateEntFactoryFromEntity(g_CurrentEntity, BP_SKID_SMOKE, 0);
        }
        PLAYER.velocityY = 0;
        if (!(g_Player.unk44 & 8)) {
            PLAYER.velocityX = 0;
        }
        PLAYER.step = PL_S_STAND;
        g_Player.unk44 = 0;
        return;
    }
    RicCreateEntFactoryFromEntity(g_CurrentEntity, BP_SKID_SMOKE, 0);
}

static s32 MarAttackKind(void) {
    if (PLAYER.anim == D_80155588) {
        return 0;
    }
    if (PLAYER.anim == D_801555A8) {
        return 1;
    }
    if (PLAYER.anim == D_801555C8) {
        return 2;
    }
    return 3;
}

static void MarPlaceShot(Entity* self, s32 i) {
    s32 x = mar_shot_offsets[i][0];

    if (self->facingLeft) {
        x = -x;
    }
    self->posX.i.hi = PLAYER.posX.i.hi + x;
    self->posY.i.hi = PLAYER.posY.i.hi + mar_shot_offsets[i][1];
    self->posY.val += PLAYER.velocityY;
}

// Saturn 060A9990 (her RicDoAttack): Square (charged = 0) or the release of
// a charge (charged = 1)
s32 MarDoAttack(s32 charged) {
    s32 grunt;
    s32 lvl;
    s32 cost;

    grunt = rand() % 6;
    lvl = 0;
    if (charged) {
        MarShowPal(mar_pal);
        lvl = MarChargeLevel();
        if (!lvl) {
            return 0;
        }
    } else if (RicDoSubweapon() == 0) {
        if (grunt == 0) {
            g_api.PlaySfx(MAR_SFX_ATK_A);
        }
        if (grunt == 1) {
            g_api.PlaySfx(MAR_SFX_ATK_B);
        }
        if (grunt == 2) {
            g_api.PlaySfx(MAR_SFX_ATK_C);
        }
        return 1;
    }
    cost = mar_mp_cost[lvl];
    if (cost > g_Status.mp) {
        return 0;
    }
    g_Status.mp -= cost;
    if (!RicCreateEntFactoryFromEntity(
            g_CurrentEntity, lvl ? BP_MAR_CHARGE_1 - 1 + lvl : BP_MAR_SHOT,
            0)) {
        return 0;
    }
    if (grunt == 0) {
        g_api.PlaySfx(MAR_SFX_ATK_A);
    }
    if (grunt == 1) {
        g_api.PlaySfx(MAR_SFX_ATK_B);
    }
    if (grunt == 2) {
        g_api.PlaySfx(MAR_SFX_ATK_C);
    }
    switch (PLAYER.step) {
    case PL_S_CROUCH:
        if (PLAYER.step_s != 2) {
            RicSetAnimation(D_801555A8);
            g_CurrentEntity->velocityX = 0;
            break;
        }
    case PL_S_STAND:
    case PL_S_WALK:
        PLAYER.step = PL_S_STAND;
        RicSetAnimation(D_80155588);
        g_CurrentEntity->velocityX = 0;
        break;
    case PL_S_FALL:
    case PL_S_JUMP:
        PLAYER.step = PL_S_JUMP;
        if (g_Player.padPressed & PAD_DOWN) {
            RicSetAnimation(mar_anim_atk_air_down);
        } else {
            RicSetAnimation(D_801555C8);
        }
        break;
    case PL_S_RUN:
        PLAYER.step = PL_S_STAND;
        RicSetAnimation(D_80155588);
        RicCreateEntFactoryFromEntity(g_CurrentEntity, BP_SKID_SMOKE, 0);
        break;
    default:
        return 0;
    }
    PLAYER.pose = 3;
    g_Player.unk46 = 1;
    PLAYER.step_s = 0x40;
    g_Player.timers[PL_T_ATTACK] = 4;
    return 1;
}

// Saturn 060A9E40: R (here Triangle or R1), her backdash: a hop back at
// 4.375 (x 4/5) with a dust puff; RicStepStand step_s 3 ends it
void MarSetBackdash(void) {
    PLAYER.step = PL_S_STAND;
    PLAYER.step_s = 3;
    PLAYER.velocityX = PLAYER.facingLeft ? FIX(3.5) : FIX(-3.5);
    PLAYER.velocityY = 0;
    RicSetAnimation(mar_anim_backdash);
    RicCreateEntFactoryFromEntity(g_CurrentEntity, BP_SKID_SMOKE, 0);
    g_Player.unk46 = 0;
    g_api.PlaySfx(SFX_SCRAPE_B); // Saturn 0x616, the same common sound
}

// Saturn 060A9BC4 (in place of RicDoCrash): MAR_PAD_KICK
void MarDoKick(void) {
    s32 kind;

    switch (PLAYER.step) {
    case PL_S_CROUCH:
        if (PLAYER.step_s != 2) {
            RicSetAnimation(mar_anim_kick_crouch);
            g_CurrentEntity->velocityX = 0;
            kind = 1;
            break;
        }
    case PL_S_STAND:
    case PL_S_WALK:
        PLAYER.step = PL_S_STAND;
        RicSetAnimation(mar_anim_kick);
        g_CurrentEntity->velocityX = 0;
        kind = 0;
        break;
    case PL_S_FALL:
    case PL_S_JUMP:
        PLAYER.step = PL_S_JUMP;
        if (PLAYER.velocityY > 0) {
            RicSetAnimation(mar_anim_kick_air_fall);
            kind = 3;
        } else {
            RicSetAnimation(mar_anim_kick_air_rise);
            kind = 2;
        }
        break;
    case PL_S_RUN:
        // 060A9DA4: her slide kick (6.625 Saturn)
        RicSetSlideKick();
        RicSetSpeedX(FIX(5.3));
        return;
    default:
        return;
    }
    RicCreateEntFactoryFromEntity(
        g_CurrentEntity, FACTORY(BP_MAR_KICK, kind), 0);
    g_api.PlaySfx(MAR_SFX_KICK); // Saturn 0x71F, her voice
    g_Player.unk46 = 2;
    PLAYER.step_s = 0x41;
    g_Player.timers[PL_T_ATTACK] = 4;
}

// Saturn 060A6B90's end: Down + Cross after both jumps
void MarSetDiveKick(s32 moving) {
    if (moving) {
        RicSetAnimation(mar_anim_dive);
        RicCreateEntFactoryFromEntity(g_CurrentEntity, BP_MAR_DIVE, 0);
    } else {
        RicSetAnimation(mar_anim_dive_down);
        RicCreateEntFactoryFromEntity(
            g_CurrentEntity, FACTORY(BP_MAR_DIVE, 1), 0);
    }
    PLAYER.step_s = 0x70;
    g_Player.unk44 &= ~0x80;
    PLAYER.velocityY = FIX(6);
    if (moving) {
        RicSetSpeedX(FIX(4.5));
    }
    g_api.PlaySfx(MAR_SFX_DIVE); // Saturn 0x5CA, her voice
}

// 69, Saturn 060B9404: the shot. Waits at her hand until her pose 4
void MarEntityShot(Entity* self) {
    s32 kind;

    switch (self->step) {
    case 0:
        self->ext.factory.unk7C = MarAttackKind();
        MarSetAttack(self, &mar_def_shot);
        self->step++;
    case 1:
        self->facingLeft = PLAYER.facingLeft;
        if (PLAYER.anim != D_80155588 && PLAYER.anim != D_801555A8 &&
            PLAYER.anim != D_801555C8 && PLAYER.anim != mar_anim_atk_air_down) {
            self->step++;
            return;
        }
        MarPlaceShot(self, self->ext.factory.unk7C + 4);
        if (PLAYER.pose > 3) {
            self->step++;
        }
        return;
    case 2:
        kind = self->ext.factory.unk7C;
        self->animSet = ANIMSET_OVL(0x14);
        self->palette = MAR_PAL_SHOT;
        self->zPriority = PLAYER.zPriority - 2;
        g_api.PlaySfx(MAR_SFX_SHOT); // Saturn 0x728
        self->facingLeft = PLAYER.facingLeft;
        if (kind == 3) {
            self->velocityX = FIX(1.98);
            self->velocityY = 0x27991;
            self->flags = FLAG_UNK_100000;
            self->anim = mar_anim_shot_down;
        } else {
            self->velocityX = FIX(2.8);
            self->flags = FLAG_POS_CAMERA_LOCKED | FLAG_UNK_100000;
            self->anim = mar_anim_shot;
        }
        if (self->facingLeft) {
            self->velocityX = -self->velocityX;
        }
        MarPlaceShot(self, kind);
        self->step++;
        return;
    case 3:
        self->hitboxWidth = 5;
        self->hitboxHeight = 4;
        self->step++;
    default:
        if (self->hitFlags) {
            DestroyEntity(self);
            return;
        }
        self->posX.val += self->velocityX;
        self->posY.val += self->velocityY;
        return;
    }
}

// 70, Saturn 060AD0E8: the kick's hitbox, over her pose window
void MarEntityKick(Entity* self) {
    s32 kind = (self->params >> 8) & 3;

    if (PLAYER.step_s != 0x41) {
        DestroyEntity(self);
        return;
    }
    self->posX.i.hi = PLAYER.posX.i.hi;
    self->posY.i.hi = PLAYER.posY.i.hi;
    self->facingLeft = PLAYER.facingLeft;
    if (self->step == 0) {
        self->flags =
            FLAG_KEEP_ALIVE_OFFCAMERA | FLAG_POS_PLAYER_LOCKED | FLAG_UNK_20000;
        if (PLAYER.pose >= mar_kick_poses[kind][0]) {
            MarSetAttack(self, &mar_def_kick[kind]);
            self->step++;
        }
    } else if (PLAYER.pose > mar_kick_poses[kind][1]) {
        DestroyEntity(self);
    }
}

// 71, Saturn 060AD248: the dive kick's hitbox; a hit makes her bounce
void MarEntityDive(Entity* self) {
    if (PLAYER.step_s != 0x70) {
        DestroyEntity(self);
        return;
    }
    self->flags = FLAG_POS_PLAYER_LOCKED | FLAG_UNK_20000;
    self->facingLeft = PLAYER.facingLeft;
    self->posY.i.hi = PLAYER.posY.i.hi;
    self->posX.i.hi = PLAYER.posX.i.hi;
    g_Player.unk44 &= ~0x80;
    if (self->step == 0) {
        MarSetAttack(self, &mar_def_dive);
        if (self->params) {
            self->hitboxOffX = 1;
            self->hitboxOffY = 27;
        }
        self->step++;
    } else if (self->hitFlags == 1) {
        g_Player.unk44 |= 0x80;
    }
    self->hitFlags = 0; // (as Richter's slide kick: one bounce per hit)
}

// 82, Saturn 060B9B88: charge level 1, a fireball growing as it flies
void MarEntityCharge1(Entity* self) {
    if (self->step == 0) {
        self->animSet = ANIMSET_OVL(0x15);
        self->palette = MAR_PAL_BALL;
        self->zPriority = PLAYER.zPriority - 2;
        self->flags = FLAG_POS_CAMERA_LOCKED;
        g_api.PlaySfx(MAR_SFX_FIRE); // Saturn 0x722
        MarPlaceShot(self, MarAttackKind());
        // the art faces left
        self->facingLeft = PLAYER.facingLeft ? 0 : 1;
        self->velocityX = self->facingLeft ? FIX(3.4) : FIX(-3.4);
        self->drawFlags = ENTITY_SCALEX | ENTITY_SCALEY;
        self->scaleX = self->scaleY = 0x40;
        self->animCurFrame = 1;
        MarSetAttack(self, &mar_def_charge1);
        self->step++;
    } else {
        if (self->hitFlags) {
            DestroyEntity(self);
            return;
        }
        self->posX.val += self->velocityX;
        self->scaleX += 0x10;
        self->scaleY += 0x10;
        if (self->scaleY > 0x100) {
            self->scaleY = 0x100;
        }
        if (self->scaleX > 0x180) {
            self->scaleX = 0x180;
        }
    }
    if (++self->animCurFrame > 8) {
        self->animCurFrame = 1;
    }
}

// 83, Saturn 060B9D88: charge level 2, an energy ball leaving fading copies
// of itself (params = the frame << 8)
void MarEntityCharge2(Entity* self) {
    if (self->step == 0) {
        self->animSet = ANIMSET_OVL(0x15);
        self->palette = MAR_PAL_BALL;
        self->zPriority = PLAYER.zPriority - 2;
        if (self->params) {
            self->zPriority--;
            self->flags = FLAG_POS_CAMERA_LOCKED;
            self->blendMode = BLEND_TRANSP | BLEND_ADD;
            self->drawFlags = ENTITY_OPACITY | ENTITY_MASK_B;
            self->opacity = 0x80;
            self->animCurFrame = self->params >> 8;
            self->ext.factory.unk7C = 12;
        } else {
            self->flags = FLAG_POS_CAMERA_LOCKED | FLAG_UNK_100000;
            self->anim = mar_anim_ball;
            self->velocityX = self->facingLeft ? FIX(-3.4) : FIX(3.4);
            MarPlaceShot(self, MarAttackKind());
            MarSetAttack(self, &mar_def_charge2);
            g_api.PlaySfx(MAR_SFX_BALL); // Saturn 0x723
        }
        self->step++;
        return;
    }
    if (self->params) {
        if (g_GameTimer & 1) {
            if (--self->ext.factory.unk7C < 0) {
                DestroyEntity(self);
                return;
            }
            self->opacity = self->ext.factory.unk7C * 10;
        }
        return;
    }
    self->posX.val += self->velocityX;
    if (!(self->pose & 1) && self->poseTimer == 1) {
        RicCreateEntFactoryFromEntity(
            self, FACTORY(BP_MAR_CHARGE_2, self->animCurFrame), 0);
    }
}

// 84, Saturn 060B96C0: charge level 3, a laser from her hand to the screen
// edge, its hitbox half its length; thins out over 16 frames
void MarEntityLaser(Entity* self) {
    Primitive* prim;
    s32 i;
    s16 x0;
    s16 x1;
    s16 t;

    if (self->step == 0) {
        self->primIndex = g_api.AllocPrimitives(PRIM_G4, 2);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->flags = FLAG_POS_CAMERA_LOCKED | FLAG_HAS_PRIMS;
        self->ext.factory.unk7C = 4;
        self->facingLeft = PLAYER.facingLeft;
        MarPlaceShot(self, MarAttackKind());
        if ((u16)self->posX.i.hi > 255) {
            DestroyEntity(self);
            return;
        }
        MarSetAttack(self, &mar_def_laser);
        if (self->facingLeft) {
            x0 = 0;
            x1 = self->posX.i.hi;
            self->hitboxWidth = self->posX.i.hi / 2;
        } else {
            x0 = self->posX.i.hi;
            x1 = 256;
            self->hitboxWidth = (256 - self->posX.i.hi) / 2;
        }
        self->hitboxOffX = self->hitboxWidth;
        prim = &g_PrimBuf[self->primIndex];
        for (i = 0; prim != NULL; i++, prim = prim->next) {
            prim->x0 = prim->x2 = x0;
            prim->x1 = prim->x3 = x1;
            // outer edge white, centre (Saturn gouraud 0xC9DF on grey)
            prim->r0 = prim->g0 = prim->b0 = prim->r1 = prim->g1 = prim->b1 =
                0xFF;
            prim->r2 = prim->r3 = 0xF8;
            prim->g2 = prim->g3 = 0x70;
            prim->b2 = prim->b3 = 0x90;
            prim->priority = PLAYER.zPriority + 2;
            prim->drawMode = DRAW_DEFAULT;
        }
        self->ext.factory.unk84 = self->posX.i.hi;
        RicCreateEntFactoryFromEntity(self, FACTORY(BP_MAR_SPARK, 1), 0);
        RicCreateEntFactoryFromEntity(self, FACTORY(BP_MAR_SPARK, 9), 0);
        // Saturn 0x724 (the sparks): the PS1 thunder of the same kind, in
        // the common bank (the user's pick; frees her bank's room)
        g_api.PlaySfx(SFX_THUNDER_B);
        self->step++;
    }
    if (!(g_GameTimer & 3)) {
        if (--self->ext.factory.unk7C <= 0) {
            DestroyEntity(self);
            return;
        }
    }
    t = self->ext.factory.unk7C;
    self->ext.factory.unk86 = self->posY.i.hi;
    prim = &g_PrimBuf[self->primIndex];
    prim->y0 = prim->y1 = self->posY.i.hi - t;
    prim->y2 = prim->y3 = self->posY.i.hi;
    prim = prim->next;
    prim->y0 = prim->y1 = self->posY.i.hi + t;
    prim->y2 = prim->y3 = self->posY.i.hi;
}

// 85, Saturn 060B99A8: the laser's sparks, a chain of short lines along a
// sine (params = segment << 8; the two chains start half a wave apart);
// the odd segments hurt
void MarEntitySpark(Entity* self) {
    Primitive* prim;
    Entity* parent;
    s32 n;

    if (self->step == 0) {
        self->primIndex = g_api.AllocPrimitives(PRIM_LINE_G2, 1);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        parent = self->ext.factory.parent;
        if (parent->entityId == 0) {
            DestroyEntity(self);
            return;
        }
        self->flags = FLAG_POS_CAMERA_LOCKED | FLAG_HAS_PRIMS;
        if (self->params & 0x100) {
            MarSetAttack(self, &mar_def_charge1);
            self->hitboxWidth = 3;
            self->hitboxHeight = 4;
        }
        prim = &g_PrimBuf[self->primIndex];
        prim->r0 = prim->g0 = prim->b0 = prim->r1 = prim->g1 = prim->b1 = 0xFF;
        prim->priority = PLAYER.zPriority + 2;
        prim->drawMode = DRAW_DEFAULT;
        self->posY.i.hi = parent->posY.i.hi;
        prim->x1 = parent->ext.factory.unk84;
        prim->y1 = parent->ext.factory.unk86;
        prim->x0 = prim->x1 + (self->facingLeft ? -6 : 6);
        self->posX.i.hi = prim->x0;
        self->hitboxOffY = rsin(self->params) >> 8;
        prim->y0 = self->posY.i.hi + self->hitboxOffY;
        self->ext.factory.unk84 = prim->x0;
        self->ext.factory.unk86 = prim->y0;
        if ((u16)(self->posX.i.hi - 1) < 255 &&
            (u16)(self->posY.i.hi - 1) < 239) {
            RicCreateEntFactoryFromEntity(
                self, FACTORY(BP_MAR_SPARK, (self->params >> 8) + 1), 0);
        }
        self->step++;
        return;
    }
    n = ++self->step_s;
    if (n > 7) {
        DestroyEntity(self);
        return;
    }
    prim = &g_PrimBuf[self->primIndex];
    prim->r0 = prim->g0 = prim->r1 = prim->g1 = (31 - n * 4) * 8;
}

// 88, Saturn 060BD150: fully charged, a guard in front of her while the
// charge lasts and 300 frames after
void MarEntityCharged(Entity* self) {
    switch (self->step) {
    case 0:
        self->flags = FLAG_POS_PLAYER_LOCKED | FLAG_UNK_20000;
        MarSetAttack(self, &mar_def_charged);
        self->ext.factory.unk7C = 300;
        RicCreateEntFactoryFromEntity(self, BP_MAR_AURA, 0);
        self->step++;
        break;
    case 1:
        if (MarChargeLevel() != 3) {
            self->step++;
        }
        break;
    case 2:
        if (--self->ext.factory.unk7C < 0) {
            RicCreateEntFactoryFromEntity(self, FACTORY(BP_MAR_AURA, 1), 0);
            self->step++;
        }
        break;
    default:
        DestroyEntity(self);
        return;
    }
    if (self->hitFlags) {
        RicCreateEntFactoryFromEntity(self, BP_MAR_AURA, 0);
        self->hitFlags = 0;
    }
    self->posX.i.hi = PLAYER.posX.i.hi;
    self->posY.i.hi = PLAYER.posY.i.hi;
    self->facingLeft = PLAYER.facingLeft;
}

// 89, Saturn 060BD244: a diamond in front of her growing (params 0x0000)
// or shrinking (0x0100) every other frame
void MarEntityAura(Entity* self) {
    Primitive* prim;
    s16 cx;
    s16 cy;
    s16 w;
    s16 h;

    switch (self->step) {
    case 0:
        self->primIndex = g_api.AllocPrimitives(PRIM_G4, 1);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        prim = &g_PrimBuf[self->primIndex];
        prim->r0 = prim->r1 = prim->r2 = prim->r3 = 0xA0;
        prim->g0 = prim->g1 = prim->g2 = prim->g3 = 0xA0;
        prim->b0 = prim->b1 = prim->b2 = prim->b3 = 0xF8;
        prim->priority = PLAYER.zPriority + 4;
        prim->drawMode = DRAW_TPAGE2 | DRAW_TPAGE | DRAW_TRANSP;
        self->flags = FLAG_HAS_PRIMS | FLAG_POS_PLAYER_LOCKED | FLAG_UNK_20000;
        if (!(self->params & 0xFF00)) {
            self->ext.factory.unk80 = 1;
            self->step = 3;
        } else if ((self->params & 0xFF00) == 0x100) {
            self->ext.factory.unk80 = 11;
            self->ext.factory.unk82 = 22;
            self->step = 2;
        } else {
            self->ext.factory.unk80 = 1;
            self->step = 1;
        }
        break;
    case 3:
        if (!(g_GameTimer & 1)) {
            break;
        }
    case 1:
        if (self->ext.factory.unk80 <= 10) {
            self->ext.factory.unk80++;
        }
        if (self->ext.factory.unk82 <= 21) {
            self->ext.factory.unk82 += 2;
        }
        if (self->ext.factory.unk80 > 10 && self->ext.factory.unk82 > 21) {
            DestroyEntity(self);
            return;
        }
        break;
    case 2:
        if (!(g_GameTimer & 1)) {
            break;
        }
        if (self->ext.factory.unk80) {
            self->ext.factory.unk80--;
        }
        if (self->ext.factory.unk82) {
            self->ext.factory.unk82--;
        }
        if (!self->ext.factory.unk82) {
            DestroyEntity(self);
            return;
        }
        break;
    }
    cx = PLAYER.posX.i.hi + (PLAYER.facingLeft ? -18 : 18);
    cy = PLAYER.posY.i.hi - 4;
    w = self->ext.factory.unk80 * 4 / 5;
    h = self->ext.factory.unk82;
    prim = &g_PrimBuf[self->primIndex];
    prim->x0 = prim->x3 = cx;
    prim->y0 = cy - h;
    prim->y3 = cy + h;
    prim->x1 = cx + w;
    prim->x2 = cx - w;
    prim->y1 = prim->y2 = cy;
}

// her shot / charge palettes and the holy water flames' (RAM copy and VRAM;
// gen_mar_assets.py FX_PALS)
static void MarFxPals(void) {
    RECT rect;
    s32 i;
    s32 j;
    u16 id;

    for (i = 0; i < LEN(mar_fx_pal_ids); i++) {
        id = mar_fx_pal_ids[i];
        for (j = 0; j < COLORS_PER_PAL; j++) {
            g_Clut[1][(id - 0x100) * COLORS_PER_PAL + j] = mar_fx_pal[i][j];
        }
        rect.x = (g_ClutIds[id] & 0x3F) << 4;
        rect.y = g_ClutIds[id] >> 6;
        rect.w = COLORS_PER_PAL;
        rect.h = 1;
        LoadImage(&rect, (u_long*)mar_fx_pal[i]);
    }
}

// from MarInit: her effect art, palettes and sprite banks (0x14, 0x15)
static void MarFxInit(void) {
    RECT rect;
    SpriteParts** banks;

    // Saturn game_2a.c new game: Maria starts with 200 HP, 99 of 999
    // hearts and 200 MP (her charge levels 2 and 3 cost 30 and 60; Richter's
    // PS1 start is 20), wearing the Dragon Helm (its DEF, and enemies' DEF
    // halved when she hits). MP max below 200 = a game she hasn't started
    // yet.
    if (g_Status.mpMax < 200) {
        g_Status.mpMax = g_Status.mp = 200;
        g_Status.hpMax = g_Status.hp = 200;
        g_Status.hearts = 99;
        g_Status.heartsMax = 999;
        g_Status.equipment[HEAD_SLOT] = ITEM_DRAGON_HELM;
        g_api.MakeAll();
    }
    // her effect art into its VRAM block (unpacked into the player sprite
    // buffers, free until the first frame is drawn)
    STATIC_ASSERT(sizeof(mar_buf) >= MAR_FX_RAW, "mar_buf too small");
    MarUnpack((u8*)mar_buf, mar_fx_lz, MAR_FX_RAW);
    rect.x = MAR_FX_X;
    rect.y = MAR_FX_Y;
    rect.w = MAR_FX_W;
    rect.h = MAR_FX_H;
    LoadImage(&rect, (u_long*)mar_buf);
    DrawSync(0); // before the sprite decoder reuses the buffers
    MarFxPals();
    banks = g_api.o.spriteBanks;
    banks[0x14] = (SpriteParts*)mar_fxbank_shot;
    banks[0x15] = (SpriteParts*)mar_fxbank_ball;
    mar_charge = 0;
    mar_pal_shown = mar_pal;
}
