// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port mod: the PSP (Dracula X Chronicles) Maria on the PS1, the
// select screen's Maria "PSP" version: BIN/MARIA2.BIN = src/maria (the PSP
// player overlay, decompiled) + this glue, built by
// mods/sotn-ultimate/tools/build_mar2.sh. Adapted from the upstream
// mods/mariapsp-forpsx/maria.c by Xeeynamo (sotn-decomp #2638; its PSP voice
// by sozud, #3457), which replaced RIC.BIN; here she is a separate
// file that src/dra/cd.c loads for PLAYER_MARIA when MOD_MARIA_PSP(), with
// BIN/MARIA2_SD.BIN (her PSP voice in Richter's voice ids' vab 1 slots:
// mods/sotn-ultimate/tools/gen_mar2_sounds.py). Differences from upstream:
// nothing in DRA is patched in place (the Saturn Maria and Richter can follow
// in the same session): the HUD icon table is the DRA pointer g_ModSubwpnIcons
// (cd.c resets it for every player overlay), the sound slots are laid out in
// the bank itself; the BSS is cleared once per load (the overlay's file has no
// BSS).
#include <maria.h>

typedef struct {
    s32 x;
    s32 y;
    s32 w;
    s32 h;
    s32 u;
    s32 v;
    s32 tpage;
    s32 clut;
} SubwpnIconParams; // src/dra/dra.h RicSubwpnIconParams

// her subweapons' HUD icons (hud_en in VRAM), indexed by the Richter
// subweapon id - 1 the HUD sees (see the conversion below)
static SubwpnIconParams g_Mar2SubwpnIcons[] = {
    {0x05, 0x09, 0x1F, 0x17, 0xE0, 0xB0, 0x01E, 0x17F}, // dagger: CAT
    {0x0C, 0x07, 0x0F, 0x17, 0xD0, 0xC0, 0x01E, 0x17F}, // axe: CARDINAL
    {0x05, 0x09, 0x1F, 0x17, 0xE0, 0xC8, 0x01E, 0x17F}, // holy water: TURTLE
    {0x0C, 0x09, 0x10, 0x17, 0x98, 0xD8, 0x01E, 0x17F}, // (DOLL)
    {0x0C, 0x09, 0x10, 0x17, 0x98, 0xD8, 0x01E, 0x17F}, // bible: DOLL
    {0x0C, 0x09, 0x10, 0x17, 0x98, 0xD8, 0x01E, 0x17F}, // (DOLL)
    {0x0C, 0x09, 0x10, 0x17, 0x98, 0xD8, 0x01E, 0x17F}, // (DOLL)
    {0x04, 0x00, 0x1F, 0x1F, 0xE0, 0xE0, 0x01E, 0x17F}, // vibhuti: DRAGON
    {0x0C, 0x09, 0x10, 0x17, 0x98, 0xD8, 0x01E, 0x17F}, // (DOLL)
};
extern SubwpnIconParams* g_ModSubwpnIcons; // DRA (src/dra/5F60C.c)

// Her subweapon drops (candles, the Alchemy Lab tubes) as on the PSP: the
// stages' dagger / axe / holy water / vibhuti drops show her Cat / Cardinal
// / Turtle / Dragon (the PSP's own drop frames 0xA1-0xC0 of DRA's item
// sprite bank, in mar2_items.h; their art is in her items_gfx, which she
// loads over the PS1 item art: the Doll sits in the Bible's place, so the
// Bible's frames already show it). DRA's ANIMSET_DRA(3) frame table is
// swapped for a copy with those animations' frames replaced; DRA puts its
// own back on every Engine_Init, before the player's init (src/dra/
// 5087C.c). The cross / stopwatch / rebound stone / agunea drops are small
// hearts for her in the stages (MOD_MARIA_PSP_NO_SUBWPN, include/game.h).
#include "mar2_items.h"
#define MAR2_ITEM_FRAMES 0xA1 // the PS1 table's length
extern s16** D_800A3B70[];    // DRA's sprite banks (ANIMSET_DRA(n))
static s16* g_Mar2ItemTable[MAR2_ITEM_FRAMES];
static void Mar2InitItemFrames(void) {
    // the PS1 frames of each drop animation (the stages' e_collect.h)
    static const u8 first[] = {
        0x3A, // axe (item 15): CARDINAL
        0x42, // dagger (item 14): CAT
        0x32, // holy water (item 17): TURTLE
        0x62, // vibhuti (item 21): DRAGON
    };
    s32 i;

    if (D_800A3B70[3] != g_Mar2ItemTable) {
        memcpy(g_Mar2ItemTable, D_800A3B70[3], sizeof(g_Mar2ItemTable));
    }
    for (i = 0; i < 32; i++) {
        g_Mar2ItemTable[first[i >> 3] + (i & 7)] = g_Mar2ItemFrames[i];
    }
    D_800A3B70[3] = g_Mar2ItemTable;
}

