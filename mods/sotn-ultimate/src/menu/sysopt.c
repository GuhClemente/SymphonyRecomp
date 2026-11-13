// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port mod: the pause menu System screen's "Extra settings" row
// (Alucard games). X opens a popup to its right, in the retail Sound box's
// window (MENU_DG_CFG_SOUND, resized while it is ours), with the Player
// Select's four Alucard options (Boss Richter, Boss Maria, Balance, Dub):
// Up / Down pick one, Left / Right (or X) change it (the flags byte,
// include/game.h MOD_STATUS_FLAGS), Triangle / Circle close it. A boss
// option is greyed and locked while that boss is alive. While the box is up
// (opening, open, closing) it owns the pad: the System list sees no input.
//
// DRA has no room for this: it follows Alucard's sprites in BIN/ARC_F2.BIN
// (retail ARC_F.BIN + this code at file offset 0x31BF0 = MOD_SYSOPT_ADDR
// 0x8016DBF0; a new file at the disc's end, loaded by src/dra/cd.c
// g_LbaBinAlucard in Alucard games only, up to the familiar overlay at
// 0x80170000). Built against the built DRA's addresses by
// mods/sotn-ultimate/tools/build_sysopt.sh (at most 0x34000 - 0x31BF0 = 9232
// bytes).
//
// DRA calls in through three stubs in its data (src/config_us.h), which
// check MOD_SYSOPT_MAGIC first: in Richter / Maria games their player file
// sits here instead, and the stubs call the retail functions:
//   ModSysOptBox   for MenuSystemDraw's cursor box (func_800F5E68),
//   ModSysOptInput for MENU_STEP_SYSTEM's MenuHandleCursorInput,
//   ModSysOptSound for MenuDraw's MenuSoundConfigDraw (the popup's window).
// The row's info bar text is this code's header (menu.c D_800A2D48[7]).
#include <game.h>
#include <sfx.h>
#include <lba.h>
#include <disk.h>

#ifndef OFF_BIN_ALK_PSP
#define OFF_BIN_ALK_PSP 0 // (the first compile, as in src/dra/cd.c)
#endif
#ifndef OFF_BIN_JP_SD
#define OFF_BIN_JP_SD 0
#endif

// DRA (src/dra/dra.h, menu.h, menu.c, cd.c)
typedef struct {
    s32 loc;
    s32 cb;
    s32 size;
    u8 vabId;
    u8 unkD;
    u8 nextCdFileType;
    u8 unkF;
} CdFile;
typedef struct {
    s16 cursorX;
    s16 cursorY;
    u16 cursorW;
    u16 cursorH;
    s32 otIdx;
} MenuContextInit;
#define MENU_DG_SETTINGS 7
#define MENU_DG_CFG_SOUND 11
#define MENU_STEP_SYSTEM 0x101
#define MOD_STEP_BOX 0x10F // (no MenuHandle case: menu.h ends at 0x107)
extern MenuContext g_Menus[];
extern MenuContextInit g_MenuInit[];
extern CdFile g_LbaSdAlkVh;
extern CdFile g_LbaSdAlkVb;
extern s32 D_8013784C; // menu.c: 1 = grey text
void func_800F5E68(MenuContext* ctx, s32 cursorIdx, s32 x, s32 y, s32 w, s32 h,
                   s32 yGap, s32 bColorMode);
void MenuDrawStr(const char* str, s32 x, s32 y, MenuContext* ctx);
void MenuHandleCursorInput(s32* nav, u8 nOptions, u32 type);
void MenuSoundConfigDraw(MenuContext* ctx);
void SsUtAllKeyOff(short mode); // (main SDK)

void ModSysOptBox(MenuContext* ctx, s32 cursorIdx, s32 x, s32 y, s32 w, s32 h,
                  s32 yGap, s32 bColorMode);
void ModSysOptInput(s32* nav, u8 nOptions, u32 type);
void ModSysOptSound(MenuContext* ctx);

// the header the stubs read (first in .text, at MOD_SYSOPT_ADDR): the
// magic, the three entries, then the info bar text (MOD_SYSOPT_BAR_TEXT;
// the info bar reads an ASCII comma as a Shift-JIS lead byte: the
// full-width comma 81 43 instead)
__asm__(".text\n"
        ".word " MOD_SYSOPT_MAGIC_STR "\n"
        ".word ModSysOptBox\n"
        ".word ModSysOptInput\n"
        ".word ModSysOptSound\n"
        ".asciz \"Boss versions\\201\\103 game balance and voices\"\n"
        ".align 2\n");

