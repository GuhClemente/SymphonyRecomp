// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Cursed Prison floor switch, sinking stone block and dust (Saturn STAGE_16
 * entities 0x16 func_060DC894, 0x17 func_060DC9E0 + 060DCC58, 0x15
 * func_060DC60C + 060DC734), ported to the PRS overlay (Saturn port mod,
 * translated from the SH-2). saturn_switch.c is the Saturn-semantics
 * reference and notes.md has the details.
 *
 * The Saturn code is itself a port of the PS1 NO3/NP3 heart-room switch and
 * gold door (src/st/e_heartroom.h) and of DoorCascadePhysics: same speeds,
 * timers, sounds and explosion. The Saturn dropped the door's tile writes
 * (060DCC58 is an empty function) and keeps the block solid only with
 * GetPlayerCollisionWith(10, 56, sides + top). This port does both: the
 * same GetPlayerCollisionWith call, plus solid tiles in the room's fg
 * tilemap while the block is up, opened row by row as it sinks (the way
 * EntityHeartRoomGoldDoor does), so that nothing passes the block.
 *
 * Include it from one of the stage's .c files (for example e_room_bg.c or
 * e_misc.c), after prs.h and with st_common.h's helpers built in the stage.
 *
 * Required from the including stage:
 *   enum EntityID entries:
 *     E_MOD_PRS_DUST    (Saturn 0x15) -> EntityModPrsDust
 *     E_MOD_PRS_SWITCH  (Saturn 0x16) -> EntityModPrsSwitch
 *     E_MOD_PRS_BLOCK   (Saturn 0x17) -> EntityModPrsBlock
 *     E_INTENSE_EXPLOSION (Saturn 6; already in prs.h)
 *   MOD_PRS_SWITCH_ANIMSET  animSet of the converted Saturn sprite bank 19
 *                           (g_Stage16SpriteBank19 @ 0x060E900C, frame table
 *                           0x060E9000, 3 frames, PS1 frame n = Saturn frame
 *                           n: 1 = block 24x112, 2 = plate 24x10)
 *   MOD_PRS_SWITCH_UNK5A    (optional, default 0) unk5A of that animset
 *   MOD_PRS_SWITCH_PAL      (optional, default 0) entity palette; the bank
 *                           has one palette, carried by the converted frames
 *   MOD_BLOCK_SOLID_TILE    a PRS fg tile id with blank art and collision 3
 *                           (solid); PRS's red door tile 0x11E is such a tile
 *   MOD_PRS_SWITCH_FLAG     (optional, default 0xF9) g_CastleFlags index
 *   Sounds: SFX_SWITCH_CLICK (0x676) and SFX_STONE_MOVE_A (0x607), same ids
 *   as the Saturn's, from include/sfx.h.
 *
 * Layout (room 0): E_MOD_PRS_BLOCK at (152,616) in slot 0x14 and
 * E_MOD_PRS_SWITCH at (192,670) in slot 0x16. The block creates the dust in
 * self + 1 (slot 0x15, which the layout leaves free) and destroys it there.
 * Explosions use AllocEntity 224..256.
 */

#ifndef MOD_PRS_SWITCH_ANIMSET
#error "define MOD_PRS_SWITCH_ANIMSET (Saturn bank 19) before e_mod_switch.h"
#endif
#ifndef MOD_BLOCK_SOLID_TILE
#error "define MOD_BLOCK_SOLID_TILE (blank art, collision 3)"
#endif
#ifndef MOD_PRS_SWITCH_UNK5A
#define MOD_PRS_SWITCH_UNK5A 0
#endif
#ifndef MOD_PRS_SWITCH_PAL
#define MOD_PRS_SWITCH_PAL 0
#endif
#ifndef MOD_PRS_SWITCH_FLAG
#define MOD_PRS_SWITCH_FLAG 0xF9 // Saturn g_CastleFlags[0xF9], unused on PS1
#endif

#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif

// TODO Z: the Saturn gives both sprites z 96, the room fg's own value. 0x5E
// is the PS1 heart room's value: under the fg (0x60), so that the floor
// tiles hide the sunk block and the pressed plate.
#ifndef MOD_PRS_SWITCH_Z
#define MOD_PRS_SWITCH_Z 0x5E
#endif
// TODO Z: the dust prims' priority, Saturn 0x97 (above Alucard's 0x94);
// the PS1 heart room door uses its own z + 0x18
#ifndef MOD_PRS_DUST_Z
#define MOD_PRS_DUST_Z 0x97
#endif
// The Saturn line colour 0xB208 (RGB555 R 8, G 16, B 12). The PS1 heart room
// and cavern doors use r 64, g 96, b 128 (bluish); the Saturn's is greener.
#ifndef MOD_PRS_DUST_R
#define MOD_PRS_DUST_R 64
#define MOD_PRS_DUST_G 128
#define MOD_PRS_DUST_B 96
#endif