// On the MARIA overlay, PL_W_NONE is used for the Doll Crash. Convert it
// back and forth so the overlay and the DRA HUD / item pickups both work:
// Bible -> Doll, Dagger -> Cat, Vibhuti -> Dragon, Axe -> Bird (Cardinal),
// Holy water -> Turtle. Ids she has no subweapon for keep the slot's
// previous one (+1 heart for a pickup).
static u32 Mar2RicToMaria(u32 ric, u32 prev, s32 heart) {
    switch (ric) {
    case SUBWPN_NONE:
    case SUBWPN_BIBLE:
        return PL_W_NONE;
    case SUBWPN_DAGGER:
        return PL_W_CAT;
    case SUBWPN_AXE:
        return PL_W_CARDINAL;
    case SUBWPN_HOLYWATER:
        return PL_W_TURTLE;
    case SUBWPN_VIBHUTI:
        return PL_W_DRAGON;
    }
    if (heart && g_Status.hearts < g_Status.heartsMax) {
        g_Status.hearts++;
    }
    return prev <= PL_W_DRAGON ? prev : PL_W_NONE;
}
static u32 Mar2MariaToRic(u32 id) {
    switch (id) {
    case PL_W_CAT:
        return SUBWPN_DAGGER;
    case PL_W_CARDINAL:
        return SUBWPN_AXE;
    case PL_W_TURTLE:
        return SUBWPN_HOLYWATER;
    case PL_W_DRAGON:
        return SUBWPN_VIBHUTI;
    }
    return SUBWPN_BIBLE; // PL_W_NONE: the Doll
}

// Her two subweapon slots, as on the PSP: g_Status.subWeapon is the one
// she throws, g_Status.D_80097C40 the other one; Circle swaps them
// (src/maria/pl_main.c, end of MarMain); a pickup replaces the OTHER one
// (the PSP's collect_subweapon_psp.h: the old one is tossed, picking up
// the one already there gives a heart). The PS1 stages pick up into, toss
// and compare with g_Status.subWeapon, so outside MarMain the two fields
// hold the slots the other way round, as Richter ids (saved like that):
//   g_Status.subWeapon   = the other slot (stage pickups land here)
//   g_Status.D_80097C40  = the slot she throws (MarPsxInit validates both)
// MarPsxMain swaps them into the PSP layout (her ids) around MarMain.
static u32 g_Mar2Pri; // her ids as left after MarMain
static u32 g_Mar2Sec;
static u32 g_Mar2SecRic; // g_Status.subWeapon as left (spots a pickup)
// the HUD (the PSP's DRA func_psp_090E4C18..func_psp_090E4CD0): both icons,
// the other one smaller / darker behind; a swap turns them in 10 frames
static s32 g_Mar2Swap = -1; // D_psp_091474B8: swap frame, -1 = none
static s32 g_Mar2HudPri;    // D_psp_091CDC80: icon in front
static s32 g_Mar2HudSec;    // D_psp_091CDC88: icon behind

int PadReadPSP() { return g_pads[0].pressed; } // raw, like her other buttons
int func_psp_090E4C58() { return g_Mar2Swap; }
void func_psp_090E4C18() {
    if (g_Mar2Swap < 0) {
        g_Mar2Swap = 0;
        g_Mar2HudPri = g_Status.subWeapon;
        g_Mar2HudSec = g_Status.D_80097C40;
    }
}
void func_psp_090E4C68() {} // MarInit: MarPsxInit syncs the HUD
// a pickup (PSP func_psp_090E4C90; D_80097C40 = the other slot, her id)
static void Mar2HudPickup(void) {
    if (g_Mar2Swap < 0) {
        g_Mar2HudSec = g_Status.D_80097C40;
    } else {
        g_Mar2HudPri = g_Status.D_80097C40;
    }
}

