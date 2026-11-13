// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/dra/menu.c (at file scope, in place)
// Saturn port: the third hand (game.h MOD_HOLDER_ON,
// mods/sotn-ultimate/src/items/mod_holder.h): an 8th equip row,
// MOD_HOLDER_SLOT, 11 px apart so the 8 fit the overview box (retail 7 rows, 13
// px), and an 8th button config row (action 8)
#define MOD_EQUIP_ROWS() (ModHolderOn() ? 8 : 7)
#define MOD_EQUIP_PITCH() (ModHolderOn() ? 11 : 13)
#define MOD_BUTTON_ROWS() (ModHolderOn() ? 8 : 7)
// the equip select's other hand (two-handed weapons): none for the holder
#define MOD_HAND_PAIR() (D_801375D0 < 2)
static const char* const g_ModHolderStr = _S("Holder");
#define MOD_HOLDER_STR g_ModHolderStr
// the holder shows as the 3rd row, after the hands, as on the Saturn; its
// slot (cursorEquip) stays MOD_HOLDER_SLOT. Slot -> row on screen, and back
static s32 ModEquipRow(s32 slot) {
    if (!ModHolderOn() || slot < 2) {
        return slot;
    }
    return slot == MOD_HOLDER_SLOT ? 2 : slot + 1;
}
static s32 ModEquipSlot(s32 row) {
    if (!ModHolderOn() || row < 2) {
        return row;
    }
    return row == 2 ? MOD_HOLDER_SLOT : row - 1;
}
