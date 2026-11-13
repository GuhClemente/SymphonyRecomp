#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: BIN/WEAPON2.BIN, the Saturn's new weapons 60-63.

The retail WEAPON0.BIN / WEAPON1.BIN hold weapons 0-58, one 0x7000-byte slot
each (0x4000 of art, a 4bpp 256x128 page, then up to 0x3000 of code and
data loaded at WEAPON0_PTR / WEAPON1_PTR). They can't grow: the files after
them are at positions DRA hard-codes (src/dra/cd.c). So weapons 60-63 are in
a file of their own, last on the disc before XA (config/disk.us.lba), a slot
per weapon and hand: weapon w, hand h at slot (w - 60) * 2 + h
(MOD_FIRST_WEAPON2, src/dra/cd.c).

A weapon is built from mods/sotn-ultimate/src/items/weapon/w_0NN.c when its port exists: this runs
mods/sotn-ultimate/tools/build_saturn_weapon.sh, which converts the Saturn's art and data
(mods/sotn-ultimate/tools/port_saturn_weapon.py) and compiles it once per hand into
build/us/mod/weapons/wHAND_0NN.bin. Until then its slots are a stand-in copy
of a retail weapon (STAND_IN), so equipping it works.
Writes build/us/WEAPON2.BIN (make disk takes build/us files).
"""
import os
import subprocess

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
SLOT = 0x7000
FIRST, LAST = 60, 63
STAND_IN = 3  # a retail one-handed weapon


def retail_slot(hand, weapon):
    data = open(os.path.join(ROOT, "disks/us/BIN/WEAPON%d.BIN" % hand), "rb").read()
    return data[weapon * SLOT:(weapon + 1) * SLOT]


def main():
    ported = [w for w in range(FIRST, LAST + 1)
              if os.path.exists(os.path.join(ROOT, "mods/sotn-ultimate/src/items/weapon/w_%03d.c" % w))]
    if ported:
        subprocess.run(["bash", os.path.join(ROOT, "mods/sotn-ultimate/tools/build_saturn_weapon.sh")]
                       + ["%03d" % w for w in ported], check=True)
    out = bytearray()
    for w in range(FIRST, LAST + 1):
        for hand in (0, 1):
            built = os.path.join(ROOT, "build/us/mod/weapons/w%d_%03d.bin" % (hand, w))
            if w in ported:
                slot = open(built, "rb").read()
                assert len(slot) == SLOT, (built, hex(len(slot)))
                src = "built"
            else:
                slot = retail_slot(hand, STAND_IN)
                src = "stand-in (weapon %d)" % STAND_IN
            out += slot
            if hand == 0:
                print("weapon %d: %s" % (w, src))
    os.makedirs(os.path.join(ROOT, "build/us"), exist_ok=True)
    open(os.path.join(ROOT, "build/us/WEAPON2.BIN"), "wb").write(out)


if __name__ == "__main__":
    main()
