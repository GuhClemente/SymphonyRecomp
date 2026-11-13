// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rgdn.h"

// Saturn port: the Hell Garden's candles are the Garden's ram-head wall
// sconces turned around, hanging (RSTAGE15 config 0x060EE440: its bank 16
// frames 0x26-0x29 at 5 ticks, whose parts the Saturn flips about the entity;
// converted as ANIMSET_OVL(2) frames 1-4,
// mods/sotn-ultimate/tools/rgdn_assets.py). All are type 0.
static u8 anim_1[] = {5, 1, 5, 2, 5, 3, 5, 4, 0, 0};
static u8* g_eBreakableAnimations[8] = {anim_1};
static u8 g_eBreakableHitboxes[8] = {8};
static u8 g_eBreakableExplosionTypes[8] = {0};
static u16 g_eBreakableanimSets[8] = {ANIMSET_OVL(2)};
static u8 blend_modes[8] = {BLEND_NO};
// the empty sconce (bank 16 frame 0x2A); the config's hitboxOffY (-16),
// which the inverted castle's candle code negates (060DC040: the flame hangs
// below the entity); the sound
static u8 g_ModCandleRemnantFrames[8] = {5};
static s16 g_ModBreakableHitboxOffY[8] = {16};
static u16 g_ModBreakableSfx[8] = {SFX_CANDLE_HIT};

// Saturn port: the glow over the candle's flame
// (mods/sotn-ultimate/tools/rgdn_assets.py)
#include "mod_candle_glow.h"
#include "../../castle/mod_candle.h"

// stripped on PSP
static u8 unused[] = {0, 0, 0, 0, 0, 0, 0, 0};

#include "../../../../../src/st/e_breakable.h"
