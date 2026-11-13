// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/map_xa.ts IMAGE.bin STREAM.xa...
// Where do an encoded stream's sectors sit in the image's SD/XA_STR1.?
// Matches each stream sector's audio data (bytes 24..2348 of the 2352) with
// the disc's sectors; prints coverage and the subheader differences.
import * as fs from "node:fs";
import * as path from "node:path";
import { createHash } from "node:crypto";
import { IsoFs } from "../src/disc.ts";
import { fileSource } from "./util.ts";

const [image, ...streams] = process.argv.slice(2);
const iso = await IsoFs.open(fileSource(image));
const xa = await iso.lookup("SD/XA_STR1.");
const nsec = Math.ceil(xa.size / 2048);
const fd = fs.openSync(image, "r");
const key = (b: Uint8Array): string => createHash("sha1").update(b.subarray(24, 2348)).digest("hex");
const disc = new Map<string, number[]>();
const sec = new Uint8Array(2352);
for (let i = 0; i < nsec; i++) {
    fs.readSync(fd, sec, 0, 2352, (xa.lba + i) * 2352);
    const k = key(sec);
    const l = disc.get(k);
    l ? l.push(i) : disc.set(k, [i]);
}
console.log("XA_STR1: lba %d, %d sectors", xa.lba, nsec);
for (const s of streams) {
    const b = fs.readFileSync(s);
    const n = b.length / 2352;
    let found = 0, multi = 0, subDiff = 0, firstAt = -1, contiguousStride = new Map<number, number>();
    let prev = -1;
    for (let r = 0; r < n; r++) {
        const ss = b.subarray(r * 2352, (r + 1) * 2352);
        const at = disc.get(key(ss));
        if (!at) {
            continue;
        }
        found++;
        if (at.length > 1) {
            multi++;
        }
        const i = at[0];
        if (firstAt < 0) {
            firstAt = i;
        }
        if (prev >= 0) {
            contiguousStride.set(i - prev, (contiguousStride.get(i - prev) ?? 0) + 1);
        }
        prev = i;
        fs.readSync(fd, sec, 0, 2352, (xa.lba + i) * 2352);
        if (Buffer.compare(Buffer.from(sec.subarray(16, 24)), ss.subarray(16, 24)) !== 0) {
            subDiff++;
        }
    }
    const strides = [...contiguousStride.entries()].sort((a, c) => c[1] - a[1]).slice(0, 3);
    console.log("%s: %d sectors, found %d (%d ambiguous), first at %d, subheader differs in %d, strides %j",
                path.basename(s), n, found, multi, firstAt, subDiff, strides);
}
