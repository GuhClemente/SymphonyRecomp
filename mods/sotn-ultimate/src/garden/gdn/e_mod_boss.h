// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Skeleton Leader boss fight (Saturn STAGE_15 room 2; entities 0x44, 0x45 and
 * 0x26-0x2D: func_060DE704 / 060DE964 / 060E78B8 / 060E8A7C / 060E8BBC /
 * 060E8D50 / 060E8FD4 / 060E9144 / 060E94A0 / 060E95E8), ported to the GDN
 * overlay (Saturn port mod, translated from the SH-2). Include after the stage
 * header and the st_common helpers. See notes.md for the fight, the slots and
 * the uncertainties; saturn_boss.c is the Saturn-semantics reference.
 *
 * The decomp names the Saturn file entbreed.c ("SkeletonBreeder"), but 0x26
 * calls TekiInit(0x194) = "Skeleton Leader" (444 HP, level 36).
 *
 * The Saturn code of this boss multiplies most of its horizontal TABLE values
 * by 5/4 at run time, so those tables are PS1 units and are used raw here.
 * Horizontal LITERALS of the Saturn code are Saturn units: SAT_X().
 *
 * Required from the including stage:
 *   MOD_BOSS_ANIMSET       animSet of the converted Saturn bank 25 (0x060F43C4,
 *                          frame table 0x060F6A90, 87 frames, Saturn frame n =
 *                          PSX frame n, 1 palette, no dy shift)
 *   MOD_BOSS_UNK5A         (optional, default 0)
 *   MOD_BOSS_DOOR_ANIMSET  animSet holding Saturn bank 16 frame 46 (the door
 *   MOD_BOSS_DOOR_FRAME    pillar, 16x64 -> 13x64) and its frame number there
 *   MOD_BOSS_DOOR_TILE     a GDN fg tile: blank art, collision 3 (solid)
 *   MOD_GDN_BOSS_FLAG      castle flag bit (collectible bit numbering) that
 *                          keeps the boss dead; proposed 831
 *   MOD_BOSS_MUSIC         the boss track (Saturn SD/SD2B.PCM; stand-in below)
 *   MOD_GDN_STAGE_MUSIC    the Garden track (Saturn SD/SD25.PCM), as in
 *                          lba_stage.c's MOD_STAGE_GDN row
 *   enum entries: E_MOD_BOSS_CONTROLLER (Sat 0x44), E_MOD_BOSS_DOOR (0x45),
 *   E_MOD_BOSS (0x26), E_MOD_BOSS_SWORD (0x27), E_MOD_BOSS_BONE (0x28),
 *   E_MOD_BOSS_BOMB (0x29), E_MOD_BOSS_GHOST (0x2A), E_MOD_BOSS_DEBRIS (0x2B),
 *   E_MOD_BOSS_BEAM (0x2C), E_MOD_BOSS_WAVE (0x2D), and the PS1 stage entities
 *   E_EXPLOSION (Sat 2), E_INTENSE_EXPLOSION (6), E_EXPLOSION_PUFF_OPAQUE
 *   (Sat 0x46, ../e_explosion_puff_opaque.h, PUFF_PARAMS_3 needed) and
 *   E_LIFE_UP_SPAWN (Sat 0x47, ../e_life_up.h).
 *   EntityExplosionVariantsSpawner (../e_misc.h, already in GDN).
 *
 * Slots: controller = layout slot 0x01 (g_Entities[65]), door = controller+1,
 * boss = g_Entities[80], sword = g_Entities[81] (boss + 1). Children use
 * AllocEntity 160..192 (beam, bones, bombs, afterimages, debris, shockwave,
 * Life Max Up), 160..256 (death fire puffs), 224..256 (explosions).
 */

#ifndef MOD_BOSS_ANIMSET
#error "define MOD_BOSS_ANIMSET before including e_mod_boss.h"
#endif
#ifndef MOD_BOSS_DOOR_ANIMSET
#error "define MOD_BOSS_DOOR_ANIMSET / MOD_BOSS_DOOR_FRAME (bank 16 frame 46)"
#endif
#ifndef MOD_BOSS_DOOR_TILE
#error "define MOD_BOSS_DOOR_TILE (blank art, collision 3)"
#endif
#ifndef MOD_BOSS_UNK5A
#define MOD_BOSS_UNK5A 0
#endif
#ifndef MOD_GDN_BOSS_FLAG
#define MOD_GDN_BOSS_FLAG 831 // g_CastleFlags[0x167] bit 7, see notes.md
#endif
#ifndef MOD_BOSS_MUSIC
#define MOD_BOSS_MUSIC MU_FESTIVAL_OF_SERVANTS // stand-in for Saturn SD2B
#endif
#ifndef MOD_GDN_STAGE_MUSIC
#define MOD_GDN_STAGE_MUSIC MU_DRACULAS_CASTLE // = the GDN row's stand-in
#endif
#define BOSS_PAL 0 // one palette, no clutBase changes

#ifndef SAT_X
#define SAT_X(v) ((v) * 4 / 5)
#endif

// TODO Z (Saturn: the boss sprite's default z; afterimage = boss - 1,
// shockwave quad = boss + 4, beam line 0x80, door 120 = 0x78, boss -16 on
// death). Room 2 fg is 0x60, its scenery 0x67-0x6A, Alucard 0x94.
#define MOD_BOSS_Z 0x90
#define MOD_BOSS_DOOR_Z 0x78
#define MOD_BOSS_BEAM_Z 0x80

// sounds: Saturn ids = PS1 ids, except the Saturn-only ones, which the
// Garden's own sound bank holds (mod_gdn_sfx.h)
#define MOD_BOSS_SFX_APPEAR SFX_EXPLODE_FAST_A   // 0x652
#define MOD_BOSS_SFX_DASH_A SFX_WHIP_TWIRL_SWISH // 0x6C7
#define MOD_BOSS_SFX_DASH_B SFX_SCRAPE_B         // 0x616
#define MOD_BOSS_SFX_THROW SFX_WEAPON_SWISH_C    // 0x60C
#define MOD_BOSS_SFX_LAND SFX_STOMP_HARD_B       // 0x647
#define MOD_BOSS_SFX_BURN SFX_UNUSED_EXPLODE_C   // 0x656
#define MOD_BOSS_SFX_WAVE SFX_BAT_ECHO_C         // 0x620 (beam + wave)
#define MOD_BOSS_SFX_DOOR SFX_STONE_MOVE_B       // 0x608
#define MOD_BOSS_SFX_BOMB MOD_SFX_BOSS_BOMB      // Saturn-only 0x8F7
#define MOD_BOSS_SFX_DEATH MOD_SFX_BOSS_DEATH    // Saturn-only 0x8F8

