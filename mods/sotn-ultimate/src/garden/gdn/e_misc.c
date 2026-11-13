// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gdn.h"

#include "../../../../../src/st/e_misc.h"

// Saturn port: the poison dripping from the greenhouse ceilings (STAGE_15
// entities 0x1E, 0x060DF160, and 0x1F, 0x060DF204). Each spawner lets a drop
// fall on its own period; the drop is enemy 82 (attack 5, poison), the same
// record on both consoles. It falls 2 px a frame and splashes on the floor.
static u16 g_ModDripPeriods[] = {200, 280, 190, 240};
static u8 g_ModDripFall[] = {3, 1, 3, 2, 3, 3, 0, 0};
static u8 g_ModDripSplash[] = {3, 4, 3, 5, 3, 6, 3, 7, 3, 8, 0xFF, 0};
static EInit g_EInitModDripSpawner = {ANIMSET_DRA(0), 0, 0, PAL_NONE, 5};
static EInit g_EInitModDrip = {ANIMSET_OVL(4), 1, 0, PAL_NONE, 82};

void EntityModDripSpawner(Entity* self) {
    Entity* entity;

    switch (self->step) {
    case 0:
        // enemy 5 as on the Saturn: unlike ENEMY_SPAWNER (4) its record has
        // FLAG_POS_CAMERA_LOCKED, which keeps it in place as the room scrolls
        InitializeEntity(g_EInitModDripSpawner);
        // fall through
    case 1:
        self->ext.timer.t = g_ModDripPeriods[self->params & 3];
        self->step++;
        break;
    case 2:
        if (--self->ext.timer.t == 0) {
            entity = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (entity != NULL) {
                CreateEntityFromEntity(E_MOD_DRIP, self, entity);
            }
            self->step = 1;
        }
        break;
    }
}

void EntityModDrip(Entity* self) {
    Collider collider;

    switch (self->step) {
    case 0:
        InitializeEntity(g_EInitModDrip);
        self->velocityY = FIX(2);
        // in front, with Alucard (Saturn 0x60 is the sprites' layer there)
        self->zPriority = g_unkGraphicsStruct.g_zEntityCenter;
        break;
    case 1:
        MoveEntity();
        AnimateEntity(g_ModDripFall, self);
        g_api.CheckCollision(self->posX.i.hi, self->posY.i.hi, &collider, 0);
        if (collider.effects & EFFECT_SOLID) {
            // Saturn port: the Saturn plays 0x7CA here, which the Garden's
            // sound bank doesn't have: silent
            // (mods/sotn-ultimate/tools/gen_gdn_sounds.py)
            EntityExplosionVariantsSpawner(self, 1, 2, 0, 0, 5, 0);
            self->hitboxState = 0;
            self->animCurFrame = 4;
            self->pose = 0;
            self->poseTimer = 0;
            self->step++;
        }
        break;
    case 2:
        if (!AnimateEntity(g_ModDripSplash, self)) {
            DestroyEntity(self);
        }
        break;
    }
}
