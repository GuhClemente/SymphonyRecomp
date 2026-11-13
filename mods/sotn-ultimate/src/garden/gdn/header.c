// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gdn.h"

void Update(void);
void ModGdnUpdate(void); // Update plus the red mist, st_update.c
void HitDetection(void);
void UpdateRoomPosition(void);
void InitRoomEntities(s32 objLayoutId);

extern RoomHeader rooms[];
extern SpriteParts* spriteBanks[];
extern u_long* cluts[];
extern RoomDef rooms_layers[];
extern GfxBank* gfxBanks[];

Overlay g_Overlay = {
    .Update = ModGdnUpdate,
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
