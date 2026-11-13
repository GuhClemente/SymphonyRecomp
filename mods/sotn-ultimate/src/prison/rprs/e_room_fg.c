// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../../../../../src/st/e_room_fg.h"

#include "rprs.h"

// Saturn port: the Nova Skeleton (Saturn RSTAGE16 0x26, RNO3's PS1 code; the
// laser's primitives from its art's tpage, PAL_NOVA_SKELETON + 2 / + 3)
#define NOVA_TPAGE MOD_NOVA_SKELETON_TPAGE
#define NOVA_CLUT (PAL_NOVA_SKELETON + 2)
#include "../../../../../src/st/e_nova_skeleton.h"
