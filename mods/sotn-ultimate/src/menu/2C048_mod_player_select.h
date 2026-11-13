// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/sel/2C048.c (at file scope, in place)
// Saturn port: the Saturn's New Game "Player select" (TITLE.PRG states
// 28/29, after name entry). Left/Right pick a character (wrapping); confirm
// slides the chosen frame left and opens a version panel at its right
// (Up/Down, confirm starts, Triangle goes back to the three characters).
// Richter: PS1 / Saturn costume (MOD_RIC_COSTUME2), Alucard: boss Richter PS1 /
// Saturn (MOD_BOSS_RIC_SATURN) and boss Maria Saturn / PSP (MOD_MARIA_BOSS_PSP:
// BIN/CENP.BIN), two rows: Up/Down change the row under the cursor, Left/Right
// move the cursor between the rows; Maria: Saturn / PSP (MOD_MARIA_PSP:
// BIN/MARIA2.BIN). The game starts through the same steps as the retail name
// entry, so Richter mode starts exactly as with the "RICHTER" name, which on a
// cleared game still skips this screen. Saturn faithful: every character is
// available from the start; define MOD_CHARSEL_NEEDS_CLEAR to offer Richter
// (and Maria) only after a clear.
//
// Art (mods/sotn-ultimate/tools/gen_title_portraits.py): the Saturn's 80x88
// portraits (VDP1 sprites, TITLE.CHR) and its VDP2 menu layer (TITLE.MAP:
// frames, "Player select" header, gothic names). VRAM x 896+, y < 256, which
// the menu never samples (F_SEL fills x 512-895; the title screen reloads its
// own graphics when it is entered again): portraits 8bpp at y 0-87, their
// CLUTs at y 88-90, the layer's three CLUTs at y 91, its art 4bpp from y 96.
#include "../../../../src/st/sel/mod_charsel_gfx.h"

#define Upd_Eng_ModCharSelect 0x13
static s32 g_ModStaffRoll;
#define MOD_CHARSEL_VRAM_X 896
#define MOD_CHARSEL_CLUT_Y MOD_CHARSEL_TEX_H
#define MOD_CHARSEL_CLUT4_Y 91
#define MOD_CHARSEL_ART_Y 96
#define MOD_CHARSEL_ART_TPAGE (MOD_CHARSEL_VRAM_X >> 6) // 4bpp, y 0
// the version panel box CLUT (ModCharselDrawPanel): y 91 after the layer's 3
#define MOD_CHARSEL_SEE_CLUT_X                                                 \
    (MOD_CHARSEL_VRAM_X + LEN(g_ModCharselClut4) * 16)
#define MOD_CHARSEL_SEE_CLUT                                                   \
    getClut(MOD_CHARSEL_SEE_CLUT_X, MOD_CHARSEL_CLUT4_Y)
// layer art in the 4bpp page (u in pixels from x 896, v from y 0)
#define MOD_CHARSEL_FRAME_U 0
#define MOD_CHARSEL_SIDE_U (MOD_CHARSEL_FRAME_W * 8) // header and names
#define MOD_CHARSEL_NAME_V (MOD_CHARSEL_ART_Y + MOD_CHARSEL_HEADER_H * 8)
// SEL's menu runs in the PS1's 384-wide mode, whose pixels are 7/8 of a
// Saturn 320-mode pixel (the 4/5 the mod uses elsewhere is for 256-wide
// screens): 80 Saturn pixels -> 91. Define 64 for the plain 4/5 narrowing.
#ifndef MOD_CHARSEL_PORTRAIT_W
#define MOD_CHARSEL_PORTRAIT_W 91
#endif
#define MOD_CHARSEL_SX(w) ((w) * MOD_CHARSEL_PORTRAIT_W / MOD_CHARSEL_TEX_W)
// The Saturn's sprite centres (72/168/264, 124) scaled the same way around
// the screen centre; y + 4 for the PS1's 240 lines against 224 (leaving
// room for the version row under the names).
#define MOD_CHARSEL_SPACING MOD_CHARSEL_SX(96)
#define MOD_CHARSEL_CENTRE_X 192
#define MOD_CHARSEL_Y(y) ((y) + 4)
#define MOD_CHARSEL_CENTRE_Y MOD_CHARSEL_Y(124)
// the layer's places on the Saturn (it sits 8 px right of the sprites):
// frame cells 3-13 x 8-22 around the portrait centre - 40, names below,
// header cells 2-19 x 4-7
#define MOD_CHARSEL_FRAME_DX MOD_CHARSEL_SX(-40)
#define MOD_CHARSEL_FRAME_Y MOD_CHARSEL_Y(64)
#define MOD_CHARSEL_NAME_Y MOD_CHARSEL_Y(184)
#define MOD_CHARSEL_HEADER_X (MOD_CHARSEL_CENTRE_X + MOD_CHARSEL_SX(24 - 168))
#define MOD_CHARSEL_HEADER_Y MOD_CHARSEL_Y(32)
#define MOD_CHARSEL_HINT_Y MOD_CHARSEL_Y(212)
// version panel: the chosen frame slides left (MOD_CHARSEL_SLIDE frames),
// the panel sits at its right, around the portrait's height
#define MOD_CHARSEL_SLIDE 8
#define MOD_CHARSEL_PANEL_FRAME_X 72

typedef struct {
    const u16* pix;
    const u16* clut;
    const u8* name; // cell map
    s32 player;
    const char* title[2]; // the version panel's title (1 or 2 lines)
    const char* opts[2];
    const char* title2; // a second row (1 line; then title is 1 line too)
    const char* opts2[2];
} ModCharsel;

// Saturn cursor order: 0 Alucard (middle), 1 Richter (left), 2 Maria (right)
const ModCharsel g_ModCharsel[] MOD_RO = {
    {g_ModCharselPix_alucard,
     g_ModCharselClut_alucard,
     g_ModCharselMap_name_alucard,
     PLAYER_ALUCARD,
     {_S("Boss Richter"), NULL},
     {_S("PS1"), _S("Saturn")},
     _S("Boss Maria"),
     {_S("Saturn"), _S("PSP")}},
    {g_ModCharselPix_richter,
     g_ModCharselClut_richter,
     g_ModCharselMap_name_richter,
     PLAYER_RICHTER,
     {_S("Costume"), NULL},
     {_S("PS1"), _S("Saturn")}},
#if defined(MOD_SATURN_MARIA)
    {g_ModCharselPix_maria,
     g_ModCharselClut_maria,
     g_ModCharselMap_name_maria,
     PLAYER_MARIA,
     {_S("Version"), NULL},
     {_S("Saturn"), _S("PSP")}},
#endif
};
// the Saturn's two Japanese lines ("please select a player") in one line
const char g_ModCharselText[] MOD_RO = _S("Select a player");

static s32 g_ModCharselIds[LEN(g_ModCharsel)]; // the offered entries
static s32 g_ModCharselCount;
static s32 g_ModCharselCursor; // kept between visits, as on the Saturn
static s32 g_ModCharselShown;  // draw the select while fading out
static s32 g_ModCharselOpen;   // version panel: 0 closed, 1..SLIDE opening
static s32 g_ModCharselOpt[LEN(g_ModCharsel)]; // version per character
static s32 g_ModCharselOpt2; // Alucard's second row (boss Maria: 1 PSP)
static s32 g_ModCharselRow;  // the panel's row under the cursor
// every panel's row before the dub: the balance (game.h MOD_FLAG_PS1_BALANCE;
// 0 = Saturn, the default, shown as the second of the "PS1" / "Saturn"
// choices, as the Boss Richter row), shared, kept between visits
static s32 g_ModCharselPs1Bal;
static const char* const g_ModCharselBalTitle MOD_RO = _S("Balance");
// the panel's rows: version, Alucard's boss Maria, the balance, the dub
#define MOD_CHARSEL_ROWS(id) (g_ModCharsel[id].title2 ? 4 : 3)
// every panel's last row: the dub (0 PSP, 1 PS1: game.h MOD_FLAG_PS1_DUB,
// 2 JP: MOD_FLAG_JP_DUB), shared by the characters, kept between visits
#define MOD_CHARSEL_DUBS 3
static s32 g_ModCharselDub;
static const char* const g_ModCharselDubTitle MOD_RO = _S("Dub");
static const char* const g_ModCharselDubOpts[MOD_CHARSEL_DUBS] MOD_RO = {
    _S("PSP"), _S("PS1"), _S("JP")};
