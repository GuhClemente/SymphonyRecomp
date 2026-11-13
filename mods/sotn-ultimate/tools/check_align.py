#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: finds misaligned memory accesses to fixed addresses in
built code. cc1-psx emits uninitialised statics as `.lcomm sym,size` and
maspsx lays them out as `.space` with no alignment (retail code pads them
by hand), so a mod static after an odd-sized one can sit on an odd address.
The PS1 raises an address error on a misaligned lw/sw/lh/sh (DuckStation
lets it pass): a crash on real hardware only.
Scans `lui rX, hi` + `lw/sw/lh/lhu/sh rY, lo(rX)` pairs (array accesses
through a computed base are not followed). usage: check_align.py ELF... ; exit 1 on a hit."""
import re
import subprocess
import sys

INS = re.compile(r"^\s*([0-9a-f]+):\s+[0-9a-f]{8}\s+(\w+)\s+(.*)$")
SIZE = {"lw": 4, "sw": 4, "lh": 2, "lhu": 2, "sh": 2}


def scan(elf):
    out = subprocess.run(["mipsel-linux-gnu-objdump", "-d", elf],
                         capture_output=True, text=True).stdout
    nm = subprocess.run(["mipsel-linux-gnu-nm", "-n", elf], capture_output=True, text=True).stdout
    syms = []
    for l in nm.splitlines():
        p = l.split()
        if len(p) == 3:
            syms.append((int(p[0], 16), p[2]))
    hits = {}
    lui = {}
    for line in out.splitlines():
        m = INS.match(line)
        if not m:
            if line.endswith(">:"):
                lui = {}  # a new function
            continue
        pc, op, args = int(m.group(1), 16), m.group(2), m.group(3)
        a = [s.strip() for s in args.split(",")]
        if op == "lui" and len(a) == 2:
            lui[a[0]] = int(a[1], 0) << 16
            continue
        if op in SIZE and len(a) == 2:
            mm = re.match(r"(-?\d+)\((\w+)\)", a[1])
            if mm and mm.group(2) in lui:
                addr = (lui[mm.group(2)] + int(mm.group(1))) & 0xFFFFFFFF
                if 0x80010000 <= addr < 0x80200000 and addr % SIZE[op]:
                    name = max((s for s in syms if s[0] <= addr), default=(0, "?"))
                    hits.setdefault(addr, (op, pc, name[1], addr - name[0]))
        if a and a[0] in lui and op not in ("sw", "sh", "sb"):
            del lui[a[0]]  # the register was overwritten
    return hits


bad = 0
for elf in sys.argv[1:]:
    hits = scan(elf)
    for addr, (op, pc, name, off) in sorted(hits.items()):
        print("%s: %s at %08x touches %08x (%s+%d), misaligned" % (elf, op, pc, addr, name, off))
    bad += len(hits)
    print("%s: %d misaligned fixed-address accesses" % (elf, len(hits)))
sys.exit(1 if bad else 0)
