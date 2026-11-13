// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_ps1_assets.ts [--work DIR]
// src/ps1_assets.ts: every assets/st path buildStage() reads (all four
// stages), made from the PS1 disc's files (disks/us, as the builder's IsoFs
// would serve them), against the repo's file; then buildStage() per stage
// with that reader, every output against the current build's file.
// Input as test_stage_assets.ts (<work>/<st>/swapped: --work DIR, else a
// temp folder made by stage_work.ts).
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { EXTRACTED, ps1AssetReader, RAW } from "../src/ps1_assets.ts";
import { buildStage } from "../src/stage_assets.ts";
import { ROOT } from "./util.ts";
import { prepareSwapped } from "./stage_work.ts";

const args = process.argv.slice(2);
let workRoot = "";
const wi = args.indexOf("--work");
if (wi >= 0) {
    workRoot = args[wi + 1];
}
if (!workRoot) {
    workRoot = fs.mkdtempSync(path.join(os.tmpdir(), "sotn_stage_work_"));
}
prepareSwapped(workRoot);
const stages = ["gdn", "prs", "rgdn", "rprs"];
const rd = (dir: string) => (p: string): Uint8Array => new Uint8Array(fs.readFileSync(path.join(dir, p)));
const same = (a: Uint8Array, b: Uint8Array): string => {
    const n = Math.min(a.length, b.length);
    for (let i = 0; i < n; i++) {
        if (a[i] !== b[i]) {
            return `DIFFERS at byte 0x${i.toString(16)} (${a[i]} vs ${b[i]})`;
        }
    }
    return a.length === b.length ? "identical" : `DIFFERS: sizes ${a.length} vs ${b.length}`;
};

const discReads: string[] = [];
const reader = ps1AssetReader(async (p: string) => {
    discReads.push(p);
    return new Uint8Array(fs.readFileSync(path.join(ROOT, "disks/us", p)));
});
let bad = 0;

// 1. the paths the four stages read (recorded through the repo reader)
const paths = new Set<string>();
for (const st of stages) {
    await buildStage(st, {
        work: rd(path.join(workRoot, st, "swapped")),
        saturn: rd(path.join(ROOT, "disks/saturn")),
        ps1Asset: (p: string) => {
            paths.add(p);
            return rd(ROOT)(p);
        },
    });
}
console.log(`=== ${paths.size} PS1 asset paths read by the four stages`);
for (const p of [...paths].sort()) {
    const r = same(await reader(p), rd(ROOT)(p));
    bad += r === "identical" ? 0 : 1;
    const how = RAW[p] ? `${RAW[p][0]} @0x${RAW[p][1].toString(16)} +0x${RAW[p][2].toString(16)}` : `WRP.BIN ${EXTRACTED[p]}`;
    console.log(`  ${p}: ${r} (${how})`);
}
console.log(`  disc files read: ${[...new Set(discReads)].sort().join(", ")}`);

// 2. buildStage() with the disc reader
for (const st of stages) {
    const r = await buildStage(st, {
        work: rd(path.join(workRoot, st, "swapped")),
        saturn: rd(path.join(ROOT, "disks/saturn")),
        ps1Asset: reader,
    });
    let n = 0;
    const diffs: string[] = [];
    for (const [p, mine] of r.files) {
        // sprites() rewrites these three afterwards: the oracle is WRP's
        const sub = p.slice(`assets/st/${st}/`.length);
        const ref = ["us/sprite_banks.json", "graphics_banks.json", "palette_def.json"].includes(sub)
            ? "assets/st/wrp/" + sub : p;
        const res = same(mine, rd(ROOT)(ref));
        if (res === "identical") {
            n++;
        } else {
            diffs.push(`${p}: ${res}`);
        }
    }
    bad += diffs.length;
    console.log(`=== buildStage(${st}) with the disc reader: ${n}/${r.files.size} outputs identical` +
                diffs.map((d) => "\n  " + d).join(""));
}
console.log(bad ? `${bad} problems` : "all identical");
process.exit(bad ? 1 : 0);
