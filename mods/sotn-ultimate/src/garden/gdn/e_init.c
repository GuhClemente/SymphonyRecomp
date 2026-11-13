// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gdn.h"

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
void EntityAlucardWaterEffect(Entity*);
void EntitySplashWater(Entity*);
void EntitySurfacingWater(Entity*);
void EntitySideWaterSplash(Entity*);
void EntitySmallWaterDrop(Entity*);
void EntityWaterDrop(Entity*);
void EntityModWaterBody(Entity*);
void EntityModWaterSurface(Entity*);
void EntityModCandleRemnant(Entity*);
void EntityModDripSpawner(Entity*);
void EntityModDrip(Entity*);
void EntityThornweed(Entity*);
void EntityCorpseweed(Entity*);
void EntityCorpseweedProjectile(Entity*);
void EntitySlinger(Entity*);
void EntitySlingerRib(Entity*);
void EntitySlingerPieces(Entity*);
void EntityModGargoyle(Entity*);
void EntityModGargoyleDebris(Entity*);
void EntityModGargoylePedestal(Entity*);
void EntityModSkBreeder(Entity*);
void EntityModSkBreederWeapon(Entity*);
void EntityModSkBreederBeast(Entity*);
void EntityModSkBreederDebris(Entity*);
void EntityModLeaper(Entity*);
void EntityModLeaperSpawner(Entity*);
void EntityModGardener(Entity*);
void EntityModGardenerShears(Entity*);
void EntityModGardenerHitbox(Entity*);
void EntityModTree(Entity*);
void EntityModTreeFace(Entity*);
void EntityModTreeDrop(Entity*);
void EntityModTreeBlood(Entity*);
void EntityModTreeLeaf(Entity*);
void EntityModTreeBase(Entity*);
void EntityModManEater(Entity*);
void EntityModManEaterDebris(Entity*);
void EntityModBossController(Entity*);
void EntityModBossDoor(Entity*);
void EntityModBoss(Entity*);
void EntityModBossSword(Entity*);
void EntityModBossBone(Entity*);
void EntityModBossBomb(Entity*);
void EntityModBossGhost(Entity*);
void EntityModBossDebris(Entity*);
void EntityModBossBeam(Entity*);
void EntityModBossWave(Entity*);
void EntityExplosionPuffOpaque(Entity*);
void EntityLifeUpSpawn(Entity*);

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
    // Saturn port: the Garden's water
    EntityAlucardWaterEffect,
    EntitySplashWater,
    EntitySurfacingWater,
    EntitySideWaterSplash,
    EntitySmallWaterDrop,
    EntityWaterDrop,
    EntityModWaterBody,
    EntityModWaterSurface,
    EntityModCandleRemnant,
    EntityModDripSpawner,
    EntityModDrip,
    EntityThornweed,
    EntityCorpseweed,
    EntityCorpseweedProjectile,
    EntitySlinger,
    EntitySlingerRib,
    EntitySlingerPieces,
    EntityModGargoyle,
    EntityModGargoyleDebris,
    EntityModGargoylePedestal,
    EntityModSkBreeder,
    EntityModSkBreederWeapon,
    EntityModSkBreederBeast,
    EntityModSkBreederDebris,
    EntityModLeaper,
    EntityModLeaperSpawner,
    EntityModGardener,
    EntityModGardenerShears,
    EntityModGardenerHitbox,
    EntityModTree,
    EntityModTreeFace,
    EntityModTreeDrop,
    EntityModTreeBlood,
    EntityModTreeLeaf,
    EntityModTreeBase,
    EntityModManEater,
    EntityModManEaterDebris,
    EntityModBossController,
    EntityModBossDoor,
    EntityModBoss,
    EntityModBossSword,
    EntityModBossBone,
    EntityModBossBomb,
    EntityModBossGhost,
    EntityModBossDebris,
    EntityModBossBeam,
    EntityModBossWave,
    EntityExplosionPuffOpaque,
    EntityLifeUpSpawn,
};

// clang-format off
EInit g_EInitBreakable = {ANIMSET_OVL(1), 0x00, 0x00, PAL_NONE, ENEMY_BREAKABLE};
#include "../../../../../src/st/e_init_common.h"
// Saturn port: the PS1 enemies, as in CHI and NO0 but for the art's place
EInit g_EInitThornweed = {MOD_THORNWEED_ANIMSET, 0x00, MOD_THORNWEED_UNK5A, PAL_THORNWEED_CORPSEWEED, ENEMY_THORNWEED};
EInit g_EInitCorpseweed = {MOD_THORNWEED_ANIMSET, 0x00, MOD_THORNWEED_UNK5A, PAL_THORNWEED_CORPSEWEED, ENEMY_CORPSEWEED};
EInit g_EInitCorpseweedProjectile = {MOD_THORNWEED_ANIMSET, 0x00, MOD_THORNWEED_UNK5A, PAL_THORNWEED_CORPSEWEED, ENEMY_CORPSEWEED_PROJECTILE};
EInit g_EInitSlinger = {MOD_SLINGER_ANIMSET, 0x01, MOD_SLINGER_UNK5A, PAL_SLINGER, ENEMY_SLINGER};
EInit g_EInitSlingerPieces = {MOD_SLINGER_ANIMSET, 0x00, MOD_SLINGER_UNK5A, PAL_SLINGER, ENEMY_PARTICLE};
EInit g_EInitSlingerRib = {MOD_SLINGER_ANIMSET, 0x16, MOD_SLINGER_UNK5A, PAL_SLINGER, ENEMY_SLINGER_RIB};
EInit g_EInitEnemy3 = {ANIMSET_OVL(1), 0x00, 0x00, PAL_NONE, ENEMY_COMMON};
