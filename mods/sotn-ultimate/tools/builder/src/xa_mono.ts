// SPDX-License-Identifier: AGPL-3.0-or-later
// The PS1's dual-mono voices as mods/sotn-ultimate/tools/repack_xa.py stores them (to_mono):
// a retail stereo voice line whose two channels are the same sound keeps only
// its left channel, as mono XA. A mono sector holds the left units of two
// consecutive stereo sectors (9 sound groups each; silence after a line's
// last sector), so the builder makes it from the player's own disc: a run =
// [XA_STR1 sector, first retail sector, second retail sector or -1].
export type MonoRun = [number, number, number];

const GROUPS = 18; // sound groups of a sector
const DATA = 24; // a raw Form 2 sector's audio
export const MONO_HALF = (GROUPS / 2) * 128; // a stereo sector's left units: 9 groups
export const MONO_PAYLOAD = 2324; // bytes 24..2348 of the sector (audio, zero tail)

// repack_xa.to_mono on one stereo sector (raw, 2352 bytes): its 9 mono
// groups. A stereo group's units alternate left / right: left unit 2k's
// params are byte 4 + 2k, its nibbles the low ones of each row's byte k. A
// mono group takes the 4 left units of two stereo groups (params twice, as
// XA stores them; unit pairs packed low / high in each row's bytes)
export function monoHalf(sector: Uint8Array): Uint8Array {
    const out = new Uint8Array(MONO_HALF);
    for (let m = 0; m < GROUPS / 2; m++) {
        const o = m * 128;
        const left: number[][] = []; // the 8 left units' nibbles (28 each)
        for (let half = 0; half < 2; half++) {
            const g = DATA + (2 * m + half) * 128;
            for (let k = 0; k < 4; k++) {
                const p = sector[g + 4 + 2 * k];
                out[o + half * 8 + k] = p;
                out[o + half * 8 + 4 + k] = p;
                left.push(Array.from({ length: 28 }, (_, r) => sector[g + 16 + r * 4 + k] & 15));
            }
        }
        for (let r = 0; r < 28; r++) {
            for (let k = 0; k < 4; k++) {
                out[o + 16 + r * 4 + k] = left[2 * k][r] | (left[2 * k + 1][r] << 4);
            }
        }
    }
    return out;
}

// a mono sector's payload from its two retail stereo sectors (null: silence)
export function monoPayload(a: Uint8Array, b: Uint8Array | null): Uint8Array {
    const out = new Uint8Array(MONO_PAYLOAD);
    out.set(monoHalf(a), 0);
    if (b) {
        out.set(monoHalf(b), MONO_HALF);
    }
    return out;
}

const READ_GAP = 64; // needed sectors this close share a read
const READ_MAX = 512; // sectors per read (1.2 MB)

// every run's payload, back to back (monoPayload per run): the retail sectors
// are scattered over the whole stream, so they are read in blocks of nearby
// sectors, each sector's half made once and put wherever runs use it.
// read(first, count): raw sectors of the stream; progress(done, total) per block
export async function monoPayloads(runs: MonoRun[], read: (first: number, count: number) => Promise<Uint8Array>,
                                   progress: (done: number, total: number) => void): Promise<Uint8Array> {
    const out = new Uint8Array(runs.length * MONO_PAYLOAD);
    const uses = new Map<number, number[]>(); // sector -> offsets in out
    const use = (sector: number, at: number) => {
        const u = uses.get(sector);
        if (u) {
            u.push(at);
        } else {
            uses.set(sector, [at]);
        }
    };
    runs.forEach(([, a, b], i) => {
        use(a, i * MONO_PAYLOAD);
        if (b >= 0) {
            use(b, i * MONO_PAYLOAD + MONO_HALF);
        }
    });
    const need = [...uses.keys()].sort((x, y) => x - y);
    const blocks: [number, number][] = []; // [first need index, end]
    for (let i = 0; i < need.length;) {
        let j = i + 1;
        while (j < need.length && need[j] - need[j - 1] <= READ_GAP && need[j] - need[i] < READ_MAX) {
            j++;
        }
        blocks.push([i, j]);
        i = j;
    }
    for (let k = 0; k < blocks.length; k++) {
        progress(k, blocks.length);
        const [i, j] = blocks[k];
        const first = need[i];
        const raw = await read(first, need[j - 1] - first + 1);
        for (let n = i; n < j; n++) {
            const o = (need[n] - first) * 2352;
            const half = monoHalf(raw.subarray(o, o + 2352));
            for (const at of uses.get(need[n])!) {
                out.set(half, at);
            }
        }
    }
    progress(blocks.length, blocks.length);
    return out;
}