// Maria's panel: PSP / JP only (neither of her versions was ever dubbed for
// the PS1); a PS1 choice left by another panel shows and starts as PSP
static const char* const g_ModCharselMarDubOpts[2] MOD_RO = {
    _S("PSP"), _S("JP")};
#define MOD_CHARSEL_NO_PS1_DUB(id) (g_ModCharsel[id].player == PLAYER_MARIA)

#if defined(VERSION_US)
// The JP dub's tables (include/game.h ModJpDubTable: generated by
// mods/sotn-ultimate/tools/repack_xa.py --dub-jp and gen_psp_sfx.py --set jp),
// copied into DRA's g_ModDub.jp (src/dra/47BB8.c; its address is checked by
// mods/sotn-ultimate/tools/build_disc.sh) every frame SEL's menu runs: every
// way into a game passes here, and DRA has no room for them
#include "../../../../src/st/sel/mod_jp_xa.h"
#include "../../../../src/st/sel/mod_jp_sfx.h"
#define MOD_DUB_STATE ((ModDubState*)0x8013644C)
static const ModJpDubTable g_ModJpDubTable MOD_RO = {
    MOD_JP_XA_COUNT ? MOD_JP_MAGIC : 0,
    MOD_JP_XA_BASE,
    MOD_JP_XA_DID_YOU_NOT,
    MOD_JP_SFX_PAIRS,
    MOD_JP_XA_BITS,
    MOD_JP_XA_WORDS};
// The PSP port's menu text (mods/sotn-ultimate/tools/gen_menu_text.py; DRA's
// tables stay retail). Fixed strings: copied into g_ModDub.fixedText, DRA's
// entries pointed at them. Strings that follow the Dub row: their DRA address
// (through g_api's tables; g_MenuStr at its address in DRA, checked by
// mods/sotn-ultimate/tools/check_menu_text.py) and both versions, for src/dra/
// game_handlers.c ModApplyDubText.
#include "../../../../src/st/sel/mod_dub_text.h"
#if defined(MOD_SATURN_STAGE_TEST)
// test builds (dialogue warp chain): its teleports and stop table add 0x2C
// bytes of DRA data before g_MenuStr
#define MOD_MENU_STR ((u8*)0x800A83B8)
#else
#define MOD_MENU_STR ((u8*)0x800A838C)
#endif
static u8** ModTextSlot(s32 table, s32 off) {
    u8* base;

    switch (table) {
    case 0:
        base = (u8*)g_api.equipDefs;
        break;
    case 1:
        base = (u8*)g_api.accessoryDefs;
        break;
    case 2:
        base = (u8*)g_api.enemyDefs;
        break;
    case 3:
        base = (u8*)g_api.relicDefs;
        break;
    default:
        base = MOD_MENU_STR;
        break;
    }
    return (u8**)(base + off);
}

static void ModDubTextCopy(void) {
    ModDubText* t = &MOD_DUB_STATE->text;
    u8* fixed = MOD_DUB_STATE->fixedText;
    const ModFixedTextSrc* f;
    const ModDubTextSrc* e;
    const u8* ins = g_ModFixedTextBytes;
    const u8* s;
    u8** slot;
    u8* d;
    s32 i, j, pos;

    STATIC_ASSERT(MOD_DUB_TEXT_COUNT <= MOD_DUB_TEXT_MAX, "too many strings");
    STATIC_ASSERT(MOD_DUB_TEXT_SIZE <= MOD_DUB_TEXT_BYTES, "too many bytes");
    STATIC_ASSERT(MOD_FIXED_TEXT_SIZE <= MOD_FIXED_TEXT_BYTES, "fixed text");
    for (i = 0, pos = 0; i < MOD_FIXED_TEXT_COUNT; i++) {
        f = &g_ModFixedTextSrc[i];
        slot = ModTextSlot(f->table, f->off);
        d = &fixed[pos];
        // built once: DRA (and so the pointer) stays until the console resets
        if (*slot != d) {
            s = *slot;
            if (f->del != 0xFF) {
                for (j = 0; j < f->pos; j++) {
                    *d++ = *s++;
                }
                s += f->del;
            }
            for (j = 0; j < f->ins; j++) {
                *d++ = ins[j];
            }
            if (f->del != 0xFF) {
                do {
                    *d++ = *s;
                } while (*s++);
            }
            *slot = &fixed[pos];
        }
        ins += f->ins;
        pos += f->room;
    }
    for (i = 0, pos = 0; i < MOD_DUB_TEXT_COUNT; i++) {
        e = &g_ModDubTextSrc[i];
        t->e[i].dst = *ModTextSlot(e->table & 7, e->off) + (e->table >> 3);
        t->e[i].len = e->len;
        t->e[i].src[0] = &t->bytes[pos];
        t->e[i].src[1] = &t->bytes[pos + e->len];
        pos += e->len * 2;
    }
    for (i = 0; i < MOD_DUB_TEXT_SIZE; i++) {
        t->bytes[i] = g_ModDubTextBytes[i];
    }
    t->count = MOD_DUB_TEXT_COUNT;
    t->magic = MOD_DUB_TEXT_MAGIC;
}

static void ModJpDubCopy(void) {
    const u32* s = (const u32*)&g_ModJpDubTable;
    u32* d = (u32*)&MOD_DUB_STATE->jp;
    s32 i;

    for (i = 0; i < sizeof(ModJpDubTable) / 4; i++) {
        d[i] = s[i];
    }
    ModDubTextCopy();
}
#endif

void func_801AEA8C(s32 arg0);

// Builds one 4bpp image from its cell map, a row of cells at a time.
static void ModCharselUploadCells(const u8* map, s32 w, s32 h, s32 u, s32 v) {
    u32 buf[MOD_CHARSEL_HEADER_W * 8];
    RECT rect;
    const u32* t;
    s32 x, y, r;

    rect.x = MOD_CHARSEL_VRAM_X + u / 4;
    rect.w = w * 2;
    rect.h = 8;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            t = map[y * w + x] ? &g_ModCharselTiles[(map[y * w + x] - 1) * 8]
                               : NULL;
            for (r = 0; r < 8; r++) {
                buf[r * w + x] = t ? t[r] : 0;
            }
        }
        rect.y = v + y * 8;
        LoadImage(&rect, (u_long*)buf);
        DrawSync(0);
    }
}

// The framed panels' box CLUT (MOD_CHARSEL_SEE_CLUT, ModPanelFrame): the
// menu's 0x200 (D_8018CD54) with its reds 13-15 cleared, so the box's red
// inside is see-through. Also used by the sound mode.
extern u16 D_8018CD54[];
static void ModSeeClutUpload(void) {
    u16 see[16];
    RECT rect;
    s32 i;

    for (i = 0; i < 16; i++) {
        see[i] = i < 13 ? D_8018CD54[i] : 0;
    }
    rect.x = MOD_CHARSEL_SEE_CLUT_X;
    rect.y = MOD_CHARSEL_CLUT4_Y;
    rect.w = 16;
    rect.h = 1;
    LoadImage(&rect, (u_long*)see);
    DrawSync(0);
}

