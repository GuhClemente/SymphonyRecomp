// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port mod: the Saturn's Maria boss fight in the Castle Center.
//
// The Saturn's CEN (STAGE_14) makes Alucard fight Maria between her first
// words and the Holy Glasses: entities 0x1D-0x25 at 060E2F8C-060E69B8, here
// ported to C from the SH-2 code.
// One CEN entity slot runs them all (E_MOD_MARIA_BOSS, the retail
// E_DUMMY_0F): params >> 8 picks the Saturn entity, params & 0xFF is the
// Saturn's params. Saturn X values (positions, speeds, distances, hitboxes)
// are at 4/5 (the Saturn draws 320 px where the PS1 draws 256); Y values are
// the Saturn's. The art and tables are rodata in mod_maria_boss_data.h
// (mods/sotn-ultimate/tools/gen_cen_maria.py). Included from e_maria.c.
//
// Her body is drawn as boss Richter's in BO6: ANIMSET_OVL(MB_ANIMSET)
// frames name images of the overlay sheet g_api.o.unk30, which DRA uploads
// into a g_PlayerDraw slot's VRAM every frame. The images are LZSS-packed;
// an entity showing one unpacks it into its slot's buffer and points the
// sheet entry there.

#include <cutscene.h>
#include "sfx.h"

extern s16** spriteBanks[];
#include "../../../../src/dra/mod_xa_music.h"
// the fight's music: the Saturn's 0xE0000123, SD/SD23.PCM, a Saturn-only
// track (mods/sotn-ultimate/tools/gen_saturn_music.py)
#define MOD_MB_MUSIC MU_MOD_MARIA_BOSS

#define MOD_MB_TABLES
#include "mod_maria_boss_data.h"
#include "../../../../src/st/cen/mod_cen_sfx.h"

// Saturn entity ids -> sub-entity numbers (params >> 8)
enum {
    MB_BODY = 0,    // 0x1D
    MB_FLASH = 1,   // 0x1E the light pillar of her summoning
    MB_AFTER = 2,   // 0x1F afterimage
    MB_KICKBOX = 3, // 0x20 her kicks' hitbox (always self + 1)
    MB_SHOT = 4,    // 0x21 thrown energy shot
    MB_BLADE = 5,   // 0x22 thrown blades
    MB_FLAME = 6,   // 0x23 flame stream
    MB_HOLY = 7,    // 0x24 holy burst
    MB_SPIRIT = 8,  // 0x25 the four guardian spirits
    MB_OVERLAY = 9, // her frames' effect parts (knuckle shield, rings...)
};

#define MB_ID(sub, p) (((sub) << 8) | (p))
// Saturn x -> PS1 x (constants)
#define SX(v) ((v) * 4 / 5)

// the Saturn's entity slots (g_Entities[96] for her; the spirits are
// self + 2..5, the kick hitbox self + 1, as on the Saturn)
#define MB_SLOT 96

// the entity's ext (60 bytes)
typedef struct {
    /* 0x00 */ s16 timer;       // Saturn 0x7C
    /* 0x02 */ s16 e7E;         // Saturn 0x7E
    /* 0x04 */ u8 bounced;      // Saturn 0x80
    /* 0x05 */ u8 noWalls;      // Saturn 0x81
    /* 0x06 */ u8 halfDone;     // Saturn 0x82: below half HP
    /* 0x07 */ u8 b87;          // Saturn 0x87
    /* 0x08 */ s16 aiTimer;     // Saturn 0x84: afterimage period
    /* 0x0A */ s16 savedHp;     // Saturn 0x88
    /* 0x0C */ u16 savedHitbox; // Saturn 0xAC
    /* 0x0E */ s16 orbX;        // Saturn 0xB0 (spirits)
    /* 0x10 */ s16 orbY;        // Saturn 0xB2
    /* 0x12 */ s16 ang[3];      // Saturn 0x8C/0x8E/0x90 (holy burst)
    /* 0x18 */ s16 shownFrame;  // the image unpacked in the slot buffer
    /* 0x1A */ u8 slot;         // g_PlayerDraw slot - 8, 0xFF none
    /* 0x1B */ u8 musicOn;
    /* 0x1C */ s16 opacity; // afterimage fade (Saturn 0x68)
    /* 0x1E */ s16 scale;   // spirit grow (Saturn sprite scale)
    /* 0x20 */ s16 trailX;  // where the last afterimage was left
    /* 0x22 */ s16 trailY;
} MbExt;
#define MB(e) ((MbExt*)&(e)->ext)

// sheet entries (DRA reads g_api.o.unk30[image]) and the unpack buffers of
// the 4 g_PlayerDraw slots (8: her body; 9-11: afterimages)
static u8* g_MbSheet[MB_NIMAGES];
static u32 g_MbBuf[4][MB_MAXRAW / 4];
static u8 g_MbSlotUsed[4];
static u8 g_MbFxQ;

// the fight's start (set by the platform when the first script ends)
s32 g_ModMariaFight;

