// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port: the third hand ("Holder", ホルダー; the Saturn's equipment[2]),
// Saturn balance and Alucard only (include/game.h MOD_HOLDER_ON). Its item
// id is kept in g_Status.saveName[10] (MOD_HOLDER_ITEM). Included by
// 7879C.c; the equip menu row and the 8th button config row are in menu.c,
// the button check in 6FDF8.c func_8010FDF8.
// - ModHolderAllows: the Saturn's holder filter (GAME func_060737A0): food or
//   medicine that needs no weapon overlay.
// - ModHolderUse: Saturn func_060A67B8 / func_060A66FC, the food and
//   medicine part of the hand path (6E42C.c func_8010EDB8) with the holder.
// - EntityModHolderFood: the Saturn moved its food weapon into ALUCARD.PRG
//   (entity 0x48, func_060B25F4) so the holder needs no weapon slot; this is
//   the PS1 w_021 food (EntityWeaponAttack + func_ptr_80170004, the Meal
//   ticket's prize) moved into DRA the same way. w_021's food sheet is only
//   in VRAM while a hand holds food, so it is drawn from the food's equip
//   icon (LoadEquipIcon slots, tpage 0x1A).

void func_8010E42C(u32 substep);
void SetPlayerAnim(u8 anim);
void AddToInventory(u16 id, EquipKind kind);

#define MOD_HOLDER_ICON 0x17       // its menu row's icon slot (and the food)
#define MOD_HOLDER_PRIZE_ICON 0x1E // the Meal ticket's food
#define MOD_FOOD_PEANUTS 6         // food sprite numbers (equip unk14)
#define MOD_FOOD_TOADSTOOL 7
#define MOD_FOOD_MEAL_TICKET 41

bool ModHolderAllows(s32 id) {
    Equipment* e;

    if (!id) {
        return true;
    }
    if (id >= HAND_ITEMS_CAP || id == ITEM_LIFE_APPLE || id == ITEM_HAMMER ||
        id == ITEM_SIGNED_PENCIL_BOARD) {
        return false;
    }
    e = &g_EquipDefs[id];
    // food runs in EntityModHolderFood here (the Saturn's food records have
    // weaponId 0xFF; the PS1's still name w_021)
    if (e->itemCategory == ITEM_FOOD) {
        return true;
    }
    return e->itemCategory == ITEM_MEDICINE && e->weaponId == 0xFF;
}

s32 ModHolderOn(void) { return MOD_HOLDER_ON(); }

// the holder's item, empty if the stored id isn't one it takes
s32 ModHolderItem(void) {
    s32 id = MOD_HOLDER_ITEM;

    return ModHolderAllows(id) ? id : 0;
}

#if defined(MOD_SATURN_STAGE_TEST)
// Test builds only (the dialogue warp chain, include/game.h): DRA has no room
// for both, so the Holder's use and its food entity are stubbed there (L2
// does nothing; the menu row stays). Normal builds keep the code below.
s32 ModHolderUse(void) { return false; }
void EntityModHolderFood(Entity* self) { DestroyEntity(self); }
#else
// ReduceWeapon (5D5BC.c) for the holder (the Saturn's accepts hand 2)
static void ModHolderReduce(s32 id) {
    if (CheckEquipmentItemCount(ITEM_DUPLICATOR, EQUIP_ACCESSORY)) {
        return;
    }
    if (!g_EquipDefs[id].isConsumable) {
        return;
    }
    if (!g_Status.equipHandCount[id]) {
        MOD_HOLDER_ITEM = ITEM_EMPTY_HAND;
        make_all();
        return;
    }
    g_Status.equipHandCount[id]--;
}