#define MOD_PRS_SFX_CLICK SFX_SWITCH_CLICK // 0x676
#define MOD_PRS_SFX_STONE SFX_STONE_MOVE_A // 0x607

// Saturn world y values (vertical units are the same on both consoles)
#define MOD_PRS_SWITCH_DOWN_Y 674 // 0x2A2: the plate pressed
#define MOD_PRS_BLOCK_DOWN_Y 728  // 0x2D8: the block sunk
#define MOD_PRS_FLOOR_Y 672       // 0x2A0: dust and explosions start here
// the block's solid tiles: one column (Saturn width 2*10 = PS1 16 px) of
// 112 / 16 rows, from its top edge (posY - 56)
#define MOD_PRS_BLOCK_HALF_H 56
#define MOD_PRS_BLOCK_ROWS 7

// st_common.h (built in the stage's st_common.c) and create_entity.h
u8 GetPlayerCollisionWith(Entity* self, u16 w, u16 h, u16 flags);
void CreateEntityFromEntity(u16 entityId, Entity* source, Entity* entity);

typedef struct {
    EntranceCascadePrim* prim; // the dust: Saturn ext.prim (+0x78)
    s32 timer;                 // the block: Saturn +0x7C (jiggle, sfx)
    s16 baseY;                 // the block: world y at init (its layout y)
    u16 tileBase;              // the block: fg index of its top solid tile
} ET_ModPrsSwitch;
#define PRS_SW_EXT(e) ((ET_ModPrsSwitch*)&(e)->ext)

// The original fg tiles under the block, saved before the first solid write.
// Initialised data (not bss), so it is reloaded with the overlay, as the
// tilemap it mirrors is; savedAt = the fg index they were read from.
static u16 g_ModPrsBlockSaved[MOD_PRS_BLOCK_ROWS] = {0};
static s16 g_ModPrsBlockSavedAt = -1;

static void ModPrsSwitchInit(u16 frame) {
    u16 init[5];

    init[0] = MOD_PRS_SWITCH_ANIMSET;
    init[1] = frame;
    init[2] = MOD_PRS_SWITCH_UNK5A;
    init[3] = MOD_PRS_SWITCH_PAL;
    init[4] = ENEMY_INTERACTABLE; // Saturn TekiInit(self, 5)
    InitializeEntity(init);       // step 0 -> 1, like the Saturn's step++
}

// Replaces the Saturn's empty func_060DCC58: the first openRows rows (from
// the top) get their original tiles back, the others are solid.
static void ModPrsBlockTiles(Entity* self, s32 openRows) {
    s32 i;
    s32 tile;
    s32 stride;

    stride = g_Tilemap.hSize * 16;
    tile = PRS_SW_EXT(self)->tileBase;
    for (i = 0; i < MOD_PRS_BLOCK_ROWS; i++) {
        if (i < openRows) {
            g_Tilemap.fg[tile] = g_ModPrsBlockSaved[i];
        } else {
            g_Tilemap.fg[tile] = MOD_BLOCK_SOLID_TILE;
        }
        tile += stride;
    }
}

static void ModPrsBlockSetupTiles(Entity* self) {
    s32 i;
    s32 x;
    s32 y;
    s32 tile;
    s32 stride;

    stride = g_Tilemap.hSize * 16;
    x = self->posX.i.hi + g_Tilemap.scrollX.i.hi;
    y = self->posY.i.hi + g_Tilemap.scrollY.i.hi;
    PRS_SW_EXT(self)->baseY = y;
    y -= MOD_PRS_BLOCK_HALF_H;
    tile = (x >> 4) + (y >> 4) * stride;
    PRS_SW_EXT(self)->tileBase = tile;
    if (g_ModPrsBlockSavedAt != tile) {
        g_ModPrsBlockSavedAt = tile;
        for (i = 0; i < MOD_PRS_BLOCK_ROWS; i++) {
            g_ModPrsBlockSaved[i] = g_Tilemap.fg[tile];
            tile += stride;
        }
    }
}

