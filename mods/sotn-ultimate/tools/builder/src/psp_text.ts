// SPDX-License-Identifier: AGPL-3.0-or-later
// The PSP English script text on the finished disc. mods/sotn-ultimate/tools/psp_script.py
// puts the PSP's own characters (the US UMD's PSPBIN overlays) into the PS1
// cutscene scripts unchanged; only the script around them is the mod's. So
// the release blanks those characters in the patch and the builder copies
// them back from the player's UMD: a run = [offset in the disc file, PSPBIN
// file index, offset there, length].
import { fixSectorCodes } from "./sector.ts";

export type TextRun = [number, number, number, number];

// builder side: the text of a span's runs, run after run, from the PSPBIN files
export function pspTextBytes(runs: TextRun[], corpus: Uint8Array[]): Uint8Array {
    const out = new Uint8Array(runs.reduce((s, r) => s + r[3], 0));
    let o = 0;
    for (const [, f, src, n] of runs) {
        out.set(corpus[f].subarray(src, src + n), o);
        o += n;
    }
    return out;
}

// the sectors of a disc file (Form 1, raw 2352) a run touches: [first, last]
export function runSectors(dst: number, n: number): [number, number] {
    return [Math.floor(dst / 2048), Math.floor((dst + n - 1) / 2048)];
}

// builder side: the text into the file's sectors on the image (at lba), and
// those sectors' EDC/ECC made again
export function writeText(img: Uint8Array, lba: number, runs: TextRun[], text: Uint8Array): void {
    const touched = new Set<number>();
    let o = 0;
    for (const [dst, , , n] of runs) {
        for (let k = 0; k < n; k++) {
            const at = dst + k;
            img[(lba + Math.floor(at / 2048)) * 2352 + 24 + (at % 2048)] = text[o + k];
        }
        const [a, b] = runSectors(dst, n);
        for (let s = a; s <= b; s++) {
            touched.add(s);
        }
        o += n;
    }
    for (const s of touched) {
        fixSectorCodes(img.subarray((lba + s) * 2352, (lba + s + 1) * 2352));
    }
}