// Saturn func_060A67B8: on a tap of the holder button (no hand attack started
// that frame); true if the item was used
s32 ModHolderUse(void) {
    Equipment* e;
    Entity* ent;
    s32 id, i, n;
    s32 atLedge;

    id = ModHolderItem();
    if (!id) {
        return false;
    }
    e = &g_EquipDefs[id];
    atLedge = g_Player.vram_flag & IN_AIR_OR_EDGE ? 1 : 0;
    // the chain limit, counting the item's entities from either hand
    for (ent = &g_Entities[16], i = 16, n = 0; i < 64; i++, ent++) {
        if (ent->ext.weapon.equipId == id) {
            n++;
        }
    }
    if (n >= e->chainLimit) {
        return false;
    }
    if (e->itemCategory == ITEM_FOOD) {
        // at most two of the holder's foods at a time
        for (ent = &g_Entities[32], i = 32, n = 0; i < 64; i++, ent++) {
            if (ent->entityId == MOD_E_HOLDER_FOOD) {
                n++;
            }
        }
        if (n >= 2) {
            return false;
        }
        ent = GetFreeEntity(32, 64);
        if (ent == NULL) {
            return false;
        }
        DestroyEntity(ent);
        ent->entityId = MOD_E_HOLDER_FOOD;
        ent->posX.val = PLAYER.posX.val;
        ent->posY.val = PLAYER.posY.val;
        ent->params = (e->unk14 & 0x7F) << 8;
        ent->ext.food.foodId = id;
    } else {
        if (e->unk14 == 20) { // Library card (the Saturn: its sprint too)
            if (PLAYER.step == Player_Stand || PLAYER.step == Player_Walk ||
                PLAYER.step == Player_Crouch || MOD_SPRINTING()) {
                func_8010E42C(0);
                ModHolderReduce(id);
                return true;
            }
            return false;
        }
        // one EntityPlayerPinkEffect (39) at a time
        for (ent = &g_Entities[32], i = 32; i < 64; i++, ent++) {
            if (ent->entityId == 39) {
                return false;
            }
        }
        // Blueprint 55, child 39, EntityPlayerPinkEffect
        ent = CreateEntFactoryFromEntity(
            g_CurrentEntity, FACTORY(BP_55, (e->unk14 & 0x7F)), id);
        if (ent == NULL) {
            return false;
        }
    }
    // the hands' food / medicine animation, when not in an attack
    if (PLAYER.step_s < 0x40) {
        if (PLAYER.step == Player_Stand) {
            SetPlayerAnim(e->playerAnim + atLedge);
        } else if (
            PLAYER.step == Player_Crouch && e->itemCategory == ITEM_FOOD) {
            n = 2;
            if (!g_Player.unk72 && PLAYER.step_s == 2) {
                n = atLedge;
                SetPlayerStep(Player_Stand);
            }
            SetPlayerAnim(e->playerAnim + n);
        }
    }
    ModHolderReduce(id);
    return true;
}

