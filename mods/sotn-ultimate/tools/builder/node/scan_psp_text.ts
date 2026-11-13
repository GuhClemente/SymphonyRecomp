// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/scan_psp_text.ts FULL.bin PSP.iso
// Where the PSP script text lands on the finished disc (node/psp_text_scan.ts),
// and which of psp_script.py's TEXT tokens stay in the patch (in the retail
// PS1 overlay too, or not PSPBIN text: the mod's own words).
import * as fs from "node:fs";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import { IsoFs } from "../src/disc.ts";
import { md5 } from "../src/md5.ts";
import { PspTextFinder } from "./psp_text_scan.ts";
import { ROOT, fileSource } from "./util.ts";

const [full, pspIso] = process.argv.slice(2);
const iso = await IsoFs.open(fileSource(full));
const psp = await IsoFs.open(fileSource(pspIso));
const DIR = "PSP_GAME/USRDIR/res/ps/PSPBIN/";
const names = fs.readdirSync(path.join(ROOT, "disks/psp/PSPBIN")).filter((n) => n.endsWith(".bin")).sort();
const corpus: Uint8Array[] = [];
for (const n of names) {
    const d = await psp.readFile(DIR + n);
    if (md5(d) !== md5(new Uint8Array(fs.readFileSync(path.join(ROOT, "disks/psp/PSPBIN", n))))) {
        console.log("%s: disks/psp differs from the US UMD", n);
    }
    corpus.push(d);
}
const plan: { tokens: string[]; files: Record<string, string> } = JSON.parse(execFileSync(
    path.join(ROOT, ".venv/bin/python"), [path.join(ROOT, "mods/sotn-ultimate/tools/builder/node/psp_text_tokens.py")],
    { cwd: ROOT, encoding: "utf8" }));
const tokens = new Set(plan.tokens);
const finder = new PspTextFinder(corpus);
let total = 0;
const covered = new Set<string>();
for (const f of Object.keys(plan.files)) {
    const data = await iso.readFile(f);
    const runs = finder.find(data, tokens);
    const n = runs.reduce((s, x) => s + x[3], 0);
    total += n;
    // the tokens this file's runs make up
    let k = 0;
    while (k < runs.length) {
        let str = "";
        let end = runs[k][0];
        while (k < runs.length && runs[k][0] === end) {
            str += String.fromCharCode(...data.subarray(runs[k][0], runs[k][0] + runs[k][3]));
            end += runs[k][3];
            k++;
        }
        covered.add(str);
    }
    console.log("%s: %d runs, %d bytes", f, runs.length, n);
}
const left = plan.tokens.filter((t) => !covered.has(t));
console.log("total %d bytes; tokens %d (%d chars), %d stay in the patch (%d chars):", total, tokens.size,
            plan.tokens.reduce((s, t) => s + t.length, 0), left.length, left.reduce((s, t) => s + t.length, 0));
for (const t of left) {
    console.log("  %s", JSON.stringify(t));
}