// st_common.h (built in the stage's st_common.c) and e_misc.h (e_misc.c)
s32 UnkCollisionFunc3(s16* sensors);
s32 UnkCollisionFunc2(s16* posX);
void PlaySfxPositional(s16 sfxId);
void EntityExplosionVariantsSpawner(
    Entity* self, u8 count, u8 params, s16 x, s16 y, u8 index, s16 xGap);

typedef enum {
    BOSS_INIT = 0,
    BOSS_INTRO = 1,
    BOSS_IDLE = 2,
    BOSS_DRAW = 3,
    BOSS_DECIDE = 4,
    BOSS_ATTACK = 5,
    BOSS_WALK = 6,
    BOSS_DASH = 7,
    BOSS_WAVE = 8,
    BOSS_LEAP = 10,
    BOSS_BACKSTEP = 11,
    BOSS_DEATH = 13,
    BOSS_HURT = 14,
    BOSS_TAUNT = 15, // unreachable on the Saturn too
    BOSS_LAUGH = 16, // the player was hit
    BOSS_GLOAT = 17, // the player died
    BOSS_LAND = 32,
    BOSS_THROW_BONE = 33,
    BOSS_THROW_BOMB = 34,
    BOSS_BURN = 48,
} BossSteps;

// controller <-> boss (Saturn bss 0x060F2180 / 0x060F217C)
#define BOSS_FIGHT_ON 1
#define BOSS_BEATEN 2
#define BOSS_GONE 4
static s32 g_ModBossFlags;
static s32 g_ModBossDoorsLocked;

// Ext fields (Saturn offsets), accessed through BOSS_EXT().
typedef struct {
    u8 timer;        // Saturn 0x78
    u8 done;         // Saturn 0x79: thrown / landed this step
    u8 playerStep;   // Saturn 0x7A: latch, 10 hit, 16 dead
    u8 dying;        // Saturn 0x7B
    s16 spin;        // Saturn 0x90 (debris)
    s32 counter;     // Saturn 0x9C (fade-in, afterimage)
    s32 width;       // Saturn 0xA6 s16 (shockwave length), here 1/256 px
    Entity* parent;  // Saturn 0xA0 (afterimage, shockwave)
    Primitive* prim; // Saturn 0x9C (beam, shockwave)
} ET_ModBoss;        // 28 bytes of the PS1 Ext
#define BOSS_EXT(e) ((ET_ModBoss*)&(e)->ext)

// Saturn func_060E7888: unlike SetStep it keeps step_s
static void BossSetStep(Entity* self, u8 step) {
    self->velocityX = 0;
    BOSS_EXT(self)->done = 0;
    self->pose = 0;
    self->poseTimer = 0;
    self->step = step;
}
static void BossFacePlayer(Entity* self) {
    self->facingLeft = (GetSideToPlayer() & 1) ^ 1;
}
static u32 BossGetRand(void) { // MTH_GetRand stand-in (low 16 bits used)
    u32 r = Random();
    return (r << 8) | Random();
}

// ---- data (STAGE_15.PRG; anims 1:1, tables in PS1 units unless noted) ----
static u8 boss_walk_time[4] = {40, 16, 32, 24}; // 0x060F4230
static u8 boss_attacks[4] = {
    BOSS_THROW_BONE, BOSS_WAVE, BOSS_THROW_BOMB, BOSS_DASH}; // 0x060F4228
static u8 boss_attacks_dying[4] = {
    BOSS_TAUNT, BOSS_LAUGH, BOSS_LAUGH, BOSS_DASH}; // 0x060F422C (unreachable)
static s32 boss_vx_dash[2] = {SAT_X(-FIX(4)), SAT_X(FIX(4))};  // 0x060F4234 Sat
static s32 boss_vx_walk[2] = {-FIX(1), FIX(1)};                // 0x060F423C
static s32 boss_vx_leap[2] = {-FIX(2), FIX(2)};                // 0x060F4244
static s32 boss_vx_back[2] = {FIX(1.5), -FIX(1.5)};            // 0x060F424C
static s16 boss_bone_dx[2] = {-32, 32};                        // 0x060F4254
static s16 boss_bomb_dx[2] = {-40, 40};                        // 0x060F4258
static s16 boss_sensors_fall[] = {0, 32, 0, 4, 8, -4, -16, 0}; // 0x060F425C
static s16 boss_sensors_walk[] = {0, 32, 8, 0};                // 0x060F426C
static s16 boss_hitbox[3][4] = { // 0x060F4274 / 0x060F4280: offX, offY, w, h
    {0, 0, 7, 22},
    {1, 13, 8, 16},
    {-5, 18, 13, 11}};
static u8 boss_anim_idle[] = {9, 1, 9, 2, 9, 3, 9, 4, 9, 5, 0, 0};
static u8 boss_anim_draw[] = {
    0x13, 5, 3,  6, 1,  7, 1,  8, 1,  9, 1,  10,   1,  11,   1,
    12,   1, 13, 2, 14, 3, 15, 5, 16, 7, 17, 0x10, 16, 0xFF, 0};
static u8 boss_anim_taunt[] = {9, 18, 7, 19, 7, 20, 7, 21, 7, 22, 7, 19, 0, 0};
static u8 boss_anim_walk[] = {9, 23, 9, 24, 9, 25, 9, 26, 0, 0};
static u8 boss_anim_dash[] = {
    8,  25,   8,  27, 8,  28,   8,  29, 8,  30, 8,  31, 8,  32,   0x10,
    33, 0x10, 34, 6,  35, 0x10, 36, 4,  37, 4,  38, 4,  39, 0xFF, 0};
static u8 boss_anim_laugh[] = {7, 40, 7, 41, 7, 40, 7, 41, 7, 40, 7, 41, 0, 0};
static u8 boss_anim_leap[] = {8, 53, 8, 55, 8, 54, 8, 55, 0, 0};
static u8 boss_anim_back[] = {11, 43, 11, 44, 11, 45, 11, 46, 0, 0};
static u8 boss_anim_land[] = {8, 48, 8, 49, 8, 50, 8, 51, 0xFF, 0};
static u8 boss_anim_bone[] = {
    6, 57, 6, 58, 6, 59, 0x18, 60, 12, 61, 6, 62, 5, 63, 0xFF, 0};
static u8 boss_anim_bomb[] = {4,  57, 4,  58,   4,  59, 0x10, 60, 4,  64,   4,
                              65, 4,  66, 0x11, 67, 4,  68,   4,  69, 0xFF, 0};
static u8 boss_anim_hurt[] = {4, 70, 4, 71, 4, 72, 0, 0};
static u8 boss_anim_hurt_land[] = {
    3, 73, 4, 74, 4, 75, 5, 76, 6, 77, 3, 78, 6, 76, 0x10, 77, 3, 73, 0xFF, 0};
