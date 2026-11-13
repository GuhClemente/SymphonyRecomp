// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port: the Saturn Maria's command specials (MARIA.PRG), included by
// pl_blueprints.c after mar_attacks.h (MARIA.BIN only).
//
// Inputs (060B9FE0..060BA914, run each frame by 060BABE0 in this order:
// Byakko, Genbu, Seiryuu, Guardian Knuckle, Suzaku, Four Beasts), then
// Square or the kick button (MAR_PAD_KICK); 20 frames between steps:
//   Guardian Knuckle  D, DF, F                       40 MP  060BADA8
//   Genbu             B, UB, U, UF, F                80 MP  060BAFF0
//   Byakko            U, D, DF, F                   100 MP  060BB4AC
//   Seiryuu           B, F, DF, D, DB, B, F         120 MP  060BBEF8
//   Suzaku            hold U 1 s, UF, F, DF, D       60 MP  060BC320
//   Four Beasts       U, UF, F, DF, D, DB, B, UB,
//                     hold U 41 frames, let go, D   150 MP  060BCFF4
// 060A9F84 starts one: her animation, step 0x30 + n, afterimage colours
// 5 + n and the move's blueprint. 060A9064 runs the steps.
// Horizontal positions, speeds and hitbox x / width are the Saturn's at 4/5.
// Art: mods/sotn-ultimate/tools/gen_mar_assets.py gen_sp (mar_sp.h).
#include "mar_sp.h"

void RicSetInvincibilityFrames(s32 kind, s16 invincibilityFrames);
void func_8015CC28(void);
void DisableAfterImage(s32 resetAnims, s32 arg1);
extern AnimationFrame mar_anim_sp_knuckle[];
extern AnimationFrame mar_anim_sp_genbu[];
extern AnimationFrame mar_anim_sp_genbu2[];
extern AnimationFrame mar_anim_sp_byakko[];
extern AnimationFrame mar_anim_sp_byakko2[];
extern AnimationFrame mar_anim_sp_byakko3[];
extern AnimationFrame mar_anim_sp_byakko4[];
extern AnimationFrame mar_anim_sp_seiryu[];
extern AnimationFrame mar_anim_sp_seiryu2[];
extern AnimationFrame mar_anim_sp_seiryu3[];
extern AnimationFrame mar_anim_sp_seiryu4[];
extern AnimationFrame mar_anim_sp_summon[];
extern AnimationFrame mar_anim_sp_summon2[];

#define MAR_PERM_BANK 0x16 // owl + stars (a stage has 24 sprite banks)
#define MAR_SLOT_BANK 0x17 // the move being cast
#define MAR_PAL_OWL 0x141  // + 1: the stars

// Saturn entity extension offsets (0x78 on) -> PS1 (0x7C on)
#define SW(e, o) (*(s16*)((u8*)(e) + (o) + 4))
#define SL(e, o) (*(s32*)((u8*)(e) + (o) + 4))
#define SE(e, o) (*(Entity**)((u8*)(e) + (o) + 4))

// ---------------------------------------------------------------- art

static s8 mar_sp_slot = -1;
static void MarHudInit(void);
static void MarVramSnap(void);
static s16** mar_sp_banks[] = {
    mar_fxbank_knuckle, mar_fxbank_genbu, mar_fxbank_byakko, mar_fxbank_seiryu,
    mar_fxbank_flame};
// set while the shot/charge atlas must not be checked or restored
// (MarVramCheck; Byakko's flames shared its rows before 2026-10-23)
static u8 mar_fx_dirty;

static void MarFxRestore(void) {
    RECT rect;
    s32 i;

    if (!mar_fx_dirty) {
        return;
    }
    mar_fx_dirty = 0;
    DrawSync(0);
    MarUnpack((u8*)mar_buf, mar_fx_lz, MAR_FX_RAW);
    rect.x = MAR_FX_X;
    rect.y = MAR_FX_Y;
    rect.w = MAR_FX_W;
    rect.h = MAR_FX_H;
    LoadImage(&rect, (u_long*)mar_buf);
    DrawSync(0);
    for (i = 0; i < 4; i++) {
        MarBufRelease(i);
    }
}

static void MarSetPal(s32 id, u16* pal) {
    RECT rect;
    s32 j;

    for (j = 0; j < COLORS_PER_PAL; j++) {
        g_Clut[1][(id - 0x100) * COLORS_PER_PAL + j] = pal[j];
    }
    rect.x = (g_ClutIds[id] & 0x3F) << 4;
    rect.y = g_ClutIds[id] >> 6;
    rect.w = COLORS_PER_PAL;
    rect.h = 1;
    LoadImage(&rect, (u_long*)pal);
}

// unpack block k (0: permanent, 1-4: the slot) through mar_buf; the player
// images unpack again at the end of the frame
static void MarSpBlock(s32 k) {
    RECT rect;
    MarSpChunk* c;
    s32 i;

    DrawSync(0);
    for (i = mar_sp_first[k]; i < mar_sp_first[k + 1]; i++) {
        c = &mar_sp_chunks[i];
        MarUnpack((u8*)mar_buf, c->lz, c->raw);
        rect.x = c->x;
        rect.y = c->y;
        rect.w = MAR_FX_W;
        rect.h = c->h;
        LoadImage(&rect, (u_long*)mar_buf);
        DrawSync(0);
    }
    for (i = 0; i < 4; i++) {
        MarBufRelease(i);
    }
}

// the slot: Guardian Knuckle 0, Genbu 1, Byakko 2, Seiryuu 3
static void MarSpLoad(s32 g) {
    if (mar_sp_slot == g) {
        return;
    }
    mar_sp_slot = g;
    MarSpBlock(g + 1);
    MarSetPal(MAR_SP_PAL_SLOT, mar_sp_group_pal[g][0]);
    MarSetPal(MAR_SP_PAL_SLOT + 1, mar_sp_group_pal[g][1]);
    g_api.o.spriteBanks[MAR_SLOT_BANK] = (SpriteParts*)mar_sp_banks[g];
}

static void MarSpInit(void) {
    s32 i;

    MarSpBlock(0);
    for (i = 0; i < LEN(mar_sp_perm_pal_ids); i++) {
        MarSetPal(mar_sp_perm_pal_ids[i], mar_sp_perm_pal[i]);
    }
    g_api.o.spriteBanks[MAR_PERM_BANK] = (SpriteParts*)mar_fxbank_perm;
    mar_sp_slot = -1;
    mar_fx_dirty = 0;
    MarHudInit();
    MarVramSnap();
}

// Her art's VRAM blocks (empty ones of F_GAME2's area: gen_mar_assets.py)
// and palettes stay hers only while nothing else uploads there, but DRA
// streams F_GAME2 (32x128 blocks, its empty ones included, and the palette
// rows) over them when a save loads after her overlay did (2026-10-23: her
// shots drew as garbage). Every 32 frames a row of the shot atlas and one of
// the permanent block are read back; if either changed, all her art and
// palettes go back
static s16 mar_vram_xy[2][2] = {
    {MAR_FX_X, MAR_FX_Y + 20}, {MAR_SP_PERM_X, MAR_SP_PERM_Y + 40}};
// (words: StoreImage moves words, to a word-aligned buffer)
static u32 mar_vram_row[2][MAR_FX_W / 2];

static void MarVramRead(s32 k, u32* row) {
    RECT rect;

    rect.x = mar_vram_xy[k][0];
    rect.y = mar_vram_xy[k][1];
    rect.w = MAR_FX_W;
    rect.h = 1;
    DrawSync(0);
    StoreImage(&rect, (u_long*)row);
    DrawSync(0);
}

static void MarVramSnap(void) {
    MarVramRead(0, mar_vram_row[0]);
    MarVramRead(1, mar_vram_row[1]);
}

static void MarVramCheck(void) {
    u32 row[MAR_FX_W / 2];
    s32 i;
    s32 k;
    s32 g;

    if (mar_fx_dirty || (g_GameTimer & 31)) {
        return;
    }
    for (k = 0; k < 2; k++) {
        MarVramRead(k, row);
        for (i = 0; i < MAR_FX_W / 2; i++) {
            if (row[i] != mar_vram_row[k][i]) {
                break;
            }
        }
        if (i < MAR_FX_W / 2) {
            break;
        }
    }
    if (k == 2) {
        return;
    }
    g = mar_sp_slot;
    mar_fx_dirty = 1;
    MarFxRestore(); // the shot atlas
    MarFxPals();
    MarSpBlock(0); // the owl and stars
    for (i = 0; i < LEN(mar_sp_perm_pal_ids); i++) {
        MarSetPal(mar_sp_perm_pal_ids[i], mar_sp_perm_pal[i]);
    }
    mar_sp_slot = -1;
    if (g >= 0) {
        MarSpLoad(g);
    }
    MarVramSnap();
}

