// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/np3/np3.h (at file scope, in place)
// Saturn port (needs MOD_SATURN from stage.h)
// Saturn port: the extended shaft (2,41)-(2,44) gets the Saturn entity list
// (mod_layout.h): no PSX shaft-bottom pool, three candles, a red door.
#define MOD_LAYOUT_ID 2
// and the first room's (layout 1, mod_layout2.h): the Power of Wolf orb in
// its own slot, not the trapdoor's
// (mods/sotn-ultimate/tools/gen_mod_layouts.py)
#define MOD_LAYOUT_ID2 1
// Saturn port: the shaft's three candles are the Saturn's candelabras with
// their glow (candle type 2, mods/sotn-ultimate/src/castle/mod_candle.h; art
// from the Cursed Prison, src/st/np3/mod_sconce.h by
// mods/sotn-ultimate/tools/gen_np3_sconce.py). NP3's own candles (types 0 and
// 1) stay as they are; none holds a stage item.
#define MOD_SATURN_CANDLES
#define MOD_ITEM_CANDLE_TYPE 0xFF
#define MOD_CANDLE_GLOW_TYPES (1 << 2)
#include "../../../../../src/st/np3/mod_sconce.h"
#define MOD_CANDLE_ANIMSET MOD_SCONCE_ANIMSET
// the stand left on the wall: NP3's entity table is retail data, so it runs
// as an existing id with its own update function
#define E_MOD_CANDLE_REMNANT E_DUMMY_0F
#define MOD_CANDLE_REMNANT_PFN EntityModCandleRemnant
void EntityModCandleRemnant(Entity* self);
