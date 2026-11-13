// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_sector.ts IMAGE.bin
// recompute the EDC/ECC of every Mode 2 sector and compare with the image's
import * as fs from "node:fs";
import { fixSectorCodes } from "../src/sector.ts";

const img = fs.readFileSync(process.argv[2]);
const n = img.length / 2352;
let f1 = 0, f2 = 0, f2noedc = 0, bad = 0;
const copy = new Uint8Array(2352);
for (let lba = 0; lba < n; lba++) {
    const s = img.subarray(lba * 2352, (lba + 1) * 2352);
    if (s[15] !== 2) {
        continue;
    }
    copy.set(s);
    fixSectorCodes(copy, true);
    const form2 = (s[18] & 0x20) !== 0;
    if (form2 && s.readUInt32LE(2348) === 0) {
        f2noedc++; // Form 2 EDC is optional: 0 = none
        continue;
    }
    if (Buffer.compare(Buffer.from(copy), s) !== 0) {
        if (bad++ < 5) {
            console.log("mismatch at sector", lba, form2 ? "form 2" : "form 1");
        }
    } else if (form2) {
        f2++;
    } else {
        f1++;
    }
}
console.log({ sectors: n, form1_ok: f1, form2_ok: f2, form2_without_edc: f2noedc, mismatches: bad });
