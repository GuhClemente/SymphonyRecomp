// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/np3/e_breakable.c and src/st/no3/
// e_breakable.c (at file scope, in place; NO3 names its mod_sconce.h with
// MOD_SCONCE_H)
// Saturn port: rodata copies with type 2, the Saturn's tall candelabra of
// the shaft (STAGE_16 bank 16 frames 1-4 at 5 ticks -> MOD_SCONCE_ANIMSET
// frames 1-4, its stand 5), which EntityBreakable uses instead
static const u8 anim_sconce[] = {5, 1, 5, 2, 5, 3, 5, 4, 0, 0};
static u8* const g_ModBreakableAnimations[8] = {
    anim_1, anim_2, (u8*)anim_sconce};
static const u8 g_ModBreakableHitboxes[8] = {8, 8, 8};
static const u8 g_ModBreakableExplosionTypes[8] = {0, 0, 0};
static const u16 g_ModBreakableAnimSets[8] = {
    ANIMSET_DRA(3), ANIMSET_DRA(3), MOD_SCONCE_ANIMSET};
static const u8 g_ModBlendModes[8] = {
    BLEND_TRANSP | BLEND_QUARTER, BLEND_TRANSP | BLEND_ADD, BLEND_NO};
#define g_eBreakableAnimations g_ModBreakableAnimations
#define g_eBreakableHitboxes g_ModBreakableHitboxes
#define g_eBreakableExplosionTypes g_ModBreakableExplosionTypes
#define g_eBreakableanimSets g_ModBreakableAnimSets
#define blend_modes g_ModBlendModes
// NP3's own candles as they are (no stand, no offset); the candelabra as
// the Cursed Prison's (its config's hitboxOffY, on the flame)
static const u8 g_ModCandleRemnantFrames[8] = {0, 0, 5};
static const s16 g_ModBreakableHitboxOffY[8] = {0, 0, -40};
static const u16 g_ModBreakableSfx[8] = {
    SFX_CANDLE_HIT, SFX_CANDLE_HIT, SFX_CANDLE_HIT};
#define MOD_SCONCE_GLOW
#if defined(MOD_SCONCE_H) // NO3's (no3_mod_shaft.h)
#include MOD_SCONCE_H
#else
#include "../../../../../src/st/np3/mod_sconce.h"
#endif
#include "../mod_candle.h"
