// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rgdn.h"

// Saturn port: the Hell Garden's item (RSTAGE15 PrizeDrops[0] = 0x12C), the
// Alucard Spear lying in room 2 (the Saturn's entity 0x0C, params 0; collected
// bit 808): 0x80 + a combined item id, the new hand items from
// MOD_DROP_HAND_BASE (include/game.h)
u16 PrizeDrops[] = {
    0x80 + MOD_DROP_HAND_BASE + ITEM_ALUCARD_SPEAR - NUM_HAND_ITEMS,
};