// w_021 func_97000_8017AB54 (BottomCornerText, its lower left form): the
// Meal ticket's "got" item name
static void ModBottomCornerText(const u8* str) {
    u8 toPrint[64];
    Primitive* prim;
    s32 i;
    u32 ch;
    u32 ch2;
    u8* chIdx = toPrint;
    s16 textWidth = 0;
    u16 n = 0;

    for (i = 0; i < 64; i++) {
        *chIdx++ = 0;
    }
    chIdx = toPrint;
    while (1) {
        ch = *str++;
        if (ch == 0xFF) {
            ch = *str++;
            if (ch == 0) {
                break;
            }
        }
        *chIdx = ch;
        chIdx++;
        textWidth += 1;
        if (ch != 0) {
            n += 1;
        }
        if (*str == 0xFF) {
            str++;
            ch = *str++;
            ch2 = ch & 0xFF;
            if (ch2 == 0) {
                break;
            }
            if (ch2 != 0xFF) {
                *chIdx = ch;
                n += 1;
            } else {
                str -= 2;
            }
        }
        chIdx++;
    }

    g_unkGraphicsStruct.BottomCornerTextPrims =
        AllocPrimitives(PRIM_SPRT, n + 4);
    if (g_unkGraphicsStruct.BottomCornerTextPrims == -1) {
        return;
    }

    prim = &g_PrimBuf[g_unkGraphicsStruct.BottomCornerTextPrims];
    prim->type = PRIM_G4;
    prim->b0 = prim->b1 = prim->b2 = prim->b3 = prim->g0 = prim->g1 = prim->g2 =
        prim->g3 = prim->r0 = prim->r1 = prim->r2 = prim->r3 = 0;
    prim->b0 = prim->b1 = 0xAF;
    textWidth <<= 3;
    textWidth |= 4;
    n = 7; // x
    prim->x0 = prim->x2 = n;
    prim->x1 = prim->x3 = n + textWidth + 0x20;
    prim->y0 = prim->y1 = 0xD0;
    prim->y2 = prim->y3 = 0xDF;
    prim->priority = 0x1EE;
    prim->drawMode = DRAW_TPAGE | DRAW_TRANSP;
    prim = prim->next;

    prim->tpage = 0x1F;
    prim->clut = PAL_UNK_197;
    prim->x0 = n - 6;
    prim->y0 = 0xCB;
    prim->u0 = 0x80;
    prim->v0 = 0;
    prim->u1 = 0x10;
    prim->v1 = 0x18;
    prim->priority = 0x1EF;
    prim->drawMode = DRAW_DEFAULT;
    prim = prim->next;

    prim->tpage = 0x1F;
    prim->clut = PAL_UNK_197;
    prim->x0 = n + textWidth + 0x16;
    prim->y0 = 0xCB;
    prim->u0 = 0xA8;
    prim->v0 = 0;
    prim->u1 = 0x10;
    prim->v1 = 0x18;
    prim->priority = 0x1EF;
    prim->drawMode = DRAW_DEFAULT;
    prim = prim->next;

    prim->type = PRIM_GT4;
    prim->y0 = prim->y1 = 0xCD;
    prim->tpage = 0x1F;
    prim->clut = PAL_UNK_197;
    prim->y2 = prim->y3 = 0xE1;
    prim->u0 = prim->u2 = 0x98;
    prim->u1 = prim->u3 = 0x9C;
    prim->v0 = prim->v1 = 2;
    prim->x0 = prim->x2 = n + 0xA;
    prim->x1 = prim->x3 = n + textWidth + 0x18;
    prim->v2 = prim->v3 = 0x16;
    prim->priority = 0x1EF;
    prim->drawMode = DRAW_DEFAULT;

    n += 0x10;
    chIdx = toPrint;
    for (prim = prim->next; prim != NULL;) {
        ch = *chIdx++;
        if (ch != 0) {
            prim->u0 = (ch & 0xF) * 8;
            prim->v0 = (ch & 0xF0) >> 1;
            prim->tpage = 0x1E;
            prim->clut = PAL_UNK_196;
            prim->v1 = 8;
            prim->u1 = 8;
            prim->priority = 0x1F0;
            prim->drawMode = DRAW_DEFAULT;
            prim->x0 = n;
            prim->y0 = 0xD4;
            prim = prim->next;
        }
        ch = *chIdx++;
        if (ch != 0) {
            prim->u0 = (ch & 0xF) * 8;
            prim->v0 = (ch & 0xF0) >> 1;
            prim->tpage = 0x1E;
            prim->clut = PAL_UNK_196;
            prim->v1 = 8;
            prim->u1 = 8;
            prim->priority = 0x1F0;
            prim->drawMode = DRAW_DEFAULT;
            prim->x0 = n;
            prim->y0 = 0xCC;
            prim = prim->next;
        }
        if ((u32)((ch + 0x10) & 0xFF) < 0xDU) {
            n += 0xC;
        } else {
            n += 8;
        }
    }
    g_unkGraphicsStruct.BottomCornerTextTimer = 0x130;
}

// w_021 func_97000_8017AF2C: caught (eaten / picked up)?
static s32 ModFoodCaught(Entity* self, s32 landed) {
    s16 xDist;
    s16 yDist;

    if (!landed) {
        if (self->velocityY < 0 || self->velocityY < FIX(2)) {
            return 0;
        }
    }
    // peanuts: only into the mouth (the catch pose)
    if (((self->params >> 8) & 0x7F) == MOD_FOOD_PEANUTS) {
        if ((PLAYER.animCurFrame != 0x7B) || (PLAYER.ext.player.anim != 0) ||
            (self->posY.i.hi > (PLAYER.posY.i.hi - 16)) ||
            (self->posY.i.hi < (PLAYER.posY.i.hi - 24)) ||
            (self->posX.i.hi > (PLAYER.posX.i.hi + 4)) ||
            (self->posX.i.hi < (PLAYER.posX.i.hi - 4))) {
            return 0;
        }
        PLAYER.pose = 3;
        PLAYER.poseTimer = 8;
        return 1;
    }
    xDist = abs(self->posX.i.hi - PLAYER.posX.i.hi + PLAYER.hitboxOffX);
    yDist = abs(self->posY.i.hi - PLAYER.posY.i.hi + PLAYER.hitboxOffY);
    if (xDist > (self->hitboxWidth + PLAYER.hitboxWidth)) {
        return 0;
    }
    if (yDist > (self->hitboxHeight + PLAYER.hitboxHeight)) {
        return 0;
    }
    return 1;
}

