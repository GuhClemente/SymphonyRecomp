// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rprs.h"

// Saturn port: the Soul Prison's candles (RSTAGE16 0x060DC040, configs
// 0x060E99C8; its bank 16 converted as ANIMSET_OVL(2),
// mods/sotn-ultimate/tools/rprs_assets.py), the Prison's turned around: type 0
// the short wall sconce (frames 6-9 -> 5-8), 1 the tall candelabra (frames
// 1-4), 2 the item holder (frame 0xC -> 9), which breaks like a vase and
// releases PrizeDrops[params & 0x1FF]. The Saturn's frames flip their parts
// about the entity, and so do the converted ones.
static u8 anim_sconce[] = {5, 5, 5, 6, 5, 7, 5, 8, 0, 0};
static u8 anim_tall[] = {5, 1, 5, 2, 5, 3, 5, 4, 0, 0};
static u8 anim_holder[] = {0x10, 9, 0, 0};
static u8* g_eBreakableAnimations[8] = {anim_sconce, anim_tall, anim_holder};
static u8 g_eBreakableHitboxes[8] = {8, 8, 8};
static u8 g_eBreakableExplosionTypes[8] = {0, 0, 0};
static u16 g_eBreakableanimSets[8] = {
    ANIMSET_OVL(2), ANIMSET_OVL(2), ANIMSET_OVL(2)};
static u8 blend_modes[8] = {BLEND_NO, BLEND_NO, BLEND_NO};
// the stands left (frames 10 / 5 -> 10 / 11); the configs' hitboxOffY, which
// the inverted castle's candle code negates (060DC040: the flames hang below
// the entity); the sounds (0x634 / 0x61D, the same ids on the PS1)
static u8 g_ModCandleRemnantFrames[8] = {10, 11, 0};
static s16 g_ModBreakableHitboxOffY[8] = {16, 40, 16};
static u16 g_ModBreakableSfx[8] = {
    SFX_CANDLE_HIT, SFX_CANDLE_HIT, SFX_GLASS_BREAK_E};

// the glow over the flames (mods/sotn-ultimate/tools/rprs_assets.py)
#include "mod_candle_glow.h"
#include "../../castle/mod_candle.h"

// stripped on PSP
static u8 unused[] = {0, 0, 0, 0, 0, 0, 0, 0};

#include "../../../../../src/st/e_breakable.h"