// 0x16 floor switch (Saturn func_060DC894)
void EntityModPrsSwitch(Entity* self) {
    u8 col;

    col = 0;
    if (self->step == 1) {
        col = GetPlayerCollisionWith(
            self, self->hitboxWidth, self->hitboxHeight, 4); // top only
    }
    switch (self->step) {
    case 0:
        ModPrsSwitchInit(2);                // frame 2: the plate
        self->zPriority = MOD_PRS_SWITCH_Z; // TODO Z (Saturn 96)
        self->hitboxWidth = SAT_X(8);       // Saturn 8
        self->hitboxHeight = 6;
        if (g_CastleFlags[MOD_PRS_SWITCH_FLAG]) {
            self->posY.i.hi = MOD_PRS_SWITCH_DOWN_Y - g_Tilemap.scrollY.i.hi;
            self->step = 2;
        }
        break;
    case 1:
        if (col) {
            PLAYER.posY.i.hi++;
            self->posY.val += FIX(0.25);
            if (self->posY.i.hi + g_Tilemap.scrollY.i.hi >
                MOD_PRS_SWITCH_DOWN_Y) {
                self->posY.i.hi =
                    MOD_PRS_SWITCH_DOWN_Y - g_Tilemap.scrollY.i.hi;
                g_CastleFlags[MOD_PRS_SWITCH_FLAG] = 1;
                g_api.PlaySfx(MOD_PRS_SFX_CLICK);
                self->step++;
            }
        }
        break;
    }
}

// 0x17 sinking block (Saturn func_060DC9E0)
void EntityModPrsBlock(Entity* self) {
    Entity* e;
    u8 col;
    s32 rows;

    // every frame, before the step switch: sides (1) + top (4)
    col = GetPlayerCollisionWith(self, SAT_X(10), MOD_PRS_BLOCK_HALF_H, 5);

    switch (self->step) {
    case 0:
        ModPrsSwitchInit(1);                // frame 1: the block
        self->zPriority = MOD_PRS_SWITCH_Z; // TODO Z (Saturn 96)
        ModPrsBlockSetupTiles(self);
        if (g_CastleFlags[MOD_PRS_SWITCH_FLAG]) {
            self->posY.i.hi = MOD_PRS_BLOCK_DOWN_Y - g_Tilemap.scrollY.i.hi;
            self->step = 0x80;
            ModPrsBlockTiles(self, MOD_PRS_BLOCK_ROWS); // Saturn 060DCC58(1)
        } else {
            ModPrsBlockTiles(self, 0); // Saturn 060DCC58(0)
        }
        break;

    case 1:
        if (g_CastleFlags[MOD_PRS_SWITCH_FLAG]) {
            g_api.PlaySfx(MOD_PRS_SFX_STONE);
            // Saturn 060DCC58(1): the rows open as the block sinks (step 2)
            CreateEntityFromEntity(E_MOD_PRS_DUST, self, self + 1);
            // the Saturn writes a raw 672 here; the dust never reads its
            // posY, so the port keeps it on screen instead
            (self + 1)->posY.i.hi = MOD_PRS_FLOOR_Y - g_Tilemap.scrollY.i.hi;
            self->step++;
        }
        break;

    case 2:
        self->posY.val += FIX(0.375);
        PRS_SW_EXT(self)->timer++;
        // it jiggles left and right while it sinks
        if (PRS_SW_EXT(self)->timer & 1) {
            self->posX.i.hi++;
        } else {
            self->posX.i.hi--;
        }
        if (!(PRS_SW_EXT(self)->timer & 0xF)) {
            g_api.PlaySfx(MOD_PRS_SFX_STONE);
        }
        if (col & 4) {
            // carry the player standing on top down with it
            PLAYER.posY.i.hi++;
            g_unkGraphicsStruct.shoveY.i.hi++;
        }
        if (self->posY.i.hi + g_Tilemap.scrollY.i.hi >
            MOD_PRS_BLOCK_DOWN_Y - 1) {
            self->posY.i.hi = MOD_PRS_BLOCK_DOWN_Y - g_Tilemap.scrollY.i.hi;
            DestroyEntity(self + 1); // the dust
            self->step++;            // 3: idle
        }
        // PS1 only: the rows the block's top has passed are open again
        rows =
            self->posY.i.hi + g_Tilemap.scrollY.i.hi - PRS_SW_EXT(self)->baseY;
        rows /= 16;
        if (self->step != 2 || rows > MOD_PRS_BLOCK_ROWS) {
            rows = MOD_PRS_BLOCK_ROWS;
        }
        ModPrsBlockTiles(self, rows);

        if (!(g_Timer & 0xF)) {
            e = AllocEntity(&g_Entities[224], &g_Entities[256]);
            if (e != NULL) {
                CreateEntityFromEntity(E_INTENSE_EXPLOSION, self, e);
                e->posY.i.hi = MOD_PRS_FLOOR_Y - g_Tilemap.scrollY.i.hi;
                e->posX.i.hi += SAT_X((Random() & 0xF) - 8);
                e->params = 0x10;
                e->drawFlags |= ENTITY_SCALEX | ENTITY_SCALEY;
                e->scaleX = e->scaleY = 192;
            }
        }
        break;
    }
}