// ---------------------------------------------------------------- input

#define MU 1
#define MD 2
#define MF 4
#define MB 8
#define C_TAP 0x00   // tapped & dir
#define C_EQ 0x10    // pressed == dir
#define C_AND 0x20   // (pressed & dir) == dir
#define C_TAPEQ 0x30 // tapped == dir
#define C_HOLD 0x40  // Suzaku: Up held > 60 frames
#define C_UPEQ 0x50  // Suzaku: Up still held, then pressed == dir
#define C_HOLD2 0x60 // Four Beasts: Up held, timer 20 -> 60
#define C_REL 0x70   // Four Beasts: Up let go

static u8 mar_seq_knuckle[] = {C_TAP | MD, C_EQ | MD | MF, C_EQ | MF, 0};
static u8 mar_seq_genbu[] = {
    C_TAP | MB, C_AND | MU | MB, C_AND | MU, C_AND | MU | MF, C_EQ | MF, 0};
static u8 mar_seq_byakko[] = {
    C_TAP | MU, C_AND | MD, C_AND | MD | MF, C_EQ | MF, 0};
static u8 mar_seq_seiryu[] = {
    C_TAP | MB,      C_EQ | MF, C_AND | MD | MF, C_EQ | MD,
    C_AND | MD | MB, C_EQ | MB, C_EQ | MF,       0};
static u8 mar_seq_suzaku[] = {
    C_HOLD, C_UPEQ | MU | MF, C_EQ | MF, C_EQ | MD | MF, C_EQ | MD, 0};
static u8 mar_seq_beasts[] = {
    C_TAPEQ | MU, C_EQ | MU | MF, C_EQ | MF, C_EQ | MD | MF,
    C_EQ | MD,    C_EQ | MD | MB, C_EQ | MB, C_EQ | MU | MB,
    C_HOLD2,      C_REL,          C_EQ | MD, 0};
// 060BABE0's order; the move each one starts
static u8* mar_seqs[] = {mar_seq_byakko,  mar_seq_genbu,  mar_seq_seiryu,
                         mar_seq_knuckle, mar_seq_suzaku, mar_seq_beasts};
static u8 mar_seq_move[] = {2, 1, 3, 0, 4, 5};
static u8 mar_sp_cost[] = {40, 80, 100, 120, 60, 150}; // 060C7B30
static struct {
    s16 state;
    s16 timer;
    s16 facing;
} mar_combo[6];

static AnimationFrame* mar_sp_anims[] = {
    mar_anim_sp_knuckle, mar_anim_sp_genbu,  mar_anim_sp_byakko,
    mar_anim_sp_seiryu,  mar_anim_sp_summon, mar_anim_sp_summon};
static u8 mar_sp_bps[] = {
    BP_MAR_KNUCKLE, BP_MAR_GENBU,  BP_MAR_BYAKKO,
    BP_MAR_SEIRYU,  BP_MAR_SUZAKU, BP_MARIA_POWERS_APPLIED};

static u32 MarDir(s32 m, s32 facing) {
    u32 r = 0;

    if (m & MU) {
        r |= PAD_UP;
    }
    if (m & MD) {
        r |= PAD_DOWN;
    }
    if (m & MF) {
        r |= facing ? PAD_LEFT : PAD_RIGHT;
    }
    if (m & MB) {
        r |= facing ? PAD_RIGHT : PAD_LEFT;
    }
    return r;
}

static s32 MarCountEnt(s32 id) {
    Entity* e;
    s32 n = 0;

    for (e = &g_Entities[16]; e < &g_Entities[31]; e++) {
        if (e->entityId == id) {
            n++;
        }
    }
    return n;
}

// Saturn: sound commands 0xF0000001 / 0xF0000002 (zero 06010400 sets the
// state 06064388, 06016C60 applies it every frame): the music volume goes
// down by 2% a frame to 40% while she casts specials 1-5 (Genbu, Byakko,
// Seiryuu, Suzaku, Four Beasts; not the Knuckle) and back up by 2% a frame
// when the cast step ends. Here on the XA volume DRA applies every frame
// (g_CdVolume, set from the track's g_XaMusicVolume)
extern s16 g_CdVolume;
extern s16 g_XaMusicVolume;
static u8 mar_duck; // 1 down / low, 2 back up
static s8 mar_duck_pct = 100;
static void MarDuck(void) {
    if (mar_duck == 1) {
        if (PLAYER.step <= PL_S_MAR_SPECIAL ||
            PLAYER.step > PL_S_MAR_SPECIAL + 5) {
            mar_duck = 2;
        } else if (mar_duck_pct > 40) {
            mar_duck_pct -= 2;
        }
    } else if (mar_duck == 2) {
        mar_duck_pct += 2;
        if (mar_duck_pct >= 100) {
            mar_duck_pct = 100;
            mar_duck = 0;
        }
    } else {
        return;
    }
    g_CdVolume = g_XaMusicVolume * mar_duck_pct / 100;
}

// Saturn 060A9F84
static void MarSetSpecial(s32 n) {
    RicSetAnimation(mar_sp_anims[n]);
    RicSetStep(PL_S_MAR_SPECIAL + n);
    func_8015FA5C(n + 5);
    g_CurrentEntity->velocityY = 0;
    g_CurrentEntity->velocityX = 0;
    // Saturn: sound command 0xF0000001 for steps 0x31-0x35 (MarDuck),
    // then her voice calling the move (0x5CC, 0x5D3, 0x5D2, 0x5D1, 0x5D0:
    // MAR_SFX_SPELL + n); none for the Four Beasts (the beasts call it)
    if (n) {
        mar_duck = 1;
    }
    if (n < 5) {
        g_api.PlaySfx(MAR_SFX_SPELL + n);
    }
    if (n == 3) {
        DisableAfterImage(1, 1);
    }
    if (n == 5) {
        RicCreateEntFactoryFromEntity(g_CurrentEntity, BP_MAR_BEASTS, 0);
    }
    RicCreateEntFactoryFromEntity(g_CurrentEntity, mar_sp_bps[n], 0);
}

static s32 MarCheckCombo(s32 i) {
    u8* seq = mar_seqs[i];
    u32 pressed =
        g_Player.padPressed & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT);
    u32 dir;
    s32 op;
    s32 hit;
    s32 move;

    if (mar_combo[i].state == 0) {
        mar_combo[i].facing = PLAYER.facingLeft;
    }
    op = seq[mar_combo[i].state];
    if (op == 0) {
        if (--mar_combo[i].timer <= 0) {
            mar_combo[i].state = 0;
            return 0;
        }
        move = mar_seq_move[i];
        if (PLAYER.step > PL_S_CROUCH &&
            !((move == 3 || move == 5) && PLAYER.step == PL_S_RUN)) {
            return 0;
        }
        if (g_Player.unk72) {
            mar_combo[i].state = 0;
            return 0;
        }
        if (g_Player.unk46) {
            return 0;
        }
        if (move == 5 && g_Player.timers[PL_T_INVINCIBLE_SCENE]) {
            return 0;
        }
        if (!(g_Player.padTapped & (PAD_SQUARE | MAR_PAD_KICK))) {
            return 0;
        }
        if (mar_sp_cost[move] > g_Status.mp) {
            return 0;
        }
        g_Status.mp -= mar_sp_cost[move];
        if (move != 4 || MarCountEnt(E_MAR_SUZAKU) < 2) {
            MarSetSpecial(move);
        }
        return 1;
    }
    dir = MarDir(op & 0xF, mar_combo[i].facing);
    switch (op & 0xF0) {
    case C_TAP:
        hit = g_Player.padTapped & dir;
        break;
    case C_TAPEQ:
        hit = g_Player.padTapped == dir;
        break;
    case C_EQ:
        hit = pressed == dir;
        break;
    case C_AND:
        hit = (pressed & dir) == dir;
        break;
    case C_HOLD:
        if (pressed & PAD_UP) {
            mar_combo[i].timer++;
        } else {
            mar_combo[i].timer = 0;
        }
        if (mar_combo[i].timer > 60) {
            mar_combo[i].state++;
        }
        return 0;
    case C_UPEQ:
        if (!(pressed & PAD_UP)) {
            mar_combo[i].timer = 0;
            mar_combo[i].state = 0;
            return 0;
        }
        if (pressed != dir) {
            return 0;
        }
        hit = 1;
        break;
    case C_HOLD2:
        if (!(pressed & PAD_UP)) {
            mar_combo[i].state = 0;
            return 0;
        }
        if (++mar_combo[i].timer > 60) {
            mar_combo[i].state++;
            mar_combo[i].timer = 20;
        }
        return 0;
    default: // C_REL
        if (pressed & PAD_UP) {
            return 0;
        }
        mar_combo[i].state++;
        mar_combo[i].timer = 20;
        return 0;
    }
    if (hit) {
        mar_combo[i].timer = 20;
        mar_combo[i].state++;
    } else if (mar_combo[i].state && --mar_combo[i].timer <= 0) {
        mar_combo[i].state = 0;
    }
    return 0;
}

