#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""The builder's manifest data for the PSP sound banks (src/psp_sfx.ts), as
mods/sotn-ultimate/tools/gen_psp_sfx.py's main() sets them up: the PS.ELF slot lists of the
packs used (from the US UMD's PS.ELF), the bank jobs in build
order (disc paths) and the notes sfx_config.yaml keys each (vabid, prog,
tone) with. Prints JSON."""
import glob
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../../.."))
sys.path.insert(0, os.path.join(ROOT, "mods/sotn-ultimate/tools"))
import gen_psp_sfx as S  # noqa: E402
import yaml  # noqa: E402

US = os.path.join(ROOT, "disks/us")


def main():
    tables = S.psp_tables()
    jobs = []
    for tok, pack in sorted(S.stages().items()):
        f = glob.glob(os.path.join(US, "*/%s/SD_Z*.VH" % tok))
        if tok in S.SKIP_STAGES or not f or "sd_" + pack not in tables:
            continue
        src = os.path.relpath(f[0][:-3], US)
        # a mod tool's bank (S.MODDED) takes the retail file's place on the disc
        if tok in S.MODDED:
            assert S.MODDED[tok] == "build/us/" + os.path.basename(src), tok
        jobs.append([tok, src, tok in S.MODDED, pack, 3])
    for i, (name, pack) in enumerate(S.SERVANTS):
        jobs.append(["srv%d" % i, "SERVANT/" + name, False, pack, 2])
    notes = {}
    for e in yaml.safe_load(open(os.path.join(ROOT, "assets/dra/us/sfx_config.yaml"))):
        if e["vabid"] in (2, 3):
            notes.setdefault("%d,%d,%d" % (e["vabid"], e["prog"], e["tone"]), set()).add(e["note"])
    # + sd_mar: the JP SD_MAR (gen_psp_grunts.jp_sd_mar)
    packs = sorted({j[3] for j in jobs} | {"mar"})
    saturn = [[tok, base, [list(c) for c in copies]]
              for tok, (base, copies) in sorted(S.SATURN_COPIES.items())]
    print(json.dumps({
        "tables": {p: tables["sd_" + p] for p in packs},
        "jobs": jobs,
        "notes": {k: sorted(v) for k, v in notes.items()},
        "saturn": saturn,
    }))


if __name__ == "__main__":
    main()