// Returns false when only Alucard is offered (the screen is skipped).
static s32 ModCharselInit(void) {
    RECT rect;
    s32 i;

    g_ModCharselCount = 0;
    for (i = 0; i < LEN(g_ModCharsel); i++) {
#if defined(MOD_CHARSEL_NEEDS_CLEAR)
        if (i && g_GameClearFlag == SAVE_FLAG_NORMAL) {
            continue;
        }
#endif
        g_ModCharselIds[g_ModCharselCount++] = i;
    }
    if (g_ModCharselCount < 2) {
        return false;
    }
    if (g_ModCharselCursor >= g_ModCharselCount) {
        g_ModCharselCursor = 0;
    }
    g_ModCharselOpen = false;
    g_ModCharselRow = 0;
    for (i = 0; i < LEN(g_ModCharsel); i++) {
        rect.x = MOD_CHARSEL_VRAM_X + i * (MOD_CHARSEL_TEX_W / 2);
        rect.y = 0;
        rect.w = MOD_CHARSEL_TEX_W / 2;
        rect.h = MOD_CHARSEL_TEX_H;
        LoadImage(&rect, (u_long*)g_ModCharsel[i].pix);
        rect.x = MOD_CHARSEL_VRAM_X;
        rect.y = MOD_CHARSEL_CLUT_Y + i;
        rect.w = MOD_CHARSEL_CLUT_LEN;
        rect.h = 1;
        LoadImage(&rect, (u_long*)g_ModCharsel[i].clut);
        ModCharselUploadCells(
            g_ModCharsel[i].name, MOD_CHARSEL_NAME_RICHTER_W,
            MOD_CHARSEL_NAME_RICHTER_H, MOD_CHARSEL_SIDE_U,
            MOD_CHARSEL_NAME_V + i * MOD_CHARSEL_NAME_RICHTER_H * 8);
    }
    for (i = 0; i < LEN(g_ModCharselClut4); i++) {
        rect.x = MOD_CHARSEL_VRAM_X + i * 16;
        rect.y = MOD_CHARSEL_CLUT4_Y;
        rect.w = 16;
        rect.h = 1;
        LoadImage(&rect, (u_long*)g_ModCharselClut4[i]);
    }
    ModSeeClutUpload(); // the version panel's box
    ModCharselUploadCells(
        g_ModCharselMap_header, MOD_CHARSEL_HEADER_W, MOD_CHARSEL_HEADER_H,
        MOD_CHARSEL_SIDE_U, MOD_CHARSEL_ART_Y);
    ModCharselUploadCells(
        g_ModCharselMap_frame, MOD_CHARSEL_FRAME_W, MOD_CHARSEL_FRAME_H,
        MOD_CHARSEL_FRAME_U, MOD_CHARSEL_ART_Y);
    MenuHideAllGfx();
    return true;
}

// One textured quad, w x h on screen from a tw x th texture rectangle.
static void ModCharselQuad(s32 x, s32 y, s32 w, s32 h, s32 u, s32 v, s32 tw,
                           s32 th, u16 tpage, u16 clut, s32 c, s32 otIdx) {
    POLY_GT4* poly = &g_CurrentBuffer->polyGT4[g_GpuUsage.gt4];

    setPolyGT4(poly);
    setSemiTrans(poly, 0);
    setShadeTex(poly, 0);
    setXY4(poly, x, y, x + w, y, x, y + h, x + w, y + h);
    setUV4(poly, u, v, u + tw, v, u, v + th, u + tw, v + th);
    setRGB0(poly, c, c, c);
    setRGB1(poly, c, c, c);
    setRGB2(poly, c, c, c);
    setRGB3(poly, c, c, c);
    poly->tpage = tpage;
    poly->clut = clut;
    AddPrim(&g_CurrentBuffer->ot[otIdx], poly);
    g_GpuUsage.gt4++;
}

// a flat quad (degenerate into a triangle when x1 == x3, y1 == y3)
static void ModCharselFlat(s32 x0, s32 y0, s32 x1, s32 y1, s32 x2, s32 y2,
                           s32 x3, s32 y3, s32 c, s32 otIdx) {
    POLY_G4* poly = &g_CurrentBuffer->polyG4[g_GpuUsage.g4];

    setPolyG4(poly);
    setSemiTrans(poly, 0);
    setXY4(poly, x0, y0, x1, y1, x2, y2, x3, y3);
    setRGB0(poly, c, c, c);
    setRGB1(poly, c, c, c);
    setRGB2(poly, c, c, c);
    setRGB3(poly, c, c, c);
    AddPrim(&g_CurrentBuffer->ot[otIdx], poly);
    g_GpuUsage.g4++;
}

// colours as 0xBBGGRR
#define MOD_RGB(r, g, b) ((r) | ((g) << 8) | ((b) << 16))
#define MOD_R(c) ((c) & 0xFF)
#define MOD_G(c) (((c) >> 8) & 0xFF)
#define MOD_B(c) (((c) >> 16) & 0xFF)
void func_801B2700(s32 tpage, s32 otIdx);

// a w x h box, its colour from top to bottom; semi: 50% see-through (needs
// a draw mode with abr 0 before it in its OT slot, see ModCharselDrawPanel)
static void ModCharselBox(
    s32 x, s32 y, s32 w, s32 h, u32 top, u32 bot, s32 semi, s32 otIdx) {
    POLY_G4* poly = &g_CurrentBuffer->polyG4[g_GpuUsage.g4];

    setPolyG4(poly);
    setSemiTrans(poly, semi);
    setXY4(poly, x, y, x + w, y, x, y + h, x + w, y + h);
    setRGB0(poly, MOD_R(top), MOD_G(top), MOD_B(top));
    setRGB1(poly, MOD_R(top), MOD_G(top), MOD_B(top));
    setRGB2(poly, MOD_R(bot), MOD_G(bot), MOD_B(bot));
    setRGB3(poly, MOD_R(bot), MOD_G(bot), MOD_B(bot));
    AddPrim(&g_CurrentBuffer->ot[otIdx], poly);
    g_GpuUsage.g4++;
}

// func_801B27A8 with a vertex colour and a VRAM CLUT id (OT slot 0x20)
static void ModCharselSprt(
    s32 x, s32 y, s32 w, s32 h, s32 u, s32 v, u16 clut, s32 tpage, u32 c) {
    SPRT* sp = &g_CurrentBuffer->sprite[g_GpuUsage.sp];

    setSprt(sp);
    setXY0(sp, x, y);
    setWH(sp, w, h);
    setUV0(sp, u, v);
    setRGB0(sp, MOD_R(c), MOD_G(c), MOD_B(c));
    sp->clut = clut;
    AddPrim(&g_CurrentBuffer->ot[0x20], sp);
    g_GpuUsage.sp++;
    func_801B2700(tpage, 0x20);
}

// the width of an 8x8 font string
static s32 ModCharselLen(const u8* s) {
    s32 n = 0;

    while (*s++ != 0xFF) {
        n += 8;
    }
    return n;
}

// the 8x8 menu font in colour c with a black drop shadow. (The last prim
// added to an OT slot is drawn first: the shadows go in after the letters,
// so they are drawn under them.) The shadows are textured quads (vertex
// colour 0): the sound mode's list would overflow the 512 sprites.
static void ModCharselTextRGB(const u8* s, s32 x, s32 y, u32 c) {
    const u8* p;
    s32 k, n;

    for (k = 0; k < 2; k++) {
        for (p = s, n = 0; *p != 0xFF; p++, n += 8) {
            if (k) {
                ModCharselQuad(
                    x + n + 1, y + 1, 8, 8, (*p & 0xF) * 8, (*p & 0xF0) / 2, 8,
                    8, 0x1E, g_ClutIds[0x196], 0, 0x20);
            } else {
                ModCharselSprt(x + n, y, 8, 8, (*p & 0xF) * 8, (*p & 0xF0) / 2,
                               g_ClutIds[0x196], 0x1E, c);
            }
        }
    }
}

// the 8x8 menu font (DrawImages8x8) at brightness c
static void ModCharselText(const u8* s, s32 x, s32 y, s32 c) {
    for (; *s != 0xFF; s++, x += 8) {
        func_801B27A8(
            x, y, 8, 8, (*s & 0xF) * 8, (*s & 0xF0) / 2, 0x196, 0x1E, 0, c);
    }
}

