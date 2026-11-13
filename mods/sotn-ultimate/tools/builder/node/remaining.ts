// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/remaining.ts FULL.bin US_TRACK1.bin MANIFEST.json
// What the patch still carries: every file of the finished disc that is new
// or differs from the retail US disc, minus the bytes the builder makes
// (the manifest's file / part / text spans; XA_STR1 listed apart).
import * as fs from "node:fs";
import { IsoFs, type FileEntry } from "../src/disc.ts";
import { fileSource } from "./util.ts";

const [full, us, manifestPath] = process.argv.slice(2);
const out = await IsoFs.open(fileSource(full));
const ret = await IsoFs.open(fileSource(us));
const m = JSON.parse(fs.readFileSync(manifestPath, "utf8"));
const made = new Map<string, number>(); // path -> bytes the builder makes
for (const s of m.spans) {
    if (s.kind === "file" || s.kind === "text") {
        made.set(s.path, (made.get(s.path) ?? 0) + s.size);
    }
}
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
const a = new Map<string, FileEntry>();
const b = new Map<string, FileEntry>();
await walk(out, "", a);
await walk(ret, "", b);
const rows: [string, number, number, string][] = [];
for (const [p, e] of a) {
    if (p.startsWith("SD/XA_STR1")) {
        continue;
    }
    const d = await out.readFile(p);
    let diff = d.length;
    let what = "new";
    if (b.has(p)) {
        const r = await ret.readFile(p);
        diff = 0;
        for (let i = 0; i < d.length; i++) {
            if (d[i] !== r[i]) {
                diff++;
            }
        }
        what = "changed";
    }
    if (diff) {
        rows.push([p, d.length, diff, what]);
    }
}
rows.sort((x, y) => y[2] - x[2]);
let total = 0;
let left = 0;
for (const [p, size, diff, what] of rows) {
    const mk = made.get(p) ?? 0;
    total += diff;
    const rest = mk >= size ? 0 : diff;
    left += rest;
    console.log("%s %s: %d bytes, %d differ%s", what.padEnd(7), p.padEnd(26), size, diff,
                mk ? (mk >= size ? "  [builder makes all]" : "  [builder makes " + mk + "]") : "");
}
console.log("files %d, differing bytes %d, not yet made by the builder (approx) %d", rows.length, total, left);