// Saturn 060BABE0, from her main loop after the high jump check
void MarCheckSpecials(void) {
    s32 i;

    for (i = 0; i < 6; i++) {
        if (MarCheckCombo(i)) {
            break;
        }
    }
}

// ---------------------------------------------------------------- steps

// the Saturn animations jump into the next one (POSE_JUMP); here they end
// and the next is set right after UpdateAnim, the same frame
static AnimationFrame* mar_sp_chain[][2] = {
    {mar_anim_sp_genbu, mar_anim_sp_genbu2},
    {mar_anim_sp_byakko, mar_anim_sp_byakko2},
    {mar_anim_sp_byakko2, mar_anim_sp_byakko3},
    {mar_anim_sp_byakko3, mar_anim_sp_byakko4},
    {mar_anim_sp_seiryu, mar_anim_sp_seiryu2},
    {mar_anim_sp_seiryu2, mar_anim_sp_seiryu3},
    {mar_anim_sp_seiryu3, mar_anim_sp_seiryu4},
    {mar_anim_sp_summon, mar_anim_sp_summon2},
};

static void MarHudUpdate(void);

// (every frame, after her UpdateAnim; also the MP bar)
void MarChainAnim(void) {
    s32 i;

    MarHudUpdate();
    if (PLAYER.poseTimer >= 0 || PLAYER.step < PL_S_MAR_SPECIAL ||
        PLAYER.step > PL_S_MAR_SPECIAL + 5) {
        return;
    }
    for (i = 0; i < LEN(mar_sp_chain); i++) {
        if (PLAYER.anim == mar_sp_chain[i][0]) {
            RicSetAnimation(mar_sp_chain[i][1]);
            g_api.UpdateAnim(D_80155964, (AnimationFrame**)D_8015538C);
            return;
        }
    }
}

// Saturn 060A9064: steps 0x30-0x35
void MarStepSpecial(void) {
    if (PLAYER.poseTimer < 0) {
        if (PLAYER.step == PL_S_MAR_SPECIAL + 5) {
            g_api.PlaySfx(MAR_SFX_BEASTS_END); // Saturn 0x727
        }
        // Saturn: sound command 0xF0000002 for steps 0x31-0x35 (MarDuck:
        // the step's end)
        MarFxRestore();
        RicSetStand(0);
        return;
    }
    RicSetInvincibilityFrames(1, 50);
    func_8015CC28();
    if (PLAYER.step == PL_S_MAR_SPECIAL) {
        RicDecelerateX(0x1666); // Saturn 0x1C00
        if (PLAYER.pose == 20 && PLAYER.poseTimer == 1) {
            PLAYER.velocityX = PLAYER.facingLeft ? FIX(-4) : FIX(4);
            g_api.PlaySfx(MAR_SFX_LUNGE); // Saturn 0x725
        }
    }
}

// ---------------------------------------------------------------- moves

// Saturn g_unkGraphicsStruct.unk28 = 4 while she casts Byakko / Seiryuu:
// the game halts (enemies, bosses, projectiles, scrolling) and only her
// code and her 0x10000 entities run (DRA's engine step 5, which runs her
// code for 4 as at the prologue, src/dra/5087C.c). Leaving it, DRA gives
// her 48 frames of invincibility and resumes the sound scripts, as the
// Saturn does. (Before v1.2: the stopwatch flag, which most bosses ignore)
static void MarTimeStop(s32 on) { g_unkGraphicsStruct.unk28 = on ? 4 : 0; }

static MarAttackDef mar_def_knuckle = {
    50, 26, 16, 26, -8, 0x1000, 2, 16, 4, 1, 0x30}; // 060C7B68
