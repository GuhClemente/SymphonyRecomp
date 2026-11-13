#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Checks the PSP port's menu text (mods/sotn-ultimate/tools/gen_menu_text.py) against the
built DRA: SEL's g_MenuStr address (src/st/sel/2C048.c MOD_MENU_STR) is
DRA's, and at every slot SEL redirects or DRA rewrites, DRA holds exactly
the retail bytes the entry was made for (build/us/menu_text.json), so the
text lands on the right string. Prints one OK / FAIL line."""
import json
import os
import re
import struct
import subprocess
import sys

from mod_work import read_src

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
DRA = open(os.path.join(ROOT, "build/us/DRA.BIN"), "rb").read()
BASE = 0x800A0000
nm = subprocess.run(["mipsel-linux-gnu-nm", os.path.join(ROOT, "build/us/dra.elf")],
                    capture_output=True, text=True).stdout
sym = {p[2]: int(p[0], 16) for p in (l.split() for l in nm.splitlines()) if len(p) == 3}
data = json.load(open(os.path.join(ROOT, "build/us/menu_text.json")))
# SEL has one address per build kind (normal / MOD_SATURN_STAGE_TEST): the
# built DRA must match one of them
sel = re.findall(r"#define MOD_MENU_STR \(\(u8\*\)(0x[0-9A-F]+)\)",
                 read_src("src/st/sel/2C048.c"))
bad = []
if sym.get("g_MenuStr") not in [int(a, 16) for a in sel]:
    bad.append("SEL MOD_MENU_STR %s, DRA g_MenuStr 0x%X" % (" / ".join(sel), sym.get("g_MenuStr", 0)))
tables = [sym["g_EquipDefs"], sym["g_AccessoryDefs"], sym["g_EnemyDefs"], sym["g_RelicDefs"], sym["g_MenuStr"]]
n = 0
for kind in ("fixed", "dub"):
    for table, off, old in data[kind]:
        want = bytes.fromhex(old)
        ptr = struct.unpack_from("<I", DRA, tables[table] + off - BASE)[0]
        have = DRA[ptr - BASE:ptr - BASE + len(want)] if BASE <= ptr < BASE + len(DRA) else b""
        if have != want:
            bad.append("%s table %d +0x%X: DRA has %s, expected %s" % (kind, table, off, have.hex(), want.hex()))
        n += 1
if bad:
    print("FAIL menu text: " + "; ".join(bad))
    sys.exit(1)
print("OK   menu text: %d fixed + %d dub-row strings land on their retail bytes; g_MenuStr at 0x%X"
      % (len(data["fixed"]), len(data["dub"]), sym["g_MenuStr"]))
