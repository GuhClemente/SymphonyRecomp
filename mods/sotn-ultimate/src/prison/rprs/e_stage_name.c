// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rprs.h"

// Saturn port: the Saturn's inverted stages have no area-name popup (no
// layout id 0x09 in RSTAGE16): EntityStageNamePopup is never placed

#ifdef VERSION_US
#include "../../../../../src/st/e_stage_name_us.h"
#endif

#if defined(VERSION_PSP) || defined(VERSION_HD)
#include "../../../../../src/st/e_stage_name_jp.h"
#endif