static u8 boss_anim_gloat[] = {
    5, 79, 5, 80, 5, 81, 5, 82, 5, 81, 5, 80, 5, 79, 5, 80, 5, 81, 5, 82, 5, 81,
    5, 80, 7, 40, 7, 41, 7, 40, 9, 18, 7, 19, 7, 20, 7, 21, 7, 22, 7, 19, 9, 18,
    7, 19, 7, 20, 7, 21, 7, 22, 7, 19, 7, 40, 7, 41, 7, 40, 0, 0};
static u8 boss_anim_kneel[] = {10, 16, 10, 15, 10, 14, 10, 13, 10, 12, 0xFF, 0};
static u8 boss_anim_rise[] = {10, 12, 10, 13, 10, 14, 10, 15, 10, 16, 0xFF, 0};
static u8 boss_anim_bone_spin[] = {4, 83, 4, 84, 0, 0};       // 0x060F4628
static s32 boss_vx_bone[2] = {-FIX(5), FIX(5)};               // 0x060F4630
static u8 boss_bomb_fuse[4] = {32, 56, 64, 72};               // 0x060F4638
static s32 boss_vx_bomb[2] = {-FIX(2.5), FIX(2.5)};           // 0x060F463C
static s16 boss_sensors_small[] = {0, 8, 0, 4, 4, -4, -8, 0}; // 0x060F4644

// 0x060F4400 / 0x060F4514 (decomp: g_Stage15SkeletonBreederPositionOffsets /
// HitboxSizes): the sword hitbox {offX, offY} / {w, h} for boss frames 14..82
static s16 boss_sword_off[69][2] = {
    {0, 0},    {-16, -3}, {-16, -3}, {-16, -2}, {0, 0},    {0, 0},   {0, 0},
    {0, 0},    {0, 0},    {-12, 0},  {-12, 0},  {-13, -2}, {-12, 1}, {-13, -2},
    {-13, -3}, {-13, -1}, {2, 9},    {2, 9},    {2, 14},   {4, 15},  {13, 2},
    {-49, 9},  {-49, 9},  {-49, 9},  {-49, 9},  {-49, 9},  {0, 0},   {0, 0},
    {0, 0},    {0, 0},    {0, 0},    {0, 0},    {0, 0},    {0, 0},   {0, 0},
    {0, 0},    {0, 0},    {0, 0},    {0, 0},    {0, 0},    {0, 0},   {0, 10},
    {0, 0},    {0, 0},    {0, 0},    {31, -8},  {16, 0},   {16, 0},  {21, 18},
    {15, 2},   {0, 0},    {0, 0},    {0, 0},    {0, 0},    {0, 0},   {0, 0},
    {0, 0},    {0, 0},    {0, 0},    {0, 0},    {0, 0},    {0, 0},   {0, 0},
    {0, 0},    {0, 0},    {0, 0},    {26, 3},   {26, 3},   {26, 3},
};
static s16 boss_sword_size[69][2] = {
    {0, 0},  {13, 2}, {13, 2}, {13, 2}, {0, 0},  {0, 0},  {0, 0},  {0, 0},
    {0, 0},  {13, 2}, {13, 2}, {13, 2}, {13, 2}, {13, 2}, {13, 2}, {13, 2},
    {13, 2}, {13, 2}, {13, 2}, {13, 2}, {13, 2}, {13, 2}, {13, 2}, {13, 2},
    {13, 2}, {13, 2}, {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},
    {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},
    {0, 0},  {13, 2}, {0, 0},  {0, 0},  {0, 0},  {6, 8},  {6, 8},  {6, 8},
    {6, 8},  {6, 8},  {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},
    {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},  {0, 0},
    {0, 0},  {2, 14}, {2, 14}, {2, 14}, {2, 14},
};

static void BossInit(Entity* self, u16 frame, u16 enemy) {
    u16 init[5];
    init[0] = MOD_BOSS_ANIMSET;
    init[1] = frame;
    init[2] = MOD_BOSS_UNK5A;
    init[3] = BOSS_PAL;
    init[4] = enemy;
    InitializeEntity(init); // step -> 1; keeps a nonzero zPriority
}

static bool BossFlagSet(void) {
    return g_CastleFlags[0x100 + (MOD_GDN_BOSS_FLAG >> 3)] &
           (1 << (MOD_GDN_BOSS_FLAG & 7));
}

// ---------------- Saturn 0x44: the fight controller ----------------
void EntityModBossController(Entity* self) {
    u16 init[5];
    Entity* e;

    switch (self->step) {
    case 0:
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = ENEMY_INTERACTABLE; // Saturn TekiInit(self, 5)
        InitializeEntity(init);
        // Saturn: TimeAttackController(22, GET_RECORD) (its own event 22)
        if (BossFlagSet()) {
            DestroyEntity(self);
            return;
        }
        g_ModBossFlags = 0;
        g_ModBossDoorsLocked = 0;
        e = &g_Entities[80];
        CreateEntityFromCurrentEntity(E_MOD_BOSS, e);
        e->posX.i.hi = SAT_X(240) - g_Tilemap.scrollX.i.hi; // Saturn 240
        e->posY.i.hi = 176 - g_Tilemap.scrollY.i.hi;
        /* fallthrough */
    case 1:
        if (g_Player.demo_timer) {
            break;
        }
        g_ModBossFlags |= BOSS_FIGHT_ON;
        // Saturn: TimeAttackController(22, SET_VISITED); no PS1 event
        self->step++;
        break;

    case 2:
        e = self + 1;
        CreateEntityFromCurrentEntity(E_MOD_BOSS_DOOR, e);
        e->posX.i.hi = SAT_X(-10) - g_Tilemap.scrollX.i.hi;
        e->posY.i.hi = 128 - g_Tilemap.scrollY.i.hi;
        e->params = 0;
        g_ModBossDoorsLocked = 1;
        self->step++;
        /* fallthrough */
    case 3:
        if (g_api.func_80131F68() != false) {
            g_api.PlaySfx(SET_UNK_90);
        }
        stopMusicFlag = true;
        currentMusicId = MOD_BOSS_MUSIC; // Saturn 0xE000012B = SD/SD2B.PCM
        self->step++;
        break;

    case 4:
        if (g_api.func_80131F68() == false) {
            stopMusicFlag = false;
            g_api.PlaySfx(currentMusicId);
            self->step++;
        }
        /* fallthrough */
    case 5:
        if (g_ModBossFlags & BOSS_BEATEN) {
            // Saturn: TimeAttackController(22, SET_RECORD)
            g_CastleFlags[0x100 + (MOD_GDN_BOSS_FLAG >> 3)] |=
                1 << (MOD_GDN_BOSS_FLAG & 7);
            if (g_api.func_80131F68() != false) {
                g_api.PlaySfx(SET_UNK_90);
            }
            currentMusicId = MOD_GDN_STAGE_MUSIC; // Saturn 0xE0000125 (SD25)
            self->step++;
        }
        break;

    case 6:
        if (g_ModBossFlags & BOSS_GONE) {
            e = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (e != NULL) {
                CreateEntityFromEntity(E_LIFE_UP_SPAWN, self, e);
                e->posX.i.hi = SAT_X(160); // Saturn 160 = the screen centre
                e->posY.i.hi = 128;
                e->params = 7; // ITEMDROP_LIFE_VESSEL: Life Max Up
                g_ModBossDoorsLocked = 0;
                stopMusicFlag = true;
                currentMusicId = MOD_GDN_STAGE_MUSIC;
                self->step++;
            }
        }
        break;

    case 7:
        if (g_api.func_80131F68() == false) {
            stopMusicFlag = false;
            g_api.PlaySfx(currentMusicId);
            self->step++;
        }
        break;
    }
}