// One character: the frame's navy inner window (the Saturn layer's colour
// 0), the portrait centred in it, the frame and the name on top. The chosen
// one's frame and name take the Saturn's cursor colours (CLUT 2).
static void ModCharselDrawChar(s32 id, s32 x, s32 sel) {
    u16 clut =
        getClut(MOD_CHARSEL_VRAM_X + (sel ? 32 : 16), MOD_CHARSEL_CLUT4_Y);
    s32 wx = x + MOD_CHARSEL_SX(MOD_CHARSEL_WIN_X);
    s32 wy = MOD_CHARSEL_FRAME_Y + MOD_CHARSEL_WIN_Y;
    s32 ww = MOD_CHARSEL_SX(MOD_CHARSEL_WIN_W);
    POLY_G4* poly = &g_CurrentBuffer->polyG4[g_GpuUsage.g4];

    setPolyG4(poly);
    setSemiTrans(poly, 0);
    setXY4(poly, wx, wy, wx + ww, wy, wx, wy + MOD_CHARSEL_WIN_H, wx + ww,
           wy + MOD_CHARSEL_WIN_H);
    setRGB0(poly, 0, 0, 0x42);
    setRGB1(poly, 0, 0, 0x42);
    setRGB2(poly, 0, 0, 0x42);
    setRGB3(poly, 0, 0, 0x42);
    AddPrim(&g_CurrentBuffer->ot[0x1E], poly);
    g_GpuUsage.g4++;

    ModCharselQuad(
        wx, wy, ww, MOD_CHARSEL_WIN_H,
        id * MOD_CHARSEL_TEX_W + (MOD_CHARSEL_TEX_W - MOD_CHARSEL_WIN_W) / 2,
        (MOD_CHARSEL_TEX_H - MOD_CHARSEL_WIN_H) / 2, MOD_CHARSEL_WIN_W,
        MOD_CHARSEL_WIN_H, 0x80 | (MOD_CHARSEL_VRAM_X >> 6),
        getClut(MOD_CHARSEL_VRAM_X, MOD_CHARSEL_CLUT_Y + id), 0x80, 0x1F);
    ModCharselQuad(
        x, MOD_CHARSEL_FRAME_Y, MOD_CHARSEL_SX(MOD_CHARSEL_FRAME_W * 8),
        MOD_CHARSEL_FRAME_H * 8, MOD_CHARSEL_FRAME_U, MOD_CHARSEL_ART_Y,
        MOD_CHARSEL_FRAME_W * 8, MOD_CHARSEL_FRAME_H * 8, MOD_CHARSEL_ART_TPAGE,
        clut, 0x80, 0x20);
    ModCharselQuad(
        x, MOD_CHARSEL_NAME_Y, MOD_CHARSEL_SX(MOD_CHARSEL_NAME_RICHTER_W * 8),
        MOD_CHARSEL_NAME_RICHTER_H * 8, MOD_CHARSEL_SIDE_U,
        MOD_CHARSEL_NAME_V + id * MOD_CHARSEL_NAME_RICHTER_H * 8,
        MOD_CHARSEL_NAME_RICHTER_W * 8, MOD_CHARSEL_NAME_RICHTER_H * 8,
        MOD_CHARSEL_ART_TPAGE, clut, 0x80, 0x20);
}

// frame left edge of offered entry i in the three-character view
static s32 ModCharselFrameX(s32 i) {
    s32 id = g_ModCharselIds[i];
    s32 x = MOD_CHARSEL_CENTRE_X;

    // the Saturn's places: Alucard in the middle, Richter left, Maria
    // right; without Maria the two are centred
    if (g_ModCharselCount == 2) {
        x += id ? -MOD_CHARSEL_SPACING / 2 : MOD_CHARSEL_SPACING / 2;
    } else if (id == 1) {
        x -= MOD_CHARSEL_SPACING;
    } else if (id == 2) {
        x += MOD_CHARSEL_SPACING;
    }
    return x + MOD_CHARSEL_FRAME_DX;
}

// The version panel ("design C"): the file menu's big gold box (F_SEL,
// tpage 0xD = VRAM x 832: u 0-255 v 120-239, only its left half is there, so
// the right half is that half mirrored; the middle rows are cut out), its red
// inside see-through through a copy of CLUT 0x200 with colours 13-15 cleared
// (MOD_CHARSEL_SEE_CLUT, built by ModCharselInit) over a 50% dark fill. Each
// row: an 8x8 title with a shadow, its choices as small boxes side by side, a
// strip of the box's chain ornament (tpage 0xC u 0 v 96, 48x6) under it.
// No cursor: the rows are answered in turn (X: the next row, Triangle: the
// one before), and a row's state shows it: 0 answered: soft gold title, its
// choice steady (blue, dark gold border); 1 current: bright gold title, its
// choice pulsing (blue fill and gold border brighten and fade); 2 to come:
// dimmed but readable.
#define MOD_CHARSEL_BOX_TPAGE 0x0D
#define MOD_CHARSEL_BOX_V 120
#define MOD_CHARSEL_BOX_HALF 91 // the panel is twice this wide
#define MOD_CHARSEL_PANEL_X 178
#define MOD_CHARSEL_PANEL_CY 139 // the panel's vertical centre
#define MOD_CHARSEL_ROW_H 41
#define MOD_CHARSEL_ROW_H4 35 // four rows (Alucard): 9 px gaps, see below
#define MOD_CHARSEL_TITLE_X (MOD_CHARSEL_PANEL_X + 18)
#define MOD_CHARSEL_TAG_X (MOD_CHARSEL_PANEL_X + 28)
#define MOD_CHARSEL_TAG_H 12
#define MOD_PANEL_SEG 56 // texture rows used at most for the box's top/bottom
#define MOD_TITLE_RGB MOD_RGB(0x9C, 0x84, 0x38) // current row title
#define MOD_SOFT_RGB MOD_RGB(0x74, 0x64, 0x3C)  // answered row title
#define MOD_DARK_GOLD MOD_RGB(164, 123, 0)      // steady choice border

// The framed panel (also the sound mode's): the box at x, y, 2 * half wide,
// h high (h > 2 * MOD_PANEL_SEG + 2 stretches a side-only band between its
// top and bottom), and its inside, navy to wine, half see-through. Box quads
// go in OT slot 0x1F: a caller drawing half see-through boxes there adds
// func_801B2700(MOD_CHARSEL_BOX_TPAGE, 0x1F) after them (abr 0).
static void ModPanelFrame(s32 x, s32 y, s32 half, s32 h) {
    POLY_GT4* poly;
    s32 t = h / 2;
    s32 b, i, qy, qh, th, v;

    if (t > MOD_PANEL_SEG) {
        t = MOD_PANEL_SEG;
    }
    b = h - 2 - t;
    if (b > MOD_PANEL_SEG) {
        b = MOD_PANEL_SEG;
    }
    // top, middle and bottom, each its left half and that half mirrored
    for (i = 0; i < 6; i++) {
        if (i < 2) {
            qy = y + 2;
            qh = th = t;
            v = MOD_CHARSEL_BOX_V;
        } else if (i < 4) {
            qy = y + 2 + t;
            qh = h - 2 - t - b;
            // the Player Select's short band: 8 rows stretched; a taller one
            // (the sound mode's) repeats rows 190-192 of the side rail, three
            // identical rows (no flecks), so its lines run unbroken, 1:1 wide
            th = qh > 24 ? 3 : 8;
            v = qh > 24 ? 190 : MOD_CHARSEL_BOX_V + MOD_PANEL_SEG - 8;
        } else {
            qy = y + h - b;
            qh = th = b;
            v = 240 - b;
        }
        if (qh <= 0) {
            continue;
        }
        ModCharselQuad(x + (i & 1) * half, qy, half, qh, 0, v, half, th,
                       MOD_CHARSEL_BOX_TPAGE, MOD_CHARSEL_SEE_CLUT, 0x80, 0x1F);
        if (i & 1) {
            poly = &g_CurrentBuffer->polyGT4[g_GpuUsage.gt4 - 1];
            poly->u0 = poly->u2 = half;
            poly->u1 = poly->u3 = 0;
        }
    }
    ModCharselBox(x + 12, y + 8, half * 2 - 24, h - 16, MOD_RGB(0, 0, 40),
                  MOD_RGB(40, 0, 24), 1, 0x1E);
    func_801B2700(MOD_CHARSEL_BOX_TPAGE, 0x1E); // abr 0 (drawn before it)
}

// a strip of the box's chain ornament (tpage 0xC u 0 v 96, 48x6), w wide
static void ModPanelChain(s32 x, s32 y, s32 w, s32 c) {
    s32 i;

    for (i = 0; i < w; i += 48) {
        ModCharselSprt(x + i, y, w - i < 48 ? w - i : 48, 6, 0, 96,
                       MOD_CHARSEL_SEE_CLUT, 0x0C, MOD_RGB(c, c, c));
    }
}

