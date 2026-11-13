// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_xa_mono.ts HOLLOW.bin US_TRACK1.bin
// How many of the hollow image's non-retail, non-blank XA_STR1 sectors
// node/xa_mono_scan.ts explains (rebuilt exactly from retail sectors).
import * as fs from "node:fs";
import { createHash } from "node:crypto";
import { IsoFs } from "../src/disc.ts";
import { findMonoRuns } from "./xa_mono_scan.ts";
import { fileSource } from "./util.ts";

const [hollowPath, usPath] = process.argv.slice(2);
const img = new Uint8Array(fs.readFileSync(hollowPath));
const us = new Uint8Array(fs.readFileSync(usPath));
const xa = await (await IsoFs.open(fileSource(hollowPath))).lookup("SD/XA_STR1.");
const rx = await (await IsoFs.open(fileSource(usPath))).lookup("SD/XA_STR1.");
const rCount = Math.ceil(rx.size / 2048);
const retail = new Set<string>();
for (let s = 0; s < rCount; s++) {
    const o = (rx.lba + s) * 2352 + 24;
    retail.add(createHash("md5").update(us.subarray(o, o + 2304)).digest("hex"));
}
const other: number[] = [];
for (let s = 0; s < Math.ceil(xa.size / 2048); s++) {
    const a = img.subarray((xa.lba + s) * 2352 + 24, (xa.lba + s) * 2352 + 24 + 2304);
    if (!a.every((b) => b === 0) && !retail.has(createHash("md5").update(a).digest("hex"))) {
        other.push(s);
    }
}
const t0 = performance.now();
const runs = findMonoRuns(img, xa.lba, other, us, rx.lba, rCount);
console.log("%d other sectors, %d rebuilt from retail ones (%d with a silent half), %s s", other.length, runs.length,
            runs.filter((r) => r[2] < 0).length, ((performance.now() - t0) / 1000).toFixed(1));