// ---------------- Saturn 0x45: the door (= PS1 NZ1 EntityBossDoors) -------
// Room 2 is one screen, (21,44); its only exit is the left one, rows 6-9.
void EntityModBossDoor(Entity* self) {
    u16 init[5];
    Entity* e;
    s32 x, i, tile;

    switch (self->step) {
    case 0:
        init[0] = MOD_BOSS_DOOR_ANIMSET;
        init[1] = MOD_BOSS_DOOR_FRAME;
        init[2] = 0;
        init[3] = 0;
        init[4] = ENEMY_INTERACTABLE;
        InitializeEntity(init);
        self->animCurFrame = MOD_BOSS_DOOR_FRAME;
        self->zPriority = MOD_BOSS_DOOR_Z; // TODO Z (Saturn 120, as NZ1 0x78)
        break;

    case 1:
        if (g_ModBossDoorsLocked) {
            self->step++;
        }
        break;

    case 2:
    case 5:
        GetPlayerCollisionWith(self, SAT_X(10), 32, 5); // NZ1 uses 8 too
        if (self->step == 2) {
            self->velocityX = self->params ? SAT_X(-FIX(0.5)) : SAT_X(FIX(0.5));
        } else {
            self->velocityX =
                self->params ? SAT_X(FIX(0.75)) : SAT_X(-FIX(0.75));
        }
        MoveEntity();
        e = AllocEntity(&g_Entities[224], &g_Entities[256]);
        if (e != NULL) {
            CreateEntityFromEntity(E_INTENSE_EXPLOSION, self, e);
            e->params = 0x10;
            e->posY.i.hi += 32;
            e->posX.i.hi -= Random() & 7;
        }
        x = self->posX.i.hi + g_Tilemap.scrollX.i.hi;
        if (self->step == 2) {
            if (!(g_Timer & 0xF)) {
                g_api.PlaySfx(MOD_BOSS_SFX_DOOR);
            }
            if (self->params ? x <= SAT_X(296) : x > SAT_X(30)) {
                self->step++;
            }
        } else if (self->params ? x > SAT_X(330) : x < 0) {
            DestroyEntity(self);
        }
        break;

    case 3:
        // Saturn writes collision 3 into its collision map, cells 97 / 110
        // (+16 a row); the PS1 writes a solid fg tile (as NZ1)
        tile = self->params ? 0x6E : 0x61;
        for (i = 0; i < 4; i++, tile += 16) {
            g_Tilemap.fg[tile] = MOD_BOSS_DOOR_TILE;
        }
        self->step++;
        /* fallthrough */
    case 4:
        if (!g_ModBossDoorsLocked) {
            tile = self->params ? 0x6E : 0x61;
            for (i = 0; i < 4; i++, tile += 16) {
                g_Tilemap.fg[tile] = 0; // room 2's col 1 is tile 0 there
            }
            self->step++;
        }
        break;
    }
}

