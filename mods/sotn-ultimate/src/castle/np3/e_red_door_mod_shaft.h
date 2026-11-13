// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/np3/e_red_door.c (at file scope, in place)
// Saturn port: room table copies (rodata, after the stage's data section,
// which cutscene scripts address directly and so must not move)
#include "../../../../../src/st/np3/mod_rooms.h"
#include "../../../../../src/st/np3/mod_layout.h"
#include "../../../../../src/st/np3/mod_layout2.h"
// and the graphics, sprite and palette tables with the shaft's wall
// sconce (mods/sotn-ultimate/tools/gen_np3_sconce.py), which header.c points at
#define MOD_SCONCE_TABLES
#include "../../../../../src/st/np3/mod_sconce.h"
// the moved save room's picture
#include "../mod_save_room_frame.h"
