// SPDX-License-Identifier: AGPL-3.0-or-later
#include "prs.h"

// Saturn port: the Saturn Prison's popup flag (g_CastleFlags[0xF8], unused
// by the PS1)
#define STAGE_FLAG 0xF8

#ifdef VERSION_US
#include "../../../../../src/st/e_stage_name_us.h"
#endif

#if defined(VERSION_PSP) || defined(VERSION_HD)
#include "../../../../../src/st/e_stage_name_jp.h"
#endif