// one dust particle (Saturn func_060DC734; PS1 DoorCascadePhysics). The
// Saturn keeps the fixed-point position in the prim's SpGourTbl entry; here
// it is x0/y0 + x1/y1 (fraction), as PS1 DoorCascadePhysics does.
static void ModPrsDustParticle(Entity* self, EntranceCascadePrim* prim) {
    s32 dx;
    s32 x;
    s32 y;

    switch (prim->p3) {
    case 1:
        dx = (Random() & 0xF) - 8; // Saturn px
        prim->x0 = self->posX.i.hi + SAT_X(dx);
        prim->y0 = MOD_PRS_FLOOR_Y - g_Tilemap.scrollY.i.hi;
        prim->x1 = 0;
        prim->y1 = 0;
        // Saturn (Random() & 0xFF) * 80 = the PS1's * 64, times 5/4
        if (dx > 0) {
            prim->velocityX.val = SAT_X((Random() & 0xFF) * 80);
        } else {
            prim->velocityX.val = -SAT_X((Random() & 0xFF) * 80);
        }
        prim->velocityY.val = ((Random() & 0xFF) << 8) - FIX(4);
        prim->drawMode = DRAW_UNK02;
        prim->p3++;
        /* fallthrough */
    case 2:
        x = (prim->x0 << 16) + (u16)prim->x1;
        y = (prim->y0 << 16) + (u16)prim->y1;
        x += prim->velocityX.val;
        y += prim->velocityY.val;
        prim->x0 = x >> 16;
        prim->x1 = x & 0xFFFF;
        prim->y0 = y >> 16;
        prim->y1 = y & 0xFFFF;
        prim->velocityY.val += FIX(0.25);
        if (prim->velocityY.val > FIX(6)) {
            prim->p3++;
        }
        break;
    case 3:
        prim->drawMode = DRAW_HIDE;
        prim->p3 = 0;
        break;
    }
}

// 0x15 dust (Saturn func_060DC60C): 64 one-pixel prims, one launched every
// other frame from the floor at the block's foot
void EntityModPrsDust(Entity* self) {
    EntranceCascadePrim* prim;
    s32 primIndex;
    u16 init[5];

    switch (self->step) {
    case 0:
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = ENEMY_INTERACTABLE; // Saturn TekiInit(self, 5)
        InitializeEntity(init);
        // Saturn AllocPrimitives(5, 64): lines with both ends on one point
        primIndex = g_api.AllocPrimitives(PRIM_TILE, 64);
        if (primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->flags |= FLAG_HAS_PRIMS;
        self->primIndex = primIndex;
        prim = (EntranceCascadePrim*)&g_PrimBuf[primIndex];
        PRS_SW_EXT(self)->prim = prim;
        while (prim != NULL) {
            ((Primitive*)prim)->u0 = 1; // a 1x1 tile
            ((Primitive*)prim)->v0 = 1;
            ((Primitive*)prim)->r0 = MOD_PRS_DUST_R;
            ((Primitive*)prim)->g0 = MOD_PRS_DUST_G;
            ((Primitive*)prim)->b0 = MOD_PRS_DUST_B;
            prim->priority = MOD_PRS_DUST_Z; // TODO Z (Saturn 0x97)
            prim->drawMode = DRAW_HIDE;
            prim->p3 = 0;
            prim = prim->next;
        }
        break;
    case 1:
        if (!(g_Timer & 1)) {
            for (prim = PRS_SW_EXT(self)->prim; prim != NULL;
                 prim = prim->next) {
                if (!prim->p3) {
                    break;
                }
            }
            if (prim != NULL) {
                prim->p3 = 1;
            }
        }
        break;
    }
    if (self->flags & FLAG_HAS_PRIMS) {
        for (prim = PRS_SW_EXT(self)->prim; prim != NULL; prim = prim->next) {
            if (prim->p3) {
                ModPrsDustParticle(self, prim);
            }
        }
    }
}
