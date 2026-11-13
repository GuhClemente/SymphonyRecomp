// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by include/game.h (at file scope, in place)
// Saturn port: the expanded layout must keep the retail size
typedef char PlayerStatusSizeCheck[sizeof(PlayerStatus) == 0x334 ? 1 : -1];

// The retail layout, used to convert saves made by the unmodded game (or by
// mod builds before the inventory expansion) when they are loaded. Saves
// written in the expanded layout carry MOD_SAVE_MAGIC in modPadding; in a
// retail US save those bytes are the unused JP familiar's stats, always 0.
// "SAT1" marked saves from mod builds with 92 body slots (and 2 alignment
// bytes before saveName); they are converted too.
// "SAT2" saves keep God Speed Shoes in RELIC_JP_0 (moved on load).
// "SAT4" saves (the current builds) are in the retail layout, so the
// unmodded game can load them too; what that layout cannot hold is kept in
// the two JP-only familiars' stat slots (save_mgr.h ModSaveExtra). "SAT3"
// saves (earlier builds) are in the expanded layout.
#define MOD_SAVE_MAGIC 0x34544153      // "SAT4"
#define MOD_SAVE_MAGIC_SAT3 0x33544153 // "SAT3"
#define MOD_SAVE_MAGIC_SAT2 0x32544153 // "SAT2"
#define MOD_SAVE_MAGIC_SAT1 0x31544153 // "SAT1"
typedef struct {
    u8 relics[NUM_RELICS];
    u8 spells[NUM_SPELLS];
    u8 equipHandCount[NUM_HAND_ITEMS];
    u8 equipBodyCount[NUM_BODY_ITEMS];
    u8 equipHandOrder[NUM_HAND_ITEMS];
    u8 equipBodyOrder[NUM_BODY_ITEMS];
    char saveName[12];
    u32 spellsLearnt;
    s32 hp;
    s32 hpMax;
    s32 hearts;
    s32 heartsMax;
    s32 mp;
    s32 mpMax;
    s32 statsBase[4];
    s32 statsEquip[4];
    s32 statsTotal[4];
    s32 level;
    u32 exp;
    s32 gold;
    s32 killCount;
    u32 D_80097BF8;
    u32 subWeapon;
    u32 equipment[2];
    u32 wornEquipment[5];
    u32 attackHands[2];
    s32 defenseEquip;
    u16 elementsWeakTo;
    u16 elementsResist;
    u16 elementsImmune;
    u16 elementsAbsorb;
    s32 timerHours;
    s32 timerMinutes;
    s32 timerSeconds;
    s32 timerFrames;
    u32 D_80097C40;
    FamiliarStats statsFamiliars[NUM_FAMILIARS];
} PlayerStatusRetail;
typedef char
    PlayerStatusRetailSizeCheck[sizeof(PlayerStatusRetail) == 0x334 ? 1 : -1];
// Field positions the save converter relies on (retail: 80097964 + offset)
#define MOD_OFFSETOF(type, field) ((u32) & ((type*)0)->field)
typedef char ModLayoutCheck
    [MOD_OFFSETOF(PlayerStatusRetail, equipBodyCount) == 0x0CF &&
             MOD_OFFSETOF(PlayerStatusRetail, saveName) == 0x22C &&
             MOD_OFFSETOF(PlayerStatusRetail, hp) == 0x23C &&
             MOD_OFFSETOF(PlayerStatusRetail, statsFamiliars) == 0x2E0 &&
             MOD_OFFSETOF(PlayerStatus, equipBodyCount) == 0x0D6 &&
             MOD_OFFSETOF(PlayerStatus, equipHandOrder) == 0x133 &&
             MOD_OFFSETOF(PlayerStatus, equipBodyOrder) == 0x1E3 &&
             MOD_OFFSETOF(PlayerStatus, saveName) == 0x240 &&
             MOD_OFFSETOF(PlayerStatus, hp) == 0x250 &&
             MOD_OFFSETOF(PlayerStatus, statsFamiliars) == 0x2F4 &&
             MOD_OFFSETOF(PlayerStatus, modPadding) == 0x330
         ? 1
         : -1];
