// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/survey_psp_copies.ts FULL.bin PSPBIN_DIR FILE...
// Survey: how much of each disc FILE node/psp_copy_scan.ts would copy back
// from the US UMD's PSPBIN files.
import * as fs from "node:fs";
import * as path from "node:path";
import { IsoFs } from "../src/disc.ts";
import { findPspCopies } from "./psp_copy_scan.ts";
import { fileSource } from "./util.ts";

const [full, dir, ...files] = process.argv.slice(2);
const iso = await IsoFs.open(fileSource(full));
const names = fs.readdirSync(dir).sort();
const corpus = names.map((n) => new Uint8Array(fs.readFileSync(path.join(dir, n))));
for (const f of files) {
    const t0 = performance.now();
    const data = await iso.readFile(f);
    const runs = findPspCopies(data, corpus);
    const n = runs.reduce((s, r) => s + r[3], 0);
    const by = new Map<string, number>();
    for (const r of runs) {
        by.set(names[r[1]], (by.get(names[r[1]]) ?? 0) + r[3]);
    }
    console.log("%s: %d bytes, %d runs copy %d (%s%%) [%s] %s s", f, data.length, runs.length, n,
                (100 * n / data.length).toFixed(1), [...by].map(([k, v]) => k + " " + v).join(", "),
                ((performance.now() - t0) / 1000).toFixed(1));
}
