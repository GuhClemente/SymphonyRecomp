// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rgdn.h"

// Owned by EntityRedDoor to animate the tiles behind the door itself.
// There is a loop in EntityRedDoor that forces to write those tiles
// at every frame based on the door state to create the animation.
// Saturn port: the Hell Garden's own door tiles
// (mods/sotn-ultimate/tools/rgdn_assets.py)
#include "mod_red_door_tiles.h"