// her pose -> effect frame (060C7B3C; the Saturn's empty frame 16 -> 0)
static u8 mar_knuckle_frames[] = {
    0, 0, 0, 0, 0, 0,  0,  0,  0,  1,  2,  3, 4, 1, 2, 3, 4, 1, 2, 3, 4,
    5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// 060BAC78: 16 sparks converging on her hand, then bursting out
static void MarParticles(Primitive* prim, s32 cx, s32 cy, s32 burst) {
    s32 r;

    for (; prim != NULL; prim = prim->next) {
        if (prim->drawMode & DRAW_HIDE) {
            if (--prim->x2 > 0) {
                continue;
            }
            r = rand();
            prim->drawMode &= ~DRAW_HIDE;
            prim->y2 = r & 0xFFF;
            prim->x3 = burst ? 1 : ((r & 7) + 14) * 2;
            continue;
        }
        r = prim->x3 >> 1;
        prim->x0 = cx + ((rcos(prim->y2) * r) >> 12) * 4 / 5;
        prim->y0 = cy + ((rsin(prim->y2) * r) >> 12);
        if (burst) {
            prim->x3 += 3;
            if (prim->x3 <= 40) {
                continue;
            }
        } else if (--prim->x3 > 3) {
            continue;
        }
        prim->drawMode |= DRAW_HIDE;
        prim->x2 = rand() & 7;
    }
}

// Saturn: Genbu's, Byakko's, Seiryuu's and Suzaku's entities count their
// frames (Saturn entity offset 0x94) and play her voice 0x5CF at one
static void MarCue(Entity* self, s32 frame) {
    if (++SW(self, 0x94) == frame) {
        g_api.PlaySfx(MAR_SFX_CUE);
    }
}

// 73, Saturn 060BADA8: Guardian Knuckle, a holy fist of light at pose 20
void MarEntityKnuckle(Entity* self) {
    static u16 cols[] = {0xFE10, 0xFF18, 0xFD08, 0xFE10};
    Primitive* prim;
    s32 cx;
    s32 cy;
    s32 r;

    if (PLAYER.anim != mar_anim_sp_knuckle) {
        DestroyEntity(self);
        return;
    }
    if (PLAYER.pose <= 20) {
        cx = PLAYER.posX.i.hi + (PLAYER.facingLeft ? 6 : -6);
        cy = PLAYER.posY.i.hi - 12;
    } else {
        cx = PLAYER.posX.i.hi + (PLAYER.facingLeft ? -28 : 28);
        cy = PLAYER.posY.i.hi - 8;
    }
    switch (self->step) {
    case 0:
        self->primIndex = g_api.AllocPrimitives(PRIM_TILE, 16);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        MarSpLoad(0);
        for (prim = &g_PrimBuf[self->primIndex]; prim != NULL;
             prim = prim->next) {
            r = rand();
            prim->priority = PLAYER.zPriority + 2;
            prim->x2 = r & 7;
            prim->y2 = r & 0xFFF;
            prim->x3 = ((r & 7) + 14) * 2;
            prim->u0 = prim->v0 = 1;
            r = cols[r & 3]; // Saturn RGB555
            prim->r0 = (r & 31) << 3;
            prim->g0 = (r >> 2) & 0xF8;
            prim->b0 = (r >> 7) & 0xF8;
            prim->drawMode = DRAW_DEFAULT;
        }
        self->animSet = ANIMSET_OVL(MAR_SLOT_BANK);
        self->palette = MAR_SP_PAL_SLOT;
        self->zPriority = PLAYER.zPriority + 4;
        self->flags = FLAG_HAS_PRIMS | FLAG_UNK_20000;
        self->step++;
        break;
    case 1:
        if (PLAYER.pose > 20) {
            MarSetAttack(self, &mar_def_knuckle);
            self->step++;
        }
        break;
    }
    self->posX.i.hi = PLAYER.posX.i.hi;
    self->posY.i.hi = PLAYER.posY.i.hi;
    self->animCurFrame = mar_knuckle_frames[PLAYER.pose];
    MarParticles(&g_PrimBuf[self->primIndex], cx, cy, self->step > 1);
}

static u8 mar_genbu_frames[] = {0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 1, 2, 3, 4,
                                1, 2, 3, 4, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0};

// 74, Saturn 060BAFF0: Genbu, heals a quarter of her max HP; a column of
// green light and the turtle (beast 1)
void MarEntityGenbu(Entity* self) {
    Primitive* prim;
    s32 x;
    s32 e;
    s32 i;

    if (PLAYER.anim != mar_anim_sp_genbu && PLAYER.anim != mar_anim_sp_genbu2) {
        RicCreateEntFactoryFromEntity(self, FACTORY(BP_RIC_BLINK, 0x58), 0);
        DestroyEntity(self);
        return;
    }
    MarCue(self, 0x38); // Saturn: her voice 0x5CF
    switch (self->step) {
    case 0:
        self->primIndex = g_api.AllocPrimitives(PRIM_G4, 2);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        MarSpLoad(1);
        for (prim = &g_PrimBuf[self->primIndex]; prim != NULL;
             prim = prim->next) {
            // Saturn gouraud 0xB3EC outside, 0xEBFA at her centre
            prim->r0 = prim->b0 = prim->r2 = prim->b2 = 0x60;
            prim->r1 = prim->b1 = prim->r3 = prim->b3 = 0xD0;
            prim->g0 = prim->g1 = prim->g2 = prim->g3 = 0xF8;
            prim->y0 = prim->y1 = 0;
            prim->priority = PLAYER.zPriority - 10;
            prim->drawMode = DRAW_HIDE | DRAW_TRANSP;
        }
        self->animSet = ANIMSET_OVL(MAR_SLOT_BANK);
        self->palette = MAR_SP_PAL_SLOT;
        self->zPriority = PLAYER.zPriority + 4;
        self->flags = FLAG_HAS_PRIMS | FLAG_UNK_20000;
        RicCreateEntFactoryFromEntity(self, FACTORY(BP_MAR_BEAST, 2), 0);
        self->step++;
    case 1:
        if (PLAYER.anim == mar_anim_sp_genbu2) {
            g_Status.hp += g_Status.hpMax >> 2;
            if (g_Status.hp > g_Status.hpMax) {
                g_Status.hp = g_Status.hpMax;
            }
            for (prim = &g_PrimBuf[self->primIndex]; prim != NULL;
                 prim = prim->next) {
                prim->drawMode &= ~DRAW_HIDE;
            }
            g_api.PlaySfx(MAR_SFX_GENBU); // Saturn 0x72F
            self->step++;
        }
        return;
    case 2:
        if (++SW(self, 0x80) > 15) {
            self->step++;
        }
        break;
    default:
        if (PLAYER.pose == 26) {
            if (SW(self, 0x80)) {
                SW(self, 0x80)--;
            } else {
                for (prim = &g_PrimBuf[self->primIndex]; prim != NULL;
                     prim = prim->next) {
                    prim->drawMode |= DRAW_HIDE;
                }
            }
        }
        break;
    }
    self->posX.i.hi = PLAYER.posX.i.hi;
    self->posY.i.hi = PLAYER.posY.i.hi;
    self->animCurFrame = mar_genbu_frames[PLAYER.pose];
    x = PLAYER.posX.i.hi + (PLAYER.facingLeft ? -3 : 2);
    e = SW(self, 0x80) * 4 / 5;
    i = x - e;
    for (prim = &g_PrimBuf[self->primIndex]; prim != NULL; prim = prim->next) {
        prim->x0 = prim->x2 = i;
        prim->x1 = prim->x3 = x;
        prim->y2 = prim->y3 = PLAYER.posY.i.hi + 24;
        i += e * 2;
    }
}

static u8 mar_byakko_frames[] = {
    8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, 8, 7, 6, 5,
    4, 3, 2, 1, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0};

// 75, Saturn 060BB4AC: Byakko. The tiger (beast 0) runs in, the screen
// darkens (Saturn colour offset -30, here a subtractive tile), she gathers
// fire (pkg 10), then breathes flames (78) for her third animation; one in
// 32 casts is the silver flame (double damage). The game halts while she
// casts, until her third animation (g_unkGraphicsStruct.unk28 = 4, as the
// Saturn; MarTimeStop).
void MarEntityByakko(Entity* self) {
    Primitive* prim;
    s32 a;

    if (PLAYER.anim != mar_anim_sp_byakko &&
        PLAYER.anim != mar_anim_sp_byakko2 &&
        PLAYER.anim != mar_anim_sp_byakko3) {
        MarTimeStop(0);
        DestroyEntity(self);
        return;
    }
    MarCue(self, 0x68); // Saturn: her voice 0x5CF
    switch (self->step) {
    case 0:
        self->primIndex = g_api.AllocPrimitives(PRIM_TILE, 1);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        MarSpLoad(2);
        prim = &g_PrimBuf[self->primIndex];
        prim->x0 = prim->y0 = 0;
        prim->u0 = 255;
        prim->v0 = 240;
        prim->r0 = prim->g0 = prim->b0 = 0;
        prim->priority = 0x1F0;
        prim->drawMode = DRAW_UNK_40 | DRAW_TRANSP;
        self->animSet = ANIMSET_OVL(MAR_SLOT_BANK);
        self->palette = MAR_SP_PAL_SLOT;
        self->zPriority = PLAYER.zPriority + 4;
        self->flags = FLAG_HAS_PRIMS | FLAG_UNK_20000 | FLAG_UNK_10000;
        if (!(rand() & 0x1F)) {
            SW(self, 0x80) = 1;
        }
        MarTimeStop(1);
        self->step++;
    case 1:
        if (SL(self, 0x84) > -30) {
            SL(self, 0x84)--;
        }
        if (PLAYER.anim == mar_anim_sp_byakko2) {
            // Saturn 0x72A: four buzzes of the common electricity tone (at
            // 0, 0.26, 0.41, 0.61 s); the PS1 SFX_ELECTRICITY plays them
            g_api.PlaySfx(SFX_ELECTRICITY);
            SW(self, 0x88) = 0;
            self->step++;
        } else if (PLAYER.pose == 15 && PLAYER.poseTimer == 1) {
            RicCreateEntFactoryFromEntity(self, FACTORY(BP_MAR_BEAST, 1), 0);
        }
        goto darken;
    case 2:
        a = ++SW(self, 0x88);
        if (a == 16 || a == 25 || a == 36) {
            g_api.PlaySfx(SFX_ELECTRICITY);
        }
        if (PLAYER.anim == mar_anim_sp_byakko3) {
            MarSpLoad(4); // the flames
            MarTimeStop(0);
            g_api.PlaySfx(MAR_SFX_BYAKKO_FLAME); // Saturn 0x72B
            self->step++;
        }
        break;
    }
    self->posX.i.hi = PLAYER.posX.i.hi;
    self->posY.i.hi = PLAYER.posY.i.hi;
    self->facingLeft = PLAYER.facingLeft;
    if (PLAYER.anim == mar_anim_sp_byakko2) {
        self->animCurFrame = mar_byakko_frames[PLAYER.pose];
    } else {
        self->animCurFrame = 0;
        self->posX.i.hi += PLAYER.facingLeft ? -18 : 18;
        self->posY.i.hi += 3;
        if (PLAYER.anim == mar_anim_sp_byakko3) {
            RicCreateEntFactoryFromEntity(
                self, FACTORY(BP_MAR_FLAME, SW(self, 0x80)), 0);
        }
    }
darken:
    prim = &g_PrimBuf[self->primIndex];
    prim->r0 = prim->g0 = prim->b0 = -SL(self, 0x84);
}

static MarAttackDef mar_def_flame = {
    20, 13, 16, 0, 0, 0x9000, 2, 16, 4, 1, 8}; // 060C7BB8
static AnimationFrame mar_anim_flame[] = {
    POSE(2, 1, 0), POSE(2, 2, 0), POSE(2, 3, 0), POSE(2, 4, 0),
    POSE(2, 5, 0), POSE(2, 6, 0), POSE(2, 7, 0), POSE_END}; // 060C7B98

// 78, Saturn 060BB334: Byakko's flames, fanned out in front of her. The
// Saturn's flame art is in its shared sprite set; here her charge fireball
// is the Saturn shared sprite bank 11 (gen_mar_assets.py shared_bank)
void MarEntityFlame(Entity* self) {
    s32 a;

    if (self->step == 0) {
        self->animSet = ANIMSET_OVL(MAR_SLOT_BANK);
        // Saturn shared bank 7 palette 12, or 5 for the silver flame
        self->palette = self->params ? MAR_SP_PAL_SLOT + 1 : MAR_SP_PAL_SLOT;
        self->anim = mar_anim_flame;
        self->flags = FLAG_POS_CAMERA_LOCKED | FLAG_UNK_100000 | FLAG_UNK_10000;
        self->zPriority = PLAYER.zPriority + 2;
        a = (rand() & 0x1FF) + (self->facingLeft ? 0x700 : 0xF00);
        self->velocityX = rcos(a) * 128; // Saturn * 160
        self->velocityY = -rsin(a) * 128;
        self->posX.val += self->velocityX * 2;
        self->posY.val += self->velocityY * 2;
        MarSetAttack(self, &mar_def_flame);
        if (self->params) {
            self->attack *= 2; // the silver flame
        }
        self->step++;
        return;
    }
    self->posY.val += self->velocityY;
    self->posX.val += self->velocityX;
    if (self->poseTimer < 0) {
        DestroyEntity(self);
    }
}

// 76, Saturn 060BBEF8: Seiryuu. The dragon beast (2) flies in; at her
// third animation the dragon (79) comes out. (The game halts until then,
// as for Byakko.)
void MarEntitySeiryu(Entity* self) {
    if (PLAYER.anim != mar_anim_sp_seiryu &&
        PLAYER.anim != mar_anim_sp_seiryu2 &&
        PLAYER.anim != mar_anim_sp_seiryu3 &&
        PLAYER.anim != mar_anim_sp_seiryu4) {
        MarTimeStop(0);
        DestroyEntity(self);
        return;
    }
    MarCue(self, 0x50); // Saturn: her voice 0x5CF
    switch (self->step) {
    case 0:
        MarSpLoad(3);
        MarTimeStop(1);
        self->flags = FLAG_UNK_10000;
        RicCreateEntFactoryFromEntity(self, FACTORY(BP_MAR_BEAST, 3), 0);
        self->step++;
    case 1:
        if (PLAYER.anim == mar_anim_sp_seiryu3) {
            RicCreateEntFactoryFromEntity(self, BP_MAR_DRAGON, 0);
            MarTimeStop(0);
            // Saturn 0x72C: a loop on channels 22/23 (src/dra/cd.c) the
            // dragon stops (MarSeiryuStop)
            g_api.PlaySfx(MAR_SFX_SEIRYU);
            self->step++;
        }
        break;
    default:
        DestroyEntity(self);
        break;
    }
}

// the end of Seiryuu's sound: SFX_UNUSED_79C is a channel 22/23 release
// (mode 2), which also forgets the loop so the menu won't key it on again
static void MarSeiryuStop(void) {
#ifdef MAR_SFX_SEIRYU_LOOP
    g_api.PlaySfx(SFX_UNUSED_79C);
#endif
}

// the dragon's roar (gen_mar_sounds.py ROAR; the Saturn has none): 2-3
// times at random, a second or more apart (it lasts 124 frames), all
// before the dragon fades. SW 0x84: the frame of the next one
static void MarDragonRoar(Entity* self) {
#if MAR_SFX_ROAR
    s32 t = SW(self, 0x82);

    if (self->step > 2) {
        return;
    }
    if (t == 1) {
        SW(self, 0x84) = 20 + (rand() & 63);
    } else if (t == SW(self, 0x84) && t + 124 < 598) {
        g_api.PlaySfx(MAR_SFX_ROAR);
        SW(self, 0x84) = t + 124 + 60 + rand() % 91;
    }
#endif
}

static MarAttackDef mar_def_dragon = {
    25, 13, 16, 0, 0, 0x1000, 2, 16, 4, 1, 0x18}; // 060C7BFA
typedef struct {
    s16 x;
    s16 y;
    s16 rot;
    s16 pad;
} MarRing;
#define DRAGON_RING(e) ((MarRing*)((u8*)(e) + 0x90)) // Saturn 0x8C, 5 entries
#define DRAGON_RINGI(e) SW(e, 0x7C)

static void MarCorner(s16* px, s16* py, s32 x, s32 y, s32 a, s32 r) {
    *px = x + ((rcos(a) * r) >> 12) * 4 / 5;
    *py = y + ((rsin(a) * r) >> 12);
}

// 79, Saturn 060BB868: Seiryuu's dragon, a head (params 0) steered with the
// d-pad and 15 body segments, each following the one before it five frames
// late; the even ones hurt. Quads textured with MAR_W.CHR package 12's
// head and scale images (the head is stored on its side). About 10 s.
void MarEntityDragon(Entity* self) {
    Primitive* prim;
    Entity* parent;
    MarRing* ring;
    s16* part;
    s32 seg;
    s32 a;
    s32 r;
    s32 t;
    u32 pad;

    seg = self->params >> 8;
    prim = &g_PrimBuf[self->primIndex];
    switch (self->step) {
    case 0:
        self->primIndex = g_api.AllocPrimitives(PRIM_GT4, 1);
        if (self->primIndex == -1) {
            if (!seg) {
                MarSeiryuStop();
            }
            DestroyEntity(self);
            return;
        }
        prim = &g_PrimBuf[self->primIndex];
        part = mar_fxbank_seiryu[seg ? 11 : 10];
        prim->tpage = part[7] >> 2;
        prim->clut = MAR_SP_PAL_SLOT + part[6]; // DRA maps it via g_ClutIds
        prim->u0 = prim->u2 = part[8];
        prim->v0 = prim->v1 = part[9];
        prim->u1 = prim->u3 = part[10];
        prim->v2 = prim->v3 = part[11];
        if (!seg) { // stored on its side: u runs down the head
            prim->u1 = part[8];
            prim->v1 = part[11];
            prim->u2 = part[10];
            prim->v2 = part[9];
        }
        // on top of the stage and its foreground, as on the Saturn
        prim->priority = 0x1E0 - seg;
        prim->drawMode = DRAW_DEFAULT;
        self->flags =
            FLAG_KEEP_ALIVE_OFFCAMERA | FLAG_HAS_PRIMS | FLAG_UNK_10000;
        self->posX.val = PLAYER.posX.val;
        self->posY.val = PLAYER.posY.val;
        SW(self, 0x80) = 0;
        if (!(seg & 1)) {
            MarSetAttack(self, &mar_def_dragon);
        }
        if (!seg) {
            self->posX.i.hi += PLAYER.facingLeft ? -22 : 22;
            self->posY.i.hi -= 12;
            self->velocityX = self->facingLeft ? FIX(-4) : FIX(4); // Saturn
        }
        self->step++;
        break;
    case 1:
        SW(self, 0x80) += 16;
        if (seg > 11) {
            if ((u16)SW(self, 0x80) >= 256 - (seg - 11) * 48) {
                self->step++;
            }
        } else if ((u16)SW(self, 0x80) > 255) {
            self->step++;
        }
        if (SW(self, 0x82) == 3 && seg <= 14) {
            RicCreateEntFactoryFromEntity(
                self, FACTORY(BP_MAR_DRAGON, seg + 1), 0);
        }
    case 2:
        if (SW(self, 0x82) > 597) { // 060C33CC
            self->step++;
            SW(self, 0x82) = 0;
        }
        break;
    case 3:
        if (SW(self, 0x80) > 15) {
            SW(self, 0x80) -= 16;
        }
        if (!SW(self, 0x80)) {
            self->attack = 0;
            prim->drawMode |= DRAW_HIDE;
            if (SW(self, 0x82) > 15) {
                self->step++;
                if (!seg) {
                    RicCreateEntFactoryFromEntity(
                        self, FACTORY(BP_MAR_STAR, 0x26), 0);
                }
            }
        }
        break;
    case 10:
        if (!seg) {
            MarSeiryuStop();
        }
        DestroyEntity(self);
        return;
    default:
        self->step++;
        break;
    }
    SW(self, 0x82)++;
    if (!seg) {
        MarDragonRoar(self);
    }
    // where it was (the Saturn records only when its parent is a segment,
    // which for the head happens through slot reuse)
    t = DRAGON_RINGI(self);
    ring = &DRAGON_RING(self)[t];
    if (++t > 4) {
        t = 0;
    }
    DRAGON_RINGI(self) = t;
    ring->x = self->posX.i.hi;
    ring->y = self->posY.i.hi;
    ring->rot = self->rotate;
    if (seg) {
        parent = self->ext.factory.parent;
        t = DRAGON_RINGI(parent) + 1;
        if (t > 4) {
            t = 0;
        }
        ring = &DRAGON_RING(parent)[t];
        self->posX.i.hi = ring->x;
        self->posY.i.hi = ring->y;
        self->rotate = ring->rot;
        r = (SW(self, 0x80) * 31) >> 8;
        a = self->rotate + 453;
        MarCorner(&prim->x0, &prim->y0, ring->x, ring->y, a, r);
        a += 1142;
        MarCorner(&prim->x2, &prim->y2, ring->x, ring->y, a, r);
        a += 906;
        MarCorner(&prim->x3, &prim->y3, ring->x, ring->y, a, r);
        a += 1142;
        MarCorner(&prim->x1, &prim->y1, ring->x, ring->y, a, r);
        return;
    }
    pad = g_Player.padPressed;
    a = -1;
    if (pad & PAD_UP) {
        a = (pad & PAD_RIGHT) ? 0xE00 : (pad & PAD_LEFT) ? 0xA00 : 0xC00;
    } else if (pad & PAD_DOWN) {
        a = (pad & PAD_RIGHT) ? 0x200 : (pad & PAD_LEFT) ? 0x600 : 0x400;
    } else if (pad & PAD_RIGHT) {
        a = 0;
    } else if (pad & PAD_LEFT) {
        a = 0x800;
    }
    if (a >= 0) {
        self->velocityX += rcos(a) * 8;
        self->velocityY += rsin(a) * 8;
        self->rotate = ratan2(self->velocityY, self->velocityX);
        self->velocityX = rcos(self->rotate) * 64;
        self->velocityY = rsin(self->rotate) * 64;
    } else {
        self->rotate = ratan2(self->velocityY, self->velocityX);
    }
    self->posX.val += self->velocityX - self->velocityX / 5;
    self->posY.val += self->velocityY;
    t = self->posX.i.hi;
    r = (SW(self, 0x80) * 63) >> 8;
    a = self->rotate + 302;
    MarCorner(&prim->x0, &prim->y0, t, self->posY.i.hi, a, r);
    a += 1444;
    MarCorner(&prim->x2, &prim->y2, t, self->posY.i.hi, a, r);
    a += 604;
    MarCorner(&prim->x3, &prim->y3, t, self->posY.i.hi, a, r);
    a += 1444;
    MarCorner(&prim->x1, &prim->y1, t, self->posY.i.hi, a, r);
}

// 87, Saturn 060B18C8: the puff a vibhuti throw leaves when it runs out
// (RicEntitySubwpnThrownVibhuti, blueprint 98): the Saturn's shared opaque
// puff in white (frames 30-43 of the permanent bank, palette 0x146:
// gen_mar_assets.py SP_PUFF), 6 px up, rising 0.5 a frame, hitting as holy
// ash until its animation ends
static MarAttackDef mar_def_ash = {
    20, 13, 16, 0, 0, 0x1000, 2, 10, 4, 1, 0}; // 060C791C
static AnimationFrame mar_anim_ash[] = {
    POSE(2, 30, 0), POSE(2, 31, 0), POSE(2, 32, 0), POSE(2, 33, 0),
    POSE(2, 34, 0), POSE(2, 35, 0), POSE(2, 36, 0), POSE(2, 37, 0),
    POSE(2, 38, 0), POSE(2, 39, 0), POSE(2, 40, 0), POSE(2, 41, 0),
    POSE(2, 42, 0), POSE(2, 43, 0), POSE_END}; // 060C792C

void MarEntityAsh(Entity* self) {
    if (self->step == 0) {
        self->animSet = ANIMSET_OVL(MAR_PERM_BANK);
        self->palette = MAR_PAL_OWL; // + the frames' clut 5
        self->anim = mar_anim_ash;
        self->zPriority = PLAYER.zPriority + 2;
        self->flags = FLAG_POS_CAMERA_LOCKED | FLAG_UNK_100000;
        self->posY.i.hi -= 6;
        self->velocityY = FIX(-0.5);
        MarSetAttack(self, &mar_def_ash);
        g_api.PlaySfx(SFX_UNUSED_FM_EXPLODE_C); // Saturn 0x65C
        self->step++;
        return;
    }
    if (self->poseTimer < 0) {
        DestroyEntity(self);
        return;
    }
    self->posY.val += self->velocityY;
}

// 80, Saturn 060BB724: a star that grows, waits (params) and shrinks
void MarEntityStar(Entity* self) {
    switch (self->step) {
    case 0:
        self->animSet = ANIMSET_OVL(MAR_PERM_BANK);
        self->palette = MAR_PAL_OWL;
        self->zPriority = 0x1BC; // Saturn 444
        self->flags =
            FLAG_KEEP_ALIVE_OFFCAMERA | FLAG_POS_CAMERA_LOCKED | FLAG_UNK_10000;
        self->drawFlags = ENTITY_SCALEX | ENTITY_SCALEY;
        self->scaleX = self->scaleY = 0x80;
        self->step++;
        break;
    case 1:
        self->scaleX += 16;
        self->scaleY += 16;
        if (self->scaleX > 383) {
            self->step++;
            SW(self, 0x7C) = self->params >> 8;
        }
        break;
    case 2:
        if (--SW(self, 0x7C) < 0) {
            self->step++;
        }
        break;
    default:
        self->scaleX -= 16;
        self->scaleY -= 16;
        if (!self->scaleX) {
            DestroyEntity(self);
            return;
        }
        break;
    }
    self->animCurFrame = 14 + SW(self, 0x7E); // Saturn 060C7BF4: 13-18
    if (++SW(self, 0x7E) > 5) {
        SW(self, 0x7E) = 0;
    }
}

static MarAttackDef mar_def_owl = {
    30, 0, 0, 0, 0, 0x1000, 2, 16, 4, 1, 0x18}; // 060C7CF0
static AnimationFrame mar_anim_owl_fly[] = {
    POSE(8, 4, 0), POSE(8, 5, 0), POSE(8, 6, 0), POSE(8, 7, 0),
    POSE_LOOP(0)}; // 060C7C5C
static AnimationFrame mar_anim_owl_fly2[] = {
    POSE(8, 8, 0), POSE(8, 9, 0), POSE(8, 10, 0), POSE(8, 11, 0),
    POSE_LOOP(0)}; // 060C7C70
static AnimationFrame mar_anim_owl_dive0[] = {POSE(5, 4, 0), POSE_LOOP(0)};
static AnimationFrame mar_anim_owl_dive1[] = {POSE(5, 8, 0), POSE_LOOP(0)};
static AnimationFrame mar_anim_owl_dive2[] = {POSE(5, 9, 0), POSE_LOOP(0)};
static AnimationFrame mar_anim_owl_perch[] = {
    POSE(4, 4, 0), POSE(4, 5, 0),  POSE(4, 6, 0), POSE(4, 7, 0), POSE(4, 4, 0),
    POSE(4, 5, 0), POSE(4, 6, 0),  POSE(4, 7, 0), POSE(5, 4, 0), POSE(5, 5, 0),
    POSE(5, 6, 0), POSE(5, 7, 0),  POSE(6, 4, 0), POSE(6, 5, 0), POSE(7, 6, 0),
    POSE(8, 7, 0), POSE(10, 1, 0), POSE(8, 2, 0), POSE(8, 3, 0), POSE(8, 2, 0),
    POSE_LOOP(16)};       // 060C7C9C
static u16 mar_owl_count; // 060C7D00

static void MarDecel(s32* v, s32 speed) {
    if (*v < 0) {
        *v += speed;
        if (*v > 0) {
            *v = 0;
        }
    } else {
        *v -= speed;
        if (*v < 0) {
            *v = 0;
        }
    }
}

// Saturn 060BC024: the next enemy to dive at (familiar style, flag 0x80000)
static Entity* MarOwlTarget(Entity* self) {
    Entity* e;
    Entity* last = NULL;
    s32 dir = self->facingLeft ? 1 : -1;
    s32 n = 0;

    for (e = &g_Entities[STAGE_ENTITY_START];
         e < &g_Entities[STAGE_ENTITY_START + 128]; e++) {
        if (!e->entityId || !e->hitboxState) {
            continue;
        }
        if (e->posX.i.hi < -16 || e->posX.i.hi > 272 || e->posY.i.hi > 240 ||
            e->posY.i.hi < 0 || e->hitPoints > 0x6FFF) {
            continue;
        }
        if (e->flags & FLAG_UNK_80000) {
            n++;
            last = e;
        } else if ((e->posX.i.hi - self->posX.i.hi) * dir > 0) {
            e->flags |= FLAG_UNK_80000;
            return e;
        }
    }
    return n ? last : NULL;
}

// Saturn 060BC0F4: steer toward her + (0x84, 0x86): 1 far, 2 there, 0 near
static s32 MarOwlSeek(Entity* self) {
    s32 dx;
    s32 dy;
    s32 d2;
    s32 speed;
    s32 t;
    s32 ret;

    dx = (PLAYER.posX.i.hi + SW(self, 0x84) - self->posX.i.hi) * 5 / 4;
    dy = PLAYER.posY.i.hi + SW(self, 0x86) - self->posY.i.hi;
    self->rotate = ratan2(-dy, dx);
    d2 = dx * dx + dy * dy;
    speed = d2 > 3600 ? 40 : d2 > 400 ? 16 : d2 > 100 ? 10 : 0;
    if (speed) {
        t = rcos(self->rotate) * speed;
        if (self->velocityX > t) {
            self->velocityX -= 0x1000;
        } else if (self->velocityX < t) {
            self->velocityX += 0x1000;
        }
        t = -rsin(self->rotate) * speed;
        if (self->velocityY > t) {
            self->velocityY -= 0x1000;
        } else if (self->velocityY < t) {
            self->velocityY += 0x1000;
        }
        return 1;
    }
    ret = 0;
    if (dx > 2) {
        self->velocityX = 0x4000;
    } else if (dx < -2) {
        self->velocityX = -0x4000;
    } else {
        ret = 1;
    }
    if (dy > 2) {
        self->velocityY = 0x4000;
    } else if (dy < -2) {
        self->velocityY = -0x4000;
    } else {
        ret++;
    }
    return ret == 2 ? 2 : 0;
}

// Saturn 060BC23C (the art faces left)
static void MarOwlFacing(Entity* self) {
    if (self->step > 4) {
        if (self->velocityX) {
            self->facingLeft = self->velocityX > 0;
        }
        return;
    }
    if (++SW(self, 0x7E) > 50) {
        SW(self, 0x7E) = 0;
        if (abs(SW(self, 0x84) + PLAYER.posX.i.hi - self->posX.i.hi) <= 23 &&
            abs(SW(self, 0x86) + PLAYER.posY.i.hi - self->posY.i.hi) <= 29) {
            if (self->facingLeft == PLAYER.facingLeft) {
                self->facingLeft = !self->facingLeft;
            }
        } else if (self->velocityX) {
            self->facingLeft = self->velocityX > 0;
        }
    }
}

// 77, Saturn 060BC320: Suzaku, a bird familiar for 2000 frames (at most two).
// It hovers near her, now and then dives at an enemy, and perches on her
// shoulder while she stands holding Up. Velocities are kept in Saturn units
// (x moves at 4/5).
void MarEntitySuzaku(Entity* self) {
    Entity* t;
    s32 flags = 0;
    s32 seek;
    s32 a;
    s32 d;
    s32 tx;
    s32 dx;
    s32 dy;

    MarCue(self, 0x58); // Saturn: her voice 0x5CF
    switch (self->step) {
    case 0:
        d = mar_owl_count++;
        self->zPriority = PLAYER.zPriority + ((d & 1) ? 4 : -4);
        self->animSet = ANIMSET_OVL(MAR_PERM_BANK);
        self->palette = MAR_PAL_OWL;
        self->scaleX = self->scaleY = 0x100;
        self->flags =
            FLAG_KEEP_ALIVE_OFFCAMERA | FLAG_POS_CAMERA_LOCKED | FLAG_UNK_20000;
        self->anim = mar_anim_owl_fly;
        self->posX.i.hi = PLAYER.posX.i.hi + (PLAYER.facingLeft ? -7 : 7);
        self->posY.i.hi = PLAYER.posY.i.hi - 50;
        RicCreateEntFactoryFromEntity(self, FACTORY(BP_MAR_BEAST, 4), 0);
        self->facingLeft = !PLAYER.facingLeft;
        self->step++;
        break;
    case 1:
        if ((PLAYER.animCurFrame & 0x7FFF) == 256) {
            RicCreateEntFactoryFromEntity(self, FACTORY(BP_MAR_STAR, 4), 0);
            // Saturn 0x72D: the PS1 common sword swing of the same shape
            g_api.PlaySfx(SFX_ANIME_SWORD_A);
            self->step++;
            self->step_s = 0;
        }
        break;
    case 2:
        if (++self->step_s > 15) {
            MarSetAttack(self, &mar_def_owl);
            self->velocityY = FIX(-2);
            self->flags |= FLAG_UNK_100000;
            self->step++;
        }
        break;
    case 3:
        self->velocityY += 0x2000;
        if (self->velocityY >= 0) {
            SW(self, 0x84) = self->posX.i.hi - PLAYER.posX.i.hi;
            SW(self, 0x86) = self->posY.i.hi - PLAYER.posY.i.hi;
            self->step++;
            self->step_s = 0;
        }
        break;
    case 4:
        flags = 1;
        seek = MarOwlSeek(self);
        self->hitFlags = 0;
        switch (self->step_s) {
        case 0:
            if (++SW(self, 0x88) > 100 && !(rand() & 0x1F)) {
                SW(self, 0x88) = 0;
                t = MarOwlTarget(self);
                SE(self, 0x8C) = t;
                if (t) {
                    SW(self, 0x90) = t->entityId;
                    tx = t->posX.i.hi +
                         (t->facingLeft ? -t->hitboxOffX : t->hitboxOffX);
                    self->rotate =
                        ratan2(self->posY.i.hi - (t->posY.i.hi + t->hitboxOffY),
                               (tx - self->posX.i.hi) * 5 / 4);
                    self->velocityX = rcos(self->rotate) * 48;
                    self->velocityY = -rsin(self->rotate) * 48;
                    if (self->velocityY >= 0) {
                        d = abs((self->rotate & 0xFFF) - 0xC00);
                        self->anim = d > 683   ? mar_anim_owl_dive2
                                     : d > 341 ? mar_anim_owl_dive1
                                               : mar_anim_owl_dive0;
                        self->pose = self->poseTimer = 0;
                    } else if (abs(self->velocityY) > abs(self->velocityX)) {
                        self->anim = mar_anim_owl_fly;
                    } else {
                        self->anim = mar_anim_owl_fly2;
                    }
                    self->step++;
                    break;
                }
            }
        case 1:
            if (PLAYER.step == PL_S_STAND && PLAYER.step_s == 1) {
                self->step_s++;
            }
            break;
        default:
            if (PLAYER.step != PL_S_STAND || PLAYER.step_s != 1) {
                SW(self, 0x84) = (rand() & 31) * 4 / 5;
                SW(self, 0x86) = -30 - (rand() & 15);
                self->anim = mar_anim_owl_fly;
                self->poseTimer = 4;
                self->pose = rand() & 3;
                self->step_s = 0;
                break;
            }
            // on her shoulder
            d = self->zPriority > PLAYER.zPriority ? 5 : -5;
            SW(self, 0x84) = PLAYER.facingLeft ? d : -d;
            SW(self, 0x86) = -15;
            if (seek == 2) {
                if (self->anim != mar_anim_owl_perch) {
                    self->anim = mar_anim_owl_perch;
                    self->pose = self->poseTimer = 0;
                }
                SW(self, 0x7C)--; // its time doesn't run while there
            }
            break;
        }
        break;
    case 5: // diving
        flags = 2;
        self->hitboxWidth = 6;
        self->hitboxHeight = 8;
        if (self->hitFlags) {
            self->step++;
            break;
        }
        t = SE(self, 0x8C);
        if (t->entityId != SW(self, 0x90)) {
            self->step++;
            break;
        }
        dx = t->posX.i.hi + (t->facingLeft ? -t->hitboxOffX : t->hitboxOffX) -
             self->posX.i.hi;
        dy = t->posY.i.hi + t->hitboxOffY - self->posY.i.hi;
        if (abs(dx) <= 16 && abs(dy) <= 20) {
            self->step++;
            break;
        }
        a = ratan2(-dy, dx * 5 / 4);
        self->velocityX += rcos(a) * 16;
        self->velocityY -= rsin(a) * 16;
        self->rotate = ratan2(-self->velocityY, self->velocityX);
        dx = self->velocityX >> 16;
        dy = self->velocityY >> 16;
        if (dx * dx + dy * dy > 9) {
            self->velocityX = rcos(self->rotate) * 48;
            self->velocityY = -rsin(self->rotate) * 48;
        }
        break;
    case 6: // flies on until off screen
        flags = 2;
        if ((u16)(self->posX.i.hi + 32) > 320 ||
            (u16)(self->posY.i.hi + 32) > 304) {
            SW(self, 0x84) = (rand() & 31) * 4 / 5;
            SW(self, 0x86) = -30 - (rand() & 15);
            self->anim = mar_anim_owl_fly2;
            self->hitboxWidth = self->hitboxHeight = 0;
            self->step++;
        }
        break;
    case 7: // back to her
        flags = 1;
        if (!MarOwlSeek(self)) {
            self->anim = mar_anim_owl_fly;
            self->step = 4;
            self->step_s = 0;
        }
        break;
    case 100: // time's up: slow down, sparkle, shrink
        d = 0;
        if (abs(self->velocityX) > 0xC000) {
            self->velocityX += self->velocityX > 0 ? -0x8000 : 0x8000;
            d++;
        }
        if (abs(self->velocityY) > 0xC000) {
            self->velocityY += self->velocityY > 0 ? -0x8000 : 0x8000;
            d++;
        }
        if (!d) {
            RicCreateEntFactoryFromEntity(self, FACTORY(BP_MAR_STAR, 4), 0);
            self->drawFlags = ENTITY_SCALEX | ENTITY_SCALEY;
            self->step++;
        }
        break;
    case 101:
        self->scaleX -= 9;
        self->scaleY -= 9;
        if (self->scaleX < 0) {
            DestroyEntity(self);
            return;
        }
        break;
    default:
        DestroyEntity(self);
        return;
    }
    if ((flags & 2) && !(g_GameTimer & 3)) {
        RicCreateEntFactoryFromEntity(
            self, FACTORY(BP_MAR_TRAIL, (self->animCurFrame & 0xFF)), 0);
    }
    self->posX.val += self->velocityX - self->velocityX / 5;
    self->posY.val += self->velocityY;
    if (flags & 1) {
        MarDecel(&self->velocityX, 0x800);
        MarDecel(&self->velocityY, 0x800);
    }
    MarOwlFacing(self);
    if ((u16)++SW(self, 0x7C) > 2000 && self->step < 100) {
        self->step = 100;
    }
    if ((u16)(self->posX.i.hi + 15) <= 286 &&
        (u16)(self->posY.i.hi + 15) <= 286 &&
        (self->anim == mar_anim_owl_fly || self->anim == mar_anim_owl_fly2 ||
         self->anim == mar_anim_owl_perch) &&
        (self->animCurFrame == 5 || self->animCurFrame == 9) &&
        self->poseTimer == 1) {
        g_api.PlaySfx(MAR_SFX_FLAP); // Saturn 0x72E
    }
}

// 81, Saturn 060BCBD8: Suzaku's trail, a copy of its frame fading in 20
void MarEntityTrail(Entity* self) {
    if (self->step == 0) {
        self->animSet = ANIMSET_OVL(MAR_PERM_BANK);
        self->palette = MAR_PAL_OWL;
        self->zPriority = PLAYER.zPriority - 5;
        self->flags = FLAG_POS_CAMERA_LOCKED;
        self->animCurFrame = self->params >> 8;
        self->blendMode = BLEND_TRANSP | BLEND_ADD;
        self->drawFlags = ENTITY_OPACITY | ENTITY_MASK_B;
        SW(self, 0x7C) = 10;
        self->step++;
    } else if ((g_GameTimer & 1) && !--SW(self, 0x7C)) {
        DestroyEntity(self);
        return;
    }
    self->opacity = SW(self, 0x7C) * 12;
}

// Saturn 060A5BE8: while timer 13 runs her palette is 71 / 72 on alternate
// frames, the global colour-cycling effect palettes (the Saturn's hit-by
// table: 68 dark, 72 holy = PS1 0x164, 0x168), i.e. PS1 PAL_CC_YELLOW_EFFECT
// and PAL_CC_GREEN_EFFECT_A; no afterimages
void MarBeastsPal(s32 on) {
    if (on) {
        PLAYER.palette = PAL_FLAG(
            (g_Timer & 1) ? PAL_CC_YELLOW_EFFECT : PAL_CC_GREEN_EFFECT_A);
        DisableAfterImage(1, 4);
    } else {
        PLAYER.palette = PAL_FLAG(PAL_RICHTER);
    }
}

static MarAttackDef mar_def_beasts = {
    20, 6, 8, 0, 0, 0x1000, 2, 20, 20, 1, 0}; // 060C7D02

// 91, Saturn 060BCFF4: Four Beasts. When her animation ends she is
// invincible for 2000 frames (timer 13, which also doubles subweapon
// damage), and her body hurts what it touches (20, holy)
void MarEntityBeasts(Entity* self) {
    switch (self->step) {
    case 0:
        MarSetAttack(self, &mar_def_beasts);
        self->flags = FLAG_KEEP_ALIVE_OFFCAMERA | FLAG_UNK_20000;
        self->step++;
        break;
    case 1:
        if (PLAYER.poseTimer < 0) {
            g_Player.timers[PL_T_INVINCIBLE_SCENE] = 2000;
            self->step++;
        }
        break;
    default:
        if (!g_Player.timers[PL_T_INVINCIBLE_SCENE]) {
            DestroyEntity(self);
            return;
        }
        if (g_Player.timers[PL_T_INVINCIBLE_SCENE] == 383) {
            RicCreateEntFactoryFromEntity(self, BP_MAR_COUNTDOWN, 0);
        }
        break;
    }
    self->posX.i.hi = PLAYER.posX.i.hi;
    self->posY.i.hi = PLAYER.posY.i.hi;
    self->facingLeft = PLAYER.facingLeft;
    self->hitboxOffX = PLAYER.hitboxOffX;
    self->hitboxOffY = PLAYER.hitboxOffY;
    self->hitboxWidth = PLAYER.hitboxWidth;
    self->hitboxHeight = PLAYER.hitboxHeight;
}

// ---------------------------------------------------------------- HUD

// Her HUD is the Saturn's (MAR_W.CHR package 5: vine gauge holder, HP / MP
// bars, hearts, the MARIA plate), drawn by DRA (src/dra/5F60C.c,
// MOD_SATURN_MARIA). Its plate glows with her charge like the Saturn's
// (which reads g_Player.unk3FC): DRA reads it from g_Player.unk24, unused in
// the PS1 US game.
static void MarHudInit(void) { g_Player.unk24 = 0; }

static void MarHudUpdate(void) {
    g_Player.unk24 = mar_charge;
    MarDuck(); // (every frame)
}

// 90, Saturn 060BCCE8: the Four Beasts countdown for its last 383 frames, a
// HUD digit (timer >> 6; 0 in the last 64) floating behind her head, 2x,
// popping to 3x when it changes. Drawn as the PS1 stopwatch's countdown is
// (RicEntitySubwpnStopwatch): the HUD digits in VRAM, tpage 0x1A clut 0x186,
// 6x9 at u 0x18 + 8 * digit (0 at + 0x50), v 0x40 (the Saturn's own digits
// didn't survive narrowing)
void MarEntityCountdown(Entity* self) {
    Primitive* prim;
    s32 n, s, w, h, x, y;

    if (!self->step) {
        self->primIndex = g_api.AllocPrimitives(PRIM_GT4, 1);
        if (self->primIndex == -1) {
            DestroyEntity(self);
            return;
        }
        prim = &g_PrimBuf[self->primIndex];
        prim->tpage = 0x1A;
        prim->clut = 0x186;
        prim->v0 = prim->v1 = 0x40;
        prim->v2 = prim->v3 = 0x49;
        prim->drawMode = DRAW_UNK_100 | DRAW_UNK02;
        self->flags =
            FLAG_KEEP_ALIVE_OFFCAMERA | FLAG_POS_CAMERA_LOCKED | FLAG_HAS_PRIMS;
        self->posX.i.hi = PLAYER.posX.i.hi;
        self->posY.i.hi = PLAYER.posY.i.hi - 40;
        self->step++;
    }
    n = g_Player.timers[PL_T_INVINCIBLE_SCENE];
    if (!n) {
        DestroyEntity(self);
        return;
    }
    n = (n >> 6) % 10;
    if (SW(self, 0x7C) > 0x100) {
        SW(self, 0x7C) -= 8;
    }
    if (SW(self, 0x7E) != n + 1) {
        SW(self, 0x7E) = n + 1;
        SW(self, 0x7C) = 0x180;
    }
    self->posX.val +=
        (PLAYER.posX.val + (PLAYER.facingLeft ? FIX(24) : FIX(-24)) -
         self->posX.val) >>
        3;
    self->posY.val += (PLAYER.posY.val - FIX(40) - self->posY.val) >> 3;
    s = SW(self, 0x7C);
    w = 6 * s >> 7; // 2x, 3x at the pop
    h = 9 * s >> 7;
    x = self->posX.i.hi - w / 2;
    y = self->posY.i.hi - h / 2;
    prim = &g_PrimBuf[self->primIndex];
    prim->u0 = prim->u2 = 0x18 + (n ? 8 * n : 0x50);
    prim->u1 = prim->u3 = prim->u0 + 6;
    prim->x0 = prim->x2 = x;
    prim->x1 = prim->x3 = x + w;
    prim->y0 = prim->y1 = y;
    prim->y2 = prim->y3 = y + h;
    prim->priority = PLAYER.zPriority + 3;
}
