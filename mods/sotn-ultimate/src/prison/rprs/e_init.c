// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rprs.h"

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
void EntityFlyingZombie(Entity*);
void EntityFlyingZombieTorso(Entity*);
void EntityBloodDrips(Entity*);
void EntityBloodSplatter(Entity*);
void EntityJackOBones(Entity*);
void EntityJackOBonesDeathParts(Entity*);
void EntityJackOBonesJack(Entity*);
void EntityNovaSkeleton(Entity*);
void EntityNovaLaser(Entity*);
void EntityNovaLaserPulse(Entity*);
void EntityNovaSkeletonDeathParts(Entity*);
void EntityModGargoyle(Entity*);
void EntityModGargoyleDebris(Entity*);
void EntityModGargoylePedestal(Entity*);
void EntityModWight(Entity*);
void EntityModWightSword(Entity*);
void EntityModWightTrail(Entity*);
void EntityModWraith(Entity*);
void EntityModWraithAfterimage(Entity*);
void EntityModWraithDrain(Entity*);
void EntityModWraithWisps(Entity*);

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
    EntityFlyingZombie,
    EntityFlyingZombieTorso,
    EntityBloodDrips,
    EntityBloodSplatter,
    EntityJackOBones,
    EntityJackOBonesDeathParts,
    EntityJackOBonesJack,
    EntityNovaSkeleton,
    EntityNovaLaser,
    EntityNovaLaserPulse,
    EntityNovaSkeletonDeathParts,
    EntityModGargoyle,
    EntityModGargoyleDebris,
    EntityModGargoylePedestal,
    EntityModWight,
    EntityModWightSword,
    EntityModWightTrail,
    EntityModWraith,
    EntityModWraithAfterimage,
    EntityModWraithDrain,
    EntityModWraithWisps,
};

// clang-format off
EInit g_EInitBreakable = {ANIMSET_OVL(1), 0x00, 0x00, PAL_NONE, ENEMY_BREAKABLE};
#include "../../../../../src/st/e_init_common.h"
// Saturn port: the PS1 enemies, as in RNO2 and RNO3 but for the art's place
// (MOD_*_ANIMSET / _UNK5A, mod_psx_sprites.h)
EInit g_EInitFlyingZombie = {MOD_FLYING_ZOMBIE_ANIMSET, 0x01, MOD_FLYING_ZOMBIE_UNK5A, PAL_FLYING_ZOMBIE, ENEMY_FLYING_ZOMBIE_2};
EInit g_EInitFlyingZombieTorso = {MOD_FLYING_ZOMBIE_ANIMSET, 0x00, MOD_FLYING_ZOMBIE_UNK5A, PAL_FLYING_ZOMBIE, ENEMY_FLYING_ZOMBIE_1};
EInit g_EInitJackOBones = {MOD_JACK_O_BONES_ANIMSET, 0x01, MOD_JACK_O_BONES_UNK5A, PAL_JACK_O_BONES, ENEMY_JACK_O_BONES};
EInit g_EInitJackOBones2 = {MOD_JACK_O_BONES_ANIMSET, 0x00, MOD_JACK_O_BONES_UNK5A, PAL_JACK_O_BONES, ENEMY_PARTICLE};
EInit g_EInitJackOBones3 = {MOD_JACK_O_BONES_ANIMSET, 0x15, MOD_JACK_O_BONES_UNK5A, PAL_JACK_O_BONES, ENEMY_JACK_O_BONES_3};
EInit g_EInitNovaSkeleton = {MOD_NOVA_SKELETON_ANIMSET, 0x01, MOD_NOVA_SKELETON_UNK5A, PAL_NOVA_SKELETON, ENEMY_NOVA_SKELETON};
EInit g_EInitNovaSkeleton2 = {MOD_NOVA_SKELETON_ANIMSET, 0x00, MOD_NOVA_SKELETON_UNK5A, PAL_NOVA_SKELETON, ENEMY_NOVA_SKELETON_LASER};
EInit g_EInitEnemy3 = {ANIMSET_OVL(1), 0x00, 0x00, PAL_NONE, ENEMY_COMMON};
