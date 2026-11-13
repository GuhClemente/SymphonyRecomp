// SPDX-License-Identifier: AGPL-3.0-or-later
#include "rgdn.h"

// Saturn port: the Hell Garden's collected-item bits 808-815 (the
// Prison's 816-823, the Soul Prison's 824-830; the Garden's are 800-807,
// its boss 831; WRP's 280, which this stage started from, is DRE's)
#define HEART_DROP_CASTLE_FLAG 808

#ifdef VERSION_PSP
#define GOLD_COLLECT_TEXT                                                      \
    _S("$1"), _S("$25"), _S("$50"), _S("$100"), _S("$250"), _S("$400"),        \
        _S("$700"), _S("$1000"), _S("$2000"), _S("$5000"),
#endif

#include "../../../../../src/st/e_collect.h"
