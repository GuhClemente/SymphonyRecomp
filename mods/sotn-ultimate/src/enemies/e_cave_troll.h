// SPDX-License-Identifier: AGPL-3.0-or-later
// The Cave Troll (RNO4 unk_55D5C.c: func_us_801D5E90 and its two helpers),
// for the Saturn port's Hell Garden. RNO4 keeps its own (partly INCLUDE_ASM)
// copy; this one has StepTowards from step_towards.h, RNO4's data (US
// RNO4.BIN 0x80182344-0x801823E9) and func_us_801D6B8C in C.
// Needs: E_CAVE_TROLL, E_CAVE_TROLL_BEAM, E_CAVE_TROLL_GLOW, E_EXPLOSION and
// the EInits g_EInitCaveTroll, g_EInitCaveTrollBeam, g_EInitCaveTrollGlow.
#include "../../../../src/st/step_towards.h"

extern EInit g_EInitCaveTroll;
extern EInit g_EInitCaveTrollBeam;
extern EInit g_EInitCaveTrollGlow;

// floor sensors (the last 8 bytes: an unused animation)
static s16 sensors_troll[] = {
    0, 25, 0, 4, 8, -4, -16, 0, 0, 25, 12, 0, 0x0206, 0x0306, 0x0440, 0x00FF};
static u8 anim_land[] = {4, 5, 6, 1, 4, 6, 0xFF, 0};
static u8 anim_turn[] = {6, 7, 8, 8, 6, 9, 0xFF, 0};
static u8 anim_glow_start[] = {
    0x10, 1, 2, 0x0A, 2, 0x0B, 0x14, 0x0A, 1, 0x12, 1, 0x11, 1, 0x10, 0xFF, 0};
static u8 anim_glow_loop[] = {1, 0x0D, 1, 0x0E, 0, 0, 0, 0};
static u8 anim_jump[] = {
    8, 1, 0x16, 0x0C, 1, 0x0E, 1, 0x0D, 1, 0x0F, 1, 0x11, 1, 0x12, 0xFF, 0};
static u8 anim_orb_grow[] = {1, 0x15, 1, 0x16, 1, 0x17, 1, 0x18, 1,    0x19,
                             1, 0x1A, 1, 0x1B, 1, 0x1C, 1, 0,    0xFF, 0};
static u8 anim_orb[] = {
    2, 0x1D, 2, 0x1E, 2, 0x1F, 2, 0x20, 2, 0x21, 2, 0x22, 0, 0, 0, 0};
// hitbox {offX, offY, width, height} by animation frame
static s8 hitboxes[][4] = {
    {0, 0, 0, 0},
    {1, 7, 4, 0x12},
    {1, 3, 7, 0x0E},
    {4, 0, 4, 0x18},
    {0, 5, 5, 0x14}};
static u8 hitbox_by_frame[] = {
    0, 1, 2, 2, 1, 1, 1, 1, 1, 1, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 2, 0, 0, 0};

void CaveTrollUpdateDust(Primitive* prim) {
    s32 yPos;
    switch (prim->p2) {
    case 0:
        LOW(prim->x2) = Random() * 8;
        prim->x3 = (Random() & 0x1F) + 0x10;
        prim->drawMode = DRAW_UNK02;
        prim->p2 += 1;

    case 1:
        yPos = (prim->y0 << 0x10) + prim->y1;
        yPos = yPos + LOW(prim->x2);
        LOW(prim->x2) -= 0x1000;
        prim->y0 = yPos >> 0x10;
        prim->y1 = yPos & 0xffff;
        if (!(prim->x3 -= 1)) {
            prim->p2 = 0;
            prim->drawMode = DRAW_HIDE;
            prim->p3 = 0;
        }
        return;
    }
}

