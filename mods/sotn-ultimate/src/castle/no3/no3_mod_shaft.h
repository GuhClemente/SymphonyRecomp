// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/no3/no3.h (at file scope, in place)
// Saturn port (needs MOD_SATURN from stage.h)
// Saturn port: the first visit's Entrance has the extended shaft too, as
// the Saturn's STAGE_0X (it reads STAGE_01.MAP, room table included): the
// shaft (2,41)-(2,44) to the Underground Garden, the save room moved to
// (3,43), the forest run on row 46 (src/st/no3/mod_rooms.h,
// mods/sotn-ultimate/tools/gen_mod_rooms.py). The shaft's entity list is
// NP3's (layout 2, mod_layout2.h; mod_layout.h is the first room's).
#define MOD_LAYOUT_ID2 2
// its three candles: the Saturn's candelabras with their glow, as in NP3
// (src/st/no3/mod_sconce.h, mods/sotn-ultimate/tools/gen_np3_sconce.py);
// NO3's own candles (types 0 and 1) stay as they are
#define MOD_SATURN_CANDLES
#define MOD_ITEM_CANDLE_TYPE 0xFF
#define MOD_CANDLE_GLOW_TYPES (1 << 2)
#define MOD_SCONCE_H "../../../../../src/st/no3/mod_sconce.h"
#include "../../../../../src/st/no3/mod_sconce.h"
#define MOD_CANDLE_ANIMSET MOD_SCONCE_ANIMSET
// the stand left on the wall: NO3's entity table is retail data, so it runs
// as an existing id with its own update function
#define E_MOD_CANDLE_REMNANT E_DUMMY_0F
#define MOD_CANDLE_REMNANT_PFN EntityModCandleRemnant
void EntityModCandleRemnant(Entity* self);
