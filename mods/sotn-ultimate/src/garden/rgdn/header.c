// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rgdn.h"

void Update(void);
void ModRgdnUpdate(void); // Update plus the red mist, st_update.c
void HitDetection(void);
void UpdateRoomPosition(void);
void InitRoomEntities(s32 objLayoutId);

extern RoomHeader rooms[];
extern SpriteParts* spriteBanks[];
extern u_long* cluts[];
extern RoomDef rooms_layers[];
extern GfxBank* gfxBanks[];

Overlay g_Overlay = {
    .Update = ModRgdnUpdate,
    .HitDetection = HitDetection,
    .UpdateRoomPosition = UpdateRoomPosition,
    .InitRoomEntities = InitRoomEntities,
    .rooms = rooms,
    .spriteBanks = spriteBanks,
    .cluts = cluts,
    .objLayoutHorizontal = NULL,
    .tileLayers = rooms_layers,
    .gfxBanks = gfxBanks,
    .UpdateStageEntities = UpdateStageEntities,
};
