// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by the Entrance stages' e_red_door_mod_shaft.h
// (NP3, NO3; at file scope, in place)
// The moved save room (3,43) (mods/sotn-ultimate/tools/gen_mod_rooms.py): its
// left doorway as the shaft alcove's opening outside (local rows 6-10, floor at
// 11). The save room picture is DRA animset 0xC, frame 2 here
// (E_ROOM_FOREGROUND params 1); its part 5 (u 200-256, v 0-128, mirrored) is
// the left wall: the doorway's lattice window and the floor ledge under it. The
// room's door is a tile lower than retail's, so this copy draws part 5 in four
// pieces: the window 16 px higher (its frame goes under the capital above, the
// lattice then starts at the alcove's ceiling), one lattice period (16 rows)
// twice more, and the floor ledge with what is below it 16 px lower, at the
// door's floor. Bank index 14 (e_room_fg.c).
s16 g_ModSaveRoomFrame[] MOD_RO = {
    11, 34, -29, -16, 32, 32, 419, 112, 128, 0, 160, 32, 32, -3, -16, 32, 32,
    419, 112, 128, 0, 160, 32, 2, -72, -15, 72, 32, 419, 112, 128, 32, 200, 64,
    2, -129, -15, 72, 64, 419, 112, 128, 64, 200, 128,
    // the window, 16 px up
    6, -129, 29, 56, 67, 419, 112, 200, 0, 256, 67,
    // its last lattice period in place, then again below it
    6, -129, 96, 56, 16, 419, 112, 200, 51, 256, 67, 6, -129, 112, 56, 16, 419,
    112, 200, 51, 256, 67,
    // the floor ledge and below, 16 px down
    6, -129, 128, 56, 61, 419, 112, 200, 67, 256, 128, 0, 0, -15, 72, 32, 419,
    112, 128, 32, 200, 64, 0, 57, -15, 72, 64, 419, 112, 128, 64, 200, 128, 36,
    73, 45, 56, 128, 419, 112, 200, 0, 256, 128, 0};
s16* g_ModSaveRoomSprites[] MOD_RO = {0, 0, g_ModSaveRoomFrame};