// the chosen-box pulse: 0..31..0 over 64 frames
static s32 ModPulse(void) {
    s32 p = g_Timer & 0x1F;

    return g_Timer & 0x20 ? 0x1F - p : p;
}

// a box with a 1 px border (OT slot 0x1F)
static void ModFramedBox(
    s32 x, s32 y, s32 w, s32 h, u32 top, u32 bot, s32 semi, u32 border) {
    ModCharselBox(x + 1, y + 1, w - 2, h - 2, top, bot, semi, 0x1F);
    ModCharselBox(x + 1, y, w - 2, 1, border, border, 0, 0x1F);
    ModCharselBox(x + 1, y + h - 1, w - 2, 1, border, border, 0, 0x1F);
    ModCharselBox(x, y + 1, 1, h - 2, border, border, 0, 0x1F);
    ModCharselBox(x + w - 1, y + 1, 1, h - 2, border, border, 0, 0x1F);
}

// the current choice's pulsing box (p: ModPulse)
static void ModPulseBox(s32 x, s32 y, s32 w, s32 h, s32 p) {
    ModFramedBox(x, y, w, h, MOD_RGB(32 + p, 49 + p, 115 + p),
                 MOD_RGB(98 + p * 2, 115 + p * 2, 180 + p * 2), 0,
                 MOD_RGB(160 + p * 3, 128 + p * 3, p * 2));
}

// one choice box: fill, 1 px border, its word centred
static void ModCharselTag(const u8* s, s32 x, s32 y, s32 w, u32 top, u32 bot,
                          s32 semi, u32 border, u32 text) {
    ModCharselTextRGB(s, x + (w - ModCharselLen(s)) / 2, y + 2, text);
    ModFramedBox(x, y, w, MOD_CHARSEL_TAG_H, top, bot, semi, border);
}

static s32 g_ModCharselBoxDy; // a row's choices below its title

static void ModCharselDrawRow(const char* title, const char* const* opts,
                              s32 nopts, s32 opt, s32 y, s32 state) {
    const u8* s;
    s32 i, w, p, c, x;

    ModCharselTextRGB((const u8*)title, MOD_CHARSEL_TITLE_X, y,
                      state == 1   ? MOD_TITLE_RGB
                      : state == 0 ? MOD_SOFT_RGB
                                   : MOD_RGB(0x5C, 0x56, 0x4E));
    p = ModPulse();
    w = nopts > 2 ? 40 : 60;
    y += g_ModCharselBoxDy;
    for (i = 0; i < nopts; i++) {
        s = (const u8*)opts[i];
        x = MOD_CHARSEL_TAG_X + i * (w + 6);
        if (i != opt) {
            c = state == 2 ? 0x48 : state ? 0x70 : 0x58;
            ModCharselTag(
                s, x, y, w, 0, 0, 1,
                state == 2 ? MOD_RGB(60, 50, 50) : MOD_RGB(90, 90, 90),
                MOD_RGB(c, c, c));
        } else if (state == 1) {
            c = 0x90 + p;
            ModCharselTextRGB(
                s, x + (w - ModCharselLen(s)) / 2, y + 2, MOD_RGB(c, c, c));
            ModPulseBox(x, y, w, MOD_CHARSEL_TAG_H, p);
        } else if (state == 0) {
            ModCharselTag(
                s, x, y, w, MOD_RGB(32, 49, 115), MOD_RGB(98, 115, 180), 0,
                MOD_DARK_GOLD, MOD_RGB(0x80, 0x80, 0x80));
        } else {
            ModCharselTag(s, x, y, w, MOD_RGB(16, 24, 58), MOD_RGB(49, 57, 90),
                          0, MOD_RGB(90, 70, 20), MOD_RGB(0x60, 0x60, 0x60));
        }
    }
}

// a panel row's ModCharselDrawRow state: answered, current, to come
static s32 ModCharselRowState(s32 row) {
    return row < g_ModCharselRow ? 0 : row == g_ModCharselRow ? 1 : 2;
}

// the panel and its rows (version, Alucard's boss Maria, the balance, the
// dub); four rows sit closer (title, choices 11 below it, the chain right
// under them) so the panel stays between the header text and the hint bar,
// with the same 9 px (seen) above each title, under the frame's top border
// or under a chain, and below the last choices
static void ModCharselDrawPanel(s32 id) {
    s32 n = MOD_CHARSEL_ROWS(id);
    s32 r = n == 4 ? MOD_CHARSEL_ROW_H4 : MOD_CHARSEL_ROW_H;
    s32 h = n == 4 ? 45 + r * 3 : 15 + r * n;
    s32 x = MOD_CHARSEL_PANEL_X;
    s32 y = MOD_CHARSEL_PANEL_CY - h / 2 + (n == 4);
    s32 k;

    ModPanelFrame(x, y, MOD_CHARSEL_BOX_HALF, h);
    y += n == 4 ? 11 : 12;
    g_ModCharselBoxDy = n == 4 ? 11 : 13;
    ModCharselDrawRow(g_ModCharsel[id].title[0], g_ModCharsel[id].opts, 2,
                      g_ModCharselOpt[id], y, ModCharselRowState(0));
    if (n == 4) {
        ModCharselDrawRow(g_ModCharsel[id].title2, g_ModCharsel[id].opts2, 2,
                          g_ModCharselOpt2, y + r, ModCharselRowState(1));
    }
    ModCharselDrawRow(
        g_ModCharselBalTitle, g_ModCharsel[0].opts, 2, !g_ModCharselPs1Bal,
        y + (n - 2) * r, ModCharselRowState(n - 2));
    if (MOD_CHARSEL_NO_PS1_DUB(id)) {
        ModCharselDrawRow(
            g_ModCharselDubTitle, g_ModCharselMarDubOpts, 2,
            g_ModCharselDub == 2, y + (n - 1) * r, ModCharselRowState(n - 1));
    } else {
        ModCharselDrawRow(
            g_ModCharselDubTitle, g_ModCharselDubOpts, MOD_CHARSEL_DUBS,
            g_ModCharselDub, y + (n - 1) * r, ModCharselRowState(n - 1));
    }
    // the chain strips between the rows, bright under the answered ones
    for (k = 0; k < n - 1; k++) {
        ModPanelChain(x + 14, y + (n == 4 ? 23 : 30) + k * r, 154,
                      k < g_ModCharselRow ? 0x80 : 0x60);
    }
    // the tags' half see-through fills (slot 0x1F) need abr 0 too
    func_801B2700(MOD_CHARSEL_BOX_TPAGE, 0x1F);
}

static void ModCharselDraw(void) {
    s32 i, id, x, y, u;
    const u8* s;

    ModCharselQuad(
        MOD_CHARSEL_HEADER_X, MOD_CHARSEL_HEADER_Y,
        MOD_CHARSEL_SX(MOD_CHARSEL_HEADER_W * 8), MOD_CHARSEL_HEADER_H * 8,
        MOD_CHARSEL_SIDE_U, MOD_CHARSEL_ART_Y, MOD_CHARSEL_HEADER_W * 8,
        MOD_CHARSEL_HEADER_H * 8, MOD_CHARSEL_ART_TPAGE,
        getClut(MOD_CHARSEL_VRAM_X, MOD_CHARSEL_CLUT4_Y), 0x80, 0x20);
    ModCharselText(
        g_ModCharselText,
        MOD_CHARSEL_HEADER_X + MOD_CHARSEL_SX(MOD_CHARSEL_HEADER_W * 8) + 8,
        MOD_CHARSEL_HEADER_Y + 20, 0x80);

    if (!g_ModCharselOpen) {
        for (i = 0; i < g_ModCharselCount; i++) {
            ModCharselDrawChar(g_ModCharselIds[i], ModCharselFrameX(i),
                               i == g_ModCharselCursor);
        }
    } else {
        // the chosen one slides to the left, its version panel at the right
        id = g_ModCharselIds[g_ModCharselCursor];
        x = ModCharselFrameX(g_ModCharselCursor);
        x += (MOD_CHARSEL_PANEL_FRAME_X - x) * g_ModCharselOpen /
             MOD_CHARSEL_SLIDE;
        ModCharselDrawChar(id, x, true);
        if (g_ModCharselOpen == MOD_CHARSEL_SLIDE) {
            ModCharselDrawPanel(id);
        }
    }

    // hint bar, as the Saturn's (D-pad select, A/C confirm, B cancel) with
    // the PS1 menus' own button icons and words
    x = MOD_CHARSEL_CENTRE_X - (3 * (16 + 4 + 48) + 2 * 16) / 2;
    y = MOD_CHARSEL_HINT_Y;
    func_801B27A8(x, y, 16, 16, 0xA0, 0x50, 0x200, 0xC, 1, 0x80);
    DrawImages8x8(STR_SELECT, x + 20, y + 4, 1);
    x += 16 + 4 + 48 + 16;
    func_801B27A8(x, y, 16, 16, 0xA0, 0x70, 0x200, 0xC, 1, 0x80);
    DrawImages8x8(STR_DECIDE, x + 20, y + 4, 1);
    x += 16 + 4 + 48 + 16;
    func_801B27A8(x, y, 16, 16, 0xA0, 0x60, 0x202, 0xC, 1, 0x80);
    DrawImages8x8(STR_CANCEL, x + 20, y + 4, 1);
}