void EntityCaveTroll(Entity* self) {
    Collider collider;
    Primitive* prim;
    Entity* explosion;
    Entity* player;
    s8* frameParts;

    s32 primIndex;
    long genericInt;
    s32 posX;
    s32 posY;
    s32 radius;
    s16 angle;

    if (self->flags & FLAG_DEAD) {
        PlaySfxPositional(SFX_QUICK_STUTTER_EXPLODE_B);
        explosion = AllocEntity(&g_Entities[224], &g_Entities[256]);
        if (explosion) {
            CreateEntityFromEntity(E_EXPLOSION, self, explosion);
            explosion->params = 2;
        }
        DestroyEntity(self);
        return;
    }

    switch (self->step) {
    case 0x0:
        InitializeEntity(g_EInitCaveTroll);
        self->animCurFrame = 1;

        primIndex = g_api.AllocPrimitives(0x11, 0x20);
        if (primIndex == -1) {
            DestroyEntity(self);
            return;
        }

        self->flags |= FLAG_HAS_PRIMS;
        self->primIndex = primIndex;

        prim = &g_PrimBuf[primIndex];
        self->ext.et_801D5E90.prim7C = prim;

        while (prim) {
            prim->u0 = (prim->v0 = 1);
            prim->r0 = 0x80;
            prim->g0 = 0x80;
            prim->b0 = 0xC0;
            prim->priority = self->zPriority + 1;
            prim->drawMode = DRAW_HIDE;
            prim = prim->next;
        }

        break;

    case 0x1:
        if (UnkCollisionFunc3(sensors_troll) & 1) {
            SetStep(2);
        }
        break;

    case 0x2:
        if (GetDistanceToPlayerX() < 0x60) {
            SetStep(3);
        }
        break;

    case 0x3:
        switch (self->step_s) {
        case 0:
            self->velocityY = FIX(-2.0);
            if (self->facingLeft) {
                self->velocityX = FIX(1.5);
            } else {
                self->velocityX = FIX(-1.5);
            }
            self->step_s += 1;

        case 1:
            MoveEntity();
            self->velocityY += FIX(0.125);
            if (self->velocityY < 0) {
                self->animCurFrame = 2;
            }
            if (self->velocityY > FIX(-0.5)) {
                self->animCurFrame = 3;
            }
            if (self->velocityY > FIX(0.5)) {
                self->animCurFrame = 4;
            };
            posX = self->posX.i.hi;
            posY = self->posY.i.hi + 0x19;

            g_api.CheckCollision(posX, posY, &collider, 0);

            if (collider.effects & EFFECT_SOLID) {
                PlaySfxPositional(SFX_STOMP_HARD_D);
                genericInt = collider.unk18;
                posX = self->posX.i.hi;
                posY = self->posY.i.hi + 0x11;

                g_api.CheckCollision(posX, posY, &collider, 0);

                if (collider.effects & EFFECT_SOLID) {
                    SetSubStep(3);
                } else {
                    self->posY.i.hi += genericInt;
                    genericInt = (GetSideToPlayer() & 0x1) ^ 0x1;
                    posX = GetDistanceToPlayerX();

                    if ((genericInt != self->facingLeft) && (0x40 < posX)) {
                        SetSubStep(3);
                    } else {
                        SetSubStep(2);
                    }

                    if ((0x80 > posX) && (posX > 0x20)) {
                        genericInt = Random();
                        if (!(genericInt & 1)) {
                            SetStep(4);
                        }
                        if ((genericInt < 3) || (self->hitPoints < 0x10)) {
                            SetStep(5);
                        }
                    }
                }
            }
            break;

        case 2:
            if (!AnimateEntity(anim_land, self)) {
                SetSubStep(0);
            }
            break;

        case 3:
            if (!AnimateEntity(anim_turn, self)) {
                self->facingLeft ^= 0x1;
                self->animCurFrame = 2;
                SetSubStep(0);
            }
        }

        break;

    case 0x4: {
        switch (self->step_s) {
        case 0:
            genericInt = (GetSideToPlayer() & 1) ^ 1;
            if (self->facingLeft == genericInt) {
                SetSubStep(2);
                break;
            }

        case 1:
            if (!AnimateEntity(anim_turn, self, explosion)) {
                self->facingLeft ^= 0x1;
                self->animCurFrame = 1;
                SetSubStep(2);
            }
            break;

        case 2:
            if (!AnimateEntity(anim_jump, self)) {
                self->velocityX = 0;
                self->velocityY = 0xfffa0000;
                self->animCurFrame = 0x13;
                PlaySfxPositional(SFX_STOMP_HARD_E);
                self->step_s += 1;
            }
            break;

        case 3:
            MoveEntity();
            self->velocityY += FIX(0.1875);
            if (self->velocityY > 0) {
                self->step_s += 1;
            }
            break;

        case 4:
            player = g_Entities;
            angle = GetAngleBetweenEntities(self, player);
            angle -= 0x400;
            angle &= 0xfff;

            if (self->facingLeft) {
                angle = 0x1000 - angle;
            }

            if (angle & 0x800) {
                SetSubStep(8);
                break;
            } else {
                self->ext.et_801D5E90.unk8C = angle;
                self->animCurFrame = 0x14;
                self->velocityY = 0;
                self->rotate = 0;
                self->drawFlags |= ENTITY_ROTATE;
                self->ext.et_801D5E90.unk91 = 1;
                self->step_s++;
            }

        case 5:
            FntPrint("angle %x\n", self->ext.et_801D5E90.unk8C);

            angle = self->ext.et_801D5E90.unk8C;
            angle -= 0x380;
            if (StepTowards(&self->rotate, angle, 0x28)) {
                self->step_s += 1;
            }
            break;

        case 6:
            explosion = AllocEntity(&g_Entities[0xA0], &g_Entities[0xC0]);
            if (explosion) {
                CreateEntityFromEntity(E_CAVE_TROLL_BEAM, self, explosion);
                explosion->facingLeft = self->facingLeft;
                angle = self->ext.et_801D5E90.unk8C;

                if (self->facingLeft) {
                    explosion->posX.i.hi += (rsin(angle) * 9) >> 12;
                } else {
                    explosion->posX.i.hi -= (rsin(angle) * 9) >> 12;
                }

                explosion->posY.i.hi += (9 * rcos(angle)) >> 0xc;
                explosion->ext.et_801D5E90.unk8C = angle;
                explosion->ext.et_801D5E90.ent9C = self;
            }

            self->ext.et_801D5E90.unk80 = 0x40;
            PlaySfxPositional(SFX_GUARD_TINK);
            self->step_s += 1;

            break;

        case 7:
            if (!(self->ext.et_801D5E90.unk80 -= 1)) {
                self->animCurFrame = 0x13;
                self->ext.et_801D5E90.unk91 = 0;
                self->step_s += 1;
            }
            break;

        case 8:
            StepTowards(&self->rotate, 0, 0x40);
            if (UnkCollisionFunc3(sensors_troll) & 1) {
                PlaySfxPositional(SFX_STOMP_HARD_C);
                self->drawFlags = ENTITY_DEFAULT;
                posY = 9;
                SetSubStep(posY);
                self->pose = 1;
            }
            break;

        case 9:
            if (!AnimateEntity(anim_land, self)) {
                genericInt = SetStep(3);
            }
            break;
        }

    } break; // case 0x4

    case 0x5:
        switch (self->step_s) {
        case 0:
            self->ext.et_801D5E90.unk90 = 0;
            genericInt = (GetSideToPlayer() & 0x1) ^ 0x1;

            if (self->facingLeft == genericInt) {
                SetSubStep(2);
                break;
            }

        case 1:
            if (!AnimateEntity(anim_turn, self, explosion)) {
                self->facingLeft ^= 0x1;
                self->animCurFrame = 1;
                SetSubStep(2);
            }
            break;

        case 2:
            explosion = AllocEntity(&g_Entities[0xA0], &g_Entities[0xC0]);
            if (explosion) {
                PlaySfxPositional(SFX_RNO4_MAGIC_GLASS_BREAK);
                CreateEntityFromEntity(E_CAVE_TROLL_GLOW, self, explosion);
                explosion->facingLeft = self->facingLeft;
                explosion->zPriority = self->zPriority + 1;
                explosion->ext.et_801D5E90.ent9C = self;
                SetSubStep(3);
            } else {
                SetStep(3);
                break;
            }

        case 3:
            if (!AnimateEntity(anim_glow_start, self)) {
                self->ext.et_801D5E90.unk90 = 1U;
                SetSubStep(4);
            }
            break;

        case 4:
            AnimateEntity(anim_glow_loop, self);
            if (!self->ext.et_801D5E90.unk90) {
                SetStep(3);
            }
            break;
        }
        break; // case 0x5

    case 0xFF:
        FntPrint("charal %x\n", self->animCurFrame);
        if (g_pads[1].pressed & PAD_SQUARE) {
            if (self->params) {
                break;
            }
            self->animCurFrame++;
            self->params |= 1;
        } else {
            self->params = 0;
        }
        if (g_pads[1].pressed & PAD_CIRCLE) {
            if (self->step_s) {
                break;
            }
            self->animCurFrame--;
            self->step_s |= 1;
        } else {
            self->step_s = 0;
        }
    }

    frameParts = &hitboxes[0][0];
    genericInt = hitbox_by_frame[self->animCurFrame];
    frameParts = frameParts;
    frameParts = frameParts + (genericInt * 4);

    self->hitboxOffX = *(frameParts++);
    self->hitboxOffY = *(frameParts++);
    self->hitboxWidth = *(frameParts++);
    self->hitboxHeight = *(frameParts++);

    if (self->ext.et_801D5E90.unk91) {
        prim = self->ext.et_801D5E90.prim7C;
        prim = FindFirstUnkPrim(prim);
        if (prim) {
            posX = self->posX.i.hi;
            posY = self->posY.i.hi;
            angle = Random() << 4;
            radius = Random() & 0x1F;
            posX += (radius * rcos(angle)) >> 12;
            posY += (radius * rsin(angle)) >> 12;
            prim->x0 = posX;
            prim->y0 = posY;
            prim->p2 = 0;
            prim->p3 = 1;
        }
    }

    prim = self->ext.et_801D5E90.prim7C;
    while (prim) {
        if (prim->p3) {
            CaveTrollUpdateDust(prim);
        }

        // This might be a fake, at the very least it's very unconventional
        // to cast a pointer to s32 first and then to Primitve* later
        // But seeing as genericInt is an all-purpose variable anyways,
        // it might just be true
        genericInt = (long)prim->next;
        if (!genericInt) {
            prim->x0 = 0;
            prim->y0 = 0;
            prim->drawMode =
                DRAW_TPAGE2 | DRAW_TPAGE | DRAW_UNK02 | DRAW_TRANSP;
        }
        prim = (Primitive*)genericInt;
    }
}

