// SPDX-License-Identifier: AGPL-3.0-or-later
#include "gdn.h"

// Saturn port: the Saturn Garden's popup flag (func_060E4064:
// g_CastleFlags[0xF0], unused by the PS1)
#define STAGE_FLAG 0xF0

#ifdef VERSION_US
#include "../../../../../src/st/e_stage_name_us.h"
#endif

#if defined(VERSION_PSP) || defined(VERSION_HD)
#include "../../../../../src/st/e_stage_name_jp.h"
#endif
