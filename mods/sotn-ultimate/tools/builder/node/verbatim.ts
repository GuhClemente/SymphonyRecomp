// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/verbatim.ts FULL.bin SOURCES_DIR... -- FILE...
// Survey: how much of each disc FILE is verbatim Saturn / PSP data, i.e. runs
// of at least K bytes found unchanged in a file under the SOURCES_DIRs (the
// extracted Saturn disc, the US UMD's res/ps). Runs of a few byte values only
// (padding, fills) don't count.
import * as fs from "node:fs";
import * as path from "node:path";
import { IsoFs } from "../src/disc.ts";
import { fileSource } from "./util.ts";

const K = 32;
const args = process.argv.slice(2);
const sep = args.indexOf("--");
const [full, ...srcDirs] = args.slice(0, sep);
const targets = args.slice(sep + 1);
const iso = await IsoFs.open(fileSource(full));

function filesUnder(d: string): string[] {
    const out: string[] = [];
    for (const e of fs.readdirSync(d, { withFileTypes: true })) {
        const p = path.join(d, e.name);
        if (e.isDirectory()) {
            out.push(...filesUnder(p));
        } else if (fs.statSync(p).size >= K) {
            out.push(p);
        }
    }
    return out;
}
const sources = srcDirs.flatMap(filesUnder);

const B = 0x01000193;
let BK = 1; // B^K mod 2^32
for (let i = 0; i < K; i++) {
    BK = Math.imul(BK, B);
}
const varied = (d: Uint8Array, o: number): boolean => {
    const seen = new Set<number>();
    for (let i = 0; i < K; i++) {
        seen.add(d[o + i]);
        if (seen.size >= 8) {
            return true;
        }
    }
    return false;
};

for (const t of targets) {
    const data = await iso.readFile(t);
    // the target's K-grams: hash -> positions
    const table = new Map<number, number[]>();
    let h = 0;
    for (let i = 0; i < data.length; i++) {
        h = (Math.imul(h, B) + data[i]) | 0;
        if (i >= K) {
            h = (h - Math.imul(data[i - K], BK)) | 0;
        }
        if (i >= K - 1 && varied(data, i - K + 1)) {
            const l = table.get(h);
            if (l) {
                l.push(i - K + 1);
            } else {
                table.set(h, [i - K + 1]);
            }
        }
    }
    const covered = new Uint8Array(data.length);
    const bySource = new Map<string, number>();
    for (const sp of sources) {
        const s = new Uint8Array(fs.readFileSync(sp));
        let g = 0;
        let n = 0;
        for (let i = 0; i < s.length; i++) {
            g = (Math.imul(g, B) + s[i]) | 0;
            if (i >= K) {
                g = (g - Math.imul(s[i - K], BK)) | 0;
            }
            if (i < K - 1) {
                continue;
            }
            const hits = table.get(g);
            if (!hits) {
                continue;
            }
            const so = i - K + 1;
            for (const to of hits) {
                if (covered[to] && covered[to + K - 1]) {
                    continue;
                }
                let k = 0;
                while (k < K && s[so + k] === data[to + k]) {
                    k++;
                }
                if (k === K) {
                    for (let j = 0; j < K; j++) {
                        if (!covered[to + j]) {
                            covered[to + j] = 1;
                            n++;
                        }
                    }
                }
            }
        }
        if (n) {
            bySource.set(path.basename(sp), n);
        }
    }
    const total = covered.reduce((a, b) => a + b, 0);
    const top = [...bySource].sort((a, b) => b[1] - a[1]).slice(0, 4).map(([k, v]) => k + " " + v).join(", ");
    console.log("%s: %d bytes, verbatim %d (%s%%)  %s", t, data.length, total,
                (100 * total / data.length).toFixed(1), top);
}
