// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rprs.h"

// Saturn port: the Soul Prison's items (RSTAGE16 PrizeDrops {0x128, 0x175,
// 0x0C7}), released by its item holders (candle type 2, params & 0x1FF): the
// Library Card (Saturn hand item 168: icon and palette 70, the PS1's
// ITEM_LIBRARY_CARD), the Opal and the Delicious Meal. 0x80 + a combined item
// id; the new hand items from MOD_DROP_HAND_BASE (include/game.h)
u16 PrizeDrops[] = {
    0x80 + ITEM_LIBRARY_CARD,
    0x80 + NUM_HAND_ITEMS + ITEM_OPAL,
    0x80 + MOD_DROP_HAND_BASE + ITEM_DELICIOUS_MEAL - NUM_HAND_ITEMS,
};
