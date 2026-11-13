// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rgdn.h"

void EntityBreakable(Entity*);
void EntityExplosion(Entity*);
void EntityPrizeDrop(Entity*);
void EntityDamageDisplay(Entity*);
void EntityRedDoor(Entity*);
void EntityIntenseExplosion(Entity*);
void EntitySoulStealOrb(Entity*);
void EntityRoomForeground(Entity*);
void EntityStageNamePopup(Entity*);
void EntityEquipItemDrop(Entity*);
void EntityRelicOrb(Entity*);
void EntityPersistentItemDrop(Entity*);
void EntityEnemyBlood(Entity*);
void EntityMessageBox(Entity*);
void EntityDummy(Entity*);
void EntityDummy(Entity*);
void EntityBackgroundBlock(Entity*);
void EntityLockCamera(Entity*);
void EntityUnkId13(Entity*);
void EntityExplosionVariants(Entity*);
void EntityGreyPuff(Entity*);
void EntityModCandleRemnant(Entity*);
void EntityCaveTroll(Entity*);
void EntityCaveTrollBeam(Entity*);
void EntityCaveTrollGlow(Entity*);
void EntityBitterfly(Entity*);
void EntityCloakedKnight(Entity*);
void EntityCloakedKnightCloak(Entity*);
void EntityCloakedKnightSword(Entity*);
void EntityCloakedKnightAura(Entity*);
void EntityVenusWeed(Entity*);
void EntityVenusWeedFlower(Entity*);
void EntityVenusWeedTendril(Entity*);
void EntityVenusWeedDart(Entity*);
void EntityVenusWeedSpike(Entity*);
void EntityModSkGuardian(Entity*);
void EntityModSkGuardianSpear(Entity*);
void EntityModSkGuardianShield(Entity*);
void EntityModSkBreederDebris(Entity*);
void EntityModCopperArmor(Entity*);
void EntityModCopperArmorHitbox(Entity*);
void EntityModIfrit(Entity*);
void EntityModIfritFlame(Entity*);
void EntityModIfritFlame2(Entity*);
void EntityModIfritAfterimage(Entity*);
void EntityModIfritPuff(Entity*);
void EntityModIfritRing(Entity*);
void EntityModIfritEmbers(Entity*);
void EntityModIfritPillar(Entity*);
void EntityModIfritAshes(Entity*);
void EntityAlucardWaterEffect(Entity*);
void EntitySplashWater(Entity*);
void EntitySurfacingWater(Entity*);
void EntitySideWaterSplash(Entity*);
void EntitySmallWaterDrop(Entity*);
void EntityWaterDrop(Entity*);
void EntityModCeilingWater(Entity*);
void EntityModCeilingShimmer(Entity*);

PfnEntityUpdate EntityUpdates[] = {
    EntityBreakable,
    EntityExplosion,
    EntityPrizeDrop,
    EntityDamageDisplay,
    EntityRedDoor,
    EntityIntenseExplosion,
    EntitySoulStealOrb,
    EntityRoomForeground,
    EntityStageNamePopup,
    EntityEquipItemDrop,
    EntityRelicOrb,
    EntityPersistentItemDrop,
    EntityEnemyBlood,
    EntityMessageBox,
    EntityDummy,
    EntityDummy,
    EntityBackgroundBlock, // unused
    EntityLockCamera,      // unused? looks debugging stuff
    EntityUnkId13,
    EntityExplosionVariants,
    EntityGreyPuff,
    // Saturn port
    EntityModCandleRemnant,
    EntityCaveTroll,
    EntityCaveTrollBeam,
    EntityCaveTrollGlow,
    EntityBitterfly,
    EntityCloakedKnight,
    EntityCloakedKnightCloak,
    EntityCloakedKnightSword,
    EntityCloakedKnightAura,
    EntityVenusWeed,
    EntityVenusWeedFlower,
    EntityVenusWeedTendril,
    EntityVenusWeedDart,
    EntityVenusWeedSpike,
    EntityModSkGuardian,
    EntityModSkGuardianSpear,
    EntityModSkGuardianShield,
    EntityModSkBreederDebris,
    EntityModCopperArmor,
    EntityModCopperArmorHitbox,
    EntityModIfrit,
    EntityModIfritFlame,
    EntityModIfritFlame2,
    EntityModIfritAfterimage,
    EntityModIfritPuff,
    EntityModIfritRing,
    EntityModIfritEmbers,
    EntityModIfritPillar,
    EntityModIfritAshes,
    EntityAlucardWaterEffect,
    EntitySplashWater,
    EntitySurfacingWater,
    EntitySideWaterSplash,
    EntitySmallWaterDrop,
    EntityWaterDrop,
    EntityModCeilingWater,
    EntityModCeilingShimmer,
};