// the retail name entry's last steps (Upd_Eng_17), for the chosen character
static void ModCharselStartGame(void) {
    if (g_PlayableCharacter == PLAYER_ALUCARD) {
        g_StageId = STAGE_ST0;
    } else {
        g_StageId = STAGE_NO3;
    }
    D_8003C730 = 0;
    D_8006C378 = -1;
    if (D_801BAF10) {
        D_80097924 = -1;
    } else {
        D_80097924 = D_801D6B04 / 15;
    }
    g_api.PlaySfx(SET_UNK_80);
    g_GameEngineStep = Upd_Eng_18;
}

static void ModCharselUpdate(void) {
    s32 n = g_ModCharselCount;
    s32 id = g_ModCharselIds[g_ModCharselCursor];
    s32 opt;

    if (g_ModCharselOpen) {
        if (g_ModCharselOpen < MOD_CHARSEL_SLIDE) {
            g_ModCharselOpen++;
        } else if (
            g_pads[0].tapped & (PAD_UP | PAD_DOWN | PAD_LEFT | PAD_RIGHT)) {
            // the current row's choice (two each: any direction swaps;
            // the dub's three: Right / Down the next, Left / Up the one
            // before, wrapping); the rows: version, Alucard's boss Maria,
            // the balance,
            // the dub (the last)
            g_api.PlaySfx(SFX_UI_MOVE);
            if (g_ModCharselRow == MOD_CHARSEL_ROWS(id) - 1 &&
                MOD_CHARSEL_NO_PS1_DUB(id)) {
                // Maria's two: any direction swaps PSP / JP
                g_ModCharselDub = g_ModCharselDub == 2 ? 0 : 2;
            } else if (g_ModCharselRow == MOD_CHARSEL_ROWS(id) - 1) {
                g_ModCharselDub += (g_pads[0].tapped & (PAD_RIGHT | PAD_DOWN))
                                       ? 1
                                       : MOD_CHARSEL_DUBS - 1;
                g_ModCharselDub %= MOD_CHARSEL_DUBS;
            } else if (g_ModCharselRow == MOD_CHARSEL_ROWS(id) - 2) {
                g_ModCharselPs1Bal ^= 1;
            } else if (g_ModCharselRow) {
                g_ModCharselOpt2 ^= 1;
            } else {
                g_ModCharselOpt[id] ^= 1;
            }
        } else if (
            (g_pads[0].tapped & PAD_CROSS) && !(g_pads[0].tapped & PAD_START) &&
            g_ModCharselRow < MOD_CHARSEL_ROWS(id) - 1) {
            // X answers the row: on to the next one (the last one starts)
            g_api.PlaySfx(SFX_UI_CONFIRM);
            g_ModCharselRow++;
        } else if (g_pads[0].tapped & (PAD_CROSS | PAD_START)) {
            // X on the last row, or Start on any: the game, as shown
            g_api.PlaySfx(SFX_UI_CONFIRM);
            opt = g_ModCharselOpt[id];
            g_PlayableCharacter = g_ModCharsel[id].player;
            MOD_STATUS_FLAGS = 0;
            if (g_PlayableCharacter == PLAYER_RICHTER) {
                g_Status.D_80097C40 = opt ? MOD_RIC_COSTUME2_MAGIC : 0;
            } else if (g_PlayableCharacter == PLAYER_ALUCARD) {
                MOD_STATUS_FLAGS = opt ? MOD_FLAG_BOSS_RIC_SATURN : 0;
                if (g_ModCharselOpt2) {
                    MOD_STATUS_FLAGS |= MOD_FLAG_MARIA_BOSS_PSP;
                }
            } else {
                MOD_STATUS_FLAGS = opt ? MOD_FLAG_MARIA_PSP : 0;
                // the PSP Maria's other subweapon slot
                // (mods/sotn-ultimate/src/maria_psp/ mar2.c; 0 = her Doll),
                // maybe left by an earlier game
                g_Status.D_80097C40 = 0;
            }
            if (g_ModCharselPs1Bal) {
                MOD_STATUS_FLAGS |= MOD_FLAG_PS1_BALANCE;
            }
            if (g_ModCharselDub == 1 && !MOD_CHARSEL_NO_PS1_DUB(id)) {
                MOD_STATUS_FLAGS |= MOD_FLAG_PS1_DUB;
            } else if (g_ModCharselDub == 2) {
                MOD_STATUS_FLAGS |= MOD_FLAG_JP_DUB;
            }
            g_ModCharselShown = true;
            ModCharselStartGame();
        }
        if (g_ModCharselOpen && g_GameEngineStep == Upd_Eng_ModCharSelect &&
            (g_pads[0].tapped & PAD_TRIANGLE)) {
            // Triangle: back to the row before; on the first, the panel
            // closes (back to the characters)
            g_api.PlaySfx(SFX_UI_ERROR);
            if (g_ModCharselRow && g_ModCharselOpen == MOD_CHARSEL_SLIDE) {
                g_ModCharselRow--;
            } else {
                g_ModCharselOpen = 0;
                g_ModCharselRow = 0;
            }
        }
        ModCharselDraw();
        return;
    }
    // Saturn: Left = cursor + 1, Right = cursor - 1 (mod 3), sfx 0x67B
    if (g_pads[0].tapped & PAD_LEFT) {
        g_api.PlaySfx(SFX_UI_MOVE);
        g_ModCharselCursor = (g_ModCharselCursor + 1) % n;
    }
    if (g_pads[0].tapped & PAD_RIGHT) {
        g_api.PlaySfx(SFX_UI_MOVE);
        g_ModCharselCursor = (g_ModCharselCursor + n - 1) % n;
    }
    if (g_pads[0].tapped & (PAD_CROSS | PAD_START)) {
        g_api.PlaySfx(SFX_UI_CONFIRM);
        g_ModCharselOpen = 1;
    } else if (g_pads[0].tapped & PAD_TRIANGLE) {
        // Saturn: cancel goes back to the name entry (sfx 0x686)
        g_api.PlaySfx(SFX_UI_ERROR);
        func_801AEA8C(0);
        g_GameEngineStep = Upd_Eng_17;
    }
    ModCharselDraw();
}

