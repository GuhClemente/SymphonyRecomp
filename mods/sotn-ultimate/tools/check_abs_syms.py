#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: stale absolute symbols in the overlays' links.

splat writes every address the asm uses inside a symbol (e.g. the
cutscenes' g_Dialogue + 0x16 as D_us_801B5552) to
build/us/config/undefined_syms_auto.us.<ovl>.txt as an absolute value. When
the mod moves an overlay's BSS (the PSP script copies, g_ModPspSpill, grow
its rodata) those still point at the retail address: BO5's EntityCutscene
then read its script pointer from the wrong place and ended at once (no
Hippogryph scene, castle flag 0x62 set, doors shut); BO2, BO6, RBO2 and RBO6
had the same bug.

mods/sotn-ultimate/config/mod_rel_syms.us.<ovl>.txt (linked after the auto file by
tools/builds/gen.py; the last assignment wins) reassigns them relative to
their retail symbol (from expected/build/us/<ovl>.map).

  check_abs_syms.py           report stale ones the linked objects use (exit 1)
  check_abs_syms.py --write   (re)write the config files for those, then
                              relink (./sotn.sh build us) and check again
"""
import bisect
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
LO, HI = 0x80170000, 0x80200000  # overlay space


def map_syms(path):
    syms = {}
    for line in open(path, errors="replace"):
        m = re.match(r"^\s+0x([0-9a-f]{8})\s+([A-Za-z_][A-Za-z0-9_]*)\s*$", line)
        if m:
            a = int(m.group(1), 16)
            if LO <= a < HI:
                syms.setdefault(m.group(2), a)
    return syms


def link_objects(ovl):
    objs, on = [], False
    for line in open(os.path.join(ROOT, "build.ninja")):
        if line.startswith("build build/us/%s.elf:" % ovl):
            on = True
            continue
        if on and line.startswith("build "):
            break
        if on:
            objs += [t for t in line.replace("$", " ").replace("|", " ").split()
                     if t.endswith(".o")]
    return objs


def undefined_in(objs):
    names = set()
    for o in objs:
        p = os.path.join(ROOT, o)
        if not os.path.exists(p):
            continue
        out = subprocess.run(["mipsel-linux-gnu-nm", "-u", p],
                             capture_output=True, text=True).stdout
        names.update(l.split()[-1] for l in out.splitlines() if l.strip())
    return names


def stale(ovl):
    auto = os.path.join(ROOT, "build/us/config/undefined_syms_auto.us.%s.txt" % ovl)
    bmap = os.path.join(ROOT, "build/us/%s.map" % ovl)
    emap = os.path.join(ROOT, "expected/build/us/%s.map" % ovl)
    if not (os.path.exists(bmap) and os.path.exists(emap)):
        return []
    B, E = map_syms(bmap), map_syms(emap)
    ea = sorted((a, n) for n, a in E.items())
    keys = [a for a, _ in ea]
    cand = []
    for line in open(auto):
        m = re.match(r"\s*(\w+)\s*=\s*0x([0-9A-Fa-f]+);", line)
        if not m:
            continue
        a = int(m.group(2), 16)
        if not (LO <= a < HI):
            continue
        i = bisect.bisect_right(keys, a) - 1
        if i < 0:
            continue
        ca, cn = ea[i]
        cand.append((m.group(1), a, cn, a - ca))
    if not cand:
        return []
    used = undefined_in(link_objects(ovl))
    linked = {}
    nm = subprocess.run(["mipsel-linux-gnu-nm", os.path.join(ROOT, "build/us/%s.elf" % ovl)],
                        capture_output=True, text=True).stdout
    for l in nm.splitlines():
        t = l.split()
        if len(t) == 3:
            linked[t[2]] = int(t[0], 16)
    out = []
    for name, a, cn, off in cand:
        if name not in used or cn not in B:
            continue
        now = linked.get(name)
        want = B[cn] + off
        out.append((name, a, cn, off, now, want))
    return out


def main():
    write = "--write" in sys.argv
    bad = 0
    for auto in sorted(glob.glob(os.path.join(
            ROOT, "build/us/config/undefined_syms_auto.us.*.txt"))):
        ovl = os.path.basename(auto)[len("undefined_syms_auto.us."):-4]
        rows = stale(ovl)
        cfg = os.path.join(ROOT, "mods/sotn-ultimate/config/mod_rel_syms.us.%s.txt" % ovl)
        wrong = [r for r in rows if r[4] != r[5]]
        if write and (wrong or os.path.exists(cfg)):
            keep = [r for r in rows if r[2] != r[0]]
            with open(cfg, "w") as f:
                f.write("/* mods/sotn-ultimate/tools/check_abs_syms.py --write: splat's absolute\n"
                        "   addresses inside a symbol, relative to it */\n")
                for name, a, cn, off, _, _ in keep:
                    f.write("%s = %s + 0x%X; /* retail 0x%08X */\n" % (name, cn, off, a))
            print("%s: %d symbols -> %s" % (ovl, len(keep), os.path.relpath(cfg, ROOT)))
        for name, a, cn, off, now, want in wrong:
            bad += 1
            print("FAIL %s: %s = %s + 0x%X: linked %s, should be %08X" % (
                ovl, name, cn, off, "%08X" % now if now is not None else "?", want))
    if not write:
        print("abs syms: %s" % ("%d stale" % bad if bad else "OK"))
    return 1 if bad and not write else 0


if __name__ == "__main__":
    sys.exit(main())
