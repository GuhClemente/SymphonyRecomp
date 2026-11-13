// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/rno3/e_red_door.c (at file scope, in place)
// Saturn port: room table copies, the shaft's layout and the graphics,
// sprite and palette tables with its candelabras (rodata, after the stage's
// data section, which must not move)
#include "../../../../../src/st/rno3/mod_rooms.h"
#include "../../../../../src/st/rno3/mod_layout.h"
#define MOD_SCONCE_TABLES
#include "../../../../../src/st/rno3/mod_sconce.h"

// The moved save room (60,20) (mods/sotn-ultimate/tools/gen_mod_rooms.py): its
// right doorway as tall as the shaft alcove's opening outside (local rows 5-9).
// The save room picture is DRA animset 0xC, frame 3 here (E_ROOM_FOREGROUND
// params 4); its part 8 (u 200-256, v 0-128, drawn turned upside down) is the
// right wall: upper lattice window, the ledge under it, the doorway's lattice
// window, the floor ledge. This copy draws part 8 in four pieces: the ledge
// (and the upper window's frame) 16 px higher, over the window's last 16
// lattice rows, and a 16-row band of the doorway's lattice (one pattern
// period) in the ledge's old place. Bank index 14 (MOD_SAVE_ROOM_ANIMSET).
s16 g_ModSaveRoomFrame[] MOD_RO = {
    11, 51, -29, -15, 32, 32, 419, 112, 128, 0, 160, 32, 49, -3, -15, 32, 32,
    419, 112, 128, 0, 160, 32, 19, -72, -16, 72, 32, 419, 112, 128, 32, 200, 64,
    27, -129, -48, 72, 64, 419, 112, 128, 64, 200, 128, 47, -129, -172, 56, 128,
    419, 112, 200, 0, 256, 128, 17, 0, -16, 72, 32, 419, 112, 128, 32, 200, 64,
    25, 57, -48, 72, 64, 419, 112, 128, 64, 200, 128,
    // the doorway window and the floor ledge, in place (from v 1: drawn
    // upside down, v 0 would wrap)
    5, 73, -111, 56, 66, 419, 112, 200, 1, 256, 67,
    // a lattice band where the ledge was
    5, 73, -127, 56, 16, 419, 112, 200, 51, 256, 67,
    // the ledge and the upper window's frame, 16 px up
    5, 73, -144, 56, 17, 419, 112, 200, 67, 256, 84,
    // the upper window's remaining lattice, in place
    5, 73, -171, 56, 27, 419, 112, 200, 100, 256, 127, 0};
s16* g_ModSaveRoomSprites[] MOD_RO = {0, 0, 0, g_ModSaveRoomFrame};
