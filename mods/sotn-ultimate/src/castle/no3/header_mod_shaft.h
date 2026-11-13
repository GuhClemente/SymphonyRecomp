// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/no3/header.c (at file scope, in place)
// Saturn port: room tables with the shaft extended to the Underground Garden
// (src/st/no3/mod_rooms.h, mods/sotn-ultimate/tools/gen_mod_rooms.py) and
// graphics, sprite and palette tables with the shaft's candelabras
// (mod_sconce.h); rodata copies in e_red_door.c. Each room load first puts
// the shaft's tiles into the stage's tileset (ModInitRoomEntities,
// e_red_door_mod_shaft.h).
extern RoomHeader g_ModRooms[];
extern RoomDef g_ModRoomsLayers[];
extern SpriteParts* g_ModSpriteBanks[];
extern u_long* g_ModCluts[];
extern GfxBank* g_ModGfxBanks[];
void ModInitRoomEntities(s32 objLayoutId);
#define MOD_ROOMS g_ModRooms
#define MOD_ROOMS_LAYERS g_ModRoomsLayers
#define MOD_SPRITE_BANKS g_ModSpriteBanks
#define MOD_CLUTS g_ModCluts
#define MOD_GFX_BANKS g_ModGfxBanks
#define MOD_INIT_ROOM_ENTITIES ModInitRoomEntities