// PSP D_psp_09147418: HUD icons by her subweapon id (hud_en in VRAM)
static SubwpnIconParams g_Mar2HudIconDefs[] = {
    {0x00C, 0x009, 0x010, 0x017, 0x098, 0x0D8, 0x01E, 0x17F}, // DOLL
    {0x00C, 0x007, 0x00F, 0x017, 0x0D0, 0x0C0, 0x01E, 0x17F}, // CARDINAL
    {0x005, 0x009, 0x01F, 0x017, 0x0E0, 0x0B0, 0x01E, 0x17F}, // CAT
    {0x005, 0x009, 0x01F, 0x017, 0x0E0, 0x0C8, 0x01E, 0x17F}, // TURTLE
    {0x004, 0x000, 0x01F, 0x01F, 0x0E0, 0x0E0, 0x01E, 0x17F}, // DRAGON
};
// cos / sin (pi * frame / 10) * 4096, frames 0-9 (the PSP uses floats)
static const s16 g_Mar2Cos[] = {
    4096, 3896, 3314, 2408, 1266, 0, -1266, -2408, -3314, -3896};
static const s16 g_Mar2Sin[] = {
    0, 1266, 2408, 3314, 3896, 4096, 3896, 3314, 2408, 1266};

// PSP func_psp_090E4968 (scale: 4096 = 1)
static void Mar2HudIcon(
    Primitive* prim, s32 idx, s32 xOfs, s32 yOfs, s32 scale, s32 color) {
    SubwpnIconParams* d;
    s32 x, y, w, h;

    if (idx < 0 || idx > PL_W_DRAGON) {
        idx = PL_W_NONE;
    }
    d = &g_Mar2HudIconDefs[idx];
    x = d->x + 2 + xOfs + d->w * (4096 - scale) / 4096;
    y = d->y + 22 + yOfs + d->h * (4096 - scale) / 4096;
    w = d->w * scale / 4096;
    h = d->h * scale / 4096;
    prim->x0 = prim->x2 = x;
    prim->x1 = prim->x3 = x + w;
    prim->y0 = prim->y1 = y;
    prim->y2 = prim->y3 = y + h;
    prim->u0 = prim->u2 = d->u;
    prim->u1 = prim->u3 = d->u + d->w;
    prim->v0 = prim->v1 = d->v;
    prim->v2 = prim->v3 = d->v + d->h;
    color &= 0xFF;
    prim->r0 = prim->g0 = prim->b0 = color;
    prim->r1 = prim->g1 = prim->b1 = color;
    prim->r2 = prim->g2 = prim->b2 = color;
    prim->r3 = prim->g3 = prim->b3 = color;
    prim->tpage = d->tpage;
    prim->clut = d->clut;
    prim->drawMode = DRAW_ABSPOS | DRAW_COLORS;
    if (prim->clut == 0x17F) {
        prim->drawMode |= DRAW_TPAGE | DRAW_TRANSP;
    }
}

