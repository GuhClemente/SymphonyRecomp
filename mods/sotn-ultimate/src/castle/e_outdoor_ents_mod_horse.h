// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/no3/e_outdoor_ents.c (at file scope, in
// place) Saturn port: Richter and Maria ride into the castle (Saturn STAGE_0X
// 0x060DC300, which the Saturn's entity 0x4E runs for any character but
// Alucard). One sprite shows the horse with its rider while the player is
// hidden and pushed as in Alucard's run; at the gate the rider leaps off and
// the horse stays there rearing. Saturn x speeds and scroll thresholds are
// x 4/5 (the PS1 values of Alucard's run below); y is unscaled. The art,
// frames and palettes are rodata (mod_horse.h, in e_red_door.c).
#include "../../../../src/st/no3/mod_horse.h"
extern u8 g_ModHorseLz[];
extern ModHorseStrip g_ModHorseStrips[];
extern s16* g_ModHorseSprites[];
extern u16 g_ModHorsePals[];

// the Saturn's hoofbeat in NO3's bank, in the slot of the unused
// SFX_SCIFI_BLAST (mods/sotn-ultimate/tools/gen_no3_sounds.py). The Saturn
// starts its ride sound (a PCM stream) here every 32 frames and stops it at
// the gate.
#define MOD_SFX_HORSE_GALLOP 0x7D1
// the Saturn's neigh at the gate, in the bank Richter and Maria get for NO3
// (BIN/NO3RIDE.BIN, src/dra/cd.c; mods/sotn-ultimate/tools/gen_no3_sounds.py),
// in the slot of the unused SFX_UNUSED_79E
#define MOD_SFX_HORSE_NEIGH 0x79E

// Saturn animations ({ticks, frame}): gallop, leap, in the air
static const u8 mod_anim_ric_gallop[] = {4, 1, 4, 2, 4, 3, 4, 4, 4, 5, 0, 0};
static const u8 mod_anim_ric_leap[] = {4, 8, 0xFF, 0};
static const u8 mod_anim_ric2_gallop[] = {
    4, 25, 4, 26, 4, 27, 4, 28, 4, 29, 0, 0};
static const u8 mod_anim_ric2_leap[] = {4, 30, 0xFF, 0};
static const u8 mod_anim_mar_gallop[] = {
    4, 9, 4, 10, 4, 11, 4, 12, 4, 13, 0, 0};
static const u8 mod_anim_mar_leap[] = {
    4, 14, 4, 15, 4, 16, 4, 17, 4, 18, 4, 19, 4, 20, 4, 21, 4, 22, 0xFF, 0};
static const u8 mod_anim_mar_air[] = {4, 23, 4, 24, 0, 0};
// the riderless horse rearing (Saturn BackgroundBlockInit 21/22)
static const u8 mod_anim_rear[] = {2, 6, 2, 7, 0xFF, 0};

// the ride's state, in the entity's ext
typedef struct {
    s32 speed;
    s32 velocityY;
    s32 posY;
    s32 ground;
    u8* gallop;
    u8* leap;
    u8* air;
    s16 strip;
} ModHorseState;

static u32 mod_horse_buf[MOD_HORSE_STRIP_BYTES / 4];

// the MARIA.BIN LZSS format (mods/sotn-ultimate/tools/gen_mar_assets.py lzss)
static void ModHorseUnpack(u8* dst, u8* src, s32 size) {
    u8* end;
    u8* p;
    s32 flags;
    s32 bit;
    u32 v;

    end = dst + size;
    while (dst < end) {
        flags = *src++;
        for (bit = 0; bit < 8 && dst < end; bit++, flags >>= 1) {
            if (flags & 1) {
                *dst++ = *src++;
            } else {
                v = src[0] | (src[1] << 8);
                src += 2;
                p = dst - (v & 0xFFF) - 1;
                for (v = (v >> 12) + 3; v; v--) {
                    *dst++ = *p++;
                }
            }
        }
    }
}

