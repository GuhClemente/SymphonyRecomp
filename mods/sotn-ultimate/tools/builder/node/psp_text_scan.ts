// SPDX-License-Identifier: AGPL-3.0-or-later
// The release side of src/psp_text.ts: where the PSP script text is on the
// finished disc (make_hollow.ts blanks it, the builder copies it back).
// psp_script.py's TEXT tokens (build/us/psp_script/patches.json) sit between
// script ops, so each is a whole printable run of the overlay: the runs that
// are a token and are made of PSPBIN text (longest pieces first) become
// TextRuns.
import type { TextRun } from "../src/psp_text.ts";

const printable = (b: number): boolean => b >= 0x20 && b <= 0x7E;
const latin1 = (d: Uint8Array): string => Buffer.from(d.buffer, d.byteOffset, d.byteLength).toString("latin1");

export class PspTextFinder {
    private corpus: string; // the PSPBIN files, "\0" between them
    private starts: number[] = [];
    private cache = new Map<string, [number, number, number][] | null>(); // token -> [file, offset, length] pieces

    constructor(files: Uint8Array[]) {
        const parts: string[] = [];
        let o = 0;
        for (const f of files) {
            this.starts.push(o);
            parts.push(latin1(f));
            o += f.length + 1;
        }
        this.corpus = parts.join("\0");
    }

    private where(i: number): [number, number] {
        let f = 0;
        while (f + 1 < this.starts.length && this.starts[f + 1] <= i) {
            f++;
        }
        return [f, i - this.starts[f]];
    }

    // a token as PSPBIN pieces, the longest first (null: not all of it is there)
    private cover(s: string): [number, number, number][] | null {
        if (this.cache.has(s)) {
            return this.cache.get(s)!;
        }
        const out: [number, number, number][] = [];
        let p = 0;
        while (p < s.length) {
            let n = s.length - p;
            let at = -1;
            for (; n > 0; n--) {
                at = this.corpus.indexOf(s.substr(p, n));
                if (at >= 0) {
                    break;
                }
            }
            if (at < 0) {
                this.cache.set(s, null);
                return null;
            }
            const [f, o] = this.where(at);
            out.push([f, o, n]);
            p += n;
        }
        this.cache.set(s, out);
        return out;
    }

    find(data: Uint8Array, tokens: Set<string>): TextRun[] {
        const out: TextRun[] = [];
        let s = -1;
        for (let i = 0; i <= data.length; i++) {
            if (i < data.length && printable(data[i])) {
                if (s < 0) {
                    s = i;
                }
                continue;
            }
            if (s >= 0) {
                // an op's arguments (up to 4 bytes) may be printable and run
                // into the text after them
                const run = latin1(data.subarray(s, i));
                for (let j = 0; j <= Math.min(4, run.length - 1); j++) {
                    const str = run.slice(j);
                    if (!tokens.has(str)) {
                        continue;
                    }
                    const pieces = this.cover(str);
                    let p = s + j;
                    for (const [f, o, n] of pieces ?? []) {
                        out.push([p, f, o, n]);
                        p += n;
                    }
                    break;
                }
                s = -1;
            }
        }
        return out;
    }
}