static void MbUnpack(u8* dst, u8* src, s32 size) {
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

// the fx quadrants (tpage 19): one per frame, as the pause menu may reuse
// the stage sprite pages
static void MbUploadFx(void) {
    RECT rect;
    s32 q;

    q = g_MbFxQ++ % MB_FX_NQUADS;
    rect.x = g_MbFxQuad[q][0];
    rect.y = g_MbFxQuad[q][1];
    rect.w = 32;
    rect.h = 128;
    LoadImage(&rect, (u_long*)&g_MbFxPix[q * 128 * 64]);
}

// once per room visit: sprite banks, sheet, palettes, fx art
static void MbSetup(void) {
    RECT rect;
    s32 i;
    s32 n;

    spriteBanks[MB_ANIMSET] = (s16**)g_MbFrameTab;
    spriteBanks[MB_FX_ANIMSET] = (s16**)g_MbFxTab;
    for (i = 0; i < 4; i++) {
        g_MbSlotUsed[i] = 0;
    }
    for (i = 0; i < MB_NPALS * COLORS_PER_PAL; i++) {
        ((u16*)g_Clut)[MB_PAL * COLORS_PER_PAL + i] = g_MbPals[i];
    }
    for (i = 0; i < MB_NPALS; i++) {
        rect.x = (g_ClutIds[MB_PAL + i] & 0x3F) << 4;
        rect.y = g_ClutIds[MB_PAL + i] >> 6;
        rect.w = COLORS_PER_PAL;
        rect.h = 1;
        LoadImage(&rect, (u_long*)&g_MbPals[i * COLORS_PER_PAL]);
    }
    n = MB_FX_NPALS + 4;
    for (i = 0; i < n * COLORS_PER_PAL; i++) {
        ((u16*)g_Clut)[MB_FX_PAL * COLORS_PER_PAL + i] = g_MbFxPals[i];
    }
    for (i = 0; i < n; i++) {
        rect.x = (g_ClutIds[MB_FX_PAL + i] & 0x3F) << 4;
        rect.y = g_ClutIds[MB_FX_PAL + i] >> 6;
        rect.w = COLORS_PER_PAL;
        rect.h = 1;
        LoadImage(&rect, (u_long*)&g_MbFxPals[i * COLORS_PER_PAL]);
    }
    for (i = 0; i < MB_FX_NQUADS; i++) {
        MbUploadFx();
    }
}

// every frame an entity of hers draws: its image into its slot's buffer
static void MbShow(Entity* self) {
    MbExt* ext = MB(self);
    s16 f = self->animCurFrame;

    g_api.o.unk30 = g_MbSheet;
    if (ext->slot == 0xFF || f <= 0 || f >= MB_NIMAGES || !g_MbRawSize[f]) {
        return;
    }
    if (ext->shownFrame != f) {
        MbUnpack(
            (u8*)g_MbBuf[ext->slot], &g_MbLz[g_MbLzOff[f]], g_MbRawSize[f]);
        ext->shownFrame = f;
    }
    g_MbSheet[f] = (u8*)g_MbBuf[ext->slot];
}

// the Saturn's AnimateEntity for {u16 ticks, u16 frame} (060E3058)
static u8 MbAnimate(u16* frames, Entity* self) {
    u16* cur;
    u8 flag = 0;

    cur = frames + self->pose * 2;
    if (!self->poseTimer) {
        if (cur[0] == 0) {
            self->pose = 0;
            self->poseTimer = frames[0];
            self->animCurFrame = frames[1];
            self->pose++;
            return 0;
        }
        if (cur[0] == 0xFF) {
            return 0;
        }
        self->poseTimer = cur[0];
        self->animCurFrame = cur[1];
        self->pose++;
        cur += 2;
        flag |= 0x80;
    }
    self->poseTimer--;
    self->animCurFrame = cur[-1];
    return flag | 1;
}

static void MbFacePlayer(Entity* self) {
    self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
}

// Saturn GetDistanceToPlayerX in Saturn units (thresholds stay the Saturn's)
static s16 MbDistX(void) { return GetDistanceToPlayerX() * 5 / 4; }

// pose == p and poseTimer == 0 (the Saturn's *(u32*)&pose == p << 16)
#define POSE_IS(p) (self->pose == (p) && self->poseTimer == 0)

// 060E3184: turn round at a wall or the room's edge (then step 13)
static void MbWallCheck(Entity* self, u8 noWalls) {
    Collider col;
    s16 x, y;
    s32 hit;

    x = self->posX.i.hi;
    y = self->posY.i.hi;
    if (self->velocityX > 0) {
        x += SX(16);
        g_api.CheckCollision(x, y, &col, 0);
        if (noWalls) {
            col.effects = 0;
        }
        if (col.effects & EFFECT_SOLID) {
            self->posX.i.hi += col.unk14;
            hit = 1;
        } else {
            hit = (x + g_Tilemap.scrollX.i.hi) > g_Tilemap.width;
        }
    } else {
        x -= SX(16);
        g_api.CheckCollision(x, y, &col, 0);
        if (noWalls) {
            col.effects = 0;
        }
        if (col.effects & EFFECT_SOLID) {
            self->posX.i.hi += col.unk1C;
            hit = 1;
        } else {
            hit = (x + g_Tilemap.scrollX.i.hi) < g_Tilemap.x;
        }
    }
    if (hit) {
        self->facingLeft ^= 1;
        MB(self)->e7E = 0;
        MB(self)->noWalls = 1;
        SetStep(13);
    }
}

static Entity* MbSpawn(Entity* self, u16 sub, u16 p) {
    Entity* e;

    e = AllocEntity(&g_Entities[144], &g_Entities[192]);
    if (e != NULL) {
        CreateEntityFromEntity(E_MOD_MARIA_BOSS, self, e);
        e->params = MB_ID(sub, p);
    }
    return e;
}

static void MbExplode(Entity* self) {
    Entity* e;

    e = AllocEntity(&g_Entities[224], &g_Entities[256]);
    if (e != NULL) {
        CreateEntityFromEntity(E_EXPLOSION, self, e);
        e->params = 0;
    }
    DestroyEntity(self);
}

static const u16 g_EInitMbBody[] = {
    ANIMSET_OVL(MB_ANIMSET), 1, 8, MB_PAL, 0x198};

// 0x1D: Maria (060E32AC)
static void MbBody(Entity* self) {
    MbExt* ext = MB(self);
    Entity* e;
    s16 dx, dy, d;
    s32 i;
    s32 a, ang;
    s16 absX;
    s16 cur, tgt;
    u8 idx;

    g_api.o.unk30 = g_MbSheet;
    if (self->hitFlags & 3) {
        if (self->step == 4) {
            SetStep(17);
        }
        if (self->step == 15) {
            SetStep(17);
        }
        if (self->step == 10 && !(Random() & 1)) {
            SetStep(17);
        }
        if (self->step == 9 && !(Random() & 1)) {
            SetStep(17);
        }
    }
    if ((self->flags & FLAG_DEAD) && self->step != 32) {
        self->hitboxState = 0;
        SetStep(32);
        self->opacity = 0x80;
        g_api.PlaySfx(MOD_SFX_MB_DEATH);
        MbFacePlayer(self);
    }
    if (self->step && !ext->halfDone &&
        self->hitPoints < g_api.enemyDefs[0x198].hitPoints / 2) {
        ext->halfDone = 1;
        SetStep(16);
        ext->savedHitbox = self->hitboxState;
        self->hitboxState = 0;
        self->hitPoints = 250;
        g_api.PlaySfx(MOD_SFX_MB_HALF);
    }
    if (ext->halfDone) {
        if (++ext->timer > 0x15F) {
            ext->timer = 0x160;
        }
    }

    switch (self->step) {
    case 0:
        MbSetup();
        InitializeEntity((u16*)g_EInitMbBody);
        ext->slot = 0;
        ext->shownFrame = -1;
        self->unk5A = 8;
        g_PlayerDraw[8].enableColorBlend = 0;
        g_PlayerDraw[8].rect0.y = 0x101;
        self->zPriority += 5;
        self->flags |= FLAG_POS_CAMERA_LOCKED;
        CreateEntityFromCurrentEntity(E_MOD_MARIA_BOSS, self + 1);
        (self + 1)->params = MB_ID(MB_KICKBOX, 0);
        CreateEntityFromCurrentEntity(E_MOD_MARIA_BOSS, self + 6);
        (self + 6)->params = MB_ID(MB_OVERLAY, 0);
        // fallthrough
    case 1:
        if (UnkCollisionFunc3(g_MbSensors3) & 1) {
            SetStep(2);
        }
        break;
    case 2:
        MbFacePlayer(self);
        if (MbDistX() <= 0xFF) {
            SetStep(14);
            if (!ext->musicOn) {
                ext->musicOn = 1;
                g_api.PlaySfx(MOD_MB_MUSIC);
            }
        }
        break;
    case 14:
        if (!MbAnimate(g_MbAnim910C, self)) {
            SetStep(4);
        }
        break;
    case 3:
        if (!self->step_s) {
            self->step_s++;
        }
        if (!MbAnimate(g_MbAnim8ECA, self)) {
            SetStep(4);
        }
        break;
    case 4:
        if (!self->step_s) {
            MbFacePlayer(self);
            ext->e7E = 0;
            self->step_s++;
        }
        UnkCollisionFunc2(g_MbSensors2);
        if (!MbAnimate(g_MbAnim8DFA, self)) {
            MbFacePlayer(self);
            ext->e7E = 1;
        }
        self->velocityX = self->facingLeft ? SX(0xF000) : SX(-0xF000);
        d = MbDistX();
        if (d > 96) {
            SetStep((Random() & 1) ? 8 : 12);
        } else if (d <= 47) {
            SetStep((Random() & 1) ? 9 : 7);
            if (ext->halfDone) {
                SetStep(9);
                break;
            }
        } else {
            if (MbDistX() != 72) {
                break;
            }
            if (!ext->halfDone) {
                if (Random() & 1) {
                    SetStep(10);
                }
            } else {
                if (Random() & 1) {
                    SetStep(18);
                }
            }
            break;
        }
        // 060E384C
        if ((GetSideToPlayer() & 1) == self->facingLeft) {
            break;
        }
        if (ext->halfDone && MbDistX() > 96 && ext->timer > 0x14F) {
            ext->timer = 0;
            SetStep((Random() & 1) ? 19 : 20);
            break;
        }
        dx = MbDistX();
        dy = PLAYER.posY.i.hi - self->posY.i.hi;
        if ((PLAYER.step_s & 0x40) && dx <= 31) {
            SetStep(PLAYER.step == 2 ? 6 : 9);
            break;
        }
        if ((g_Player.status & PLAYER_STATUS_UNK400) && dx <= 79 &&
            (dy >= 0 ? dy <= 31 : -dy <= 31)) {
            SetStep((Random() & 1) ? 9 : 10);
            break;
        }
        if (dy >= 0 ? dy > 16 : -dy > 16) {
            SetStep(11);
            if (dx > 0xA0) {
                SetStep(15);
            }
        }
        break;
    case 5:
        if (!self->step_s) {
            self->step_s++;
            PlaySfxPositional(0x647);
        }
        if (!MbAnimate(g_MbAnim8EA4, self)) {
            SetStep(4);
        }
        break;
    case 6:
        switch (self->step_s) {
        case 0:
            if (!MbAnimate(g_MbAnim8E64, self)) {
                SetSubStep(1);
            }
            break;
        case 1:
            if (!MbAnimate(g_MbAnim9034, self)) {
                SetSubStep(2);
            }
            break;
        case 2:
            if (!MbAnimate(g_MbAnim904E, self)) {
                if (!(PLAYER.step_s & 0x40)) {
                    SetStep(3);
                } else if (Random() & 1) {
                    SetStep(10);
                } else {
                    SetSubStep(1);
                }
            }
            break;
        }
        break;
    case 7:
        switch (self->step_s) {
        case 0:
            self->velocityX = self->velocityY = 0;
            self->step_s++;
            g_api.PlaySfx((Random() & 1) ? MOD_SFX_MB_ATK1 : MOD_SFX_MB_ATK2);
            // fallthrough
        case 1:
            if (!MbAnimate(g_MbAnim8FEC, self)) {
                SetSubStep(2);
            }
            if (self->animCurFrame == 0x96) {
                self->velocityX = self->facingLeft ? SX(0x20000) : SX(-0x20000);
            }
            UnkCollisionFunc2(g_MbSensors2);
            self->velocityX -= self->velocityX / 32;
            break;
        case 2:
            if (!MbAnimate(g_MbAnim900E, self)) {
                SetStep(4);
            }
            break;
        }
        break;
    case 8:
        switch (self->step_s) {
        case 0:
            MbFacePlayer(self);
            self->step_s++;
            // fallthrough
        case 1:
            if (!MbAnimate(g_MbAnim9060, self)) {
                SetStep(4);
                break;
            }
            if (POSE_IS(2)) {
                g_api.PlaySfx(MOD_SFX_MB_THROW);
                e = MbSpawn(self, MB_SHOT, ext->halfDone);
                if (e != NULL) {
                    e->facingLeft = self->facingLeft;
                    e->posY.i.hi -= 10;
                }
            }
            break;
        }
        break;
    case 9:
        switch (self->step_s) {
        case 0:
            self->velocityX = self->facingLeft ? SX(-0x3C000) : SX(0x3C000);
            self->step_s++;
            PlaySfxPositional(0x617);
            // fallthrough
        case 1:
            UnkCollisionFunc2(g_MbSensors2);
            // (both of the Saturn's branches slow down to the left)
            self->velocityX += SX(-0x1333);
            if (!MbAnimate(g_MbAnim8EF2, self)) {
                SetSubStep(2);
            }
            break;
        case 2:
            if (!MbAnimate(g_MbAnim8F08, self)) {
                SetStep(4);
            }
            break;
        }
        break;
    case 10:
        switch (self->step_s) {
        case 0:
            MbFacePlayer(self);
            self->step_s++;
            self->velocityX = self->facingLeft ? SX(0x48000) : SX(-0x48000);
            // fallthrough
        case 1:
            self->velocityX += self->facingLeft ? SX(-0xCCC) : SX(0xCCC);
            UnkCollisionFunc2(g_MbSensors2);
            if (!MbAnimate(g_MbAnim8F22, self)) {
                SetSubStep(2);
            }
            break;
        case 2:
            self->velocityX += self->facingLeft ? SX(-0xCCC) : SX(0xCCC);
            if (self->animCurFrame == 0x82 || self->animCurFrame == 0x83 ||
                self->animCurFrame == 71) {
                self->velocityX = 0;
            }
            UnkCollisionFunc2(g_MbSensors2);
            if (!MbAnimate(g_MbAnim8F58, self)) {
                SetStep(4);
            }
            break;
        }
        break;
    case 11:
        switch (self->step_s) {
        case 0:
            MbFacePlayer(self);
            self->step_s++;
            // fallthrough
        case 1:
            if (!MbAnimate(g_MbAnim8F7A, self)) {
                SetStep(15);
            }
            if (POSE_IS(3)) {
                if (Random() & 1) {
                    g_api.PlaySfx(MOD_SFX_MB_ATK1);
                }
                for (i = 0; i < 3; i++) {
                    e = MbSpawn(self, MB_BLADE, i);
                    if (e == NULL) {
                        continue;
                    }
                    e->facingLeft = self->facingLeft;
                    if (self->facingLeft) {
                        e->posX.i.hi += g_MbBladeX[i];
                    } else {
                        e->posX.i.hi -= g_MbBladeX[i];
                    }
                    e->posY.i.hi -= 10;
                }
            }
            break;
        }
        break;
    case 12:
        switch (self->step_s) {
        case 0:
            self->velocityX = self->facingLeft ? SX(0x20000) : SX(-0x20000);
            ext->bounced = 0;
            SetSubStep(1);
            break;
        case 1:
            if (!MbAnimate(g_MbAnim8E2C, self)) {
                SetSubStep(2);
            }
            break;
        case 2:
            UnkCollisionFunc2(g_MbSensors2);
            if (!MbAnimate(g_MbAnim8E3E, self)) {
                SetSubStep(3);
                self->velocityY = FIX(-4);
                self->velocityX *= 2;
                g_api.PlaySfx(
                    (Random() & 1) ? MOD_SFX_MB_JUMP : MOD_SFX_MB_ATK2);
            }
            break;
        case 3:
            MbAnimate(g_MbAnim8FC2, self);
            goto air;
        }
        break;
    case 13:
        switch (self->step_s) {
        case 0:
            self->velocityX = 0;
            self->velocityY = FIX(-2);
            MoveEntity();
            if (!MbAnimate(g_MbAnim8E8E, self)) {
                self->velocityX = self->facingLeft ? SX(0x20000) : SX(-0x20000);
                self->velocityY = FIX(-4);
                ext->bounced = 0;
                SetSubStep(1);
            }
            break;
        case 1:
            MbAnimate(g_MbAnim8FA8, self);
            // her kick connected (the kick hitbox, self + 1, took a hit)
            if ((self + 1)->hitFlags & 0x80) {
                PlaySfxPositional(0x678);
                self->velocityX = -self->velocityX / 2;
                self->velocityY = FIX(-3);
                ext->bounced = 1;
                self->step_s++;
            }
            goto air;
        case 2:
        air:
            // 060E45AC / 060E46F8: the jump's arc
            if (self->velocityY > FIX(-1)) {
                self->velocityY -= 0x1000;
            }
            if (self->velocityY > FIX(1)) {
                if (UnkCollisionFunc3(g_MbSensors3) & 1) {
                    ext->noWalls = 0;
                    SetStep(5);
                    break;
                }
            } else {
                MoveEntity();
                self->velocityY += 0x4000;
            }
            if (!ext->bounced) {
                MbWallCheck(self, ext->noWalls);
            }
            break;
        }
        break;
    case 15:
        switch (self->step_s) {
        case 0:
            self->velocityX = SX(0x20000);
            MbFacePlayer(self);
            if (!self->facingLeft) {
                self->velocityX = -self->velocityX;
            }
            SetSubStep(1);
            // fallthrough
        case 1:
            if (!MbAnimate(g_MbAnim8E2C, self)) {
                SetSubStep(2);
            }
            break;
        case 2:
            UnkCollisionFunc2(g_MbSensors2);
            if (!MbAnimate(g_MbAnim8E3E, self)) {
                SetStep(4);
            }
            break;
        }
        break;
    case 16:
        // below half HP: knocked back, then she summons the spirits
        switch (self->step_s) {
        case 0:
            self->velocityX = self->facingLeft ? SX(-0x4000) : SX(0x4000);
            goto fall;
        case 1:
            self->velocityX = self->facingLeft ? SX(-0x4000) : SX(0x4000);
            UnkCollisionFunc2(g_MbSensors2);
            if (!MbAnimate(g_MbAnim8EE4, self)) {
                SetSubStep(2);
            }
            break;
        case 2:
            if (!MbAnimate(g_MbAnim93C0, self)) {
                SetSubStep(3);
            }
            break;
        case 3:
            if (!MbAnimate(g_MbAnim8ECA, self)) {
                SetSubStep(4);
                // her blue tint for the rest of the fight
                self->drawFlags =
                    ENTITY_OPACITY | ENTITY_MASK_R | ENTITY_MASK_G;
                self->opacity = 0xF8;
                MbSpawn(self, MB_FLASH, 0);
                PlaySfxPositional(0x636);
                g_api.PlaySfx(MOD_SFX_MB_SUMMON);
            }
            break;
        case 4:
            if (!MbAnimate(g_MbAnim910C, self)) {
                SetStep(19);
                self->hitboxState = ext->savedHitbox;
                for (i = 0; i < 4; i++) {
                    e = self + 2 + i;
                    CreateEntityFromEntity(E_MOD_MARIA_BOSS, self, e);
                    e->params = MB_ID(MB_SPIRIT, i + 1);
                }
            }
            break;
        }
        break;
    case 17:
        // hurt
        switch (self->step_s) {
        case 0:
            MbFacePlayer(self);
            self->step_s = 1;
            self->velocityX = self->facingLeft ? SX(-0x30000) : SX(0x30000);
            self->velocityY = 0;
            ext->savedHitbox = self->hitboxState;
            self->hitboxState = 0;
            g_api.PlaySfx((Random() & 1) ? MOD_SFX_MB_HURT2 : MOD_SFX_MB_HURT1);
            // fallthrough
        case 1:
            if ((u16)(self->animCurFrame - 107) <= 1) {
                self->velocityX += self->facingLeft ? SX(0x2666) : SX(-0x2666);
                UnkCollisionFunc2(g_MbSensors2);
            }
            if (!MbAnimate(g_MbAnim8EE4, self)) {
                if (Random() & 1) {
                    SetSubStep(2);
                } else {
                    SetStep(15);
                    self->hitboxState = ext->savedHitbox;
                }
                self->velocityX = 0;
            }
            break;
        case 2:
            if (!MbAnimate(g_MbAnim8E64, self)) {
                if (!(Random() & 1)) {
                    SetStep(10);
                } else {
                    SetStep(ext->halfDone ? 20 : 11);
                }
                self->hitboxState = ext->savedHitbox;
            }
            break;
        }
        break;
    case 18:
        // invulnerable dash with afterimages, double damage
        switch (self->step_s) {
        case 0:
            MbFacePlayer(self);
            SetSubStep(1);
            ext->savedHp = self->hitPoints;
            self->hitPoints = 0x7FFF;
            self->attack *= 2;
            ext->b87 = 1;
            self->drawFlags |= ENTITY_BLINK;
            g_api.PlaySfx(MOD_SFX_MB_DASH);
            // fallthrough
        case 1:
            if (!MbAnimate(g_MbAnim930E, self)) {
                MbFacePlayer(self);
                SetStep(9);
                self->velocityX = self->velocityY = 0;
                self->hitPoints = ext->savedHp;
                self->attack /= 2;
                ext->b87 = 0;
                self->drawFlags &= ~ENTITY_BLINK;
            }
            a = self->animCurFrame - 0x109;
            if (a < 0 || a > 13 || (a >= 3 && a <= 5)) {
                self->velocityX = 0;
                break;
            }
            self->velocityX = a < 3 ? SX(0x40000) : SX(0x3000);
            if (!self->facingLeft) {
                self->velocityX = -self->velocityX;
            }
            UnkCollisionFunc2(g_MbSensors2);
            break;
        }
        break;
    case 19:
        // flame stream
        switch (self->step_s) {
        case 0:
            MbFacePlayer(self);
            SetSubStep(1);
            ext->savedHitbox = self->hitboxState;
            self->hitboxState = 0;
            self->drawFlags |= ENTITY_BLINK;
            g_api.PlaySfx(MOD_SFX_MB_FLAME1);
            // fallthrough
        case 1:
            if (!MbAnimate(g_MbAnim923A, self)) {
                SetSubStep(2);
                g_api.PlaySfx(MOD_SFX_MB_FLAME2);
            }
            break;
        case 2:
            if (!MbAnimate(g_MbAnim92EC, self)) {
                SetStep(9);
                self->hitboxState = ext->savedHitbox;
                self->drawFlags &= ~ENTITY_BLINK;
            }
            e = MbSpawn(self, MB_FLAME, 0);
            if (e != NULL) {
                e->facingLeft = self->facingLeft;
            }
            break;
        }
        break;
    case 20:
        // holy burst
        switch (self->step_s) {
        case 0:
            ext->savedHitbox = self->hitboxState;
            self->hitboxState = 0;
            self->step_s = 1;
            self->drawFlags |= ENTITY_BLINK;
            if (Random() & 1) {
                g_api.PlaySfx(MOD_SFX_MB_HOLY);
            }
            // fallthrough
        case 1:
            if (!MbAnimate(g_MbAnim917E, self)) {
                SetSubStep(2);
            }
            break;
        case 2:
            if (!MbAnimate(g_MbAnim91A8, self)) {
                SetStep(9);
                MbFacePlayer(self);
                self->hitboxState = ext->savedHitbox;
                self->drawFlags &= ~ENTITY_BLINK;
            }
            if (self->pose == 0x0B && self->poseTimer == 0x30) {
                e = MbSpawn(self, MB_HOLY, 0);
                if (e != NULL) {
                    e->posY.i.hi -= 26;
                    e->posX.i.hi =
                        self->posX.i.hi + (self->facingLeft ? SX(8) : -SX(8));
                }
            }
            break;
        }
        break;
    case 32:
        // defeated: knocked back, kneels, the second script starts, then she
        // walks to the room's middle and stands
        switch (self->step_s) {
        case 0:
            self->hitboxState = 0;
            self->velocityX = self->facingLeft ? SX(-0x2A000) : SX(0x2A000);
        fall:
            if (UnkCollisionFunc3(g_MbSensors3) & 1) {
                SetSubStep(1);
            }
            self->animCurFrame = 107;
            break;
        case 1:
            self->step_s = 2;
            self->velocityX = self->facingLeft ? SX(-0x30000) : SX(0x30000);
            self->velocityY = 0;
            self->hitboxState = 0;
            break;
        case 2:
            self->hitboxState = 0;
            if ((u16)(self->animCurFrame - 107) <= 1) {
                self->velocityX += self->facingLeft ? SX(0x2666) : SX(-0x2666);
                UnkCollisionFunc2(g_MbSensors2);
            }
            if (!MbAnimate(g_MbAnim8EE4, self)) {
                MbFacePlayer(self);
                SetSubStep(3);
            }
            break;
        case 3:
            self->hitboxState = 0;
            self->hitboxWidth = self->hitboxHeight = 0;
            if (!MbAnimate(g_MbAnim93E0, self)) {
                SetSubStep(4);
            }
            break;
        case 4:
            self->hitboxState = 0;
            self->hitboxWidth = self->hitboxHeight = 0;
            if (!MbAnimate(g_MbAnim940A, self)) {
                SetSubStep(5);
            }
            break;
        case 5:
            if (!MbAnimate(g_MbAnim8ECA, self)) {
                SetSubStep(6);
                // the second script; the platform walks Alucard to the middle
                CreateEntityFromCurrentEntity(E_CUTSCENE, &g_Entities[200]);
                g_Entities[200].params = 1;
                // the dialogue's portraits (VRAM x 0-56, y 256-328) and its
                // five text line buffers (x 0-64, y 384-443; 432 is line 4's)
                // share her slot's VRAM page: her image goes below them
                // (y 444-511) until the platform's last step puts it back
                g_PlayerDraw[8].rect0.y = 444;
                for (e = &g_Entities[64]; e < &g_Entities[256]; e++) {
                    if (e->entityId == E_PLATFORM) {
                        e->step = 12;
                    }
                }
            }
            self->hitboxState = 0;
            self->hitboxWidth = self->hitboxHeight = 0;
            break;
        case 6:
            self->hitboxState = 0;
            self->hitboxWidth = self->hitboxHeight = 0;
            MbAnimate(g_MbAnim8DFA, self);
            absX = self->posX.i.hi + g_Tilemap.scrollX.i.hi;
            if (absX <= SX(0x185)) {
                self->posX.val += SX(0xF000);
                self->facingLeft = 1;
            }
            if (absX > SX(0x190)) {
                self->posX.val += SX(-0xF000);
                self->facingLeft = 0;
            }
            absX = self->posX.i.hi + g_Tilemap.scrollX.i.hi;
            if ((u16)(absX - SX(0x186)) <= SX(10)) {
                self->animCurFrame = 38;
                self->facingLeft = 1;
                SetSubStep(32);
            }
            break;
        }
        break;
    }

    // 060E4F4A: afterimages while dashing / breathing fire (steps 18, 19);
    // as the user saw on the Saturn, her trail is on whenever she moves
    // (walk, run, slide, jump): one every 5 frames once she has moved
    absX = self->posX.i.hi + g_Tilemap.scrollX.i.hi;
    d = self->posY.i.hi + g_Tilemap.scrollY.i.hi;
    if (self->step == 18 || self->step == 19 ||
        (self->step > 3 && self->step != 32 &&
         (absX - ext->trailX > 1 || ext->trailX - absX > 1 ||
          d - ext->trailY > 1 || ext->trailY - d > 1))) {
        if (ext->aiTimer++ > 4) {
            ext->trailX = absX;
            ext->trailY = d;
            ext->aiTimer = 0;
            e = AllocEntity(&g_Entities[112], &g_Entities[130]);
            if (e != NULL) {
                CreateEntityFromEntity(E_MOD_MARIA_BOSS, self, e);
                e->params = MB_ID(MB_AFTER, 0);
                e->facingLeft = self->facingLeft;
                e->animCurFrame = self->animCurFrame;
            }
        }
    }
    // the frame's hitbox
    if (self->step != 32 && self->animCurFrame > 0 &&
        self->animCurFrame < MB_NFRAMES) {
        idx = g_MbHitboxIdx[self->animCurFrame];
        self->hitboxOffX = g_MbHitbox[idx][0];
        self->hitboxOffY = g_MbHitbox[idx][1];
        self->hitboxWidth = g_MbHitbox[idx][2];
        self->hitboxHeight = g_MbHitbox[idx][3];
    }
    // she stays in the arena (Saturn x 0x15E-0x262)
    absX = self->posX.i.hi + g_Tilemap.scrollX.i.hi;
    if (self->velocityX < 0) {
        if (absX <= SX(0x15D)) {
            self->posX.i.hi = SX(0x15E) - g_Tilemap.scrollX.i.hi;
        }
    } else if (absX > SX(0x262)) {
        self->posX.i.hi = SX(0x262) - g_Tilemap.scrollX.i.hi;
    }
    MbUploadFx();
    // the spirits circle her (radius 36), turning to their path
    if (self->step != 32 && ext->halfDone) {
        self->rotate += self->facingLeft ? 0xC0 : -0xC0;
        for (i = 0; i < 4; i++) {
            e = self + 2 + i;
            ang = (self->rotate + (i << 10)) & 0xFFF;
            MB(e)->orbX = self->posX.i.hi + ((rcos(ang) * SX(36)) >> 12);
            MB(e)->orbY = self->posY.i.hi + ((rsin(ang) * 36) >> 12);
            tgt = (ang - 0x400) & 0xFFF;
            cur = e->rotate;
            if (cur - tgt >= 0 ? cur - tgt < 16 : tgt - cur < 16) {
                e->rotate = tgt;
            } else if (cur > tgt) {
                e->rotate -= 16;
            } else {
                e->rotate += 16;
            }
        }
    }
    MbShow(self);
}

// 0x1F: afterimage (060E5DF0)
static void MbAfter(Entity* self) {
    MbExt* ext = MB(self);
    PlayerDraw* pd;
    s32 i;

    switch (self->step) {
    case 0:
        // a free g_PlayerDraw slot (10-11; 9 is the overlay's) or none
        ext->slot = 0xFF;
        for (i = 2; i < 4; i++) {
            if (!g_MbSlotUsed[i]) {
                g_MbSlotUsed[i] = 1;
                ext->slot = i;
                break;
            }
        }
        if (ext->slot == 0xFF) {
            DestroyEntity(self);
            return;
        }
        self->step = 1;
        self->animSet = ANIMSET_OVL(MB_ANIMSET);
        self->unk5A = 8 + ext->slot;
        self->palette = MB_PAL;
        self->zPriority = g_Entities[MB_SLOT].zPriority - 1;
        self->flags = FLAG_POS_CAMERA_LOCKED;
        self->hitboxState = 0;
        self->blendMode = BLEND_TRANSP;
        self->drawFlags = ENTITY_OPACITY;
        ext->shownFrame = -1;
        ext->opacity = 0x80;
        break;
    case 1:
        ext->opacity -= 6;
        if (ext->opacity <= 6) {
            g_MbSlotUsed[ext->slot] = 0;
            DestroyEntity(self);
            return;
        }
        break;
    }
    pd = &g_PlayerDraw[8 + ext->slot];
    pd->enableColorBlend = 0;
    self->opacity = ext->opacity;
    MbShow(self);
}

static const u16 g_EInitMbKick[] = {ANIMSET_OVL(MB_ANIMSET), 0, 0, 0, 0x70};

// 0x20: her kicks' hitbox, always self + 1 (060E5F50)
static void MbKickBox(Entity* self) {
    Entity* parent = self - 1;
    u16 f;
    s32 k;

    if (!self->step) {
        InitializeEntity((u16*)g_EInitMbKick);
        self->animSet = 0;
        self->flags |= FLAG_POS_CAMERA_LOCKED;
    }
    self->posX.i.hi = parent->posX.i.hi;
    self->posY.i.hi = parent->posY.i.hi;
    self->facingLeft = parent->facingLeft;
    f = parent->animCurFrame - 0x88;
    k = 0;
    self->attack = 45;
    switch (f) {
    case 0x8C - 0x88:
    case 0x8D - 0x88:
    case 0x90 - 0x88:
    case 0x91 - 0x88:
    case 0xB9 - 0x88:
    case 0xBA - 0x88:
        k = 1;
        self->attack = 50;
        break;
    case 0:
    case 1:
        k = 2;
        break;
    case 0x96 - 0x88:
    case 0x9A - 0x88:
    case 0x9B - 0x88:
    case 0x9C - 0x88:
    case 0x9D - 0x88:
        k = 3;
        break;
    case 0xA6 - 0x88:
    case 0xA7 - 0x88:
    case 0xA8 - 0x88:
    case 0xAA - 0x88:
    case 0xAB - 0x88:
        k = 4;
        break;
    }
    self->hitboxOffX = g_MbKick[k][0];
    self->hitboxOffY = g_MbKick[k][1];
    self->hitboxWidth = g_MbKick[k][2];
    self->hitboxHeight = g_MbKick[k][3];
    if (parent->step == 32 || parent->entityId != E_MOD_MARIA_BOSS) {
        DestroyEntity(self);
    }
}

static const u16 g_EInitMbShot[] = {ANIMSET_OVL(MB_FX_ANIMSET), 1, 0, 0, 0x84};

// 0x21: her thrown energy shot (060E615C)
static void MbShot(Entity* self) {
    if (self->hitParams || (self->flags & FLAG_DEAD)) {
        MbExplode(self);
        return;
    }
    if (!self->step) {
        InitializeEntity((u16*)g_EInitMbShot);
        self->flags |= FLAG_UNK_2000;
        // drawn half-transparent on the Saturn: a blue-white glow
        self->blendMode = BLEND_ADD;
        if (!(self->params & 0xFF)) {
            self->velocityX = self->facingLeft ? SX(0x20000) : SX(-0x20000);
            self->attack = 60;
        } else {
            self->velocityX = self->facingLeft ? SX(0x40000) : SX(-0x40000);
            self->attack = 65;
        }
        // the first stage's dart art points the other way round from her
        // frames (the second stage's comet, frames 9-16, does not)
        if (!(self->params & 0xFF)) {
            self->facingLeft ^= 1;
        }
    }
    MoveEntity();
    MbAnimate((self->params & 0xFF) ? g_MbAnim95E2 : g_MbAnim95D0, self);
}

static const u16 g_EInitMbBlade[] = {
    ANIMSET_OVL(MB_FX_ANIMSET), MB_FX_BLADE, 0, 0, 7};

// 0x22: her thrown blades, three at once (060E6300)
static void MbBlade(Entity* self) {
    s32 p = self->params & 0xFF;

    if (self->hitParams || (self->flags & FLAG_DEAD)) {
        MbExplode(self);
        return;
    }
    if (!self->step) {
        InitializeEntity((u16*)g_EInitMbBlade);
        self->flags |= FLAG_UNK_2000;
        self->attack = 50;
        self->velocityX = g_MbBladeV[p][0];
        self->velocityY = g_MbBladeV[p][1];
        if (!self->facingLeft) {
            self->velocityX = -self->velocityX;
        }
        self->hitboxWidth = self->hitboxHeight = 4;
        self->drawFlags = ENTITY_ROTATE;
    }
    MoveEntity();
    self->velocityY += 0x3000;
    self->rotate += 0x180;
}

static const u16 g_EInitMbFlame[] = {
    ANIMSET_OVL(MB_FX_ANIMSET), MB_FX_FIRE, 0, 0, 0x84};

// 0x23: her flame stream (060E6498)
static void MbFlame(Entity* self) {
    s32 ang;

    if (self->hitParams || (self->flags & FLAG_DEAD)) {
        MbExplode(self);
        return;
    }
    if (!self->step) {
        InitializeEntity((u16*)g_EInitMbFlame);
        self->flags |= FLAG_UNK_2000;
        self->attack = 70;
        self->zPriority += 2;
        ang = (Random() & 0x1FF) + (self->facingLeft ? 0xF00 : 0x700);
        self->velocityX = rcos(ang) * SX(160);
        self->velocityY = -(rsin(ang) * 128);
        self->posX.val += self->velocityX * 2;
        self->posY.val += self->velocityY * 2;
        self->blendMode = BLEND_ADD;
        // the flame art points the other way round from her frames
        self->facingLeft ^= 1;
        return;
    }
    self->posX.val += self->velocityX;
    self->posY.val += self->velocityY;
    if (!MbAnimate(g_MbAnim9640, self)) {
        DestroyEntity(self);
    }
}

static const u16 g_EInitMbSpirit[] = {
    ANIMSET_OVL(MB_FX_ANIMSET), 17, 0, 0, 0x199};

// 0x25: the four guardian spirits circling her (060E664C)
static void MbSpirit(Entity* self) {
    MbExt* ext = MB(self);
    s32 p = self->params & 0xFF;
    Entity* boss = self - 1 - p;
    u16 init[5];
    s32 i;

    if (boss->step == 32 || boss->entityId != E_MOD_MARIA_BOSS) {
        DestroyEntity(self);
        return;
    }
    switch (self->step) {
    case 0:
        for (i = 0; i < 5; i++) {
            init[i] = g_EInitMbSpirit[i];
        }
        init[4] = 0x198 + p;
        InitializeEntity(init);
        self->palette = PAL_FLAG(MB_SPIRIT_PAL + p - 1);
        self->flags |= FLAG_POS_CAMERA_LOCKED;
        self->hitPoints = 0x7FFF;
        self->hitboxWidth = SX(10);
        self->hitboxHeight = 8;
        self->drawFlags = ENTITY_ROTATE | ENTITY_SCALEX | ENTITY_SCALEY;
        ext->scale = 0;
        self->step = 1;
        // fallthrough
    case 1:
        if (g_Timer & 1) {
            if (++ext->scale > 31) {
                ext->scale = 32;
                self->step = 2;
            }
        }
        break;
    }
    // the Saturn's sprite scale grows to 32: half size
    self->scaleX = self->scaleY = ext->scale * 4;
    self->posX.i.hi = ext->orbX;
    self->posY.i.hi = ext->orbY;
    MbAnimate(g_MbAnim9684, self);
}

// 0x1E: the light pillar as she summons the spirits (060E5A30): two
// gradient bands from her x that widen to 40 px each side, hold, and close
static void MbFlash(Entity* self) {
    MbExt* ext = MB(self);
    Primitive* prim;
    s32 primIndex;
    s16 x;
    s32 i;

    x = self->posX.i.hi;
    switch (self->step) {
    case 0:
        primIndex = g_api.AllocPrimitives(PRIM_G4, 2);
        if (primIndex == -1) {
            return;
        }
        self->flags |= FLAG_HAS_PRIMS | FLAG_POS_CAMERA_LOCKED;
        self->primIndex = primIndex;
        self->hitboxState = 0;
        prim = &g_PrimBuf[primIndex];
        for (i = 0; i < 2; i++) {
            prim->x0 = prim->x1 = prim->x2 = prim->x3 = x - 1 + i;
            prim->y0 = prim->y1 = 0;
            prim->y2 = prim->y3 = 0xF0;
            // white at her x, red at the band's outer edge (Saturn colour
            // 0xC210 with gouraud 0xFBDE in the middle, 0x8010 outside)
            if (i) {
                prim->r0 = prim->g0 = prim->b0 = 0xF0;
                prim->r2 = prim->g2 = prim->b2 = 0xF0;
                prim->r1 = prim->r3 = 0xC0;
                prim->g1 = prim->b1 = prim->g3 = prim->b3 = 0x10;
            } else {
                prim->r1 = prim->g1 = prim->b1 = 0xF0;
                prim->r3 = prim->g3 = prim->b3 = 0xF0;
                prim->r0 = prim->r2 = 0xC0;
                prim->g0 = prim->b0 = prim->g2 = prim->b2 = 0x10;
            }
            prim->priority = g_Entities[MB_SLOT].zPriority - 1;
            prim->drawMode = DRAW_TPAGE | DRAW_TPAGE2 | DRAW_TRANSP;
            prim = prim->next;
        }
        PlaySfxPositional(0x636);
        self->step = 2;
        ext->e7E = 0;
        break;
    case 2:
        // the bands widen 1 px a frame to 40 each side
        if (++ext->e7E >= 40) {
            self->step = 3;
            ext->timer = 64;
        }
        break;
    case 3:
        if (!--ext->timer) {
            self->step = 4;
        }
        break;
    case 4:
        if (!--ext->e7E) {
            DestroyEntity(self);
            return;
        }
        break;
    }
    prim = &g_PrimBuf[self->primIndex];
    i = SX(ext->e7E);
    // the two bands meet at her x (a quad leaves out its right edge column)
    prim->x0 = prim->x2 = x - i;
    prim->x1 = prim->x3 = x;
    prim = prim->next;
    prim->x0 = prim->x2 = x;
    prim->x1 = prim->x3 = x + i;
}

#define MB_RAYS 32
#define MB_STAR 8
#define MB_SPARKS 80
static const u16 g_EInitMbHoly[] = {ANIMSET_OVL(MB_ANIMSET), 0, 0, 0, 0x1A3};

// 0x24: her holy burst (060E5190): 32 thin rainbow rays that grow from her
// for 12 frames and turn (VDP1 lines; their additive overlap makes the
// bright star at the middle), 80 sparks flying out, for 112 frames
static void MbHoly(Entity* self) {
    MbExt* ext = MB(self);
    Primitive* prim;
    s32 primIndex;
    s32 i, len, ang, c1, c2, c3;
    s16 x, y;
    s32 spd;

    x = self->posX.i.hi;
    y = self->posY.i.hi;
    if (!self->step) {
        InitializeEntity((u16*)g_EInitMbHoly);
        self->step = 1;
        self->animSet = 0;
        self->flags |= FLAG_UNK_2000 | FLAG_POS_CAMERA_LOCKED;
        // Saturn 120 x 96 (as the whole box: the rays reach about that far)
        self->hitboxWidth = 72;
        self->hitboxHeight = 72;
        primIndex =
            g_api.AllocPrimitives(PRIM_LINE_G2, MB_RAYS + MB_STAR + MB_SPARKS);
        if (primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->flags |= FLAG_HAS_PRIMS;
        self->primIndex = primIndex;
        prim = &g_PrimBuf[primIndex];
        for (i = 0; i < MB_RAYS + MB_STAR + MB_SPARKS; i++) {
            prim->priority = self->zPriority + 2;
            prim->drawMode = DRAW_HIDE;
            if (i >= MB_RAYS + MB_STAR) {
                // a spark: a 2x2 tile; x1/y1 its position (1/16 px), x2/y2
                // its speed (1/16 px per frame), x3 its delay
                prim->type = PRIM_TILE;
                ang = (Random() & 15) << 8;
                spd = g_MbSparkSpeed[Random() & 7] >> 12;
                prim->x2 = SX((rcos(ang) * spd) >> 12);
                prim->y2 = (rsin(ang) * spd) >> 12;
                prim->x3 = (Random() & 15) + 8;
                prim->x1 = x << 4;
                prim->y1 = y << 4;
                prim->x0 = x;
                prim->y0 = y;
                prim->u0 = prim->v0 = 2;
                prim->r0 = prim->g0 = prim->b0 = 0xC0;
            }
            prim = prim->next;
        }
        ext->timer = 0;
        return;
    }
    ext->ang[0] += 112;
    ext->ang[1] += 64;
    ext->ang[2] += 16;
    prim = &g_PrimBuf[self->primIndex];
    for (i = 0; i < MB_RAYS; i++) {
        // the Saturn's gouraud colour per ray (blue, green, red 5-bit
        // components from three turning angles)
        c1 = ((112 * rcos(ext->ang[0] + i * 0x110)) >> 12) + 128;
        c2 = ((112 * rsin(ext->ang[1] + i * 36)) >> 12) + 128;
        c3 = ((112 * rcos(ext->ang[2])) >> 12) + 128;
        len = ext->timer < 12 ? ext->timer + 1 : 12;
        // (x1.4: the Saturn's rays reach that far on screen)
        len = (len * (g_MbRays[i] >> 8) * 7 / 5) >> 8;
        if (self->step == 2 && (g_Timer & 1)) {
            len = -len;
        }
        ang = (ext->timer * 16 + i * 0x80) & 0xFFF;
        prim->x0 = x;
        prim->y0 = y;
        prim->x1 = x + SX((rcos(ang) * len) >> 12);
        prim->y1 = y + ((rsin(ang) * len) >> 12);
        // half-transparent, fading out towards the tip
        prim->r0 = prim->g0 = prim->b0 = 0xE0;
        prim->r1 = c3 / 2;
        prim->g1 = c2 / 2;
        prim->b1 = c1 / 2;
        prim->drawMode = DRAW_TPAGE | DRAW_TRANSP;
        prim = prim->next;
    }
    // the star at the middle: 8 short white spokes
    for (i = 0; i < MB_STAR; i++) {
        ang = i * 0x200;
        len = (i & 1) ? 7 : 14;
        prim->x0 = x;
        prim->y0 = y;
        prim->x1 = x + ((rcos(ang) * len) >> 12);
        prim->y1 = y + ((rsin(ang) * len) >> 12);
        prim->r0 = prim->g0 = prim->b0 = 0xFF;
        prim->r1 = prim->g1 = prim->b1 = 0x40;
        prim->drawMode = DRAW_TPAGE | DRAW_TPAGE2 | DRAW_TRANSP;
        prim = prim->next;
    }
    for (i = 0; i < MB_SPARKS; i++) {
        if (prim->x3) {
            prim->x3--;
        } else {
            prim->x1 += prim->x2;
            prim->y1 += prim->y2;
            prim->x0 = prim->x1 >> 4;
            prim->y0 = prim->y1 >> 4;
            prim->drawMode = DRAW_TPAGE | DRAW_TPAGE2 | DRAW_TRANSP;
        }
        prim = prim->next;
    }
    if (self->step == 2 && !(g_Timer & 7)) {
        PlaySfxPositional(0x6AF);
    }
    ext->timer++;
    if (self->step == 1 && ext->timer > 80) {
        self->step = 2;
    }
    if (ext->timer > 112) {
        DestroyEntity(self);
    }
}

// the effect parts of her frames (the Saturn draws them with her, in their
// own palettes, half-transparent): her frame's overlay image, in g_PlayerDraw
// slot 9, over her
static void MbOverlay(Entity* self) {
    MbExt* ext = MB(self);
    Entity* body = &g_Entities[MB_SLOT];
    s16 f;

    if (body->entityId != E_MOD_MARIA_BOSS || (body->params >> 8) != MB_BODY) {
        DestroyEntity(self);
        return;
    }
    if (!self->step) {
        self->step = 1;
        ext->slot = 1;
        ext->shownFrame = -1;
        self->animSet = ANIMSET_OVL(MB_ANIMSET);
        self->unk5A = 9;
        self->palette = MB_PAL;
        self->flags = FLAG_POS_CAMERA_LOCKED;
        self->blendMode = BLEND_ADD;
        g_PlayerDraw[9].enableColorBlend = 0;
    }
    self->posX.val = body->posX.val;
    self->posY.val = body->posY.val;
    self->facingLeft = body->facingLeft;
    self->zPriority = body->zPriority + 1;
    f = body->animCurFrame;
    self->animCurFrame = (f > 0 && f < MB_NFRAMES) ? g_MbOverlayOf[f] : 0;
    MbShow(self);
}

// one CEN entity for all of them
void EntityModMariaBoss(Entity* self) {
    switch (self->params >> 8) {
    case MB_BODY:
        MbBody(self);
        break;
    case MB_FLASH:
        MbFlash(self);
        break;
    case MB_AFTER:
        MbAfter(self);
        break;
    case MB_KICKBOX:
        MbKickBox(self);
        break;
    case MB_SHOT:
        MbShot(self);
        break;
    case MB_BLADE:
        MbBlade(self);
        break;
    case MB_FLAME:
        MbFlame(self);
        break;
    case MB_HOLY:
        MbHoly(self);
        break;
    case MB_SPIRIT:
        MbSpirit(self);
        break;
    case MB_OVERLAY:
        MbOverlay(self);
        break;
    }
}
