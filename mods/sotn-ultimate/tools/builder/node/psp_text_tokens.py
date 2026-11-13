#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""The PSP script text the release blanks (mods/sotn-ultimate/tools/builder/node/
psp_text_scan.ts): every TEXT token mods/sotn-ultimate/tools/psp_script.py wrote (its
build/us/psp_script/patches.json copies, PSP and JP, and CEN's C-array copies
in src/st/cen/mod_psp_script.h, disassembled), and the disc files that hold
the copies with the retail overlay each was built from. Prints JSON."""
import json
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../../.."))
sys.path.insert(0, os.path.join(ROOT, "mods/sotn-ultimate/tools"))
import psp_script as P  # noqa: E402

# overlays copied after the script went in (build_disc.sh): disc file -> retail
COPIES = {"BIN/CENP.BIN": "ST/CEN/CEN.BIN", "BIN/BO6RIC2.BIN": "BOSS/BO6/BO6.BIN",
          "BIN/SELRIC2.BIN": "ST/SEL/SEL.BIN"}


def walk(o, out):
    if isinstance(o, dict):
        for k, v in o.items():
            if k in ("tokens", "jp_tokens"):
                out.update(t[1] for t in v if t[0] == "TEXT")
            else:
                walk(v, out)
    elif isinstance(o, list):
        for x in o:
            walk(x, out)


def main():
    tokens = set()
    walk(json.load(open(P.PATCHES)), tokens)
    src = open(os.path.join(ROOT, "src/st/cen/mod_psp_script.h")).read()
    for m in re.finditer(r"u8 g_Mod(?:Psp|Jp)Copy\d+\[\][^=]*= \{(.*?)\};", src, re.S):
        b = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]+)", m.group(1)))
        toks, _ = P.disassemble(b, 0)
        tokens.update(t[1] for t in toks if t[0] == "TEXT")
    files = {retail: retail for retail, _, _ in P.OVERLAYS.values()}
    files.update(COPIES)
    print(json.dumps({"tokens": sorted(tokens), "files": files}))


if __name__ == "__main__":
    main()