// ---------------- Saturn 0x26: the Skeleton Leader ----------------
void EntityModBoss(Entity* self) {
    Entity* e;
    s32 d, f, k, x;

    if (self->step > BOSS_INTRO) {
        if (PLAYER.step == Player_Hit || PLAYER.step == Player_Kill) {
            BOSS_EXT(self)->playerStep = PLAYER.step; // 10 / 16
        }
        if (self->step >= BOSS_DRAW && self->step <= BOSS_ATTACK) {
            if (BOSS_EXT(self)->playerStep == Player_Hit) {
                BossFacePlayer(self);
                BossSetStep(self, BOSS_LAUGH);
            } else if (BOSS_EXT(self)->playerStep == Player_Kill) {
                BossFacePlayer(self);
                BossSetStep(self, BOSS_GLOAT);
            }
            BOSS_EXT(self)->playerStep = 0;
        }
        if (!BOSS_EXT(self)->dying && (self->flags & FLAG_DEAD)) {
            self->zPriority -= 16; // TODO Z (Saturn sprite z - 16)
            self->flags &= ~FLAG_DEAD;
            self->hitboxState = 0;
            BOSS_EXT(self)->dying = 1;
            BossSetStep(self, BOSS_HURT);
            k = (GetSideToPlayer() & 1) ^ 1; // not stored to facingLeft
            self->velocityX = boss_vx_back[k] / 2;
            self->velocityY = -FIX(5);
        }
        if (self->hitParams && self->step > BOSS_HURT) {
            BossSetStep(self, BOSS_HURT);
            k = (GetSideToPlayer() & 1) ^ 1;
            self->velocityX = boss_vx_back[k] / 2;
            self->velocityY = -FIX(2.5);
        }
    }

    switch (self->step) {
    case BOSS_INIT:
        BossInit(self, 0, 0x194); // "Skeleton Leader"
        self->zPriority = MOD_BOSS_Z;
        self->step = BOSS_INTRO;
        self->step_s = 0;
        BOSS_EXT(self)->timer = 0;
        BOSS_EXT(self)->done = 0;
        BOSS_EXT(self)->playerStep = 0;
        BOSS_EXT(self)->dying = 0;
        CreateEntityFromEntity(E_MOD_BOSS_SWORD, self, self + 1);
        self->animCurFrame = 0;
        self->drawFlags = ENTITY_OPACITY;
        self->opacity = 0;
        BOSS_EXT(self)->counter = 0;
        break;

    case BOSS_INTRO:
        switch (self->step_s) {
        case 0:
            BossFacePlayer(self);
            if (GetDistanceToPlayerX() > SAT_X(127)) {
                break;
            }
            self->step_s = 1;
            BOSS_EXT(self)->timer = 16;
            e = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (e != NULL) {
                CreateEntityFromEntity(E_MOD_BOSS_BEAM, self, e);
                e->posY.i.hi = self->posY.i.hi + 32; // the floor
            }
            break;
        case 1:
            if (--BOSS_EXT(self)->timer == 0) {
                self->step_s = 2;
            }
            break;
        case 2:
            if (!(UnkCollisionFunc3(boss_sensors_fall) & 1)) {
                break;
            }
            self->step_s = 3;
            e = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (e != NULL) {
                CreateEntityFromEntity(E_MOD_BOSS_BONE, self, e);
                e->facingLeft = self->facingLeft;
            }
            g_api.func_800EA5AC(6, 47, 95, 0); // green background flash
            break;
        case 3: // fades in (Saturn opacity 0..143; PS1 0x80 = normal)
            self->animCurFrame = 1;
            if (g_Timer & 1) {
                BOSS_EXT(self)->counter++;
            }
            self->opacity =
                BOSS_EXT(self)->counter > 0x80 ? 0x80 : BOSS_EXT(self)->counter;
            if (BOSS_EXT(self)->counter > 143) {
                self->step_s = 4;
            }
            if (!(g_Timer & 3)) {
                e = AllocEntity(&g_Entities[224], &g_Entities[256]);
                if (e != NULL) {
                    CreateEntityFromEntity(E_EXPLOSION, self, e);
                    e->params = 0x11;
                    // the smoke rises in front of the boss (Saturn capture);
                    // InitializeEntity keeps a nonzero zPriority
                    e->zPriority = self->zPriority + 2;
                    e->posX.i.hi += SAT_X((Random() & 0x1F) - 16);
                    e->posY.i.hi += 32 - (Random() & 0x1F);
                }
            }
            if (!(g_Timer & 7)) {
                PlaySfxPositional(MOD_BOSS_SFX_APPEAR);
            }
            break;
        case 4:
            if (g_ModBossFlags & BOSS_FIGHT_ON) {
                self->step = BOSS_IDLE;
                self->step_s = 0;
            }
            break;
        }
        break;

    case BOSS_IDLE:
        BossFacePlayer(self);
        AnimateEntity(boss_anim_idle, self);
        if (GetDistanceToPlayerX() <= SAT_X(139)) {
            BossSetStep(self, BOSS_DRAW);
        }
        if (self->hitFlags & 3) {
            BossSetStep(self, BOSS_DRAW);
        }
        break;

    case BOSS_DRAW:
        if (!AnimateEntity(boss_anim_draw, self)) {
            BossSetStep(self, BOSS_DASH);
        }
        break;

    case BOSS_DECIDE:
        BossFacePlayer(self);
        BossSetStep(self, BOSS_WALK);
        self->velocityX = boss_vx_walk[self->facingLeft];
        BOSS_EXT(self)->timer = boss_walk_time[Random() & 3];
        d = GetDistanceToPlayerX();
        if (d > SAT_X(160)) {
            BOSS_EXT(self)->timer = 6;
            BossSetStep(self, BOSS_LEAP);
        }
        if (d <= SAT_X(99)) {
            BossSetStep(self, BOSS_ATTACK);
        }
        break;

    case BOSS_ATTACK:
        BossFacePlayer(self);
        if (!BOSS_EXT(self)->dying) {
            BossSetStep(self, boss_attacks[Random() & 3]);
            if (self->step == BOSS_WAVE) {
                self->step_s = 0;
            }
        } else {
            BossSetStep(self, boss_attacks_dying[Random() & 3]);
        }
        break;

    case BOSS_WALK:
        self->velocityX = boss_vx_walk[self->facingLeft];
        UnkCollisionFunc2(boss_sensors_walk);
        AnimateEntity(boss_anim_walk, self);
        if (--BOSS_EXT(self)->timer == 0) {
            BossSetStep(self, BOSS_DECIDE);
        }
        break;

    case BOSS_DASH:
        // pose 10 with timer 0: the next AnimateEntity starts frame 35
        if (self->pose == 10 && self->poseTimer == 0) {
            g_api.PlaySfx(MOD_BOSS_SFX_DASH_A);
            g_api.PlaySfx(MOD_BOSS_SFX_DASH_B);
        }
        UnkCollisionFunc2(boss_sensors_walk);
        if (!AnimateEntity(boss_anim_dash, self)) {
            if (Random() & 1) {
                BOSS_EXT(self)->timer = 6;
                BossSetStep(self, BOSS_BACKSTEP);
            } else {
                BOSS_EXT(self)->timer = 16;
                BossSetStep(self, BOSS_WALK);
            }
        }
        if (self->animCurFrame == 35) {
            self->velocityX = boss_vx_dash[self->facingLeft];
        }
        f = self->animCurFrame - 35;
        if (f < 0 || f > 4 || BOSS_EXT(self)->dying) {
            break;
        }
        if (!(g_Timer & 7)) {
            e = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (e != NULL) {
                CreateEntityFromEntity(E_MOD_BOSS_GHOST, self, e);
                e->facingLeft = self->facingLeft;
                e->animCurFrame = self->animCurFrame;
                BOSS_EXT(e)->parent = self;
            }
        }
        if (g_Timer & 1) { // dust at the feet
            x = SAT_X(Random() & 7);
            k = (Random() & 3) + 30;
            EntityExplosionVariantsSpawner(
                self, 1, 1, x, k, (Random() & 3) + 1, 0);
        }
        break;

    case BOSS_WAVE:
        switch (self->step_s) {
        case 0:
            BossFacePlayer(self);
            if (GetDistanceToPlayerX() > SAT_X(23)) {
                self->step_s = 1;
            } else {
                BossSetStep(self, BOSS_DECIDE);
            }
            break;
        case 1:
            if (AnimateEntity(boss_anim_kneel, self)) {
                break;
            }
            self->step_s = 2;
            BOSS_EXT(self)->timer = 64;
            e = AllocEntity(&g_Entities[160], &g_Entities[192]);
            if (e != NULL) {
                CreateEntityFromEntity(E_MOD_BOSS_WAVE, self, e);
                e->facingLeft = self->facingLeft;
                BOSS_EXT(e)->parent = self;
            }
            break;
        case 2: // shakes 1 px left / right
            self->posX.i.hi += (g_Timer & 1) ? 1 : -1;
            self->animCurFrame = 12;
            if (--BOSS_EXT(self)->timer == 0) {
                self->step_s = 3;
            }
            break;
        case 3:
            if (!AnimateEntity(boss_anim_rise, self)) {
                BossSetStep(self, BOSS_WALK);
                self->step_s = 0;
                BOSS_EXT(self)->timer = 16;
            }
            break;
        }
        break;

    case BOSS_LEAP:
    case BOSS_BACKSTEP:
        if (BOSS_EXT(self)->timer) { // crouches before the jump
            if (self->step == BOSS_LEAP) {
                self->animCurFrame = 52;
                self->velocityX = boss_vx_leap[self->facingLeft];
                self->velocityY = -FIX(6);
            } else {
                self->animCurFrame = 42;
                self->velocityX = boss_vx_back[self->facingLeft];
                self->velocityY = -FIX(5);
            }
            BOSS_EXT(self)->timer--;
            break;
        }
        AnimateEntity(
            self->step == BOSS_LEAP ? boss_anim_leap : boss_anim_back, self);
        if (UnkCollisionFunc3(boss_sensors_fall) & 1) {
            self->animCurFrame = self->step == BOSS_LEAP ? 56 : 47;
            BossSetStep(self, BOSS_LAND);
            g_api.PlaySfx(MOD_BOSS_SFX_LAND);
        }
        break;

    case BOSS_LAND:
        if (!AnimateEntity(boss_anim_land, self)) {
            BossFacePlayer(self);
            BossSetStep(self, BOSS_DECIDE);
        }
        break;

    case BOSS_THROW_BONE:
    case BOSS_THROW_BOMB:
        k = self->step == BOSS_THROW_BONE;
        if (!AnimateEntity(k ? boss_anim_bone : boss_anim_bomb, self)) {
            BossSetStep(self, BOSS_DECIDE);
        }
        if (self->animCurFrame != (k ? 62 : 67) || BOSS_EXT(self)->done ||
            BOSS_EXT(self)->dying) {
            break;
        }
        BOSS_EXT(self)->done = 1;
        e = AllocEntity(&g_Entities[160], &g_Entities[192]);
        if (e != NULL) {
            CreateEntityFromEntity(
                k ? E_MOD_BOSS_BONE : E_MOD_BOSS_BOMB, self, e);
            e->facingLeft = self->facingLeft;
            e->posX.i.hi += k ? boss_bone_dx[self->facingLeft]
                              : boss_bomb_dx[self->facingLeft];
        }
        g_api.PlaySfx(MOD_BOSS_SFX_THROW);
        break;

    case BOSS_HURT:
        if (UnkCollisionFunc3(boss_sensors_fall) & 1) {
            if (!BOSS_EXT(self)->done) {
                self->pose = 0;
                self->poseTimer = 0;
                BOSS_EXT(self)->done = 1;
                if (BOSS_EXT(self)->dying) {
                    BossSetStep(self, BOSS_DEATH);
                }
            }
            if (!AnimateEntity(boss_anim_hurt_land, self)) {
                BossSetStep(self, BOSS_DECIDE);
            }
        } else {
            AnimateEntity(boss_anim_hurt, self);
            if (!BOSS_EXT(self)->dying && !(g_Timer & 3)) {
                e = AllocEntity(&g_Entities[160], &g_Entities[192]);
                if (e != NULL) {
                    CreateEntityFromEntity(E_MOD_BOSS_DEBRIS, self, e);
                    e->params = Random() & 1; // 0 a bone piece, 1 a bomb
                }
            }
        }
        break;

    case BOSS_DEATH:
        g_ModBossFlags |= BOSS_BEATEN;
        e = AllocEntity(&g_Entities[224], &g_Entities[256]);
        if (e != NULL) {
            CreateEntityFromEntity(E_EXPLOSION, self, e);
            e->params = 3;
        }
        BossSetStep(self, BOSS_BURN);
        BOSS_EXT(self)->timer = 160;
        g_api.PlaySfx(MOD_BOSS_SFX_DEATH);
        break;

    case BOSS_BURN:
        if (--BOSS_EXT(self)->timer == 0) {
            g_ModBossFlags |= BOSS_GONE;
            DestroyEntity(self);
            return;
        }
        e = AllocEntity(&g_Entities[160], &g_Entities[256]);
        if (e != NULL) {
            CreateEntityFromEntity(E_EXPLOSION_PUFF_OPAQUE, self, e);
            e->facingLeft = Random() & 1;
            e->posY.i.hi += 40;
            e->params = 3;
        }
        break;

    case BOSS_TAUNT: // unreachable (kept for completeness)
        AnimateEntity(boss_anim_taunt, self);
        if (self->step_s == 0) {
            self->step_s = 1;
            BOSS_EXT(self)->timer = 48;
        } else if (self->step_s != 1) {
            break;
        }
        d = GetDistanceToPlayerX();
        if (d > SAT_X(160)) {
            BOSS_EXT(self)->timer = 6;
            BossSetStep(self, BOSS_LEAP);
        }
        if (d <= SAT_X(79)) {
            BOSS_EXT(self)->timer = 6;
            BossSetStep(self, BOSS_BACKSTEP);
        }
        if (--BOSS_EXT(self)->timer == 0) {
            BossSetStep(self, BOSS_DASH);
        }
        break;

    case BOSS_LAUGH:
        if (!AnimateEntity(boss_anim_laugh, self)) {
            BossSetStep(self, BOSS_DECIDE);
        }
        break;

    case BOSS_GLOAT:
        AnimateEntity(boss_anim_gloat, self);
        break;
    }

    // epilogue: the body hitbox by frame
    if (self->step == BOSS_INTRO) {
        self->hitboxWidth = 0;
        self->hitboxHeight = 0;
    } else {
        f = self->animCurFrame;
        k = 0;
        if ((u16)(f - 35) <= 4 || (u16)(f - 48) <= 3 || (u16)(f - 60) <= 9) {
            k = 1;
        }
        if ((u16)(f - 74) <= 4) {
            k = 2;
        }
        self->hitboxOffX = boss_hitbox[k][0];
        self->hitboxOffY = boss_hitbox[k][1];
        self->hitboxWidth = boss_hitbox[k][2];
        self->hitboxHeight = boss_hitbox[k][3];
    }
    // keeps it inside the room (Saturn x 56..264)
    if (self->step != BOSS_DEATH) {
        x = self->posX.i.hi + g_Tilemap.scrollX.i.hi;
        if (self->velocityX < 0) {
            if (x < SAT_X(56)) {
                self->posX.i.hi = SAT_X(56) - g_Tilemap.scrollX.i.hi;
            }
        } else if (x > SAT_X(264)) {
            self->posX.i.hi = SAT_X(264) - g_Tilemap.scrollX.i.hi;
        }
    }
}