// Saturn port: the Saturn's sound mode (TITLE.PRG entry 3, game state 7,
// 0x060AB1E0), reached there from the PRESS START menu (Game Start / Sound
// Mode / Stereo-Mono, no unlock; its sound-effect list, 499 ids from 0x33E,
// needs a clear, g_GameClearFlag). Its 43-track playlist (0x060B2CF4,
// {music id, length}) in the same order, each Saturn SDxx.PCM as its PS1 XA
// track (chroma-matched) or a mod one (src/dra/mod_xa_music.h). The names:
// the Saturn draws a title sprite per track; these are the OST's (the 2018
// LP keeps this order; v1.2: No.35 and No.38-41 renamed after a player's
// report, checked by ear: Chaconne C Moll, Beginning 1/2, Bloody Tears 1/2).
// v1.2: a 44th, the fairy's Nocturne in Japanese after the English one
// (MOD_SOUND_NOCTURNE_JP).
// Up/Down pick (Left/Right: a page, L1/R1: 10 tracks), X plays the track or
// stops the one playing, Triangle goes back to the menu (and its music).
#include "../../../../src/dra/mod_xa_music.h"
typedef struct {
    const char* name;
    u16 id;
} ModSoundTrack;
// MU_NOCTURNE plays the PSP's English recording with the PSP dub and the
// PS1's Japanese one otherwise (src/dra/sound.c ModXaConfig, which reads the
// dub bits when the track starts): the menu clears them (English), and this
// id sets the PS1 dub bit while it plays
#define MOD_SOUND_NOCTURNE_JP (MU_NOCTURNE | 0x8000)
static const ModSoundTrack g_ModSoundTracks[] MOD_RO = {
    {_S("Metamorphosis"), MU_METAMORPHOSIS},                   // SD2E
    {_S("Prologue"), MU_PROLOGUE},                             // SD19
    {_S("Dance of Illusions"), MU_DANCE_OF_ILLUSIONS},         // SD18
    {_S("Moonlight Nocturne"), MU_MOONLIGHT_NOCTURNE},         // SD2F
    {_S("Prayer"), MU_PRAYER},                                 // SD15
    {_S("Dracula's Castle"), MU_DRACULAS_CASTLE},              // SD0A
    {_S("Dance of Gold"), MU_DANCE_OF_GOLD},                   // SD04
    {_S("Marble Gallery"), MU_MARBLE_GALLERY},                 // SD09
    {_S("Tower of Mist"), MU_TOWER_OF_MIST},                   // SD0C
    {_S("Nocturne"), MU_NOCTURNE},                             // SD20 (PSP)
    {_S("Nocturne (JP)"), MOD_SOUND_NOCTURNE_JP},              // SD20
    {_S("Wood Carving Partita"), MU_WOOD_CARVING_PARTITA},     // SD06
    {_S("Door of Holy Spirits"), MU_DOOR_OF_HOLY_SPIRITS},     // SD11
    {_S("Festival of Servants"), MU_FESTIVAL_OF_SERVANTS},     // SD0F
    {_S("Land of Benediction"), MU_LAND_OF_BENEDICTION},       // SD1F
    {_S("Requiem for the Gods"), MU_REQUIEM_FOR_THE_GODS},     // SD01
    {_S("Crystal Teardrops"), MU_CRYSTAL_TEARDROPS},           // SD07
    {_S("Abandoned Pit"), MU_ABANDONED_PIT},                   // SD03
    {_S("Rainbow Cemetery"), MU_RAINBOW_CEMETERY},             // SD05
    {_S("Silence"), MU_SILENCE},                               // SD1E
    {_S("Lost Painting"), MU_LOST_PAINTING},                   // SD10
    {_S("Dance of Pales"), MU_DANCE_OF_PALES},                 // SD02
    {_S("Curse Zone"), MU_CURSE_ZONE},                         // SD12
    {_S("Enchanted Banquet"), MU_ENCHANTED_BANQUET},           // SD1A
    {_S("Wandering Ghosts"), MU_WANDERING_GHOSTS},             // SD0D
    {_S("The Tragic Prince"), MU_THE_TRAGIC_PRINCE},           // SD0B
    {_S("The Door to the Abyss"), MU_THE_DOOR_TO_THE_ABYSS},   // SD0E
    {_S("Heavenly Doorway"), MU_HEAVENLY_DOORWAY},             // SD08
    {_S("Death Ballad"), MU_DEATH_BALLAD},                     // SD14
    {_S("Blood Relations"), MU_BLOOD_RELATIONS},               // SD16
    {_S("Metamorphosis II"), MU_METAMORPHOSIS_II},             // SD30
    {_S("Final Toccata"), MU_FINAL_TOCATTA},                   // SD17
    {_S("Black Banquet"), MU_BLACK_BANQUET},                   // SD13
    {_S("Metamorphosis III"), MU_METAMORPHOSIS_III},           // SD31
    {_S("The Old Librarian"), MU_MOD_OLD_LIBRARIAN},           // SD24
    {_S("Chaconne C Moll"), MU_MOD_SOUND_2D},                  // SD2D
    {_S("Vampire Killer Remix 1"), MU_MOD_UNDERGROUND_GARDEN}, // SD25
    {_S("Vampire Killer Remix 2"), MU_MOD_SOUL_PRISON},        // SD2C
    {_S("Beginning Remix 1"), MU_MOD_HELL_GARDEN},             // SD29
    {_S("Beginning Remix 2"), MU_MOD_SOUND_28},                // SD28
    {_S("Bloody Tears Remix 1"), MU_MOD_RICHTER_ENTRANCE},     // SD2A
    {_S("Bloody Tears Remix 2"), MU_MOD_SKELETON_LEADER},      // SD2B
    {_S("Maria Battle"), MU_MOD_MARIA_BOSS},                   // SD23
    {_S("I Am the Wind"), MU_STAFF_CREDITS},                   // SD1D
};
const char g_ModSoundText[] MOD_RO = _S("Now playing");
const char g_ModSoundHint[] MOD_RO = _S("Play/Stop");
const char g_ModSoundBack[] MOD_RO = _S("Back");
#define MOD_SOUND_N LEN(g_ModSoundTracks)
#define MOD_SOUND_ROWS 11
#define MOD_SOUND_JUMP 10 // L1 / R1: 10 tracks back / forward
// the list in the Player Select's framed panel (ModPanelFrame), screen wide
#define MOD_SOUND_PANEL_X 24
#define MOD_SOUND_PANEL_Y 8
#define MOD_SOUND_PANEL_HALF 168
#define MOD_SOUND_PANEL_H 202
#define MOD_SOUND_IN_X (MOD_SOUND_PANEL_X + 16) // inside: boxes and chains
#define MOD_SOUND_IN_W (MOD_SOUND_PANEL_HALF * 2 - 32)
#define MOD_SOUND_X (MOD_SOUND_IN_X + 8) // the list's number column
#define MOD_SOUND_Y 49                   // row 01: 4 px under the upper chain
#define MOD_SOUND_ROW_H 12
#define MOD_SOUND_GOLD MOD_RGB(0xA8, 0x90, 0x40) // the current row's text
static s32 g_ModSoundCursor;  // kept between visits, as on the Saturn
static s32 g_ModSoundTop;     // the first row shown
static s32 g_ModSoundPlaying; // the track playing + 1, 0 none
// the track to start once the music has stopped + 1, 0 none: sound.c ignores
// a request for the id already playing, so going between the two Nocturnes
// (one id) stops the music first
static s32 g_ModSoundPending;

void func_801AE9A8(void);

static void ModSoundInit(void) {
    MenuHideAllGfx();
    ModSeeClutUpload(); // the panel's box
    g_ModSoundPlaying = 0;
    g_ModSoundPending = 0;
}

static void ModSoundStart(s32 k) {
    u16 id = g_ModSoundTracks[k].id;

    MOD_STATUS_FLAGS &= ~(MOD_FLAG_PS1_DUB | MOD_FLAG_JP_DUB);
    if (id == MOD_SOUND_NOCTURNE_JP) {
        MOD_STATUS_FLAGS |= MOD_FLAG_PS1_DUB;
    }
    // sound.c fades out the one playing first
    g_api.PlaySfx(id & 0x7FFF);
}

static void ModSoundNumber(s32 n, s32 x, s32 y, u32 c) {
    u8 s[3];

    s[0] = 0x10 + n / 10; // '0' - 0x20, the 8x8 font's encoding
    s[1] = 0x10 + n % 10;
    s[2] = 0xFF;
    ModCharselTextRGB(s, x, y, c);
}

// the playing track's mark: a small gold bar left of its number
static void ModSoundMark(s32 y) {
    ModCharselBox(MOD_SOUND_IN_X + 3, y + 1, 3, 6, MOD_RGB(255, 213, 24),
                  MOD_DARK_GOLD, 0, 0x20);
}

