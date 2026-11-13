// SPDX-License-Identifier: AGPL-3.0-or-later
// SOTN Ultimate: included by src/st/wrp/warp.c (at file scope, in place)
// Saturn port: the Saturn's six warp rooms (WARP.PRG 0x060E1FA4). The sixth,
// next to the Underground Garden, joins the cycle once visited (its
// WRP_UNLOCKS bit, 1 << 5). The retail table stays in the data section.
static const Point16 ModWarpRoomCoords[] = {
    {0x0F, 0x26}, // Entrance
    {0x23, 0x2C}, // Abandoned pit to the Catacomb
    {0x3B, 0x11}, // Outer Wall
    {0x28, 0x0C}, // Castle Keep
    {0x25, 0x15}, // Olrox's Quarters
    {0x02, 0x2D}, // Underground Garden
};
#define WARP_ROOM_COORDS ModWarpRoomCoords