// the food's quad: its equip icon (the prize's in its own slot), centred
static void ModFoodDraw(Entity* self, s32 show) {
    Primitive* prim = &g_PrimBuf[self->primIndex];
    Equipment* e = &g_EquipDefs[self->ext.food.foodId];
    s32 slot = self->ext.food.unk7E ? MOD_HOLDER_PRIZE_ICON : MOD_HOLDER_ICON;

    LoadEquipIcon(e->icon, e->iconPalette, slot);
    prim->tpage = 0x1A;
    prim->clut = 0x1D0 + slot;
    prim->u0 = prim->u2 = (slot & 7) * 16;
    prim->u1 = prim->u3 = prim->u0 + 16;
    prim->v0 = prim->v1 = ((slot & 0xF8) * 2) | 0x80;
    prim->v2 = prim->v3 = prim->v0 + 16;
    prim->x0 = prim->x2 = self->posX.i.hi - 8;
    prim->x1 = prim->x3 = self->posX.i.hi + 8;
    prim->y0 = prim->y1 = self->posY.i.hi - 8;
    prim->y2 = prim->y3 = self->posY.i.hi + 8;
    prim->priority = self->zPriority;
    prim->drawMode = show ? DRAW_DEFAULT : DRAW_HIDE;
}

// food eaten: heal by its attack once the player's heal is free (w_021)
static s32 ModFoodEat(Entity* self) {
    if (g_Player.healKind == 0) {
        g_Player.healKind = 1;
        g_Player.healAmount = self->attack;
        DestroyEntity(self);
        return 1;
    }
    return 0;
}

// caught: the Meal ticket's prize is picked up (w_021 func_ptr_80170004),
// a toadstool stuns (padSim: down in the air, none on the floor), other food
// is eaten; true if the entity is gone
static s32 ModFoodGot(Entity* self, s32 food, s32 padSim) {
    if (self->ext.food.unk7E) {
        if (g_unkGraphicsStruct.BottomCornerTextTimer) {
            FreePrimitives(g_unkGraphicsStruct.BottomCornerTextPrims);
            g_unkGraphicsStruct.BottomCornerTextTimer = 0;
        }
        PlaySfx(SFX_ITEM_PICKUP);
        AddToInventory(self->ext.food.foodId, EQUIP_HAND);
        ModBottomCornerText((const u8*)g_EquipDefs[self->ext.food.foodId].name);
        DestroyEntity(self);
        return 1;
    }
    if (food == MOD_FOOD_TOADSTOOL) {
        self->ext.food.timer = 12;
        self->step = 4;
        g_Player.padSim = padSim;
        g_Player.demo_timer = 20; // stunned for 20 frames
        return 0;
    }
    if (ModFoodEat(self)) {
        return 1;
    }
    self->ext.food.timer = 8;
    self->step = 3;
    return 0;
}

