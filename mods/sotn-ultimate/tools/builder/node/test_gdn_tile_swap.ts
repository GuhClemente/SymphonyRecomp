// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_gdn_tile_swap.ts
// src/gdn_tile_swap.ts against mods/sotn-ultimate/tools/gdn_tile_swap.py: per stage (gdn,
// rgdn, prs, rprs) the native Saturn map converter writes its output to a
// temp folder (<stage>/conv); one copy gets the Python apply() (with the
// atlases it reads: $MOD_WORK/tile_atlases, tile_atlases.ts), the other, in
// memory, the TS port with the atlases rebuilt from disks/
// (gdn_tile_swap_recipe.ts); every file must match byte for byte, and the
// rebuilt atlases must be the approved art (APPROVED: md5 of its tiles).
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import { applyTileSwap, STAGE_PLANS, type WorkFiles } from "../src/gdn_tile_swap.ts";
import { md5 } from "../src/md5.ts";
import { convertStage, readDir, rebuildAtlases, STAGE_MAPS } from "./stage_work.ts";
import { ROOT } from "./util.ts";

const TMP = fs.mkdtempSync(path.join(os.tmpdir(), "sotn_tile_swap_"));
const PY = path.join(ROOT, ".venv/bin/python");
// recipe set -> md5 of its atlas's tiles (16x16 RGBA each, in order): the
// art approved in review (2026-10-26 / 27)
const APPROVED: Record<string, string> = {
    gdn: "e4187c9756405121d31ca149d9506051", rgdn: "11e7172a1259f6e20a8f6fc1c6dccfb8",
    prs: "a32acbdfbba44f25d46a57a53bdc66a1", rprs: "2360ffca8894f5c2db1ae4028f6e562b",
};
const PY_CALL: Record<string, string> = {
    gdn: "g.apply(W, False)",
    rgdn: "g.apply(W, True)",
    prs: "g.apply(W, data=g.PRS_DATA, switch=g.PRS_SWITCH, own='', empty=True)",
    rprs: "g.apply(W, data=g.PRS_DATA, switch=g.PRS_SWITCH, own='_rprs', empty=True)",
};

const same = (a: Uint8Array, b: Uint8Array): boolean => a.length === b.length && a.every((v, i) => v === b[i]);
let bad = 0;

// 1. the converter output, pristine
const conv: Record<string, WorkFiles> = {};
for (const st of Object.keys(STAGE_MAPS)) {
    conv[st] = convertStage(st, path.join(TMP, st, "conv"));
}

// 2. the atlases, rebuilt from the discs: the approved art
const t0 = performance.now();
const atlases = rebuildAtlases(conv);
const ms0 = performance.now() - t0;
for (const [set, tiles] of Object.entries(atlases)) {
    const raw = new Uint8Array(tiles.length * 1024);
    tiles.forEach((t, i) => raw.set(t, i * 1024));
    const ok = md5(raw) === APPROVED[set];
    bad += ok ? 0 : 1;
    console.log("atlas %s: %d tiles, md5 %s%s", set, tiles.length, md5(raw), ok ? " APPROVED" : " DIFFERS");
}
console.log("atlases rebuilt in %s ms", ms0.toFixed(0));

// 3. apply: Python on a copy of the folder, TS in memory
for (const st of Object.keys(STAGE_PLANS)) {
    const pyDir = path.join(TMP, st, "py");
    fs.cpSync(path.join(TMP, st, "conv"), pyDir, { recursive: true });
    const stats = execFileSync(PY, ["-c", "import json, sys; sys.path.insert(0, sys.argv[1]); " +
        "import gdn_tile_swap as g; W = sys.argv[2]; print(json.dumps(" + PY_CALL[st] + "))",
                                    path.join(ROOT, "mods/sotn-ultimate/tools"), pyDir], { encoding: "utf8" }).trim();
    const work = readDir(path.join(TMP, st, "conv"));
    const t0 = performance.now();
    const r = applyTileSwap(st, work, atlases);
    const ms = performance.now() - t0;
    const py = readDir(pyDir);
    const names = new Set([...py.keys(), ...work.keys()]);
    let differ = 0;
    const pyChanged: string[] = [];
    for (const n of names) {
        const a = py.get(n);
        const b = work.get(n);
        if (!a || !b || !same(a, b)) {
            differ++;
            console.log("  %s: %s DIFFERS", st, n);
        }
        const c = conv[st].get(n);
        if (a && (!c || !same(a, c))) {
            pyChanged.push(n);
        }
    }
    const statsOk = JSON.stringify(JSON.parse(stats)) === JSON.stringify(r.stats);
    const ok = differ === 0 && statsOk;
    bad += ok ? 0 : 1;
    console.log("%s: %d files compared, %d differ; changed %s; stats %s%s; %s ms%s", st, names.size, differ,
                pyChanged.sort().join(" "), JSON.stringify(r.stats), statsOk ? "" : " (python " + stats + ")",
                ms.toFixed(0), ok ? " IDENTICAL" : " DIFFER");
}
fs.rmSync(TMP, { recursive: true });
console.log(bad ? `${bad} FAILED` : "all identical");
process.exitCode = bad ? 1 : 0;