#define ROW 6     // the row, after the retail six (16 px apart: y 0x90)
#define ROWS 4    // the popup's options
#define POP_X 172 // the popup's window (the Sound box's is 172, 112, 57, 32)
#define POP_Y 124 // (ends at 188: the info bar starts at 193)
#define POP_W 168 // (the right margin as wide as the left one)
#define POP_H 64
#define VALUE_DX 112

static const char g_SysOptName[4][8] = {
    _S("PS1"), _S("Saturn"), _S("PSP"), _S("JP")};
static const char* const g_SysOptLabel[ROWS] = {
    _S("Boss Richter"), _S("Boss Maria"), _S("Balance"), _S("Dub")};
#define SYSOPT_NAME() _S("Extra settings")
#define SYSOPT_LABEL(r) g_SysOptLabel[r]
// each option: the flag bits it owns, and per choice (the Player Select's
// order, src/st/sel/2C048.c) their value and its name
typedef struct {
    u8 mask;
    u8 n;
    u8 val[3];
    u8 name[3];
} SysOptRow;
static const SysOptRow g_SysOptRows[ROWS] = {
    {MOD_FLAG_BOSS_RIC_SATURN, 2, {0, MOD_FLAG_BOSS_RIC_SATURN}, {0, 1}},
    {MOD_FLAG_MARIA_BOSS_PSP, 2, {0, MOD_FLAG_MARIA_BOSS_PSP}, {1, 2}},
    {MOD_FLAG_PS1_BALANCE, 2, {MOD_FLAG_PS1_BALANCE, 0}, {0, 1}},
    {MOD_FLAG_PS1_DUB | MOD_FLAG_JP_DUB,
     3,
     {0, MOD_FLAG_PS1_DUB, MOD_FLAG_JP_DUB},
     {2, 0, 3}},
};
#define DUB 3

// the popup's cursor (loaded with the file: 0 after each game load)
static s32 g_PopRow = 0;
// a Dub change made since the current stage's bank was loaded: 1 + the dub
// bits it was loaded with (0: none). Kept until the bank is reloaded
static s32 g_StageDubOld = 0;
#define DUB_BITS (MOD_FLAG_PS1_DUB | MOD_FLAG_JP_DUB)

// the choice shown: the one whose bits match (else the first)
static s32 SysOptCur(const SysOptRow* row) {
    s32 i;

    for (i = row->n - 1; i > 0; i--) {
        if ((MOD_STATUS_FLAGS & row->mask) == row->val[i]) {
            break;
        }
    }
    return i;
}

// the locked options (bit per option): a boss option waits while that boss
// is alive (boss Richter in BO6 / BO6RIC2: enemy 0x133 ENEMY_RICHTER; boss
// Maria in CEN / CENP: the mod's record 0x198 on both fights' body): the
// overlay loaded is the other version's. They change again after the fight
// (a new choice takes effect the next time that boss's area loads)
static s32 SysOptLocks(void) {
    Entity* e;
    s32 locks = 0;

    for (e = &g_Entities[STAGE_ENTITY_START];
         e < &g_Entities[TOTAL_ENTITY_COUNT]; e++) {
        if (e->pfnUpdate && e->hitPoints > 0) {
            if (e->enemyId == 0x133) {
                locks |= 1;
            }
            if (e->enemyId == MOD_PSP_MARIA_ENEMY) {
                locks |= 2;
            }
        }
    }
    // Dub: locked in Castle Center (CEN): its bank goes with the CEN / CENP
    // overlay loaded and Boss Maria's choice (cd.c's StageSfx pre-hook, for
    // STAGE_CEN only: RCEN has no such bank), so the stage bank is not
    // reloaded there; the Dub changes after the next area load
    if (g_StageId == STAGE_CEN) {
        locks |= 1 << DUB;
    }
    // Boss Maria / Boss Richter: the version goes with the overlay loaded
    // (cd.c: CEN or CENP.BIN for her, BO6 or BO6RIC2.BIN for him), so a
    // change inside that area would only show after the next load: locked
    // there, not only during the fight
    if (g_StageId == STAGE_CEN) {
        locks |= 2;
    }
    if (g_StageId == STAGE_BO6) {
        locks |= 1;
    }
    return locks;
}

// The popup is the Sound box's window with our size (MenuContext.unk1C:
// 2 hidden, 3 opening, 0 open, 1 closing) while the menu is in
// MOD_STEP_BOX (from X on the row until the closing animation's end)
//
// closed at once, the Sound box's own window again (also when the System
// screen is left with the box still up: Start, or any other way)
static void PopReset(MenuContext* pop) {
    MenuContextInit* init = &g_MenuInit[MENU_DG_CFG_SOUND];

    pop->unk1C = 2;
    pop->cursorX = init->cursorX;
    pop->cursorY = init->cursorY;
    pop->cursorW = init->cursorW;
    pop->cursorH = init->cursorH;
}

