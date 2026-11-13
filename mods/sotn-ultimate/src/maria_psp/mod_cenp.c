// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port mod: the PSP (Dracula X Chronicles) Maria boss fight on the
// PS1, in BIN/CENP.BIN: CEN with this fight in the Saturn fight's place,
// which src/dra/cd.c loads instead of CEN.BIN when the select screen's
// Alucard panel has "Maria boss: PSP" (MOD_MARIA_BOSS_PSP()).
//
// The PSP boss (src/st/cen_psp/e_maria*.c) is a copy of her player engine
// that its fight logic drives with simulated pad input; it is compiled with
// mipsel gcc as MARIA2.BIN's src/maria is
// (mods/sotn-ultimate/tools/build_cenp.sh) and linked after CEN's own code.
// This file is its PS1 glue (the PSP-only DRA calls, the palettes, sounds, the
// pause menu reload) and the hand-off with the scene: CEN's platform and
// scripts keep the Saturn fight's flow (cen.h MOD_MARIA_BOSS): the first script
// ends -> the platform sets g_ModMariaFight -> the PSP fight starts (its
// g_CutsceneFlags 0x10); her death ends (0x20) -> the second script, the
// platform walks Alucard to the middle, the Holy Glasses as retail. compiled in
// build_cenp.sh mirror tree, where ../cen forwards to CEN own files
#include "../cen/cen.h"
#include <cutscene.h>
#include "../cen/mod_cen_sfx.h"
#include "mod_cenp_sfx.h"

// the Saturn fight's two scripts (the PS1 scene split around the fight;
// cut from mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h by
// build_cenp.sh)
#define MOD_RO __attribute__((section(".rodata")))
#include "mb_scripts.h"

s32 g_ModMariaFight; // the platform: the first script has ended

// her stats: the mod's Maria record (the Saturn fight's, HP 400 as the
// PSP's), her contact attack the PSP's 25; her PSP defence and elements in
// CenpDealDamage
EInit g_EInitUnk = {ANIMSET_DRA(0), 0x00, 0x00, PAL_NONE, 0x198};
#define CENP_ATTACK 25

extern u32 g_CutsceneFlags;
extern unsigned char* maria_sprites[];
void EntityMariaPspMain(Entity* self);

// the PSP's "graphics were lost" flag (DRA BSS there): on the PS1, set for
// one frame after the pause menu or the map (the frame counter ran while
// the game timer stood still); e_maria.c then reloads her art
s32 D_psp_091CE570;

void func_psp_091040A0(u_long** _gfxBank) {
    GfxBank* gfxBank;
    GfxLoad* gfxLoad;
    s32 i;

    gfxBank = (GfxBank*)_gfxBank;
    if (gfxBank->kind == GFX_BANK_NONE || gfxBank->kind == -1) {
        return;
    }
    for (i = 0; i < LEN(g_GfxLoad); i++) {
        gfxLoad = &g_GfxLoad[i];
        if (gfxLoad->kind == GFX_BANK_NONE) {
            gfxLoad->kind = gfxBank->kind;
            gfxLoad->unk6 = 0;
            gfxLoad->unk8 = 0;
            gfxLoad->next = gfxBank->entries;
            return;
        }
    }
}

// a stage palette (cen.h MarPalette: 0x240-0x24A, the Saturn fight's in
// CEN.BIN), into g_Clut and the VRAM CLUT
void func_psp_0892667C(s32 paletteID, u16* paletteData) {
    RECT rect;
    s32 id = paletteID & 0x3FF;
    s32 i;

    for (i = 0; i < COLORS_PER_PAL; i++) {
        ((u16*)g_Clut)[id * COLORS_PER_PAL + i] = paletteData[i];
    }
    rect.x = (g_ClutIds[id] & 0x3F) << 4;
    rect.y = g_ClutIds[id] >> 6;
    rect.w = COLORS_PER_PAL;
    rect.h = 1;
    LoadImage(&rect, (u_long*)paletteData);
}

