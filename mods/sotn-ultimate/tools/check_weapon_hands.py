#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: every weapon slot the build changed in WEAPON0.BIN (the
right hand, compiled by the decomp) must be rebuilt in WEAPON1.BIN (the left
hand, retail + mods/sotn-ultimate/tools/build_weapon1.sh). A retail left-hand copy of a
changed weapon runs retail code: e.g. against the mod's g_Status layout
(HAND_ITEMS_CAP moved fields by 0x14) the Alucard Shield's Shield Rod spell
read MP from the learned-spells field and ended at once (v1.1).
Prints OK, or FAIL with the weapon ids to add to build_weapon1.sh."""
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
SLOT = 0x7000


def read(p):
    return open(os.path.join(ROOT, p), "rb").read()


r0, b0 = read("disks/us/BIN/WEAPON0.BIN"), read("build/us/WEAPON0.BIN")
r1, b1 = read("disks/us/BIN/WEAPON1.BIN"), read("build/us/WEAPON1.BIN")
stale = []
for w in range(len(r0) // SLOT):
    s = slice(w * SLOT, (w + 1) * SLOT)
    if r0[s] != b0[s] and r1[s] == b1[s]:
        stale.append(w)
if stale:
    print("FAIL weapons changed in WEAPON0 but retail in WEAPON1: %s "
          "(add them to build_weapon1.sh in build_disc.sh)" % " ".join("%03d" % w for w in stale))
    sys.exit(1)
print("OK   WEAPON1 rebuilds every weapon the mod changes in WEAPON0")
