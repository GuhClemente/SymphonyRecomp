// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/leftover_scan.ts HOLLOW.bin US_TRACK1.bin DIR...
// Sanity check of the patch's content: every file of the hollow image (but
// XA_STR1) scanned for runs of 32+ bytes found as they are in the files under
// DIR... (the Saturn disc, the UMD's res/ps), leaving out runs the retail US
// disc's files also have (the PS1's own data, which the PSP port shares).
import * as fs from "node:fs";
import * as path from "node:path";
import { IsoFs, type FileEntry } from "../src/disc.ts";
import { findPspCopies } from "./psp_copy_scan.ts";
import { fileSource } from "./util.ts";

const [hollowPath, usPath, ...dirs] = process.argv.slice(2);
const hollow = await IsoFs.open(fileSource(hollowPath));
const us = await IsoFs.open(fileSource(usPath));
const filesUnder = (d: string): string[] => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) =>
    e.isDirectory() ? filesUnder(path.join(d, e.name)) : [path.join(d, e.name)]);
const srcNames = dirs.flatMap(filesUnder).filter((f) => fs.statSync(f).size >= 32 && !/\/SD\//.test(f));
const sources = srcNames.map((f) => new Uint8Array(fs.readFileSync(f)));
async function walk(fsys: IsoFs, dir: string, acc: Map<string, FileEntry>): Promise<void> {
    for (const [name, e] of await fsys.listDir(dir)) {
        const p = dir ? dir + "/" + name : name;
        if (e.dir) {
            await walk(fsys, p, acc);
        } else {
            acc.set(p, e);
        }
    }
}
const files = new Map<string, FileEntry>();
await walk(hollow, "", files);
const usFiles = new Map<string, FileEntry>();
await walk(us, "", usFiles);
const retail: Buffer[] = [];
for (const p of usFiles.keys()) {
    if (!p.startsWith("SD/XA_STR1")) {
        retail.push(Buffer.from(await us.readFile(p)));
    }
}
let total = 0;
for (const p of [...files.keys()].sort()) {
    if (p.startsWith("SD/XA_STR1")) {
        continue;
    }
    const data = await hollow.readFile(p);
    // FAST=1: only the same retail file (the release's own rule), else every one
    let same: Buffer | null = null;
    try {
        same = Buffer.from(await us.readFile(p));
    } catch {
        // a new file
    }
    const runs = findPspCopies(data, sources).filter(([dst, , , n]) => {
        const b = Buffer.from(data.subarray(dst, dst + n));
        return process.env.FAST ? !same || same.indexOf(b) < 0 : !retail.some((r) => r.indexOf(b) >= 0);
    });
    const n = runs.reduce((s, r) => s + r[3], 0);
    if (n) {
        total += n;
        const by = new Map<string, number>();
        for (const r of runs) {
            const k = path.basename(srcNames[r[1]]);
            by.set(k, (by.get(k) ?? 0) + r[3]);
        }
        console.log("%s: %d bytes in %d runs [%s]", p, n, runs.length,
                    [...by].sort((a, b) => b[1] - a[1]).slice(0, 4).map(([k, v]) => k + " " + v).join(", "));
    }
}
console.log("total %d bytes found verbatim in the given discs' files", total);
