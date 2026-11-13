// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/no3/e_red_door.c (at file scope, in place)
// Saturn port: the shaft to the Underground Garden, as NP3's
// (mods/sotn-ultimate/src/castle/np3/e_red_door_mod_shaft.h): room table
// copies (rodata: NO3's data section must not move), the shaft's entity
// list, the candelabras' graphics, sprite and palette tables (header.c
// points at them) and the moved save room's picture
#include "../../../../../src/st/no3/mod_rooms.h"
#include "../../../../../src/st/no3/mod_layout2.h"
#define MOD_SCONCE_TABLES
#include "../../../../../src/st/no3/mod_sconce.h"
#include "../mod_save_room_frame.h"

// NO3 has no room for a tileset copy (gen_mod_rooms.py): the shaft's tiles
// go into empty slots of the stage's own, as each room loads (the overlay's
// data is reloaded with it)
void ModInitRoomEntities(s32 objLayoutId) {
    TileDefinition* td = MOD_TILEDEF_PATCH;
    const u16* p;

    for (p = g_ModTiledefPatch[0]; p < g_ModTiledefPatch[LEN(g_ModTiledefPatch)];
         p += 5) {
        td->gfxPage[p[0]] = p[1];
        td->gfxIndex[p[0]] = p[2];
        td->clut[p[0]] = p[3];
        td->collision[p[0]] = p[4];
    }
    InitRoomEntities(objLayoutId);
}
