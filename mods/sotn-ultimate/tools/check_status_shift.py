#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: verify the inventory expansion only re-offset g_Status
# accesses in overlays the mod doesn't otherwise touch.
#
# For each built file that differs from retail (except the ones the mod
# changes on purpose), the file must keep its size and every differing
# 32-bit word must be the same MIPS instruction with only its 16-bit
# immediate changed. That is exactly what moving a struct field produces.
import hashlib
import os
import re
import struct
import sys

os.chdir(os.path.join(os.path.dirname(__file__), "../../.."))
# weapon 21 throws food: the mod adds two food sprites; weapon 17 (Bat
# Pentagram shards) follows the Balance row, as do the Saturn hitboxes of
# 1 (Knuckles), 4 (Holy Rod), 5 (Star Flail), 14 (Runesword), 31
# (Basilard), 33 (Nunchaku)
MODDED_WEAPONS = (0, 1, 4, 5, 14, 17, 21, 31, 33)
INTENDED = {"DRA.BIN", "TOP.BIN", "SEL.BIN", "NO4.BIN", "BO3.BIN", "NZ1.BIN", "CAT.BIN", "NP3.BIN", "NO3.BIN", "NO0.BIN", "F_NP3.BIN", "F_NO3.BIN", "WRP.BIN", "F_WRP.BIN", "RNO3.BIN", "F_RNO3.BIN", "RWRP.BIN", "F_RWRP.BIN", "RNO4.BIN", "RNO0.BIN", "NO2.BIN", "ARE.BIN", "TT_005.BIN"}  # changed by the mod beyond offsets
# Richter: the JP v1.2 flame whip fix (src/ric/pl_whip.c)
INTENDED |= {"RIC.BIN"}
# the stages whose red / sealed doors also open while Alucard sprints
# (MOD_SPRINTING in include/game.h): code only, data checked by
# check_stage_data.py
INTENDED |= {"BO0.BIN", "BO1.BIN", "BO2.BIN", "BO4.BIN", "BO5.BIN", "BO6.BIN", "BO7.BIN", "CEN.BIN", "CHI.BIN", "DAI.BIN", "DRE.BIN", "LIB.BIN", "MAR.BIN", "NO1.BIN", "NZ0.BIN", "RARE.BIN", "RBO0.BIN", "RBO1.BIN", "RBO2.BIN", "RBO3.BIN", "RBO4.BIN", "RBO5.BIN", "RBO6.BIN", "RBO7.BIN", "RBO8.BIN", "RCAT.BIN", "RCEN.BIN", "RCHI.BIN", "RDAI.BIN", "RLIB.BIN", "RNO1.BIN", "RNO2.BIN", "RNZ0.BIN", "RNZ1.BIN", "RTOP.BIN", "ST0.BIN"}

retail = {}
for line in open("config/check.us.sha"):
    sha, path = line.split()
    retail[path] = sha

def find_retail(name):
    for root, _, files in os.walk("disks/us"):
        if name in files:
            return os.path.join(root, name)
    return None

status = 0
checked = 0
for path, sha in sorted(retail.items()):
    name = os.path.basename(path)
    built = open(path, "rb").read()
    if hashlib.sha1(built).hexdigest() == sha or name in INTENDED:
        continue
    orig_path = "disks/us/SLUS_000.67" if name == "main.exe" else find_retail(name)
    orig = open(orig_path, "rb").read()
    if name == "WEAPON0.BIN":
        # weapons the mod changes on purpose (0x7000-byte slots per weapon)
        for wid in MODDED_WEAPONS:
            lo, hi = wid * 0x7000, (wid + 1) * 0x7000
            orig = orig[:lo] + bytes(hi - lo) + orig[hi:]
            built = built[:lo] + bytes(hi - lo) + built[hi:]
    if len(orig) != len(built):
        print(f"FAIL {name}: size {len(orig)} -> {len(built)}")
        status = 1
        continue
    bad = []
    for i in range(0, len(orig) - 3, 4):
        a = struct.unpack("<I", orig[i : i + 4])[0]
        b = struct.unpack("<I", built[i : i + 4])[0]
        if a != b and ((a ^ b) & 0xFFFF0000 or a >> 26 == 0):
            bad.append(hex(i))
    changed = sum(1 for i in range(0, len(orig), 4) if orig[i : i + 4] != built[i : i + 4])
    if bad:
        print(f"FAIL {name}: {len(bad)} non-immediate changes, first at {bad[:5]}")
        status = 1
    else:
        checked += 1
        print(f"OK   {name}: {changed} instruction immediates changed")
print(f"{checked} files only re-offset g_Status accesses")
sys.exit(status)