// ---------------- Saturn 0x27: the sword hitbox (boss + 1) ----------------
void EntityModBossSword(Entity* self) {
    Entity* parent = self - 1;
    s32 i;

    if (self->step == 0) {
        BossInit(self, 0, 0x18D); // attack 120, cut
        self->animCurFrame = 0;   // invisible
        self->attack /= 2;
        self->hitboxState = 1;
    }
    i = (u8)parent->animCurFrame;
    if (i <= 14) {
        i = 14;
    }
    i -= 14;
    if (i > 68) {
        i = 68; // safety: the Saturn table ends at frame 82
    }
    self->hitboxOffX = boss_sword_off[i][0];
    self->hitboxOffY = boss_sword_off[i][1];
    self->hitboxWidth = boss_sword_size[i][0];
    self->hitboxHeight = boss_sword_size[i][1];
    self->facingLeft = parent->facingLeft;
    if (parent->step == BOSS_DASH) {
        self->attackElement |= ELEMENT_POISON; // Saturn |= 0x80, see notes
    } else {
        self->attackElement = ELEMENT_UNK_1;
    }
    self->posX.i.hi = parent->posX.i.hi;
    self->posY.i.hi = parent->posY.i.hi;
    if (BOSS_EXT(parent)->dying || parent->entityId != E_MOD_BOSS) {
        DestroyEntity(self);
    }
}

