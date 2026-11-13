// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/leftover_detail.ts HOLLOW.bin US_TRACK1.bin MAP BASE FILE DIR...
// node/leftover_scan.ts for one disc FILE, each run listed with the link-map
// symbol it is in (the nearest symbol at or before it in MAP, file loaded at BASE).
import * as fs from "node:fs";
import * as path from "node:path";
import { IsoFs } from "../src/disc.ts";
import { findPspCopies } from "./psp_copy_scan.ts";
import { fileSource } from "./util.ts";

const [hollowPath, usPath, mapPath, baseHex, file, ...dirs] = process.argv.slice(2);
const base = parseInt(baseHex, 16);
const hollow = await IsoFs.open(fileSource(hollowPath));
const us = await IsoFs.open(fileSource(usPath));
const filesUnder = (d: string): string[] => fs.readdirSync(d, { withFileTypes: true }).flatMap((e) =>
    e.isDirectory() ? filesUnder(path.join(d, e.name)) : [path.join(d, e.name)]);
const srcNames = dirs.flatMap(filesUnder).filter((f) => fs.statSync(f).size >= 32 && !/\/SD\//.test(f));
const sources = srcNames.map((f) => new Uint8Array(fs.readFileSync(f)));
const data = await hollow.readFile(file);
let retail: Buffer | null = null;
try {
    retail = Buffer.from(await us.readFile(file));
} catch {
    // a new file
}
const syms: [number, string][] = [];
for (const line of fs.readFileSync(mapPath, "utf8").split("\n")) {
    const m = /^\s+0x([0-9a-f]{8})\s+(\w+)\s*$/.exec(line);
    if (m) {
        syms.push([parseInt(m[1], 16), m[2]]);
    }
}
syms.sort((a, b) => a[0] - b[0]);
const symAt = (a: number): string => {
    let best = "?";
    for (const [x, n] of syms) {
        if (x > a) {
            break;
        }
        best = n + "+0x" + (a - x).toString(16);
    }
    return best;
};
for (const [dst, f, so, n] of findPspCopies(data, sources)) {
    const b = Buffer.from(data.subarray(dst, dst + n));
    if (retail && retail.indexOf(b) >= 0) {
        continue;
    }
    console.log("0x%s +%d  %s  <- %s 0x%s", dst.toString(16), n, symAt(base + dst), path.basename(srcNames[f]), so.toString(16));
}