// one strip of the art (32 quadrant rows) into VRAM per call
static void ModHorseUploadStrip(ModHorseState* ride) {
    ModHorseStrip* s;
    RECT rect;

    if (ride->strip >= MOD_HORSE_NSTRIPS) {
        return;
    }
    s = &g_ModHorseStrips[ride->strip++];
    ModHorseUnpack((u8*)mod_horse_buf, &g_ModHorseLz[s->off], s->rows * 64);
    rect.x = s->x;
    rect.y = s->y;
    rect.w = 32;
    rect.h = s->rows;
    // the buffer is reused for the next strip one frame later: the main loop
    // waits for the GPU queue (DrawSync) every frame
    LoadImage(&rect, (u_long*)mod_horse_buf);
}

static void ModHorseInit(Entity* self) {
    RECT rect;
    s32 i;

    // the sprite bank (NO3's banks 10+ are empty), in the table the stage
    // header gives DRA: with the Garden shaft that is the rodata copy
    // g_ModSpriteBanks (castle/no3/header_mod_shaft.h), not spriteBanks
    g_api.o.spriteBanks[MOD_HORSE_BANK] = (SpriteParts*)g_ModHorseSprites;
    // the palettes (RAM copy and VRAM)
    for (i = 0; i < MOD_HORSE_NPALS * COLORS_PER_PAL; i++) {
        ((u16*)g_Clut)[MOD_HORSE_PAL * COLORS_PER_PAL + i] = g_ModHorsePals[i];
    }
    for (i = 0; i < MOD_HORSE_NPALS; i++) {
        rect.x = (g_ClutIds[MOD_HORSE_PAL + i] & 0x3F) << 4;
        rect.y = g_ClutIds[MOD_HORSE_PAL + i] >> 6;
        rect.w = COLORS_PER_PAL;
        rect.h = 1;
        LoadImage(&rect, (u_long*)&g_ModHorsePals[i * COLORS_PER_PAL]);
    }
    self->animSet = MOD_HORSE_ANIMSET;
    self->unk5A = 0;
    self->palette = 0;
    self->facingLeft = 0;
    self->animCurFrame = 0;
}

// the horse left at the gate (params 0x100 | palette offset: 2 = Maria's
// white horse)
static void ModHorseRear(Entity* self) {
    if (!self->step) {
        InitializeEntity(g_EInitSpawner);
        // stay at the gate as the camera follows the rider: DRA shifts only
        // the entities with this flag by the scroll (src/dra/5087C.c); a
        // spawner's flags lack it and so it stayed on screen, following her
        self->flags |= FLAG_POS_CAMERA_LOCKED;
        self->animSet = MOD_HORSE_ANIMSET;
        self->unk5A = 0;
        self->palette = self->params & 0xFF;
        self->facingLeft = 0;
        self->zPriority = 0x88;
        self->pose = 0;
        self->poseTimer = 0;
    }
    AnimateEntity((u8*)mod_anim_rear, self);
}

