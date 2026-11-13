// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/no4/no4.h (at file scope, in place)
// Saturn port: Crystal cloak's room (40,38), layout 25, also holds Glaring
// Cloth (mod_layout.h). Scylla's overlay has its own copy of the room with
// the same extra prize index and collected flag, see src/boss/bo3/bo3.h.
// The E_PERSISTENT_ITEM_DROP uses an index past PrizeDrops[] (which lives
// in data and can't grow); its collected flag, HEART_DROP_CASTLE_FLAG
// 0x40 + 40, is in a g_CastleFlags byte no stage uses (0x10D).
#define MOD_LAYOUT_ID 25
// Saturn port: the Cursed Prison's red door in room 5 (37,33)-(37,34),
// layout 6 (mod_layout2.h); its doorway, loading room and transition are in
// mod_rooms.h (mods/sotn-ultimate/tools/gen_prs_links.py)
#define MOD_LAYOUT_ID2 6
#define MOD_EXTRA_PRIZE_INDEX 40
#define MOD_EXTRA_PRIZE_DROP (0x80 + NUM_HAND_ITEMS + ITEM_GLARING_CLOTH)
