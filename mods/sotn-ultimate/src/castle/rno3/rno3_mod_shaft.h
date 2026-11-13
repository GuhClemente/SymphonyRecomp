// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/rno3/rno3.h (at file scope, in place)
// Saturn port: the shaft (61,19)-(61,22), extended up to the Hell Garden as
// on the Saturn (RSTAGE01 room 17; mod_rooms.h,
// mods/sotn-ultimate/tools/gen_mod_rooms.py), gets the Saturn's entity list
// (mod_layout.h): the red door to the loading room (60,19) and three candles,
// the Saturn's hanging candelabras with their glow (candle type 2,
// e_breakable.c; art src/st/rno3/mod_sconce.h by
// mods/sotn-ultimate/tools/gen_np3_sconce.py). RNO3's own candles stay as they
// are.
#define MOD_LAYOUT_ID 2
#define MOD_SATURN_CANDLES
#define MOD_ITEM_CANDLE_TYPE 0xFF
#define MOD_CANDLE_GLOW_TYPES (1 << 2)
#include "../../../../../src/st/rno3/mod_sconce.h"
#define MOD_CANDLE_ANIMSET MOD_SCONCE_ANIMSET
// the stand left on the ceiling: RNO3's entity table is retail data, so it
// runs as an existing id with its own update function
#define E_MOD_CANDLE_REMNANT E_DUMMY_F
#define MOD_CANDLE_REMNANT_PFN EntityModCandleRemnantR

void EntityModCandleRemnantR(Entity* self);
extern EInit g_EInitCommon;
