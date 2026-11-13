// SPDX-License-Identifier: AGPL-3.0-or-later
// The release side of PSP data copied as it is: the runs of a disc file
// (at least K bytes, not a fill of a few byte values) found unchanged in the
// US UMD's PSPBIN files. They are blanked in the patch and the builder copies
// them back from the player's UMD, as the script text (src/psp_text.ts:
// TextRun = [offset in the file, PSPBIN file index, offset there, length]).
import type { TextRun } from "../src/psp_text.ts";

const K = 32;
const B = 0x01000193;
let BK = 1;
for (let i = 0; i < K; i++) {
    BK = Math.imul(BK, B);
}

function varied(d: Uint8Array, o: number): boolean {
    const seen = new Set<number>();
    for (let i = 0; i < K; i++) {
        seen.add(d[o + i]);
        if (seen.size >= 8) {
            return true;
        }
    }
    return false;
}

// covered: bytes of data already taken by other spans (left alone)
export function findPspCopies(data: Uint8Array, corpus: Uint8Array[], covered?: Uint8Array): TextRun[] {
    const table = new Map<number, number[]>(); // K-gram hash -> positions in data
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
    // best[pos] = [file, offset, length] of the longest match starting at pos
    const best = new Map<number, [number, number, number]>();
    corpus.forEach((s, f) => {
        let g = 0;
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
                // extend both ways is not needed: every start position is tried
                let n = 0;
                while (to + n < data.length && so + n < s.length && s[so + n] === data[to + n]) {
                    n++;
                }
                if (n >= K && n > (best.get(to)?.[2] ?? 0)) {
                    best.set(to, [f, so, n]);
                }
            }
        }
    });
    const out: TextRun[] = [];
    let p = 0;
    const starts = [...best.keys()].sort((a, b) => a - b);
    for (const st of starts) {
        if (st < p) {
            continue;
        }
        let [f, so, n] = best.get(st)!;
        // stop at bytes another span holds
        let k = 0;
        while (k < n && !(covered && covered[st + k])) {
            k++;
        }
        n = k;
        if (n >= K) {
            out.push([st, f, so, n]);
            p = st + n;
        }
    }
    return out;
}