// PSP func_psp_090E4CD0, called by DRA's DrawRichterHudSubweapon through
// g_ModMar2HudIcons: prim = the subweapon icon, prim->next = the extra HUD
// sprite (only her HUD has it); returns the latter
extern Primitive* (*g_ModMar2HudIcons)(Primitive* prim); // DRA 5F60C.c
static Primitive* Mar2HudIcons(Primitive* prim) {
    s32 c, s, tmp;
    s32 xa, ya, sa, ca, xb, yb, sb, cb;

    if (g_Mar2Swap >= 0) {
        c = g_Mar2Cos[g_Mar2Swap];
        s = g_Mar2Sin[g_Mar2Swap];
        xa = (5 * 4096 - 5 * c) / 4096;
        ya = -(6 * s) / 4096;
        sa = 3686 + c / 10;
        ca = (-127 * 4096 + 127 * c / 2) / 4096;
        xb = (5 * 4096 + 5 * c) / 4096;
        yb = (6 * s) / 4096;
        sb = 3686 - c / 10;
        cb = (-127 * 4096 - 127 * c / 2) / 4096;
        if (++g_Mar2Swap >= 10) {
            g_Mar2Swap = -1;
            tmp = g_Mar2HudPri;
            g_Mar2HudPri = g_Mar2HudSec;
            g_Mar2HudSec = tmp;
        }
    }
    if (g_Mar2Swap < 0) {
        xa = ya = yb = 0;
        sa = 4096;
        ca = 0xFF;
        xb = 10;
        sb = 3277; // 0.8
        cb = 0x80;
    }
    // the PSP's colours are for its GPU (0xFF = normal); on the PS1 0x80 is
    // normal, so tone them down (front 0xBF, back 0x60)
    ca = (ca & 0xFF) * 3 / 4;
    cb = (cb & 0xFF) * 3 / 4;
    Mar2HudIcon(prim, g_Mar2HudPri, xa, ya, sa, ca);
    prim = prim->next;
    Mar2HudIcon(prim, g_Mar2HudSec, xb, yb, sb, cb);
    return prim;
}

void func_800FE8F0() {}

