// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/patch_contents.ts HOLLOW.bin US_TRACK1.bin
// What the patch carries: per disc file of the hollow image, the bytes that
// are neither blank (the builder makes them) nor found in the retail US disc
// as they are (xdelta copies those from the player's disc). XA_STR1 counted by
// sector: a sector whose audio isn't blank and isn't a retail XA sector.
import * as fs from "node:fs";
import { createHash } from "node:crypto";
import { IsoFs, type FileEntry } from "../src/disc.ts";
import { fileSource } from "./util.ts";

const [hollowPath, usPath] = process.argv.slice(2);
const hollow = await IsoFs.open(fileSource(hollowPath));
const us = await IsoFs.open(fileSource(usPath));
const himg = fs.readFileSync(hollowPath);
const uimg = fs.readFileSync(usPath);

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

// retail XA sectors by the hash of their audio bytes
const xaU = await us.lookup("SD/XA_STR1.");
const retailXa = new Set<string>();
for (let s = 0; s < Math.ceil(xaU.size / 2048); s++) {
    const o = (xaU.lba + s) * 2352;
    retailXa.add(createHash("md5").update(uimg.subarray(o + 24, o + 24 + 2304)).digest("hex"));
}
const xa = files.get("SD/XA_STR1.")!;
let blank = 0;
let retail = 0;
let other = 0;
const runs: [number, number][] = [];
for (let s = 0; s < Math.ceil(xa.size / 2048); s++) {
    const o = (xa.lba + s) * 2352;
    const a = himg.subarray(o + 24, o + 24 + 2304);
    if (a.every((b) => b === 0)) {
        blank++;
    } else if (retailXa.has(createHash("md5").update(a).digest("hex"))) {
        retail++;
    } else {
        other++;
        const last = runs[runs.length - 1];
        if (last && last[1] === s - 1) {
            last[1] = s;
        } else {
            runs.push([s, s]);
        }
    }
}
console.log("XA_STR1: %d sectors: %d blank (builder), %d retail, %d other (%s MB)", Math.ceil(xa.size / 2048), blank, retail,
            other, (other * 2324 / 1e6).toFixed(1));
console.log("  biggest other runs: " + runs.sort((a, b) => (b[1] - b[0]) - (a[1] - a[0])).slice(0, 8)
    .map(([a, b]) => a + "-" + b + " (" + (b - a + 1) + ")").join(", "));