// Alucard's grunt bank (vab 1: VAB/SD_ALK, BIN/ALK_PSP.BIN or the JP dub's
// copy in BIN/JP_SD.BIN, all laid out alike) is loaded with his player file
// only (src/dra/cd.c's CdFile_AlucardPrg branch, which sets
// g_LbaSdAlkVh/Vb.loc: so .loc is the bank loaded). After a Dub change, load
// the new one as that chain does (CdFile_4: the VH, then the VB), once the CD
// is idle. The current stage's bank (vab 3: boss / enemy voices) follows
// right after in the same CD chain (g_LbaSdAlkVh.nextCdFileType =
// CdFile_StageSfx, g_LoadOvlIdx = g_StageId: UpdateCd's pre-hook picks the
// PSP / JP / retail bank as on a stage load), all voices keyed off first
// (the pause already keyed off and saved the held 20-23 pairs and the
// scripts; they key on again through the new VAB header on unpause). The
// link is cleared below once the chain moved on, and by cd.c's
// CdFile_AlucardPrg branch at every game load. The menu has stopped the
// music; its exit waits for the whole chain (g_IsUsingCd); nothing plays
// from vab 1 / 3 while it is open. Castle Center keeps its bank until the
// next area load (its bank goes with the CEN / CENP overlay loaded, which
// Boss Maria may have changed since)
static void SysOptBanks(void) {
    s32 off = MOD_JP_DUB_ON() && OFF_BIN_JP_SD ? OFF_BIN_JP_SD - 0x610D
              : MOD_PSP_DUB_ON()               ? OFF_BIN_ALK_PSP - 0x610D
                                               : 0;
    s32 stage =
        g_StageDubOld && (g_StageDubOld - 1) != (MOD_STATUS_FLAGS & DUB_BITS) &&
        g_StageId != STAGE_CEN;

    if (g_LbaSdAlkVh.nextCdFileType != CdFile_NoNext &&
        g_LoadFile != CdFile_4) {
        g_LbaSdAlkVh.nextCdFileType = CdFile_NoNext; // (the chain moved on)
    }
    if (!OFF_BIN_ALK_PSP || g_CdStep != CdStep_None || g_IsUsingCd) {
        return;
    }
    if (g_LbaSdAlkVh.loc != 0x610D + off) {
        g_LbaSdAlkVh.loc = 0x610D + off;
        g_LbaSdAlkVb.loc = 0x6110 + off;
        g_LoadFile = CdFile_4;
        if (stage) {
            g_LbaSdAlkVh.nextCdFileType = CdFile_StageSfx;
        }
    } else if (stage) {
        g_LoadFile = CdFile_StageSfx;
    } else {
        g_StageDubOld = 0; // (changed back)
        return;
    }
    if (stage) {
        SsUtAllKeyOff(0);
        g_LoadOvlIdx = g_StageId;
    }
    g_StageDubOld = 0;
    g_CdStep = CdStep_LoadInit;
}

// The box's input, while g_MenuStep is MOD_STEP_BOX: only when it is fully
// open (Up / Down pick, Left / Right / X change, Triangle / Circle close)
static void PopInput(MenuContext* pop) {
    const SysOptRow* row;
    s32 v;

    MenuHandleCursorInput(&g_PopRow, ROWS, 0); // Up / Down, wraps
    row = &g_SysOptRows[g_PopRow];
    // X steps on as Right does
    if (g_pads[0].tapped & PAD_CROSS) {
        g_pads[0].repeat |= PAD_RIGHT;
    }
    if (g_pads[0].repeat & (PAD_LEFT | PAD_RIGHT)) {
        if ((SysOptLocks() >> g_PopRow) & 1) {
            g_api.PlaySfx(SFX_UI_ERROR);
        } else {
            v = SysOptCur(row);
            MenuHandleCursorInput(&v, row->n, 5); // wraps, SFX_UI_MOVE
            if (g_PopRow == DUB && !g_StageDubOld) {
                // the stage's bank was loaded with these
                g_StageDubOld = 1 + (MOD_STATUS_FLAGS & DUB_BITS);
            }
            MOD_STATUS_FLAGS = (MOD_STATUS_FLAGS & ~row->mask) | row->val[v];
            if (g_PopRow == DUB) {
                // the familiar's bank has the dub's voices too: the menu's
                // exit reloads it as when another one is chosen
                // (CdFile_ServantChr; it is summoned again)
                g_ServantLoaded = 0;
            }
            // the PS1 balance has no third hand: its item back to the
            // inventory
            if (MOD_PS1_BALANCE() && MOD_HOLDER_ITEM) {
                g_api.AddToInventory(MOD_HOLDER_ITEM, EQUIP_HAND);
                MOD_HOLDER_ITEM = 0;
            }
        }
    }
    // Triangle / Circle close it as the Sound box closes (MenuHide)
    if (g_pads[0].tapped & (PAD_TRIANGLE | PAD_CIRCLE)) {
        g_api.PlaySfx(SFX_UI_CONFIRM);
        pop->unk1C = 1;
        pop->unk1D = 0;
    }
}