void func_psp_091040A0(u_long** _gfxBank) {
    GfxBank* gfxBank;
    GfxLoad* gfxLoad;
    s32 i;

    gfxBank = (GfxBank*)_gfxBank; // avoid problems with the game.h signature
    if (gfxBank->kind == GFX_BANK_NONE) {
        return;
    }
    if (gfxBank->kind == -1) {
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

void func_psp_0892667C(s32 paletteID, u16* paletteData) {
    int x = (paletteID & 15) * 16;
    int y = (paletteID >> 4) & 15;
    RECT rect = {x, 240 + y, 16, 1};
    memcpy(
        &g_Clut[(paletteID & 0x300) >> 8][(paletteID & 0xFF) * COLORS_PER_PAL],
        paletteData, PALETTE_LEN);
    LoadImage(&rect, (u_long*)paletteData);
}

// her PSP voice ids 0x8E6-0x8F5 -> the PS1 Richter voice ids whose vab 1
// slots hold them in BIN/MARIA2_SD.BIN
// (mods/sotn-ultimate/tools/gen_mar2_sounds.py has the same table): hurt and
// death where Richter's hurt and death are, so DRA / stage code that plays
// those for a non-Alucard player plays hers
#define MARIA_SFX_FIRST SFX_VO_MAR_8E6
#define MARIA_SFX_LAST SFX_VO_MAR_DEATH
static const u16 g_Mar2SfxSlot[16] = {
    0x6F9, // 8E6 MARY2A: SFX_VO_RIC_ATTACK_A
    0x6FB, // 8E7 MARY2B: SFX_VO_RIC_ATTACK_C
    0x6FA, // 8E8 MARY2C: SFX_VO_RIC_ATTACK_B
    0x6FC, // 8E9 MARY2D: SFX_VO_RIC_ATTACK_YELL
    0x6FD, // 8EA MARY2E
    0x6FE, // 8EB MARY2F
    0x700, // 8EC MARY2G
    0x701, // 8ED MARY2Q
    0x705, // 8EE MARY2P
    0x706, // 8EF MARY2N
    0x707, // 8F0 MARY2O
    0x708, // 8F1 MARY2M
    0x702, // 8F2 MARY2R: SFX_VO_RIC_PAIN_B
    0x703, // 8F3 MARY2T: SFX_VO_RIC_PAIN_C
    0x704, // 8F4 MARY2V: SFX_VO_RIC_PAIN_D
    0x6FF, // 8F5 MARY2W: SFX_VO_RIC_DEATH
};

void PlaySfx(s32 sfx) { g_api.PlaySfx(sfx); }

void MarPlaySfx(s32 sfx) {
    if (sfx >= MARIA_SFX_FIRST && sfx <= MARIA_SFX_LAST) {
        sfx = g_Mar2SfxSlot[sfx - MARIA_SFX_FIRST];
    }
    PlaySfx(sfx);
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

s16 AllocPrimitives(PrimitiveType kind, s32 count) {
    return g_api.AllocPrimitives(kind, count);
}

void MARIA_Load();
void MarInit(s16 initParam);

// the loaded file has no BSS: clear it once per load (this flag is in the
// file's data, so every fresh load sets it again); symbols from mar2.ld
extern u8 mar2_bss_start[];
extern u8 mar2_bss_end[];
static int g_Mar2Fresh __attribute__((section(".data"))) = 1;

static void MarPsxInit(u16 initParam) {
    if (g_Mar2Fresh) {
        memset(mar2_bss_start, 0, mar2_bss_end - mar2_bss_start);
        g_Mar2Fresh = 0;
        MARIA_Load();
        g_api.CalcDealDamageMaria = g_api.func_800FD664;
        g_api.CalcPlayerDamageMaria = g_api.CalcPlayerDamage;
    }
    g_ModSubwpnIcons = g_Mar2SubwpnIcons;
    g_ModMar2HudIcons = Mar2HudIcons;
    Mar2InitItemFrames();

    // Call the actual init function
    MarInit(initParam);

    // her two slots (see Mar2RicToMaria); a swap in progress is dropped.
    // Without MOD_FLAG_MARIA_SUB2 (include/game.h: a new game, or a save
    // from before the two slots) subWeapon is her only subweapon and
    // D_80097C40 is not hers: front = that one (Doll on a new game: DRA's
    // InitStatsAndGear zeroes subWeapon), back = the Doll, as a PSP new
    // game (its InitStatsAndGear zeroes both). Any id she has no subweapon
    // for becomes the Doll.
    if (!(MOD_STATUS_FLAGS & MOD_FLAG_MARIA_SUB2)) {
        g_Mar2Pri = Mar2RicToMaria(g_Status.subWeapon, PL_W_NONE, 0);
        g_Mar2Sec = PL_W_NONE;
        MOD_STATUS_FLAGS |= MOD_FLAG_MARIA_SUB2;
    } else {
        g_Mar2Pri = Mar2RicToMaria(g_Status.D_80097C40, PL_W_NONE, 0);
        g_Mar2Sec = Mar2RicToMaria(g_Status.subWeapon, PL_W_NONE, 0);
    }
    g_Status.D_80097C40 = Mar2MariaToRic(g_Mar2Pri);
    g_Status.subWeapon = g_Mar2SecRic = Mar2MariaToRic(g_Mar2Sec);
    g_Mar2Swap = -1;
    g_Mar2HudPri = g_Mar2Pri;
    g_Mar2HudSec = g_Mar2Sec;
}

void MarMain(void);
static void MarPsxMain() {
    u32 other = g_Status.subWeapon;

    // PSP layout, her ids: subWeapon = thrown, D_80097C40 = the other one
    g_Status.subWeapon = Mar2RicToMaria(g_Status.D_80097C40, g_Mar2Pri, 0);
    g_Status.D_80097C40 = Mar2RicToMaria(other, g_Mar2Sec, 1);
    if (other != g_Mar2SecRic) {
        Mar2HudPickup();
    }
    MarMain(); // Circle swaps the two
    g_Mar2Pri = g_Status.subWeapon;
    g_Mar2Sec = g_Status.D_80097C40;
    if (g_Mar2Swap < 0) {
        g_Mar2HudPri = g_Mar2Pri;
        g_Mar2HudSec = g_Mar2Sec;
    }
    g_Status.D_80097C40 = Mar2MariaToRic(g_Mar2Pri);
    g_Status.subWeapon = g_Mar2SecRic = Mar2MariaToRic(g_Mar2Sec);
}

#include "gen/maria.h"
void MarUpdatePlayerEntities(void);
void MarGetPlayerSensor(Collider* col);
// needs to stay at the bottom for the struct to fall to 0x8013C000
// (build_mar2.sh checks it and maria_sprites at 0x8013C020)
PlayerOvl MARIA_override = {
    MarPsxMain,    MarPsxInit,    MarUpdatePlayerEntities, MarGetPlayerSensor,
    maria_sprites, maria_sprites, maria_sprites,           maria_sprites,
};
