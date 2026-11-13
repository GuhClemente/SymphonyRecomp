// SPDX-License-Identifier: AGPL-3.0-or-later
#include "prs.h"

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
void EntitySlinger(Entity*);
void EntitySlingerRib(Entity*);
void EntitySlingerPieces(Entity*);
void EntityBoneArcher(Entity*);
void EntityBoneArcherArrow(Entity*);
void EntitySpearGuard(Entity*);
void EntitySpearGuardBlock(Entity*);
void EntityThrownSpear(Entity*);
void EntityModGargoyle(Entity*);
void EntityModGargoyleDebris(Entity*);
void EntityModGargoylePedestal(Entity*);
void EntityModPrsDust(Entity*);
void EntityModPrsSwitch(Entity*);
void EntityModPrsBlock(Entity*);
void EntityModSpecter(Entity*);
void EntityModWispSpawner(Entity*);
void EntityModWisp(Entity*);
void EntityModWispFlame(Entity*);
void EntityModWispAfterimage(Entity*);
void EntityModWispSparks(Entity*);

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
    EntitySlinger,
    EntitySlingerRib,
    EntitySlingerPieces,
    EntityBoneArcher,
    EntityBoneArcherArrow,
    EntitySpearGuard,
    EntitySpearGuardBlock,
    EntityThrownSpear,
    EntityModGargoyle,
    EntityModGargoyleDebris,
    EntityModGargoylePedestal,
    EntityModPrsDust,
    EntityModPrsSwitch,
    EntityModPrsBlock,
    EntityModSpecter,
    EntityModWispSpawner,
    EntityModWisp,
    EntityModWispFlame,
    EntityModWispAfterimage,
    EntityModWispSparks,
};

// clang-format off
EInit g_EInitBreakable = {ANIMSET_OVL(1), 0x00, 0x00, PAL_NONE, ENEMY_BREAKABLE};
#include "../../../../../src/st/e_init_common.h"
// Saturn port: the PS1 enemies, as in NO0 and NO4 but for the art's place
EInit g_EInitSlinger = {MOD_SLINGER_ANIMSET, 0x01, MOD_SLINGER_UNK5A, PAL_SLINGER, ENEMY_SLINGER};
EInit g_EInitSlingerPieces = {MOD_SLINGER_ANIMSET, 0x00, MOD_SLINGER_UNK5A, PAL_SLINGER, ENEMY_PARTICLE};
EInit g_EInitSlingerRib = {MOD_SLINGER_ANIMSET, 0x16, MOD_SLINGER_UNK5A, PAL_SLINGER, ENEMY_SLINGER_RIB};
EInit g_EInitBoneArcher = {MOD_BONE_ARCHER_ANIMSET, 0x01, MOD_BONE_ARCHER_UNK5A, PAL_BONE_ARCHER, ENEMY_BONE_ARCHER};
EInit g_EInitBoneArcherArrow = {MOD_BONE_ARCHER_ANIMSET, 0x00, MOD_BONE_ARCHER_UNK5A, PAL_BONE_ARCHER, ENEMY_BONE_ARCHER_ARROW};
EInit g_EInitSpearGuard = {MOD_SPEAR_GUARD_ANIMSET, 0x05, MOD_SPEAR_GUARD_UNK5A, PAL_SPEAR_GUARD, ENEMY_SPEAR_GUARD};
EInit g_EInitThrownSpear = {MOD_SPEAR_GUARD_ANIMSET, 0x05, MOD_SPEAR_GUARD_UNK5A, PAL_SPEAR_GUARD, ENEMY_SPEAR_GUARD_THROWN_SPEAR};
EInit g_EInitEnemy3 = {ANIMSET_OVL(1), 0x00, 0x00, PAL_NONE, ENEMY_COMMON};