// ---------------- Saturn 0x28: a thrown bone ----------------
void EntityModBossBone(Entity* self) {
    if (self->step == 0) {
        BossInit(self, 83, 0x18D);
        self->zPriority = MOD_BOSS_Z + 1; // TODO Z
        self->attack /= 4;
        self->flags |= FLAG_DESTROY_IF_OUT_OF_CAMERA |
                       FLAG_DESTROY_IF_BARELY_OUT_OF_CAMERA;
        self->velocityX = boss_vx_bone[self->facingLeft];
        self->hitboxWidth = 2;
        self->hitboxHeight = 2;
    }
    if ((self->flags & FLAG_DEAD) && self->step_s == 0) { // knocked away
        self->velocityX = boss_vx_bone[GetSideToPlayer() & 1] / 2;
        self->velocityY = -FIX(2);
        self->step_s = 1;
        self->hitboxState = 0;
    }
    AnimateEntity(boss_anim_bone_spin, self);
    MoveEntity();
    if (self->step_s) {
        self->velocityY += FIX(0.25);
    }
}

// shared by the bomb and the debris bomb (Saturn steps 2/3 and 33/34); the
// bomb allocates its explosion in 160..192, the debris bomb in 224..256
static void BossBombExplode(Entity* self, u8 w, s32 lo, s32 hi) {
    Entity* e = AllocEntity(&g_Entities[lo], &g_Entities[hi]);
    if (e != NULL) {
        CreateEntityFromEntity(E_EXPLOSION, self, e);
        e->params = 3;
        g_api.PlaySfx(MOD_BOSS_SFX_BOMB);
    }
    self->hitboxWidth = SAT_X(w);
    self->hitboxHeight = 16;
    self->hitboxOffY = -12;
    self->animCurFrame = 0;
    self->step++;
}
static bool BossBombBurn(Entity* self) {
    if (!(g_Timer & 7)) {
        PlaySfxPositional(MOD_BOSS_SFX_BURN);
    }
    if (BOSS_EXT(self)->timer++ > 32) {
        DestroyEntity(self);
        return true;
    }
    return false;
}

// ---------------- Saturn 0x29: a thrown bomb (rotates) ----------------
void EntityModBossBomb(Entity* self) {
    if ((self->flags & FLAG_DEAD) && self->step <= 2) {
        self->step = 2;
    }
    switch (self->step) {
    case 0:
        BossInit(self, 85, 0xB8);         // attack 23, fire
        self->zPriority = MOD_BOSS_Z + 1; // TODO Z
        self->attack *= 2;
        self->hitboxWidth = 0;
        self->hitboxHeight = 0;
        self->drawFlags = ENTITY_ROTATE;
        BOSS_EXT(self)->timer = boss_bomb_fuse[Random() & 3];
        self->velocityY = -FIX(6);
        self->velocityX = boss_vx_bomb[self->facingLeft];
        self->animCurFrame = 85;
        break;
    case 1:
        self->rotate += 0x100;
        if (UnkCollisionFunc3(boss_sensors_small) & 1) {
            self->step = 2;
        }
        if (--BOSS_EXT(self)->timer == 0) {
            self->step = 2;
        }
        if (BOSS_EXT(self)->timer <= 23) {
            self->animCurFrame = 86;
        }
        break;
    case 2:
        BossBombExplode(self, 17, 160, 192);
        break;
    case 3:
        BossBombBurn(self);
        break;
    }
}

// ---------------- Saturn 0x2A: a dash afterimage ----------------
void EntityModBossGhost(Entity* self) {
    s32 t;

    if (BOSS_EXT(self)->parent->flags & FLAG_DEAD) {
        DestroyEntity(self);
        return;
    }
    switch (self->step) {
    case 0:
        t = self->animCurFrame;
        BossInit(self, t, ENEMY_INTERACTABLE);
        self->animCurFrame = t;
        self->drawFlags = ENTITY_OPACITY | ENTITY_MASK_B; // Saturn 0x48
        self->opacity = 0x80;
        BOSS_EXT(self)->counter = 0;
        self->zPriority = BOSS_EXT(self)->parent->zPriority - 1; // TODO Z
        break;
    case 1:
        t = ++BOSS_EXT(self)->counter;
        if (t > 31) {
            self->step++;
        } else {
            self->opacity = 0x80 - t * 4;
        }
        if (t == 16) {
            // Saturn sprite flag 0x80 (meaning unconfirmed): half-transparent
            self->blendMode = BLEND_TRANSP;
        }
        if (t == 24) {
            self->drawFlags |= ENTITY_BLINK;
        }
        break;
    case 2:
        DestroyEntity(self);
        return;
    }
}

// ---------------- Saturn 0x2B: debris shed when hurt ----------------
void EntityModBossDebris(Entity* self) {
    switch (self->step) {
    case 0:
        if (self->params) {
            BossInit(self, 86, 0xB8);
            self->attack = 25;
        } else {
            BossInit(self, 83, 0x18D);
            self->attack = 30;
        }
        self->zPriority = MOD_BOSS_Z + 1; // TODO Z
        self->drawFlags = ENTITY_ROTATE;
        self->rotate = BossGetRand() & 0xFFF;
        BOSS_EXT(self)->spin = (BossGetRand() & 127) + 32;
        if (self->params == 0) {
            self->animCurFrame = 83;
            self->hitboxWidth = SAT_X(6);
            self->hitboxHeight = 6;
            self->step = 16;
        } else {
            self->animCurFrame = 86;
            self->hitboxWidth = 0;
            self->hitboxHeight = 0;
            BOSS_EXT(self)->spin = -BOSS_EXT(self)->spin;
            BOSS_EXT(self)->timer = (u8)Random() % 48 + 32;
            self->step = 32;
        }
        self->velocityY = ((s32)(BossGetRand() & 0x1FFF) - 0x6000) << 4;
        self->velocityX = SAT_X(((s32)(BossGetRand() & 0x7FFF) - 0x4000) << 4);
        break;
    case 16:
        if (UnkCollisionFunc3(boss_sensors_small) & 1) {
            self->hitboxState = 0;
            self->drawFlags = ENTITY_ROTATE | ENTITY_BLINK;
            self->step++;
        }
        break;
    case 17: // bounces until the bounce is too small
        self->velocityY = -(self->velocityY / 2);
        if (self->velocityY > -0x100) {
            DestroyEntity(self);
            return;
        }
        self->step++;
        break;
    case 18:
        if (UnkCollisionFunc3(boss_sensors_small) & 1) {
            self->step--;
        }
        break;
    case 32:
        if (UnkCollisionFunc3(boss_sensors_small) & 1) {
            self->step++;
        }
        if (--BOSS_EXT(self)->timer == 0) {
            self->step++;
        }
        break;
    case 33:
        BossBombExplode(self, 20, 224, 256);
        BOSS_EXT(self)->timer = 0;
        break;
    case 34:
        if (BossBombBurn(self)) {
            return;
        }
        break;
    }
    self->rotate += BOSS_EXT(self)->spin;
}

