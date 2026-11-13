#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: a filter after maspsx (stdin -> stdout) for the mod
overlays built outside the matching build (build_mar.sh, build_mar2.sh).
cc1-psx emits uninitialised statics as `.lcomm sym,size` and maspsx lays
them out in .bss as `sym: .space size` with no alignment (the retail code
pads them by hand, __pad__ symbols), so a mod static after an odd-sized one
sat on an odd address: a misaligned lw/sh, which real hardware traps
(DuckStation lets it pass) and which DMA (StoreImage) rounds down, writing
over the byte before it (seen 2026-10-23). Each .bss label gets the
alignment of its size: 4 for multiples of 4, 2 for even sizes."""
import re
import sys

LABEL = re.compile(r"^([A-Za-z_.$][\w.$]*):\s*$")
SPACE = re.compile(r"^\s*\.space\s+(\d+)\s*$")

lines = sys.stdin.read().split("\n")
out = []
bss = False
for i, line in enumerate(lines):
    s = line.strip()
    if s.startswith(".section") or s in (".text", ".data", ".rdata", ".sdata"):
        bss = s.startswith(".section") and ".bss" in s or s == ".bss"
    elif s == ".bss":
        bss = True
    if bss:
        m = LABEL.match(s)
        if m:
            j = i + 1
            while j < len(lines) and not lines[j].strip():
                j += 1
            sp = SPACE.match(lines[j]) if j < len(lines) else None
            if sp:
                n = int(sp.group(1))
                if n % 4 == 0:
                    out.append("\t.balign 4")
                elif n % 2 == 0:
                    out.append("\t.balign 2")
    out.append(line)
sys.stdout.write("\n".join(out))
