// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/leftover_real.ts HOLLOW.bin FULL.bin US_TRACK1.bin DIR...
// node/leftover_scan.ts's runs (FAST rule) split by what the hollow image
// holds there: bytes that are still the full disc's (real leftovers) vs bytes
// the patch blanked (zeros that only extend a match).
import * as fs from "node:fs";
import * as path from "node:path";
import { IsoFs, type FileEntry } from "../src/disc.ts";
import { findPspCopies } from "./psp_copy_scan.ts";
import { fileSource } from "./util.ts";

const [hollowPath, fullPath, usPath, ...dirs] = process.argv.slice(2);
const hollow = await IsoFs.open(fileSource(hollowPath));
const full = await IsoFs.open(fileSource(fullPath));
const us = await IsoFs.open(fileSource(usPath));
const filesUnder = (d: string): string[] => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) =>
    e.isDirectory() ? filesUnder(path.join(d, e.name)) : [path.join(d, e.name)]);
const srcNames = dirs.flatMap(filesUnder).filter((f) => fs.statSync(f).size >= 32 && !/\/SD\//.test(f));
const sources = srcNames.map((f) => new Uint8Array(fs.readFileSync(f)));
const files = (process.env.FILES ?? "").split(",").filter(Boolean);
let realTotal = 0;
for (const p of files) {
    const h = await hollow.readFile(p);
    const f = await full.readFile(p);
    let same: Buffer | null = null;
    try {
        same = Buffer.from(await us.readFile(p));
    } catch {
        // new
    }
    let real = 0;
    let blank = 0;
    let longest = 0;
    const by = new Map<string, number>();
    for (const [dst, src, , n] of findPspCopies(h, sources)) {
        // the run with the full disc's bytes (blanking may have broken a retail match)
        if (same && (same.indexOf(h.subarray(dst, dst + n)) >= 0 || (process.env.FULLCHECK && same.indexOf(f.subarray(dst, dst + n)) >= 0))) {
            continue;
        }
        let r = 0;
        let cur = 0;
        for (let k = 0; k < n; k++) {
            const isReal = h[dst + k] !== 0 || f[dst + k] === 0; // a zero the full disc also has is real
            if (isReal) {
                r++;
                cur++;
                longest = Math.max(longest, cur);
            } else {
                cur = 0;
            }
        }
        real += r;
        blank += n - r;
        by.set(path.basename(srcNames[src]), (by.get(path.basename(srcNames[src])) ?? 0) + r);
    }
    realTotal += real;
    console.log("%s: real %d, blanked %d, longest real stretch %d [%s]", p, real, blank, longest,
                [...by].sort((a, b) => b[1] - a[1]).slice(0, 3).map(([k, v]) => k + " " + v).join(", "));
}
console.log("real bytes in leftover runs: %d", realTotal);
