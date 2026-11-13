// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/rno3/e_breakable.c (at file scope, in
// place)
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
// the stand (frame 5) stays; the hitbox sits on the flames, which hang below
// the entity (the art is turned around: the Saturn's -40 upside down)
static const u8 g_ModCandleRemnantFrames[8] = {0, 0, 5};
static const s16 g_ModBreakableHitboxOffY[8] = {0, 0, 40};
static const u16 g_ModBreakableSfx[8] = {
    SFX_CANDLE_HIT, SFX_CANDLE_HIT, SFX_CANDLE_HIT};
#define MOD_SCONCE_GLOW
#include "../../../../../src/st/rno3/mod_sconce.h"
#include "../mod_candle.h"

// the stand left on the ceiling, turned around like the candle
void EntityModCandleRemnantR(Entity* self) {
    EntityModCandleRemnant(self);
    self->drawFlags = ENTITY_ROTATE;
    self->rotate = ROT(180);
}

#define EntityBreakable EntityModBreakable
#include "../../../../../src/st/e_breakable.h"
#undef EntityBreakable
