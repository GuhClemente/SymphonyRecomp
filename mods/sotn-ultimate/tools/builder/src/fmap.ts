// SPDX-License-Identifier: AGPL-3.0-or-later
// The castle map blueprint with the Saturn-only cells -> build/us/F_MAP.BIN
// (the disc's F_MAP.BIN): a byte-exact port of mods/sotn-ultimate/tools/paint_fmap.py (see
// its header for both formats). Reads the PS1's F_MAP.BIN and the Saturn's
// SUB_DISP.MAP; the cell lists and the mod's moved save room are mod data.
export type DiscRead = (path: string) => Promise<Uint8Array>;

const SAT2PS: Record<number, number> = { 1: 2, 4: 4, 6: 5 };
const BOUGHT = new Set<string>(); // "x,y", as D_800A2C0C
const range = (a: number, b: number): number[] => Array.from({ length: b - a }, (_, i) => a + i);
const CELLS: [number, number][] = [
    [2, 41], [2, 42], [2, 43], [2, 44], [3, 44], [2, 45], [3, 45],
    ...range(4, 22).map((x): [number, number] => [x, 44]),
    ...range(4, 14).map((x): [number, number] => [x, 45]),
];
const PRISON: [number, number][] = [
    ...range(32, 37).map((x): [number, number] => [x, 34]),
    ...range(33, 36).map((x): [number, number] => [x, 35]),
    ...range(27, 36).map((x): [number, number] => [x, 36]),
];
const MOD_SAVE: [number, number] = [3, 43];
// (X, Y): the old save room (3,42)'s left edge, middle row: its door to the
// shaft is wall
const OLD_SAVE_DOOR: [number, number] = [4 * 3, 4 * 42 + 2];
// PS1 window column -> Saturn columns
const COLS: number[][] = [[0], [1], [2, 3], [4], [5]];

// 256x256 4bpp, low nibble = left pixel
export function paintFmap(fmap: Uint8Array, subDisp: Uint8Array): Uint8Array {
    if (fmap.length !== 0x8000 || subDisp.length < 160 * 256) {
        throw new Error("fmap: input size");
    }
    const ps = new Uint8Array(256 * 256);
    for (let i = 0; i < 0x8000; i++) {
        ps[i * 2] = fmap[i] & 15;
        ps[i * 2 + 1] = fmap[i] >> 4;
    }
    const sat = (y: number, x: number): number => {
        const b = subDisp[y * 160 + (x >> 1)];
        return x & 1 ? b & 15 : b >> 4;
    };
    const satWindow = (x: number, y: number): number[] => {
        const w = new Array<number>(25).fill(0);
        for (let r = 0; r < 5; r++) {
            COLS.forEach((scs, c) => {
                let v = 0;
                for (const sc of scs) {
                    v = v || sat(4 * y + r, 5 * x + sc);
                }
                if (v) {
                    const m = SAT2PS[v];
                    if (m === undefined) {
                        throw new Error("fmap: Saturn colour " + v);
                    }
                    v = m === 2 && BOUGHT.has(x + "," + y) ? 1 : m;
                }
                w[r * 5 + c] = v;
            });
        }
        return w;
    };
    const cells = [...CELLS, ...PRISON];
    const targets = new Set(cells.map(([x, y]) => x + "," + y));
    const orig = ps.slice();
    const cellNonempty = (cx: number, cy: number): boolean =>
        cx >= 0 && cx < 64 && cy >= 0 && cy < 64 && orig[(4 * cy + 2) * 256 + 4 * cx + 2] !== 0;
    for (const [x, y] of cells) {
        const w = satWindow(x, y);
        for (let r = 0; r < 5; r++) {
            for (let c = 0; c < 5; c++) {
                const p = (4 * y + r) * 256 + 4 * x + c;
                const v = w[r * 5 + c];
                const interior = r >= 1 && r <= 3 && c >= 1 && c <= 3;
                if (interior || v) {
                    ps[p] = v;
                    continue;
                }
                const nb: [number, number][] = [];
                if (c === 0) nb.push([x - 1, y]);
                if (c === 4) nb.push([x + 1, y]);
                if (r === 0) nb.push([x, y - 1]);
                if (r === 4) nb.push([x, y + 1]);
                const keep = nb.some(([nx, ny]) => !targets.has(nx + "," + ny) && cellNonempty(nx, ny));
                ps[p] = keep ? orig[p] : 0;
            }
        }
    }
    // the mod's moved save room
    const shaft = BOUGHT.has("2,43") ? 1 : 2;
    const [sx, sy] = MOD_SAVE;
    for (let r = 0; r < 5; r++) {
        for (let c = 0; c < 5; c++) {
            const p = (4 * sy + r) * 256 + 4 * sx + c;
            if (r >= 1 && r <= 3 && c >= 1 && c <= 3) {
                ps[p] = 4;
            } else if (c === 0 && r === 2) {
                ps[p] = shaft;
            } else if (c === 0) {
                ps[p] = 0;
            } else if (c === 4 || r === 0 || r === 4) {
                if (ps[p] !== 1 && ps[p] !== 2) {
                    ps[p] = 0;
                }
            }
        }
    }
    ps[OLD_SAVE_DOOR[1] * 256 + OLD_SAVE_DOOR[0]] = 0;
    const out = new Uint8Array(0x8000);
    for (let i = 0; i < 0x8000; i++) {
        out[i] = (ps[i * 2] & 15) | (ps[i * 2 + 1] << 4);
    }
    return out;
}

export async function buildFmap(saturn: DiscRead, ps1: DiscRead): Promise<Uint8Array> {
    return paintFmap(await ps1("F_MAP.BIN"), await saturn("SUB_DISP.MAP"));
}