// ---------------- Saturn 0x2C: the light beam before the boss appears -----
void EntityModBossBeam(Entity* self) {
    u16 init[5];
    Primitive* prim;
    s32 primIndex;

    if (self->step == 0) {
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = ENEMY_INTERACTABLE;
        InitializeEntity(init);
        primIndex = g_api.AllocPrimitives(PRIM_LINE_G2, 1); // Saturn 0x8005
        if (primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->primIndex = primIndex;
        self->flags |= FLAG_HAS_PRIMS;
        self->hitboxState = 0;
        self->hitboxWidth = 0;
        self->hitboxHeight = 0;
        prim = &g_PrimBuf[primIndex];
        BOSS_EXT(self)->prim = prim;
        prim->x0 = prim->x1 = self->posX.i.hi;
        prim->y0 = prim->y1 = self->posY.i.hi;
        // Saturn gouraud 0x9310 / 0x9084 over base 0xC210
        prim->r0 = 128;
        prim->g0 = 192;
        prim->b0 = 32;
        prim->r1 = 32;
        prim->g1 = 32;
        prim->b1 = 32;
        prim->priority = MOD_BOSS_BEAM_Z; // TODO Z (Saturn 0x80)
        prim->drawMode = DRAW_TPAGE2 | DRAW_TPAGE | DRAW_UNK02 | DRAW_TRANSP;
        PlaySfxPositional(MOD_BOSS_SFX_WAVE);
    }
    if (self->step_s) {
        DestroyEntity(self);
        return;
    }
    prim = BOSS_EXT(self)->prim;
    prim->y0 -= 16; // the head shoots up 16 px a frame
    if (prim->y0 & 0xFF00) {
        self->step_s++;
    }
}

// ---------------- Saturn 0x2D: the ground shockwave ----------------
void EntityModBossWave(Entity* self) {
    u16 init[5];
    Primitive* prim;
    s32 primIndex, w, aw;

    if (BOSS_EXT(self)->parent->flags & FLAG_DEAD) {
        DestroyEntity(self);
        return;
    }
    if (self->step == 0) {
        init[0] = 0;
        init[1] = 0;
        init[2] = 0;
        init[3] = 0;
        init[4] = 0x36; // attack 43, cut
        InitializeEntity(init);
        BOSS_EXT(self)->timer = 96;
        self->posX.i.hi += self->facingLeft ? SAT_X(10) : SAT_X(-10);
        self->posY.i.hi -= 24;
        primIndex = g_api.AllocPrimitives(PRIM_G4, 1); // Saturn 0x8003
        if (primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        self->primIndex = primIndex;
        self->flags |= FLAG_HAS_PRIMS;
        prim = &g_PrimBuf[primIndex];
        BOSS_EXT(self)->prim = prim;
        prim->x0 = prim->x1 = prim->x2 = prim->x3 = self->posX.i.hi;
        prim->y0 = prim->y1 = prim->y2 = prim->y3 = self->posY.i.hi;
        if (Random() & 1) { // top: Saturn 0xFA90 (blue) or 0xA2D2 (yellow)
            prim->r0 = prim->r1 = 128;
            prim->g0 = prim->g1 = 160;
            prim->b0 = prim->b1 = 240;
        } else {
            prim->r0 = prim->r1 = 144;
            prim->g0 = prim->g1 = 176;
            prim->b0 = prim->b1 = 64;
        }
        prim->r2 = prim->r3 = 128; // bottom: Saturn 0xA110 (dark red)
        prim->g2 = prim->g3 = 64;
        prim->b2 = prim->b3 = 64;
        prim->priority = BOSS_EXT(self)->parent->zPriority + 4; // TODO Z
        prim->drawMode = DRAW_UNK02;
        self->velocityY = 0;
        self->velocityX = self->facingLeft ? SAT_X(0x5000) : SAT_X(-0x5000);
        BOSS_EXT(self)->width = 0;
        self->hitboxWidth = 0;
        self->hitboxHeight = 16;
        PlaySfxPositional(MOD_BOSS_SFX_WAVE);
    } else {
        MoveEntity();
        if (!self->facingLeft) {
            self->velocityX -= SAT_X(0x800);
            BOSS_EXT(self)->width -= SAT_X(6 << 8);
            if (BOSS_EXT(self)->timer <= 56) {
                self->velocityX -= SAT_X(0x8000);
            }
        } else {
            self->velocityX += SAT_X(0x800);
            BOSS_EXT(self)->width += SAT_X(6 << 8);
            if (BOSS_EXT(self)->timer <= 56) {
                self->velocityX += SAT_X(0x8000);
            }
        }
        prim = BOSS_EXT(self)->prim;
        w = BOSS_EXT(self)->width >> 8;
        // v0/v1 top, v2/v3 bottom (Saturn A,B top; D,C bottom); 30 px high
        prim->x0 = prim->x2 = self->posX.i.hi;
        prim->x1 = prim->x3 = self->posX.i.hi + w;
        prim->y0 = prim->y1 = self->posY.i.hi - 15;
        prim->y2 = prim->y3 = self->posY.i.hi + 15;
        aw = w < 0 ? -w : w;
        self->hitboxWidth = aw >> 2;
        self->hitboxOffX = (aw >> 2) - aw + SAT_X(4);
        // flickers: drawn every other frame (Saturn drawMode 2 / 10)
        prim->drawMode = (g_Timer & 1) ? DRAW_UNK02 : (DRAW_UNK02 | DRAW_HIDE);
    }
    if (--BOSS_EXT(self)->timer == 0) {
        DestroyEntity(self);
    }
}
