// SPDX-License-Identifier: AGPL-3.0-or-later
#include "prs.h"

// Saturn port: the Prison's items (STAGE_16 PrizeDrops {0x098, 0x12E, 0x0C6}),
// released by its item holders (candle type 2, params & 0x1FF): the Karma
// Coin, the Astral Dagger and the Smelly Rice. 0x80 + a combined item id; the
// new hand items from MOD_DROP_HAND_BASE (include/game.h)
u16 PrizeDrops[] = {
    0x80 + ITEM_KARMA_COIN,
    0x80 + MOD_DROP_HAND_BASE + ITEM_ASTRAL_DAGGER - NUM_HAND_ITEMS,
    0x80 + MOD_DROP_HAND_BASE + ITEM_SMELLY_RICE - NUM_HAND_ITEMS,
};
