// SPDX-License-Identifier: AGPL-3.0-or-later
// The release side of src/xa_mono.ts: the XA_STR1 sectors of a (hollow) disc
// that are the PS1's voices as repack_xa.py made them mono, each matched to
// its two retail stereo sectors; checked by rebuilding it.
import { createHash } from "node:crypto";
import { MONO_HALF, monoPayload, type MonoRun } from "../src/xa_mono.ts";
import { monoHalf } from "../src/xa_mono.ts";

const h = (b: Uint8Array): string => createHash("md5").update(b).digest("hex");

// img: the disc image (raw); xaLba: its XA_STR1; sectors: those to explain;
// retail: the retail image and its XA_STR1 (lba, sector count)
export function findMonoRuns(img: Uint8Array, xaLba: number, sectors: number[],
                             retail: Uint8Array, rLba: number, rCount: number): MonoRun[] {
    const halves = new Map<string, number>();
    for (let s = 0; s < rCount; s++) {
        const o = (rLba + s) * 2352;
        const k = h(monoHalf(retail.subarray(o, o + 2352)));
        if (!halves.has(k)) {
            halves.set(k, s);
        }
    }
    const zero = h(new Uint8Array(MONO_HALF));
    const out: MonoRun[] = [];
    for (const s of sectors) {
        const o = (xaLba + s) * 2352 + 24;
        const a = halves.get(h(img.subarray(o, o + MONO_HALF)));
        const second = h(img.subarray(o + MONO_HALF, o + 2 * MONO_HALF));
        const b = second === zero ? -1 : halves.get(second);
        if (a === undefined || b === undefined) {
            continue;
        }
        const ro = (x: number): Uint8Array => retail.subarray((rLba + x) * 2352, (rLba + x + 1) * 2352);
        const mine = monoPayload(ro(a), b < 0 ? null : ro(b));
        if (h(mine) === h(img.subarray(o, o + mine.length))) {
            out.push([s, a, b]);
        }
    }
    return out;
}