// clang-format off
EInit g_EInitBreakable = {ANIMSET_OVL(1), 0x00, 0x00, PAL_NONE, ENEMY_BREAKABLE};
#include "../../../../../src/st/e_init_common.h"
// Saturn port: the PS1 enemies, as in RNO4, RNZ0, RNZ1 and RNO3 but for the
// art's place (MOD_*_ANIMSET / _UNK5A, mod_psx_sprites.h)
EInit g_EInitCaveTroll = {MOD_CAVE_TROLL_ANIMSET, 0x00, MOD_CAVE_TROLL_UNK5A, PAL_CAVE_TROLL, ENEMY_CAVE_TROLL};
EInit g_EInitCaveTrollBeam = {MOD_CAVE_TROLL_ANIMSET, 0x00, MOD_CAVE_TROLL_UNK5A, PAL_CAVE_TROLL, ENEMY_CAVE_TROLL_UNK_9A};
EInit g_EInitCaveTrollGlow = {MOD_CAVE_TROLL_ANIMSET, 0x00, MOD_CAVE_TROLL_UNK5A, PAL_CAVE_TROLL, ENEMY_CAVE_TROLL_UNK_9A};
EInit g_EInitBitterfly = {MOD_BITTERFLY_ANIMSET, 0x01, MOD_BITTERFLY_UNK5A, PAL_BITTERFLY, ENEMY_BITTERFLY};
EInit g_EInitCloakedKnight = {MOD_CLOAKED_KNIGHT_ANIMSET, 0x01, MOD_CLOAKED_KNIGHT_UNK5A, PAL_CLOAKED_KNIGHT, ENEMY_CLOAKED_KNIGHT};
EInit g_EInitCloakedKnightAura = {MOD_CLOAKED_KNIGHT_ANIMSET, 0x01, MOD_CLOAKED_KNIGHT_UNK5A, PAL_CLOAKED_KNIGHT, ENEMY_PARTICLE};
EInit g_EInitCloakedKnightSword = {MOD_CLOAKED_KNIGHT_ANIMSET, 0x01, MOD_CLOAKED_KNIGHT_UNK5A, PAL_CLOAKED_KNIGHT, ENEMY_CLOAKED_KNIGHT_SWORD};
EInit g_EInitVenusWeedRoot = {MOD_VENUS_WEED_ANIMSET, 0x00, MOD_VENUS_WEED_UNK5A, PAL_VENUS_WEED, ENEMY_BLUE_VENUS_WEED_ROOT};
EInit g_EInitVenusWeedFlower = {MOD_VENUS_WEED_ANIMSET, 0x00, MOD_VENUS_WEED_UNK5A, PAL_VENUS_WEED, ENEMY_BLUE_VENUS_WEED_FLOWER};
EInit g_EInitVenusWeedTendril = {MOD_VENUS_WEED_ANIMSET, 0x00, MOD_VENUS_WEED_UNK5A, PAL_VENUS_WEED, ENEMY_BLUE_VENUS_WEED_TENDRIL};
EInit g_EInitVenusWeedDart = {MOD_VENUS_WEED_ANIMSET, 0x37, MOD_VENUS_WEED_UNK5A, PAL_VENUS_WEED, ENEMY_BLUE_VENUS_WEED_DART};
EInit g_EInitEnemy3 = {ANIMSET_OVL(1), 0x00, 0x00, PAL_NONE, ENEMY_COMMON};