static void ModHorseRide(Entity* self) {
    Entity* player = &PLAYER;
    Tilemap* tilemap = &g_Tilemap;
    ModHorseState* ride = (ModHorseState*)&self->ext;
    Entity* horse;
    bool gallop = false;
    bool init = false;

    player->animCurFrame = 0;
    g_Player.padSim = PAD_RIGHT;
    switch (self->step) {
    case 0:
        InitializeEntity(g_EInitSpawner);
        ModHorseInit(self);
        g_Player.padSim = 0;
        g_Player.demo_timer = 255;
        player->posX.i.hi = 0;
        g_unkGraphicsStruct.unk14 = 0;
        g_PauseAllowed = false;
        ride->velocityY = FIX(-4.375);
        ride->strip = 0;
        if (g_PlayableCharacter == PLAYER_MARIA) {
            ride->gallop = (u8*)mod_anim_mar_gallop;
            ride->leap = (u8*)mod_anim_mar_leap;
            ride->air = (u8*)mod_anim_mar_air;
            self->params = 2; // the rearing horse's palette offset
        } else if (MOD_RIC_COSTUME2()) {
            // Saturn costume (RIC2): the Saturn's own Richter rider, 1-5, 8
            ride->gallop = (u8*)mod_anim_ric_gallop;
            ride->leap = (u8*)mod_anim_ric_leap;
            ride->air = (u8*)mod_anim_ric_leap;
            self->params = 0;
        } else {
            // PS1 costume: the Saturn's alternate-costume rider, 25-30
            ride->gallop = (u8*)mod_anim_ric2_gallop;
            ride->leap = (u8*)mod_anim_ric2_leap;
            ride->air = (u8*)mod_anim_ric2_leap;
            self->params = 0;
        }
        init = true;
        break;

    // off-screen: the player is pushed right until the scene starts; the art
    // goes to VRAM meanwhile
    case 1:
        ModHorseUploadStrip(ride);
        player->posX.val += FIX(6);
        if (tilemap->scrollX.i.hi > 0x800 && ride->strip >= MOD_HORSE_NSTRIPS) {
            self->step++;
        }
        break;

    // the horse gallops in, the camera catches up
    case 2:
        AnimateEntity(ride->gallop, self);
        player->posX.val += FIX(8.5);
        g_unkGraphicsStruct.unk14 += 4;
        if (g_unkGraphicsStruct.unk14 == 192) {
            self->step++;
            ride->speed = FIX(4.5);
        }
        gallop = true;
        break;

    case 3:
        AnimateEntity(ride->gallop, self);
        if (g_unkGraphicsStruct.unk14 > 128) {
            g_unkGraphicsStruct.unk14--;
            ride->speed = FIX(3.5);
        } else {
            ride->speed = FIX(4.5);
        }
        player->posX.val += ride->speed;
        if (ride->speed == FIX(4.5)) {
            self->step++;
        }
        gallop = true;
        break;

    // at the gate the rider leaps off and the horse stays behind, rearing
    case 4:
        AnimateEntity(ride->gallop, self);
        player->posX.val += FIX(4.5);
        gallop = true;
        if (tilemap->scrollX.i.hi > 0xF80) {
            gallop = false;
            horse = AllocEntity(&g_Entities[192], &g_Entities[256]);
            if (horse != NULL) {
                CreateEntityFromEntity(E_PUSH_ALUCARD, player, horse);
                horse->params = 0x100 | self->params;
            }
            self->pose = 0;
            self->poseTimer = 0;
            AnimateEntity(ride->leap, self);
            ride->ground = player->posY.val;
            player->posY.i.hi -= 32;
            ride->posY = player->posY.val;
            g_api.PlaySfx(MOD_SFX_HORSE_NEIGH);
            self->step++;
        }
        break;

    // the leap (an arc about 87 px high), then the fall to the next room
    case 5:
    case 6:
        ride->velocityY += FIX(0.109375);
        ride->posY += ride->velocityY;
        if (ride->posY > ride->ground) {
            ride->posY = ride->ground;
        }
        player->posY.val = ride->posY;
        player->velocityY = 0;
        if (self->step == 5) {
            if (!AnimateEntity(ride->leap, self)) {
                self->pose = 0;
                self->poseTimer = 0;
                self->step++;
            }
        } else {
            AnimateEntity(ride->air, self);
        }
        player->posX.val += FIX(4.5);
        break;
    }
    if (!init) {
        g_Player.demo_timer = 1;
    }
    self->posX.val = player->posX.val;
    self->posY.val = player->posY.val;
    self->zPriority = player->zPriority;
    if (gallop && !(g_Timer & 31)) {
        g_api.PlaySfx(MOD_SFX_HORSE_GALLOP);
    }
}