void EntityCaveTrollBeam(Entity* self) {
    Primitive* prim;
    Entity* ent;
    s32 xPosition;
    s32 primIndex;
    s32 yPosition;
    s16 angle;

    if (self->hitFlags && !(self->hitFlags & 0x80)) {
        self->hitboxState = 0;
        SetStep(2);
    }

    switch (self->step) {
    case 0:
        InitializeEntity(g_EInitCaveTrollBeam);
        primIndex = g_api.AllocPrimitives(PRIM_LINE_G2, 1);

        if (primIndex == -1) {
            DestroyEntity(self);
            return;
        }

        self->flags |= FLAG_HAS_PRIMS;
        self->primIndex = primIndex;
        prim = &g_PrimBuf[primIndex];
        self->ext.et_801D68E0.unk7C = prim;

        prim->x0 = (prim->x1 = self->posX.i.hi);
        prim->y0 = (prim->y1 = self->posY.i.hi);
        prim->r0 = 0xFF;
        prim->g0 = 0x60;
        prim->b0 = 0x60;
        prim->r1 = 0x80;
        prim->g1 = 0x40;
        prim->b1 = 0x40;
        prim->priority = self->zPriority;
        prim->drawMode =
            DRAW_TPAGE2 | DRAW_TPAGE | DRAW_COLORS | DRAW_UNK02 | DRAW_TRANSP;

        angle = self->ext.et_801D68E0.unk8C;

        if (self->facingLeft) {
            angle = -angle;
        }

        self->velocityX = rsin(angle) * (-0x80);
        self->velocityY = rcos(angle) << 7;
        self->ext.et_801D68E0.unk80 = 0x20;

    case 1:
        MoveEntity();

        prim = self->ext.et_801D68E0.unk7C;
        xPosition = (prim->x0 = self->posX.i.hi);
        yPosition = (prim->y0 = self->posY.i.hi);

        if (!(self->ext.et_801D68E0.unk80 -= 1)) {
            self->step = 2;
        }

        break;

    case 2:
        self->velocityX = -self->velocityX;
        self->velocityY = -self->velocityY;
        self->step++;

    case 3:
        MoveEntity();

        prim = self->ext.et_801D68E0.unk7C;
        xPosition = (prim->x0 = self->posX.i.hi);
        yPosition = (prim->y0 = self->posY.i.hi);

        prim = self->ext.et_801D68E0.unk7C;
        xPosition -= prim->x1;
        yPosition -= prim->y1;

        if ((abs(xPosition) < 8) && (abs(yPosition) < 8)) {
            DestroyEntity(self);
        }
    }

    ent = self->ext.et_801D68E0.unk9C;
#if defined(VERSION_PSP) || defined(FIX_UB)
    // Non-PSX versions fix a null pointer here
    if (ent != NULL) {
#else
    if (1) {
#endif
        if (ent->entityId != E_CAVE_TROLL) {
            DestroyEntity(self);
        }
    }
}

// RNO4 func_us_801D6B8C (INCLUDE_ASM there): the glowing orb the troll
// holds out (E_CAVE_TROLL_GLOW): it waits for the troll's pose 4, grows when
// the troll's unk90 is set, then shrinks, its hitbox reaching for the player
void EntityCaveTrollGlow(Entity* self) {
    Entity* troll;
    s32 angle;
    s32 reach;
    s32 dist;
    s32 dx;

    switch (self->step) {
    case 0:
        InitializeEntity(g_EInitCaveTrollGlow);
        self->animCurFrame = 0;
        break;
    case 1:
        if (self->ext.et_801D5E90.ent9C->pose == 4) {
            self->step++;
        }
        break;
    case 2:
        AnimateEntity(anim_orb_grow, self);
        troll = self->ext.et_801D5E90.ent9C;
        if (troll->ext.et_801D5E90.unk90) {
            self->drawFlags = ENTITY_SCALEX | ENTITY_SCALEY;
            self->scaleX = self->scaleY = 0x180;
            self->facingLeft = 0;
            self->ext.et_801D5E90.unk80 = 0x60;
            self->zPriority = troll->zPriority - 1;
            SetStep(3);
        }
        break;
    case 3:
        AnimateEntity(anim_orb, self);
        if (self->scaleX < 0x200) {
            self->scaleY += 0x20;
            self->scaleX = self->scaleY;
        } else if (!--self->ext.et_801D5E90.unk80) {
            SetStep(4);
        }
        /* fallthrough */
    case 4:
        AnimateEntity(anim_orb, self);
        if (!self->scaleX) {
            self->ext.et_801D5E90.ent9C->ext.et_801D5E90.unk90 = 0;
            DestroyEntity(self);
            return;
        }
        self->scaleY -= 4;
        self->scaleX = self->scaleY;
        angle = GetAngleBetweenEntities(self, &g_Entities[0]);
        reach = (self->scaleX * 15) >> 7;
        dx = GetDistanceToPlayerX();
        // (both calls are GetDistanceToPlayerX in the retail code)
        dist = GetDistanceToPlayerX();
        dist = SquareRoot0(dx * dx + dist * dist) - 4;
        if (dist < reach) {
            reach = dist;
        }
        self->hitboxOffX = (reach * rcos((s16)angle)) >> 12;
        self->hitboxOffY = (reach * rsin((s16)angle)) >> 12;
        break;
    }
    self->palette = (g_Timer & 1) ? 0x8163 : 0x819C;
    if (self->ext.et_801D5E90.ent9C->entityId != E_CAVE_TROLL) {
        DestroyEntity(self);
    }
}
