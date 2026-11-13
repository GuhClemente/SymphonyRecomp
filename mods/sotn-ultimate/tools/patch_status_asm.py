#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: move g_Status field offsets in asm-only functions.
#
# The inventory expansion (HAND_ITEMS_CAP/BODY_ITEMS_CAP in include/game.h)
# grows the item arrays inside PlayerStatus, so every later field moves. C code
# follows the struct; functions still built from splat asm hard-code
# `g_Status + 0xNNN`. This rewrites those offsets for the new layout.
#
# Works from pristine copies kept in $MOD_WORK/retail-asm, so running it
# again never shifts an offset twice. Run after `make extract`.
import os
import re
import shutil
import sys
from mod_work import work_dir  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
PRISTINE = work_dir("retail-asm")
ASM = os.path.join(ROOT, "asm/us")

# Retail layout: start offset, element count of each array
RETAIL = {"handCount": (0x026, 169), "bodyCount": (0x0CF, 90),
          "handOrder": (0x129, 169), "bodyOrder": (0x1D2, 90)}
NEW = {"handCount": (0x026, 176), "bodyCount": (0x0D6, 93),
       "handOrder": (0x133, 176), "bodyOrder": (0x1E3, 93)}
TAIL_RETAIL, TAIL_NEW = 0x22C, 0x240  # saveName onwards moves by 0x14
FAMILIARS_END = 0x2E0 + 5 * 12  # stats of the 5 US familiars


def remap(off):
    for name, (start, count) in RETAIL.items():
        if start <= off < start + count:
            return NEW[name][0] + off - start
    if TAIL_RETAIL <= off < FAMILIARS_END:
        return off - TAIL_RETAIL + TAIL_NEW
    if off >= FAMILIARS_END:
        sys.exit(f"g_Status+{off:#x} points at a dropped JP familiar slot")
    return off


pattern = re.compile(r"g_Status \+ 0x([0-9A-Fa-f]+)")
patched = 0
for root, _, files in os.walk(ASM):
    for fn in files:
        if not fn.endswith(".s"):
            continue
        path = os.path.join(root, fn)
        rel = os.path.relpath(path, ASM)
        backup = os.path.join(PRISTINE, rel)
        source = backup if os.path.exists(backup) else path
        text = open(source, errors="ignore").read()
        if not pattern.search(text):
            continue
        new = pattern.sub(lambda m: "g_Status + 0x%X" % remap(int(m.group(1), 16)), text)
        if new == text:
            continue
        if not os.path.exists(backup):
            os.makedirs(os.path.dirname(backup), exist_ok=True)
            shutil.copy2(path, backup)
        if open(path, errors="ignore").read() != new:
            open(path, "w").write(new)
        patched += 1
print(f"g_Status offsets updated in {patched} asm files")
