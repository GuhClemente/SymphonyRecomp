// SPDX-License-Identifier: AGPL-3.0-or-later
// Where an encoded XA stream's sectors sit in the image's SD/XA_STR1.:
// each stream sector is matched by its audio data (bytes 24..2348; the
// subheader's file/channel are renumbered by repack_xa.py). A sector whose
// data appears more than once (silence, or Hell Garden's shared passages)
// takes the candidate that continues the stream's stride.
import * as fs from "node:fs";
import { createHash } from "node:crypto";

export type Run = [number, number, number, number]; // [disc index, stride, count, stream index]

const key = (b: Uint8Array): string => createHash("sha1").update(b.subarray(24, 2348)).digest("hex");

export class XaIndex {
    private at = new Map<string, number[]>();

    constructor(fd: number, lba: number, sectors: number) {
        const sec = new Uint8Array(2352);
        for (let i = 0; i < sectors; i++) {
            fs.readSync(fd, sec, 0, 2352, (lba + i) * 2352);
            const k = key(sec);
            const l = this.at.get(k);
            l ? l.push(i) : this.at.set(k, [i]);
        }
    }

    // stream (raw 2352-byte sectors) -> disc index of each of its sectors
    place(stream: Uint8Array, taken: Set<number>): number[] {
        const n = stream.length / 2352;
        const out: number[] = [];
        const cands = (r: number): number[] =>
            (this.at.get(key(stream.subarray(r * 2352, (r + 1) * 2352))) ?? []).filter((i) => !taken.has(i));
        for (let r = 0; r < n; r++) {
            const c = cands(r);
            if (!c.length) {
                throw new Error("stream sector " + r + " is not on the disc");
            }
            let pick = c[0];
            // interleave strides: 8 (stereo), 16 (mono, repack_xa.py)
            if (r === 0 && n > 1) { // the lead-in is often silence: start where sector 1 follows
                const next = new Set(cands(1));
                pick = c.find((i) => next.has(i + 8) || next.has(i + 16)) ?? c[0];
            } else if (r > 0) {
                const p = out[r - 1];
                const want = [r > 1 ? p - out[r - 2] : 0, 8, 16].filter((k) => k > 0).map((k) => p + k);
                pick = want.find((w) => c.includes(w)) ?? c[0];
            }
            out.push(pick);
            taken.add(pick);
        }
        return out;
    }
}

export function toRuns(idx: number[]): Run[] {
    const runs: Run[] = [];
    let r = 0;
    while (r < idx.length) {
        const stride = r + 1 < idx.length ? idx[r + 1] - idx[r] : 1;
        let n = 1;
        while (r + n < idx.length && idx[r + n] - idx[r + n - 1] === stride) {
            n++;
        }
        runs.push([idx[r], n > 1 ? stride : 1, n, r]);
        r += n;
    }
    return runs;
}