// Saturn ALUCARD entity 0x48 (func_060B25F4) = w_021 EntityWeaponAttack, and
// the Meal ticket's prize (func_ptr_80170004: the same toss and bounces,
// ext.food.unk7E set, foodId = the prize, picked up instead of eaten)
void EntityModHolderFood(Entity* self) {
    Collider col;
    s16 shift;
    s32 v;
    s32 food = (self->params >> 8) & 0x7F;

    switch (self->step) {
    case 0:
        self->primIndex = AllocPrimitives(PRIM_GT4, 1);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->flags = FLAG_POS_CAMERA_LOCKED | FLAG_HAS_PRIMS;
        self->zPriority = PLAYER.zPriority + 2;
        self->hitboxHeight = self->hitboxWidth = 8;
        self->attack = g_EquipDefs[self->ext.food.foodId].attack;
        if (food == MOD_FOOD_MEAL_TICKET) {
            // a random food of the 41, or the Saturn's two (as w_021)
            food = rand() % 43;
            self->ext.food.foodId =
                food < 41 ? ITEM_ORANGE + food : ITEM_SMELLY_RICE + food - 41;
            self->ext.food.unk7E = 1;
        }
        self->posY.i.hi -= 4;
        v = (rand() & 0x3FFF) + FIX(0.875); // 1 +- 0.125
        self->velocityY = FIX(-2);
        if (food == MOD_FOOD_PEANUTS && !self->ext.food.unk7E) {
            // a peanut flies up, to be caught in the mouth
            self->posX.i.hi += PLAYER.facingLeft ? -8 : 8;
            v = (rand() & 0xFFF) | FIX(1.0 / 16);
            self->velocityY = self->ext.food.unk80 = FIX(-5.5);
        }
        self->velocityX = PLAYER.facingLeft ? -v : v;
        self->step++;
        break;
    case 1: // in the air
        if (ModFoodCaught(self, 0)) {
            if (ModFoodGot(self, food, PAD_DOWN)) {
                return;
            }
            break;
        }
        self->posX.val += self->velocityX;
        self->posY.val += self->velocityY;
        self->velocityY += FIX(20.0 / 128);
        CheckCollision(self->posX.i.hi, (s16)(self->posY.i.hi + 8), &col, 0);
        if (col.effects & EFFECT_SOLID) {
            self->posY.i.hi += col.unk18;
            if (food == MOD_FOOD_PEANUTS && !self->ext.food.unk7E) {
                // bounces, then is gone
                self->ext.food.unk80 /= 2;
                self->velocityY = self->ext.food.unk80;
                if (self->velocityY > FIX(-1)) {
                    DestroyEntity(self);
                    return;
                }
                self->posY.val += self->velocityY;
            } else {
                self->ext.food.timer = self->ext.food.unk7E ? 0x200 : 0x80;
                self->step++;
                break;
            }
        }
        shift = self->velocityX < 0 ? -4 : 4;
        CheckCollision(
            (s16)(shift + self->posX.i.hi), self->posY.i.hi, &col, 0);
        if (col.effects & EFFECT_SIDE) {
            self->posX.i.hi += shift < 0 ? col.unkC : col.unk4;
            self->velocityX /= -2;
            if (food == MOD_FOOD_PEANUTS && !self->ext.food.unk7E) {
                DestroyEntity(self);
                return;
            }
        }
        shift = food == MOD_FOOD_PEANUTS ? -4 : -8;
        CheckCollision(
            self->posX.i.hi, (s16)(shift + self->posY.i.hi), &col, 0);
        if (col.effects & EFFECT_SOLID) {
            self->posY.i.hi += 1 + col.unk20;
            self->velocityX /= 2;
            self->velocityY = FIX(1);
        }
        break;
    case 2: // on the floor (food slides to a stop)
        if (!self->ext.food.unk7E) {
            v = self->velocityX;
            if (v > FIX(36.0 / 128)) {
                v -= FIX(36.0 / 128);
            } else if (v < -FIX(36.0 / 128)) {
                v += FIX(36.0 / 128);
            } else {
                v = 0;
            }
            self->velocityX = v;
            self->posX.val += v;
        }
        if (ModFoodCaught(self, 1)) {
            if (ModFoodGot(self, food, 0)) {
                return;
            }
            break;
        }
        if (--self->ext.food.timer == 0) {
            DestroyEntity(self);
            return;
        }
        break;
    case 3: // eaten, waiting for the heal to be free
        if (--self->ext.food.timer == 0) {
            if (ModFoodEat(self)) {
                return;
            }
            self->ext.food.timer = 8;
        }
        break;
    case 4: // the toadstool: the player takes a hit
        if (--self->ext.food.timer == 0) {
            PLAYER.hitParams = 0x81;
            PLAYER.hitPoints = 0;
            PLAYER.hitboxState = 0;
            DestroyEntity(self);
            return;
        }
        break;
    }
    // hidden once eaten; blinks in its last frames on the floor
    ModFoodDraw(
        self, self->step < 3 && !(self->step == 2 &&
                                  self->ext.food.timer <= 24 && (g_Timer & 1)));
}
#endif