void func_psp_089285A0(s32 angle, MATRIX* out) {
    short c = (short)rcos(angle);
    short s = (short)rsin(angle);
    out->m[0][0] = c;
    out->m[0][1] = (s16)-s;
    out->m[0][2] = 0;
    out->m[1][0] = s;
    out->m[1][1] = c;
    out->m[1][2] = 0;
    out->m[2][0] = 0;
    out->m[2][1] = 0;
    out->m[2][2] = 4096;
    out->t[2] = 0;
    out->t[1] = 0;
    out->t[0] = 0;
}

// her PSP voice (0x8E6-0x8F5, + MAR_SFX_OFFSET in this engine): clip i in
// the slot of stage id g_CenpVoicePsp[i] of the CEN bank cd.c loads for her,
// the same layout in every dub (mods/sotn-ultimate/tools/gen_cenp_sounds.py,
// CENP.BIN's tail): her PSP English clips (PSP dub), the Japanese ones (JP
// dub), the Saturn boss's sounds (PS1 dub). 0x8EC (Guardian Knuckle) has none:
// she has no such attack
static const u16 g_CenpVoicePsp[16] = CENP_PSP_VOICE;

s32 CenpSfx(s32 sfxId) {
    if (sfxId >= SFX_VO_MAR_8E6 + MAR_SFX_OFFSET &&
        sfxId <= SFX_VO_MAR_DEATH + MAR_SFX_OFFSET) {
        sfxId -= SFX_VO_MAR_8E6 + MAR_SFX_OFFSET;
        return g_CenpVoicePsp[sfxId];
    }
    if (sfxId == SFX_MARIA_ANIMALS_APPEAR) {
        // in her own bank on the PSP (not loaded in an Alucard game)
        return SFX_WEAPON_APPEAR;
    }
    return sfxId;
}

#undef PlaySfx
void PlaySfx(s32 sfxId) { g_api.PlaySfx(CenpSfx(sfxId)); }

// DRA functions the PSP engine calls directly: through g_api (DRA is
// relinked by make disk) or, as MARIA2.BIN's glue, a stub
s16 AllocPrimitives(PrimitiveType kind, s32 count) {
    return g_api.AllocPrimitives(kind, count);
}
void func_800FE8F0(void) {}

// the PSP's own damage (wall slams) on her, not on the player's g_Status
static bool CenpDamage(DamageParam* damage) {
    if (MARIA.hitPoints <= damage->damageTaken) {
        MARIA.hitPoints = 0;
        return true;
    }
    MARIA.hitPoints -= damage->damageTaken;
    return false;
}

// CEN's hit detection (collision.h, MOD_CENP) deals damage to her here: DRA's
// DealDamage reads g_EnemyDefs[enemyId], the mod's Maria record 0x198 that
// the Saturn fight (CEN.BIN) and the Library share, so her PSP defence and
// elements (include/game.h MOD_PSP_MARIA_*) are swapped in for this one call
u16 CenpDealDamage(Entity* enemy, Entity* attacker) {
    EnemyDef* def;
    s16 defense;
    u16 strengths;
    u16 immunes;
    u16 result;

    if (enemy->enemyId != MOD_PSP_MARIA_ENEMY) {
        return g_api.DealDamage(enemy, attacker);
    }
    def = &g_api.enemyDefs[MOD_PSP_MARIA_ENEMY];
    defense = def->defense;
    strengths = def->strengths;
    immunes = def->immunes;
    def->defense = MOD_PSP_MARIA_DEFENSE;
    def->strengths = MOD_PSP_MARIA_STRONG;
    def->immunes = MOD_PSP_MARIA_IMMUNE;
    result = g_api.DealDamage(enemy, attacker);
    def->defense = defense;
    def->strengths = strengths;
    def->immunes = immunes;
    return result;
}

static s32 g_CenpTimer;
static s32 g_CenpGameTimer;
static u8 g_CenpState; // 0 the first script, 1 the fight, 2 defeated
static u8 g_CenpAttackSet;

// the overlay file holds no BSS: the PS1 EntityMaria clears it when it
// starts (symbols from build_cenp.sh's link script)
extern u8 cenp_bss_start[];
extern u8 cenp_bss_end[];
void CenpInit(void) {
    u8* p;

    for (p = cenp_bss_start; p < cenp_bss_end; p++) {
        *p = 0;
    }
}