// The list in the Player Select's framed panel: the gothic title, a chain,
// the rows, a chain and the track playing. The current row: gold text in
// the pulsing box of the panel's current choice; the playing one: soft gold
// text in a dark box with a steady gold border, and a gold mark.
static void ModSoundDraw(void) {
    s32 i, k, x, y, p;
    const u8* s;

    ModPanelFrame(MOD_SOUND_PANEL_X, MOD_SOUND_PANEL_Y, MOD_SOUND_PANEL_HALF,
                  MOD_SOUND_PANEL_H);
    ModGothic(
        "sound mode", MOD_CHARSEL_CENTRE_X - 80, MOD_SOUND_PANEL_Y + 10, 0x200);
    ModPanelChain(MOD_SOUND_IN_X, MOD_SOUND_Y - 12, MOD_SOUND_IN_W, 0x80);
    p = ModPulse();
    for (i = 0; i < MOD_SOUND_ROWS; i++) {
        k = g_ModSoundTop + i;
        y = MOD_SOUND_Y + i * MOD_SOUND_ROW_H;
        s = (const u8*)g_ModSoundTracks[k].name;
        if (k + 1 == g_ModSoundPlaying) {
            ModSoundMark(y);
        }
        if (k == g_ModSoundCursor) {
            ModSoundNumber(k + 1, MOD_SOUND_X, y, MOD_SOUND_GOLD);
            ModCharselTextRGB(s, MOD_SOUND_X + 24, y, MOD_SOUND_GOLD);
            ModPulseBox(
                MOD_SOUND_IN_X, y - 2, MOD_SOUND_IN_W, MOD_SOUND_ROW_H, p);
        } else if (k + 1 == g_ModSoundPlaying) {
            ModSoundNumber(k + 1, MOD_SOUND_X, y, MOD_TITLE_RGB);
            ModCharselTextRGB(s, MOD_SOUND_X + 24, y, MOD_TITLE_RGB);
            // see-through dark: half the panel behind it plus a little navy
            // (the panel there is darker than behind the Player Select's
            // unchosen boxes, so plain 50% black looked nearly black)
            ModFramedBox(
                MOD_SOUND_IN_X, y - 2, MOD_SOUND_IN_W, MOD_SOUND_ROW_H,
                MOD_RGB(14, 16, 36), MOD_RGB(14, 16, 36), 1, MOD_DARK_GOLD);
        } else {
            ModSoundNumber(k + 1, MOD_SOUND_X, y, MOD_SOFT_RGB);
            ModCharselTextRGB(
                s, MOD_SOUND_X + 24, y, MOD_RGB(0x78, 0x78, 0x78));
        }
    }
    // the playing row's half see-through box (slot 0x1F) needs abr 0
    func_801B2700(MOD_CHARSEL_BOX_TPAGE, 0x1F);
    // more above / below: small arrows at the list's right
    x = MOD_SOUND_IN_X + MOD_SOUND_IN_W - 12;
    if (g_ModSoundTop > 0) {
        y = MOD_SOUND_Y;
        ModCharselFlat(
            x, y + 5, x + 4, y + 1, x + 8, y + 5, x + 4, y + 1, 0x80, 0x20);
    }
    if (g_ModSoundTop + MOD_SOUND_ROWS < MOD_SOUND_N) {
        y = MOD_SOUND_Y + (MOD_SOUND_ROWS - 1) * MOD_SOUND_ROW_H;
        ModCharselFlat(
            x, y + 2, x + 4, y + 6, x + 8, y + 2, x + 4, y + 6, 0x80, 0x20);
    }
    y = MOD_SOUND_Y + MOD_SOUND_ROWS * MOD_SOUND_ROW_H + 1;
    ModPanelChain(MOD_SOUND_IN_X, y, MOD_SOUND_IN_W, 0x80);
    if (g_ModSoundPlaying) {
        y += 10;
        ModSoundMark(y);
        ModCharselTextRGB(
            (const u8*)g_ModSoundText, MOD_SOUND_X, y, MOD_TITLE_RGB);
        ModCharselTextRGB(
            (const u8*)g_ModSoundTracks[g_ModSoundPlaying - 1].name,
            MOD_SOUND_X + 12 * 8, y, MOD_RGB(0x80, 0x80, 0x80));
    }

    // hint bar, as the Player Select's
    x = MOD_CHARSEL_CENTRE_X -
        (16 + 4 + 48 + 16 + 16 + 4 + 72 + 16 + 16 + 4 + 32) / 2;
    y = MOD_CHARSEL_HINT_Y;
    func_801B27A8(x, y, 16, 16, 0xA0, 0x50, 0x200, 0xC, 1, 0x80);
    DrawImages8x8(STR_SELECT, x + 20, y + 4, 1);
    x += 16 + 4 + 48 + 16;
    func_801B27A8(x, y, 16, 16, 0xA0, 0x70, 0x200, 0xC, 1, 0x80);
    DrawImages8x8((u8*)g_ModSoundHint, x + 20, y + 4, 1);
    x += 16 + 4 + 72 + 16;
    func_801B27A8(x, y, 16, 16, 0xA0, 0x60, 0x202, 0xC, 1, 0x80);
    DrawImages8x8((u8*)g_ModSoundBack, x + 20, y + 4, 1);
}

static void ModSoundMove(s32 d) {
    s32 k = g_ModSoundCursor + d;

    if (d == 1 || d == -1) {
        k = (k + MOD_SOUND_N) % MOD_SOUND_N; // the ends wrap
    } else if (k < 0) {
        k = 0;
    } else if (k >= MOD_SOUND_N) {
        k = MOD_SOUND_N - 1;
    }
    if (k != g_ModSoundCursor) {
        g_api.PlaySfx(SFX_UI_MOVE);
        g_ModSoundCursor = k;
    }
}

static void ModSoundUpdate(void) {
    if (g_pads[0].tapped & PAD_UP) {
        ModSoundMove(-1);
    } else if (g_pads[0].tapped & PAD_DOWN) {
        ModSoundMove(1);
    } else if (g_pads[0].tapped & PAD_LEFT) {
        ModSoundMove(-MOD_SOUND_ROWS);
    } else if (g_pads[0].tapped & PAD_RIGHT) {
        ModSoundMove(MOD_SOUND_ROWS);
    } else if (g_pads[0].tapped & PAD_L1) {
        ModSoundMove(-MOD_SOUND_JUMP);
    } else if (g_pads[0].tapped & PAD_R1) {
        ModSoundMove(MOD_SOUND_JUMP);
    }
    if (g_ModSoundCursor < g_ModSoundTop) {
        g_ModSoundTop = g_ModSoundCursor;
    } else if (g_ModSoundCursor >= g_ModSoundTop + MOD_SOUND_ROWS) {
        g_ModSoundTop = g_ModSoundCursor - MOD_SOUND_ROWS + 1;
    }

    if (g_ModSoundPending && !g_api.func_80131F68()) {
        ModSoundStart(g_ModSoundPending - 1);
        g_ModSoundPending = 0;
    }
    if (g_pads[0].tapped & PAD_CROSS) {
        g_ModSoundPending = 0;
        if (g_ModSoundPlaying == g_ModSoundCursor + 1) {
            g_api.PlaySfx(SET_STOP_MUSIC);
            g_ModSoundPlaying = 0;
        } else {
            if (g_ModSoundPlaying &&
                ((g_ModSoundTracks[g_ModSoundPlaying - 1].id ^
                  g_ModSoundTracks[g_ModSoundCursor].id) &
                 0x7FFF) == 0) {
                g_api.PlaySfx(SET_STOP_MUSIC);
                g_ModSoundPending = g_ModSoundCursor + 1;
            } else {
                ModSoundStart(g_ModSoundCursor);
            }
            g_ModSoundPlaying = g_ModSoundCursor + 1;
        }
    } else if (g_pads[0].tapped & PAD_TRIANGLE) {
        g_ModSoundPending = 0;
        g_api.PlaySfx(SFX_UI_ERROR);
        // back to the menu and its music (unless that is what plays)
        if (g_ModSoundPlaying == 0 ||
            g_ModSoundTracks[g_ModSoundPlaying - 1].id != MU_PRAYER) {
            g_api.PlaySfx(MU_PRAYER);
        }
        MOD_STATUS_FLAGS &= ~(MOD_FLAG_PS1_DUB | MOD_FLAG_JP_DUB);
        func_801AE9A8();
        func_801AD490();
        g_GameEngineStep = Upd_Eng_MainMenuIdle;
        return;
    }
    ModSoundDraw();
}