// MenuSystemDraw's cursor box: runs every frame the System window is drawn
// (the System list, the box, and the menu's closing fade). While the box is
// up the menu is in MOD_STEP_BOX, a step MenuHandle has no case for (as
// MENU_STEP_SYSTEM_SOUND is for the Sound box): the System list takes no
// input, and the info bar is not drawn again every frame (MENU_STEP_SYSTEM
// re-renders it with the BIOS font each frame: with the box's text on top
// that ran past one frame, 30 fps), so the bar keeps its text as during
// Sound. The box's own input is handled here
void ModSysOptBox(MenuContext* ctx, s32 cursorIdx, s32 x, s32 y, s32 w, s32 h,
                  s32 yGap, s32 bColorMode) {
    MenuContext* pop = &g_Menus[MENU_DG_CFG_SOUND];

    if (g_MenuStep == MOD_STEP_BOX) {
        if (pop->cursorW != POP_W || pop->unk1C == 2) {
            PopReset(pop); // closed: back to the System list
            g_MenuStep = MENU_STEP_SYSTEM;
        } else if (pop->unk1C == 0) {
            PopInput(pop);
        }
    } else {
        if (pop->cursorW == POP_W) {
            PopReset(pop); // the menu left the box some other way (Start)
        }
        // the choice is final (the box closed, or the System screen or the
        // menu is closing, Start included): the banks follow
        SysOptBanks();
    }
    func_800F5E68(ctx, cursorIdx, x, y, w, h, yGap, bColorMode);
    MenuDrawStr(SYSOPT_NAME(), 0x20, 0x30 + ROW * 0x10, ctx);
}

void ModSysOptSound(MenuContext* ctx) {
    const SysOptRow* row;
    s32 r, locks;
    s32 x = ctx->cursorX + 4;
    s32 y = ctx->cursorY + 4;

    if (ctx->cursorW != POP_W) {
        MenuSoundConfigDraw(ctx);
        return;
    }
    // only while the box's step is the menu's (else an orphan: reset)
    if (g_MenuStep != MOD_STEP_BOX) {
        PopReset(ctx);
        return;
    }
    locks = SysOptLocks();
    for (r = 0, row = g_SysOptRows; r < ROWS; r++, row++, y += 16) {
        D_8013784C = (locks >> r) & 1;
        MenuDrawStr(SYSOPT_LABEL(r), x, y, ctx);
        MenuDrawStr(
            g_SysOptName[row->name[SysOptCur(row)]], x + VALUE_DX, y, ctx);
    }
    D_8013784C = 0;
    func_800F5E68(
        ctx, g_PopRow, ctx->cursorX + 2, ctx->cursorY + 2, POP_W - 4, 12, 4, 1);
}

// MENU_STEP_SYSTEM's cursor input: the list's 7 rows; X on "Extra
// settings" opens the box (as MenuShow does for Sound) and enters
// MOD_STEP_BOX. Its tap is consumed (MENU_STEP_SYSTEM would buzz)
void ModSysOptInput(s32* nav, u8 nOptions, u32 type) {
    MenuContext* pop = &g_Menus[MENU_DG_CFG_SOUND];

    if (*nav > ROW) {
        *nav = ROW; // (a save from the build with four rows here)
    }
    MenuHandleCursorInput(nav, ROW + 1, type);
    if (*nav == ROW && (g_pads[0].tapped & PAD_CROSS)) {
        pop->cursorX = POP_X;
        pop->cursorY = POP_Y;
        pop->cursorW = POP_W;
        pop->cursorH = POP_H;
        pop->unk1C = 3;
        pop->unk1D = 0;
        g_MenuStep = MOD_STEP_BOX;
        g_api.PlaySfx(SFX_UI_CONFIRM);
        g_pads[0].tapped = 0;
        g_pads[0].repeat = 0;
    }
}