// the PS1 EntityMaria's update in BIN/CENP.BIN (self: the layout's entity;
// she is MARIA, g_Entities[STAGE_ENTITY_START], as on the PSP)
void EntityMariaPsp(Entity* self) {
    Entity* e;

    D_psp_091CE570 = g_CenpTimer && (g_Timer - g_CenpTimer) >
                                        (g_GameTimer - g_CenpGameTimer) + 2;
    g_CenpTimer = g_Timer;
    g_CenpGameTimer = g_GameTimer;

    // ANIMSET_PL_MARIA's images (DRA: o.unk30 for overlay sheets)
    g_api.o.unk30 = (u8**)maria_sprites;
    // PSP-only API entries (unused slots on the PS1; MARIA2.BIN sets its own)
    g_api.CalcDealDamageMaria = g_api.func_800FD664;
    g_api.CalcPlayerDamageMaria = CenpDamage;

    if (g_ModMariaFight) {
        // the first script is over: the PSP fight starts
        g_ModMariaFight = 0;
        g_CutsceneFlags |= 0x10;
        g_CenpState = 1;
        g_PlayerDraw[MAR_UNK5A_BODY].rect0.y = 0x101;
        // the PSP's prefight script ends with SCRIPT_UNKNOWN_20(0x333): its
        // (JP-numbered) MU_BLOOD_RELATIONS, US 0x336
        g_api.PlaySfx(MU_BLOOD_RELATIONS);
        g_api.TimeAttackController(
            TIMEATTACK_EVENT_GET_HOLYGLASSES, TIMEATTACK_SET_VISITED);
    }

    // her body's image slot (cen.h MAR_UNK5A_BODY, x 1-16): page 16's
    // VRAM, which the dialogue's portraits (x 0-55, y 256-327) and its five
    // text line buffers (x 0-63, y 384 + 12 * line, 384-443: cutscene.c,
    // CS_LINE_MAX 4) use -> below them outside the fight: y 444-511 (her
    // frames are 67 rows at most, 64 but one; DRA's 1-row clear above the
    // frame lands on line 4's blank last row). At 0x101 in the fight (no
    // dialogue; the afterimages use y 0x181). y 432 was line 4's buffer:
    // her frames showed in the text box. Set on the state changes only:
    // after the second script e_chamber.c puts it back at 0x101, where the
    // other users of slot 8 expect it (BO6's boss Richter hit flash reads
    // y 0x101 directly; left at 444 it drew stale VRAM)
    if (g_CenpState == 0) {
        g_PlayerDraw[MAR_UNK5A_BODY].rect0.y = 444;
    }

    EntityMariaPspMain(self);

    // she is not the layout's entity (that is self): the stage's Update()
    // never runs its per-entity bookkeeping on her, which clears the hit
    // HitDetection reports after the update; left set, her engine re-entered
    // PL_S_HIT every time her invincibility ended (knocked back to the wall,
    // vulnerable one frame per cycle, Alucard's hits never landing)
    MARIA.hitFlags = 0;
    MARIA.hitParams = 0;
    MARIA.flags &= ~0xF; // the hit-flash count Update() would run down

    if (!g_CenpAttackSet && MARIA.animSet == ANIMSET_PL_MARIA) {
        g_CenpAttackSet = 1; // once, after MarInit's InitializeEntity
        MARIA.attack = CENP_ATTACK;
    }
    if (g_CenpState == 1 && (g_CutsceneFlags & 0x20)) {
        // her death is over: the second script, Alucard to the middle
        g_CenpState = 2;
        g_PlayerDraw[MAR_UNK5A_BODY].rect0.y = 444;
        CreateEntityFromEntity(E_CUTSCENE, self, &g_Entities[200]);
        g_Entities[200].params = 1;
        for (e = &g_Entities[64]; e < &g_Entities[256]; e++) {
            if (e->entityId == E_PLATFORM) {
                e->step = 12;
            }
        }
    }
}
