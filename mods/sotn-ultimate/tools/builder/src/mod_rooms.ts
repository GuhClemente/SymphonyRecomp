// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn rooms the mod adds to the Castle Entrance (NP3, and NO3 for the
// first visit), the Reverse Entrance (RNO3) and the warp rooms (WRP, RWRP):
// a byte-exact port of
// mods/sotn-ultimate/tools/gen_mod_rooms.py (see its header for what each part does).
//
// Inputs: the player's Saturn disc (STAGE_01 / RSTAGE01 / WARP / RWARP .PRG
// and .MAP) and PS1 disc (each stage's overlay ST/<S>/<S>.BIN, read for its
// rooms, layer and tile tables, and its texture ST/<S>/F_<S>.BIN). The script
// reads the same tables from the repo's extracted assets (assets/st/<s>/*.bin,
// src/st/<s>/gen/layers.h, rooms.c); parseStage() rebuilds them from the
// overlay. Mod constants only come from the script.
//
// Output: per header (src/st/<s>/mod_rooms.h), its arrays of plain numbers;
// and the stage textures with the Saturn's art ported in (build/us/F_<S>.BIN).
import { tilematch, usedTiles, type TileDefs } from "./saturn_tilematch.ts";

export type Reader = (path: string) => Promise<Uint8Array>;

export interface CArrayOut {
    size: number; // bytes per element
    values: number[];
}

export interface GenResult {
    headers: Map<string, Map<string, CArrayOut>>; // header path -> array name -> numbers
    binaries: Map<string, Uint8Array>; // repo path -> bytes
}

// ---------------------------------------------------------------------------
// The stage overlay's tables (loaded at 0x80180000; its header's rooms
// pointer at +0x10, rooms_layers at +0x20). The generated layers.h defines
// layer_empty, layers[] and rooms_layers back to back, in that order.

const OVL = 0x80180000;

export interface PsxLayer {
    addr: number;
    tilemapAddr: number;
    tilemapBytes: Uint8Array; // u16 LE, (right - left + 1) * (bottom - top + 1) * 256 cells
    tilemap: number[];
    tdAddr: number;
    left: number;
    top: number;
    right: number;
    bottom: number;
    params: number; // rect flags byte (0x40 / 0x41: a loading room)
    zPriority: number;
    flags: number;
}

export interface PsxStage {
    ov: Uint8Array;
    rooms: number[][]; // 8 bytes each
    layers: PsxLayer[]; // layers[]
    roomdefs: [number, number][]; // rooms_layers: (fg, bg) index into layers, -1 = layer_empty
    tiledefs: Map<number, TileDefs>; // by address
    layerEmpty: number; // addresses
    roomsLayers: number;
}

const u32At = (d: Uint8Array, o: number): number => (d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (d[o + 3] << 24)) >>> 0;

export function u16s(d: Uint8Array): number[] {
    const out: number[] = new Array(d.length >> 1);
    for (let i = 0; i < out.length; i++) {
        out[i] = d[2 * i] | (d[2 * i + 1] << 8);
    }
    return out;
}

export function u16Bytes(v: number[]): Uint8Array {
    const out = new Uint8Array(v.length * 2);
    for (let i = 0; i < v.length; i++) {
        out[2 * i] = v[i] & 0xff;
        out[2 * i + 1] = (v[i] >> 8) & 0xff;
    }
    return out;
}

export function parseStage(ov: Uint8Array): PsxStage {
    const rd = (a: number): number => u32At(ov, a - OVL);
    const roomsP = rd(OVL + 0x10);
    const rl = rd(OVL + 0x20);
    const rooms: number[][] = [];
    for (let a = roomsP - OVL; ov[a] !== 0x40; a += 8) {
        rooms.push(Array.from(ov.subarray(a, a + 8)));
    }
    let empty = rl - 16;
    while (rd(empty) !== 0) {
        empty -= 16;
    }
    for (let i = 0; i < 16; i++) {
        if (ov[empty - OVL + i]) {
            throw new Error("layer_empty not found");
        }
    }
    const n = (rl - empty) / 16 - 1;
    const tiledefs = new Map<number, TileDefs>();
    const layers: PsxLayer[] = [];
    for (let i = 0; i < n; i++) {
        const a = empty + 16 * (i + 1);
        const tilemapAddr = rd(a);
        const tdAddr = rd(a + 4);
        const rect = rd(a + 8);
        const left = rect & 63;
        const top = (rect >>> 6) & 63;
        const right = (rect >>> 12) & 63;
        const bottom = (rect >>> 18) & 63;
        const size = (right - left + 1) * (bottom - top + 1) * 512;
        const tilemapBytes = ov.slice(tilemapAddr - OVL, tilemapAddr - OVL + size);
        if (!tiledefs.has(tdAddr)) {
            const p = [0, 1, 2, 3].map((j) => rd(tdAddr + 4 * j));
            const part = (j: number): Uint8Array => ov.slice(p[j] - OVL, (j < 3 ? p[j + 1] : tdAddr) - OVL);
            tiledefs.set(tdAddr, { tiles: part(0), pages: part(1), cluts: part(2), cols: part(3) });
        }
        layers.push({
            addr: a, tilemapAddr, tilemapBytes, tilemap: u16s(tilemapBytes), tdAddr, left, top, right, bottom,
            params: rect >>> 24, zPriority: ov[a + 12 - OVL] | (ov[a + 13 - OVL] << 8),
            flags: ov[a + 14 - OVL] | (ov[a + 15 - OVL] << 8),
        });
    }
    const roomdefs: [number, number][] = [];
    const idx = (p: number): number => (p === empty ? -1 : (p - empty) / 16 - 1);
    for (let k = 0; ; k++) {
        const f = rd(rl + 8 * k);
        const b = rd(rl + 8 * k + 4);
        if (!(empty <= f && f < rl && empty <= b && b < rl && (f - empty) % 16 === 0 && (b - empty) % 16 === 0)) {
            break;
        }
        roomdefs.push([idx(f), idx(b)]);
    }
    return { ov, rooms, layers, roomdefs, tiledefs, layerEmpty: empty, roomsLayers: rl };
}

// the tilemaps layers.json lists with tiledef tdAddr, in its order (per
// RoomDef: bg, then fg), for saturn_tilematch.py's candidate tiles;
// override: replacement bytes by layer index (tilemaps edited in place)
export function layersJsonTilemaps(st: PsxStage, tdAddr: number, override: Map<number, Uint8Array> = new Map()): Uint8Array[] {
    const out: Uint8Array[] = [];
    for (const [f, b] of st.roomdefs) {
        for (const i of [b, f]) {
            if (i >= 0 && st.layers[i].tdAddr === tdAddr) {
                out.push(override.get(i) ?? st.layers[i].tilemapBytes);
            }
        }
    }
    return out;
}

export function copyDefs(td: TileDefs): TileDefs {
    return { tiles: td.tiles.slice(), pages: td.pages.slice(), cluts: td.cluts.slice(), cols: td.cols.slice() };
}

export function check(cond: boolean, msg: string): void {
    if (!cond) {
        throw new Error("gen_mod_rooms: " + msg);
    }
}

export function findRoom(rooms: number[][], b: number[]): number {
    const i = rooms.findIndex((r) => r[0] === b[0] && r[1] === b[1] && r[2] === b[2] && r[3] === b[3]);
    check(i >= 0, "no room " + b.join(","));
    return i;
}

// bytes -> a Map key
export function bkey(d: Uint8Array): string {
    let s = "";
    for (let i = 0; i < d.length; i += 4096) {
        s += String.fromCharCode.apply(null, Array.from(d.subarray(i, i + 4096)));
    }
    return s;
}

// Counter.most_common(1)[0][0]: the first inserted of the most counted
export function mostCommon<K>(c: Map<K, number>): K {
    let best: K | undefined;
    let bn = -1;
    for (const [k, n] of c) {
        if (n > bn) {
            best = k;
            bn = n;
        }
    }
    return best as K;
}

export function count<K>(m: Map<K, number>, k: K): void {
    m.set(k, (m.get(k) ?? 0) + 1);
}

const anyAlpha = (c: Uint8Array): boolean => {
    for (let i = 3; i < c.length; i += 4) {
        if (c[i]) {
            return true;
        }
    }
    return false;
};

// ---------------------------------------------------------------------------

const TP_NP3_TO_GDN = 0x83;
const TP_NO3_TO_GDN = 0x8b; // MOD_TP_NO3_TO_GDN
// NO3's forest rooms: room -> old rect, new row
const NO3_FOREST: [number, [number, number, number, number, number]][] = [
    [18, [0, 44, 17, 44, 46]], [19, [18, 44, 18, 44, 46]]];
const SAVE_ROOM = [3, 42];
const SAVE_ROOM_LAYOUT_FROM = [5, 39];
const ALCOVE_ROWS = [39, 40, 41, 42];
const RETAIL_ROWS = 21;
type Graft = [number[], number, number[]]; // rows, source column, columns
const ALCOVE_CEILING: Graft = [[36, 37, 38], 13, [14, 15]];
const ALCOVE_FLOOR: Graft = [[43, 44], 12, [13, 14, 15]];
const TRAPDOOR_TILES = [0x129, 0x132, 0x6c8, 0x6c9];
// the tile ids src/st/<s>/e_red_door_tiles.c lists (g_RedDoorTiles; NP3
// and RNO3 list the same ids), kept out of the free tile slots
const DOOR_TILES = [0x06c7, 0x04fa, 0x030f, 0x0000];
type Override = [[number, number], [number, number], string];
const TILE_OVERRIDES: Override[] = [[[30, 2], [23, 8], "H"]];
const PORT_MIN_ERR = 30;

const rgb = (c: number): [number, number, number] =>
    [Math.floor(((c & 31) * 255) / 31), Math.floor((((c >> 5) & 31) * 255) / 31), Math.floor((((c >> 10) & 31) * 255) / 31)];

const hwOff = (X: number, Y: number): number =>
    (((X >> 6) << 2) | ((Y >> 7) << 1) | ((X % 64) >> 5)) * 0x2000 + ((Y % 128) * 32 + (X % 32)) * 2;

// port_cells: Saturn art for the cells no PSX tile matches, into fbin's free
// 16x16 slots (fbin, td, free and tiles are updated)
function portCells(fbin: Uint8Array, stageDefs: TileDefs[], poolTm: number[], td: TileDefs, free: number[],
                   tiles: number[], coll: number[], err: number[], px: Uint8Array, overrides: Override[],
                   palPairs: [Uint8Array, number][] | null): void {
    const hw = (X: number, Y: number): number => {
        const o = hwOff(X, Y);
        return fbin[o] | (fbin[o + 1] << 8);
    };
    const refs = new Set<number>();
    for (const d of stageDefs) {
        const n = Math.min(d.tiles.length, d.pages.length);
        for (let t = 0; t < n; t++) {
            refs.add((d.tiles[t] << 8) | d.pages[t]);
        }
    }
    const slots: [number, number][] = [];
    for (const page of [4, 5, 6, 0, 1, 2, 3]) {
        for (let i = 0; i < 256; i++) {
            const u = (i & 15) * 16;
            const v = i & 0xf0;
            if (refs.has((page << 8) | i) || (page < 4 && v >= 0xf0)) {
                continue;
            }
            let blank = true;
            for (let y = 0; y < 16 && blank; y++) {
                for (let k = 0; k < 4; k++) {
                    if (hw(page * 64 + (u >> 2) + k, v + y) !== 0) {
                        blank = false;
                        break;
                    }
                }
            }
            if (blank) {
                slots.push([page, i]);
            }
        }
    }

    const poolSet = new Set(poolTm);
    poolSet.delete(0);
    const clutSet = new Set<number>();
    for (const t of poolSet) {
        clutSet.add(td.cluts[t]);
    }
    const cluts = [...clutSet].sort((a, b) => a - b);
    const pals = new Map<number, number[]>();
    for (const k of cluts) {
        const p: number[] = [];
        for (let j = 0; j < 16; j++) {
            p.push(hw(16 * (k % 16) + j, 0xf0 + (k >> 4)));
        }
        pals.set(k, p);
    }

    // votes[k][rgb][j]
    const votes = new Map<number, Map<number, Map<number, number>>>();
    for (const [block, t] of palPairs ?? []) {
        const page = td.tiles[t];
        const i = td.pages[t];
        const k = td.cluts[t];
        const u = (i & 15) * 16;
        const v = i & 0xf0;
        for (let y = 0; y < 16; y++) {
            for (let x = 0; x < 16; x++) {
                const q = (y * 16 + x) * 4;
                if (!block[q + 3]) {
                    continue;
                }
                const j = (hw(page * 64 + ((u + x) >> 2), v + y) >> (((u + x) % 4) * 4)) & 15;
                if (j) {
                    let table = votes.get(k);
                    if (table === undefined) {
                        votes.set(k, (table = new Map()));
                    }
                    const c = (block[q] << 16) | (block[q + 1] << 8) | block[q + 2];
                    let ctr = table.get(c);
                    if (ctr === undefined) {
                        table.set(c, (ctr = new Map()));
                    }
                    count(ctr, j);
                }
            }
        }
    }

    const quantizeLearned = (block: Uint8Array): [number, number[]] => {
        let best: [number, number, number[]] | null = null;
        for (const [k, table] of votes) {
            const keys = [...table.keys()];
            const idx: number[] = [];
            let tot = 0;
            for (let q = 0; q < 256; q++) {
                if (!block[q * 4 + 3]) {
                    idx.push(0);
                    continue;
                }
                const r = block[q * 4];
                const g = block[q * 4 + 1];
                const b = block[q * 4 + 2];
                const c = (r << 16) | (g << 8) | b;
                let near = c;
                let e = 0;
                if (!table.has(c)) {
                    let bn = -1;
                    for (const kk of keys) {
                        const dr = r - (kk >> 16);
                        const dg = g - ((kk >> 8) & 255);
                        const db = b - (kk & 255);
                        const d = dr * dr + dg * dg + db * db;
                        if (bn < 0 || d < e) {
                            bn = kk;
                            e = d;
                        }
                    }
                    near = bn;
                }
                idx.push(mostCommon(table.get(near)!));
                tot += e;
            }
            if (best === null || tot < best[0]) {
                best = [tot, k, idx];
            }
        }
        return [best![1], best![2]];
    };

    const quantize = (block: Uint8Array): [number, number[]] => {
        if (votes.size) {
            return quantizeLearned(block);
        }
        let best: [number, number, number[]] | null = null;
        for (const [k, pal] of pals) {
            const opts: [number, [number, number, number]][] = [];
            for (let j = 1; j < 16; j++) {
                if (pal[j]) {
                    opts.push([j, rgb(pal[j])]);
                }
            }
            const idx: number[] = [];
            let tot = 0;
            for (let q = 0; q < 256; q++) {
                const r = block[q * 4];
                const g = block[q * 4 + 1];
                const b = block[q * 4 + 2];
                if (!block[q * 4 + 3]) {
                    idx.push(0);
                    continue;
                }
                let bj = -1;
                let be = 0;
                for (const [j, c] of opts) {
                    const e = (r - c[0]) ** 2 + (g - c[1]) ** 2 + (b - c[2]) ** 2;
                    if (bj < 0 || e < be) {
                        bj = j;
                        be = e;
                    }
                }
                check(bj >= 0, "min() of an empty palette");
                idx.push(bj);
                tot += be;
            }
            if (best === null || tot < best[0]) {
                best = [tot, k, idx];
            }
        }
        return [best![1], best![2]];
    };

    const writeCell = (page: number, i: number, pix: (y: number, x: number) => number): void => {
        const u = (i & 15) * 16;
        const v = i & 0xf0;
        for (let y = 0; y < 16; y++) {
            for (let x4 = 0; x4 < 4; x4++) {
                let val = 0;
                for (let b = 0; b < 4; b++) {
                    val |= pix(y, x4 * 4 + b) << (b * 4);
                }
                const o = hwOff(page * 64 + (u >> 2) + x4, v + y);
                fbin[o] = val & 0xff;
                fbin[o + 1] = val >> 8;
            }
        }
    };

    const order: number[] = [];
    for (let n = 0; n < tiles.length; n++) {
        if (err[n] > PORT_MIN_ERR) {
            order.push(n);
        }
    }
    order.sort((a, b) => err[b] - err[a]); // stable
    const ported = new Map<string, number>();
    for (const n of order) {
        const block = px.subarray(n * 1024, (n + 1) * 1024);
        const key = bkey(block);
        if (!ported.has(key)) {
            if (!slots.length) {
                continue;
            }
            const [page, i] = slots.shift()!;
            const [k, idx] = quantize(block);
            writeCell(page, i, (y, x) => idx[y * 16 + x]);
            check(free.length > 0, "no free tile");
            const t = free.shift()!;
            td.tiles[t] = page;
            td.pages[t] = i;
            td.cluts[t] = k;
            td.cols[t] = coll[n];
            ported.set(key, t);
        }
        tiles[n] = ported.get(key)!;
    }
    for (const [[ty, tx], [sy, sx], flip] of overrides) {
        const src = tiles[sy * 16 + sx];
        const page = td.tiles[src];
        const i = td.pages[src];
        const u = (i & 15) * 16;
        const v = i & 0xf0;
        let pix: number[][] = [];
        for (let y = 0; y < 16; y++) {
            const row: number[] = [];
            for (let x = 0; x < 16; x++) {
                row.push((hw(page * 64 + ((u + x) >> 2), v + y) >> (((u + x) % 4) * 4)) & 15);
            }
            pix.push(row);
        }
        if (flip.includes("H")) {
            pix = pix.map((r) => r.slice().reverse());
        }
        if (flip.includes("V")) {
            pix = pix.slice().reverse();
        }
        check(slots.length > 0, "no free slot");
        const [page2, i2] = slots.shift()!;
        writeCell(page2, i2, (y, x) => pix[y][x]);
        const t = free.shift()!;
        td.tiles[t] = page2;
        td.pages[t] = i2;
        td.cluts[t] = td.cluts[src];
        td.cols[t] = coll[ty * 16 + tx];
        tiles[ty * 16 + tx] = t;
    }
}

// narrow_region: the cells' columns re-narrowed from the full-width art
function narrowRegion(cellPx: Uint8Array, fw: number, fpx: Uint8Array, cells: number[]): Uint8Array {
    const cols = [...new Set(cells.map((n) => n % 16))].sort((a, b) => a - b);
    const rows = [...new Set(cells.map((n) => n >> 4))].sort((a, b) => a - b);
    check(cols.length === cols[cols.length - 1] - cols[0] + 1, "narrow_region columns");
    const x0 = cols[0] * 20;
    const x1 = (cols[cols.length - 1] + 1) * 20;
    const y0 = rows[0] * 16;
    const y1 = (rows[rows.length - 1] + 1) * 16;
    const diff = (a: number, b: number): number => {
        let d = 0;
        for (let y = y0; y < y1; y++) {
            const p = (y * fw + a) * 4;
            const q = (y * fw + b) * 4;
            d += (fpx[p] - fpx[q]) ** 2 + (fpx[p + 1] - fpx[q + 1]) ** 2 + (fpx[p + 2] - fpx[q + 2]) ** 2 +
                ((fpx[p + 3] > 0) === (fpx[q + 3] > 0) ? 0 : 3 * 255 * 255);
        }
        return d;
    };
    const keep: number[] = [];
    for (let x = x0; x < x1; x++) {
        keep.push(x);
    }
    const target = Math.floor(((x1 - x0) * 4) / 5);
    while (keep.length > target) {
        let bi = -1;
        let bk = 0;
        for (let i = 1; i < keep.length - 1; i++) {
            const k = Math.min(diff(keep[i], keep[i - 1]), diff(keep[i], keep[i + 1]));
            if (bi < 0 || k < bk) {
                bi = i;
                bk = k;
            }
        }
        keep.splice(bi, 1);
    }
    const out = cellPx.slice();
    for (const n of cells) {
        const r = n >> 4;
        const c = n % 16;
        for (let y = 0; y < 16; y++) {
            for (let x = 0; x < 16; x++) {
                const sx = keep[(c - cols[0]) * 16 + x];
                const sy = r * 16 + y;
                const o = (sy * fw + sx) * 4;
                out.set(fpx.subarray(o, o + 4), n * 1024 + (y * 16 + x) * 4);
            }
        }
    }
    return out;
}

function smoothNp3Step(fg: number[], bg: number[], coll: number[]): void {
    const at = (y: number, x: number): number => y * 16 + x;
    const want = [0x80, 0x80, 0x03, 0x80, 0x01, 0x03];
    const got = [[46, 8], [45, 9], [44, 10], [43, 10], [43, 11], [43, 12]].map(([y, x]) => coll[at(y, x)]);
    check(got.every((v, i) => v === want[i]), "NP3 step cells");
    const slope = at(45, 9);
    const under = at(46, 9);
    const corner = at(43, 11);
    const cells: [number, [number, number, number]][] = [
        [at(43, 12), [fg[corner], bg[at(43, 12)], coll[corner]]],
        [at(43, 11), [fg[slope], bg[at(43, 11)], 0x80]],
        [at(44, 10), [fg[slope], bg[at(40, 10)], 0x80]],
        [at(43, 10), [0, bg[at(43, 10)], 0x00]],
        [at(44, 11), [fg[under], bg[at(40, 11)], 0x03]],
        [at(45, 10), [fg[under], bg[at(41, 10)], 0x03]],
    ];
    for (const [n, [t, b, c]] of cells) {
        fg[n] = t;
        bg[n] = b;
        coll[n] = c;
    }
}

const u8Arr = (v: ArrayLike<number>): CArrayOut => ({ size: 1, values: Array.from(v) });
const u16Arr = (v: number[]): CArrayOut => ({ size: 2, values: v.slice() });

function tdArrays(out: Map<string, CArrayOut>, prefix: string, td: TileDefs): void {
    out.set(prefix + "Page", u8Arr(td.tiles));
    out.set(prefix + "Index", u8Arr(td.pages));
    out.set(prefix + "Clut", u8Arr(td.cluts));
    out.set(prefix + "Coll", u8Arr(td.cols));
}

function roomsArray(rooms: number[][]): CArrayOut {
    const v: number[] = [];
    for (const r of rooms) {
        v.push(...r);
    }
    v.push(0x40);
    return { size: 1, values: v };
}

// the free tile ids: unreserved, unused by the stage first
function freeTiles(n: number, reserved: Set<number>, used: Set<number>): number[] {
    const a: number[] = [];
    const b: number[] = [];
    for (let i = 1; i < n; i++) {
        if (!reserved.has(i)) {
            (used.has(i) ? b : a).push(i);
        }
    }
    return a.concat(b);
}

function stageUsed(st: PsxStage, tdAddr: number): Set<number> {
    const s = new Set<number>();
    for (const l of st.layers) {
        if (l.tdAddr === tdAddr) {
            for (const t of l.tilemap) {
                s.add(t);
            }
        }
    }
    return s;
}

function applyCombos(newFg: number[], fgColl: number[], td: TileDefs, free: number[]): void {
    const combos = new Map<number, number>();
    for (let n = 0; n < newFg.length; n++) {
        const t = newFg[n];
        const c = fgColl[n];
        check(t !== 0 || c === 0, "cell " + n + ": solid without a tile");
        if (!t || td.cols[t] === c) {
            continue;
        }
        const key = t * 256 + c;
        if (!combos.has(key)) {
            const i = free.shift()!;
            combos.set(key, i);
            td.tiles[i] = td.tiles[t];
            td.pages[i] = td.pages[t];
            td.cluts[i] = td.cluts[t];
            td.cols[i] = c;
        }
        newFg[n] = combos.get(key)!;
    }
    check(newFg.every((t, n) => td.cols[t] === fgColl[n]), "combos");
}

function graftAlcove(layers: number[][], [rows, src, dsts]: Graft): void {
    for (const y of rows) {
        for (const x of dsts) {
            for (const l of layers) {
                l[y * 16 + x] = l[y * 16 + src];
            }
        }
    }
}

interface Ctx {
    sat: Reader;
    psx: Reader;
}

async function loadStage(psx: Reader, up: string): Promise<[PsxStage, Uint8Array]> {
    const st = parseStage(await psx(`ST/${up}/${up}.BIN`));
    const fbin = await psx(`ST/${up}/F_${up}.BIN`);
    return [st, fbin.slice()];
}

// stage "np3", or "no3" (the first visit's Entrance: the shaft's tiles go
// into empty slots of the stage's own tileset, g_ModTiledefPatch, and its
// forest rooms move to row 46)
async function np3(ctx: Ctx, res: GenResult, stage: string = "np3"): Promise<void> {
    const patch = stage === "no3";
    const tp = patch ? TP_NO3_TO_GDN : TP_NP3_TO_GDN;
    const [st, fbin] = await loadStage(ctx.psx, stage.toUpperCase());
    const prg = await ctx.sat("STAGE_01.PRG");
    const map = await ctx.sat("STAGE_01.MAP");
    const { rooms, layers, roomdefs } = st;
    if (patch) {
        for (const [i, [l, t, r, b]] of NO3_FOREST) {
            check(rooms[i].slice(0, 4).join() === [l, t, r, b].join(), stage + " forest room " + i);
        }
    }
    const shaft = 1;
    check(rooms[shaft].slice(0, 4).join() === "2,41,2,42", stage + " shaft");
    const load = findRoom(rooms, [15, 41, 15, 41]);
    const fg = layers[roomdefs[rooms[shaft][4]][0]];
    const bg = layers[roomdefs[rooms[shaft][4]][1]];
    const tdAddr = fg.tdAddr; // <s>_tiledef_<s>
    const defs = st.tiledefs.get(tdAddr)!;

    const mFg = tilematch(prg, map, 17, 0, fbin, defs, usedTiles([fg.tilemapBytes]), true);
    const mBg = tilematch(prg, map, 17, 2, fbin, defs, usedTiles(layersJsonTilemaps(st, tdAddr)), false);
    const newFg = mFg.tiles.slice();
    const newBg = mBg.tiles.slice();
    const fgColl = Array.from(mFg.coll!);
    let fgErr = mFg.errs.slice();
    const fgPx = mFg.cells;
    check(newFg.length === 1024 && newBg.length === 1024 && fgColl.length === 1024, stage + " shaft size");

    const td = copyDefs(defs);
    const used = stageUsed(st, tdAddr);
    const reserved = new Set<number>([0, ...newFg, ...DOOR_TILES, ...TRAPDOOR_TILES]);
    const emptySlot = (d: TileDefs, i: number): boolean => !d.tiles[i] && !d.pages[i] && !d.cluts[i] && !d.cols[i];
    // NO3: only slots no tilemap uses whose four entries are all 0
    const free = patch
        ? freeTiles(td.cols.length, reserved, used).filter((i) => !used.has(i) && emptySlot(td, i))
        : freeTiles(td.cols.length, reserved, used);

    const retailFg = fg.tilemap;
    const retailBg = bg.tilemap;
    const n = RETAIL_ROWS * 16;
    for (let i = 0; i < n; i++) {
        check(td.cols[retailFg[i]] === fgColl[i], "retail rows differ");
        newFg[i] = retailFg[i];
        newBg[i] = retailBg[i];
    }
    fgErr = fgErr.map((e, i) => (i < n ? 0 : e));

    portCells(fbin, [...st.tiledefs.values()], fg.tilemap, td, free, newFg, fgColl, fgErr, fgPx, TILE_OVERRIDES, null);

    for (const y of ALCOVE_ROWS) {
        check(fgColl[y * 16 + 15] === 3 && fgColl[y * 16 + 14] === 0, "alcove row " + y);
        newFg[y * 16 + 15] = newFg[y * 16 + 14];
        newBg[y * 16 + 15] = newBg[y * 16 + 14];
        fgColl[y * 16 + 15] = 0;
    }
    check(fgColl[(ALCOVE_ROWS[0] - 1) * 16 + 15] === 3, "alcove top");
    check(fgColl[(ALCOVE_ROWS[ALCOVE_ROWS.length - 1] + 1) * 16 + 15] === 3, "alcove bottom");
    for (const g of [ALCOVE_CEILING, ALCOVE_FLOOR]) {
        graftAlcove([newFg, newBg, fgColl], g);
    }
    smoothNp3Step(newFg, newBg, fgColl);
    applyCombos(newFg, fgColl, td, free);

    const save = findRoom(rooms, [...SAVE_ROOM, ...SAVE_ROOM]);
    const sfg = layers[roomdefs[rooms[save][4]][0]];
    const saveFg = sfg.tilemap.slice();
    const scols = st.tiledefs.get(sfg.tdAddr)!.cols;
    const door: number[][] = [];
    for (let y = 0; y < 16; y++) {
        door.push([0, 1, 2].map((x) => saveFg[y * 16 + x]));
    }
    for (let y = 6; y < 10; y++) {
        check(door[y].every((t) => scols[t] === 0), "door rows");
    }
    check([...door[10], ...door[11], ...door[5]].every((t) => scols[t] === 3), "door frame");
    for (let y = 6; y < 12; y++) {
        for (let x = 0; x < 3; x++) {
            saveFg[y * 16 + x] = door[y - 1][x];
        }
    }
    for (let y = 6; y < 11; y++) {
        check(fgColl[(SAVE_ROOM[1] + 1 - 41) * 256 + y * 16 + 15] === 0, "alcove rows");
    }
    for (const [y, src] of [[6, 6], [7, 7]]) {
        for (let x = 0; x < 3; x++) {
            saveFg[y * 16 + x] = door[src][x];
        }
    }
    for (let y = 6; y < 11; y++) {
        for (let x = 0; x < 3; x++) {
            check(scols[saveFg[y * 16 + x]] === 0, "door open");
        }
    }

    const newLayerId = roomdefs.length;
    const saveLayerId = roomdefs.length + 1;
    const outRooms = rooms.map((r) => r.slice());
    outRooms[shaft][3] = 44;
    outRooms[shaft][6] = 9;
    const lr = rooms[load];
    outRooms.push([3, 44, 3, 44, newLayerId, lr[5], lr[6], lr[7]]);
    outRooms.push([4, 44, 4, 44, tp, 0xff, 0, 0]);
    if (patch) {
        // the forest run to row 46, with its layers' rects (only checked here:
        // the layers are not among the arrays)
        for (const [i, [, t, , b, row]] of NO3_FOREST) {
            outRooms[i][1] = outRooms[i][3] = row;
            for (const idx of roomdefs[rooms[i][4]]) {
                if (idx < 0) {
                    continue;
                }
                const lay = layers[idx];
                if (!lay.left && !lay.top && !lay.right && !lay.bottom) {
                    continue; // a layer without a rect (the forest's backdrop)
                }
                check(lay.top === t && lay.bottom === b, stage + " forest layer rect " + idx);
                let users = 0;
                for (const r2 of rooms) {
                    const rd = roomdefs[r2[4]];
                    check(rd !== undefined, stage + " room layer id " + r2[4]);
                    users += rd[0] === idx || rd[1] === idx ? 1 : 0;
                }
                check(users === 1, "forest layer " + idx + " shared");
            }
        }
        for (const [cx, cy] of [[2, 43], [2, 44], [3, 44], [4, 44]]) {
            check(!outRooms.some((r, i) => i !== shaft && r.slice(0, 4).join() !== [cx, cy, cx, cy].join() &&
                                     r[0] <= cx && cx <= r[2] && r[1] <= cy && cy <= r[3]),
                  stage + " cell taken " + cx + "," + cy);
        }
    }
    const [sx, sy] = SAVE_ROOM;
    outRooms[save].splice(0, 5, sx, sy + 1, sx, sy + 1, saveLayerId);
    const realSave = rooms[findRoom(rooms, [...SAVE_ROOM_LAYOUT_FROM, ...SAVE_ROOM_LAYOUT_FROM])];
    outRooms[save][7] = realSave[7];
    check(!rooms.some((r, i) => i !== save && r[0] <= sx && sx <= r[2] && r[1] <= sy + 1 && sy + 1 <= r[3]),
          "save room target taken");

    const arrays = new Map<string, CArrayOut>();
    arrays.set("g_ModShaftFg", u16Arr(newFg));
    arrays.set("g_ModShaftBg", u16Arr(newBg));
    arrays.set("g_ModSaveFg", u16Arr(saveFg));
    if (patch) {
        // {slot, gfxPage, gfxIndex, clut, collision} for the stage's tileset
        const changed: number[] = [];
        for (let i = 0; i < td.cols.length; i++) {
            if (td.tiles[i] !== defs.tiles[i] || td.pages[i] !== defs.pages[i] ||
                td.cluts[i] !== defs.cluts[i] || td.cols[i] !== defs.cols[i]) {
                changed.push(i);
            }
        }
        check(changed.length > 0 && !changed.some((i) => used.has(i) || reserved.has(i)), "patched slots in use");
        check(changed.every((i) => emptySlot(defs, i)), "patched slots not empty");
        arrays.set("g_ModTiledefPatch", u16Arr(changed.flatMap((i) => [i, td.tiles[i], td.pages[i], td.cluts[i], td.cols[i]])));
    } else {
        tdArrays(arrays, "g_ModShaftTd", td);
    }
    arrays.set("g_ModRooms", roomsArray(outRooms));
    res.headers.set(`src/st/${stage}/mod_rooms.h`, arrays);
    res.binaries.set(`build/us/F_${stage.toUpperCase()}.BIN`, fbin);
}

const RNO3_SHAFT = 1;
const RNO3_SAVE_ROOM = [60, 21];
const RNO3_SAVE_LAYOUT_FROM = [58, 24];
const RNO3_LOAD_FROM = [48, 22];
const RNO3_ALCOVE_ROWS = [21, 22, 23, 24];
const RNO3_RETAIL_FROM = 64 - RETAIL_ROWS;
const RNO3_ALCOVE_FLOOR: Graft = [[25, 26, 27], 2, [1, 0]];
const RNO3_ALCOVE_CEILING: Graft = [[19, 20], 3, [2, 1, 0]];
const RNO3_TILE_OVERRIDES: Override[] = [[[33, 13], [40, 7], "H"]];
const RNO3_FOREST: [number, number[]][] = [[18, [46, 17, 63, 17]], [19, [45, 17, 45, 17]]];
const RNO3_SCONCE_GFX_ID = 9;
const RNO3_TRAPDOOR: [[number, number], [number, number]][] = [[[5, 24], [0x129, 0x6c8]], [[5, 23], [0x132, 0x6c9]]];

async function rno3(ctx: Ctx, res: GenResult): Promise<void> {
    const [st, fbin] = await loadStage(ctx.psx, "RNO3");
    const prg = await ctx.sat("RSTAGE01.PRG");
    const map = await ctx.sat("RSTAGE01.MAP");
    const { rooms, layers, roomdefs } = st;
    const shaft = RNO3_SHAFT;
    check(rooms[shaft].slice(0, 4).join() === "61,21,61,22", "rno3 shaft");
    const load = findRoom(rooms, [...RNO3_LOAD_FROM, ...RNO3_LOAD_FROM]);
    const fg = layers[roomdefs[rooms[shaft][4]][0]];
    const bg = layers[roomdefs[rooms[shaft][4]][1]];
    for (const [i] of RNO3_FOREST) {
        const p = rooms[i].slice(0, 2).join();
        check(p === "46,19" || p === "45,19", "rno3 forest room");
    }
    const tdAddr = fg.tdAddr; // rno3_tiledef_rno3
    const defs = st.tiledefs.get(tdAddr)!;
    const mFg = tilematch(prg, map, 17, 0, fbin, defs, usedTiles([fg.tilemapBytes]), true);
    const mBg = tilematch(prg, map, 17, 2, fbin, defs, usedTiles(layersJsonTilemaps(st, tdAddr)), false);
    const newFg = mFg.tiles.slice();
    const newBg = mBg.tiles.slice();
    const fgColl = Array.from(mFg.coll!);
    const fgErr = mFg.errs.slice();
    const fgPx = mFg.cells;
    const bgPx = mBg.cells;
    check(newFg.length === 1024 && newBg.length === 1024 && fgColl.length === 1024, "rno3 shaft size");

    const td = copyDefs(defs);
    const used = stageUsed(st, tdAddr);
    const reserved = new Set<number>([0, ...newFg, ...DOOR_TILES]);
    const free = freeTiles(td.cols.length, reserved, used);

    const retailFg = fg.tilemap;
    const retailBg = bg.tilemap;
    check(retailFg.length === 512 && retailBg.length === 512, "rno3 retail shaft size");
    const a = RNO3_RETAIL_FROM * 16;
    const b = (RNO3_RETAIL_FROM - 32) * 16;
    const retailColl = retailFg.slice(b).map((t) => td.cols[t]);
    let differ = 0;
    for (let i = 0; i < retailColl.length && a + i < 1024; i++) {
        differ += retailColl[i] !== fgColl[a + i] ? 1 : 0;
    }
    check(differ <= 1, "retail rows differ in " + differ + " cells");
    for (let i = 0; i < 1024 - a; i++) {
        fgColl[a + i] = retailColl[i];
        newFg[a + i] = retailFg[b + i];
        newBg[a + i] = retailBg[b + i];
        fgErr[a + i] = 0;
    }

    const cell = (px: Uint8Array, n: number): Uint8Array => px.subarray(n * 1024, (n + 1) * 1024);
    const learn = (px: Uint8Array, layer: number[]): Map<string, [Uint8Array, number]> => {
        const m = new Map<string, [Uint8Array, Map<number, number>]>();
        for (let n = a; n < 1024; n++) {
            const c = cell(px, n);
            const k = bkey(c);
            let e = m.get(k);
            if (e === undefined) {
                m.set(k, (e = [c, new Map()]));
            }
            count(e[1], layer[n]);
        }
        const out = new Map<string, [Uint8Array, number]>();
        for (const [k, [c, ctr]] of m) {
            out.set(k, [c, mostCommon(ctr)]);
        }
        return out;
    };
    const fgMap = learn(fgPx, newFg);
    const bgMap = learn(bgPx, newBg);
    const bgVisible = [...bgMap.values()].filter(([c]) => anyAlpha(c));
    for (let n = 0; n < a; n++) {
        let c = cell(fgPx, n);
        if (anyAlpha(c)) {
            const e = fgMap.get(bkey(c));
            if (e !== undefined && e[1]) {
                newFg[n] = e[1];
                fgErr[n] = 0;
            } else {
                fgErr[n] = Math.max(fgErr[n], PORT_MIN_ERR + 1);
            }
        }
        c = cell(bgPx, n);
        if (!anyAlpha(c)) {
            newBg[n] = 0;
            continue;
        }
        const e = bgMap.get(bkey(c));
        if (e !== undefined) {
            newBg[n] = e[1];
            continue;
        }
        let best = -1;
        let bd = 0;
        for (let i = 0; i < bgVisible.length; i++) {
            const k = bgVisible[i][0];
            let d = 0;
            for (let j = 0; j < 1024; j++) {
                const x = c[j] - k[j];
                d += x * x;
            }
            if (best < 0 || d < bd) {
                best = i;
                bd = d;
            }
        }
        newBg[n] = bgVisible[best][1];
    }
    const palPairs: [Uint8Array, number][] = [];
    for (let n = a; n < 1024; n++) {
        if (newFg[n] && anyAlpha(cell(fgPx, n))) {
            palPairs.push([cell(fgPx, n), newFg[n]]);
        }
    }

    portCells(fbin, [...st.tiledefs.values()], fg.tilemap, td, free, newFg, fgColl, fgErr, fgPx,
              RNO3_TILE_OVERRIDES, palPairs);

    for (const y of RNO3_ALCOVE_ROWS) {
        check(fgColl[y * 16] === 3 && fgColl[y * 16 + 1] === 0, "alcove row " + y);
        newFg[y * 16] = newFg[y * 16 + 1];
        newBg[y * 16] = newBg[y * 16 + 1];
        fgColl[y * 16] = 0;
    }
    check(fgColl[(RNO3_ALCOVE_ROWS[0] - 1) * 16] === 3, "alcove top");
    check(fgColl[(RNO3_ALCOVE_ROWS[RNO3_ALCOVE_ROWS.length - 1] + 1) * 16] === 3, "alcove bottom");
    for (const g of [RNO3_ALCOVE_FLOOR, RNO3_ALCOVE_CEILING]) {
        graftAlcove([newFg, newBg, fgColl], g);
    }
    applyCombos(newFg, fgColl, td, free);

    const save = findRoom(rooms, [...RNO3_SAVE_ROOM, ...RNO3_SAVE_ROOM]);
    const sfg = layers[roomdefs[rooms[save][4]][0]];
    const saveFg = sfg.tilemap.slice();
    const scols = st.tiledefs.get(sfg.tdAddr)!.cols;
    const door: number[][] = [];
    for (let y = 0; y < 16; y++) {
        door.push([13, 14, 15].map((x) => saveFg[y * 16 + x]));
    }
    for (let y = 6; y < 10; y++) {
        check(door[y].every((t) => scols[t] === 0), "door rows");
        check(fgColl[(16 + y) * 16] === 0, "alcove open");
    }
    check(door[10].every((t) => scols[t] === 3), "door floor");
    check(fgColl[(16 + 10) * 16] === 3, "alcove floor");
    check(fgColl[(16 + 5) * 16] === 0 && fgColl[(16 + 4) * 16] === 3, "alcove ceiling");
    for (const [y, src] of [[5, 6], [6, 7]]) {
        for (let x = 0; x < 3; x++) {
            saveFg[y * 16 + 13 + x] = door[src][x];
        }
    }
    for (let y = 5; y < 10; y++) {
        for (let x = 0; x < 3; x++) {
            check(scols[saveFg[y * 16 + 13 + x]] === 0, "door open");
        }
    }

    const r0Def = roomdefs[rooms[0][4]];
    check(rooms[0].slice(0, 4).join() === "60,23,61,25" && r0Def[0] >= 0, "rno3 room 0");
    const room0Fg = layers[r0Def[0]].tilemap.slice();
    for (const [[y, x], [old, nw]] of RNO3_TRAPDOOR) {
        check(room0Fg[y * 32 + x] === old, "rno3 trapdoor tile");
        room0Fg[y * 32 + x] = nw;
    }
    const lfg = layers[roomdefs[rooms[load][4]][0]];
    check(lfg.params === 0x40, "rno3 loading room exit");

    const newLayerId = roomdefs.length;
    const saveLayerId = roomdefs.length + 1;
    const outRooms = rooms.map((r) => r.slice());
    outRooms[shaft][1] = 19;
    outRooms[shaft][6] = RNO3_SCONCE_GFX_ID;
    for (const [i, bounds] of RNO3_FOREST) {
        outRooms[i].splice(0, 4, ...bounds);
    }
    const lr = rooms[load];
    outRooms.push([60, 19, 60, 19, newLayerId, lr[5], lr[6], lr[7]]);
    outRooms.push([59, 19, 59, 19, TP_NP3_TO_GDN, 0xff, 0, 0]);
    const [sx, sy] = RNO3_SAVE_ROOM;
    outRooms[save].splice(0, 5, sx, sy - 1, sx, sy - 1, saveLayerId);
    const realSave = rooms[findRoom(rooms, [...RNO3_SAVE_LAYOUT_FROM, ...RNO3_SAVE_LAYOUT_FROM])];
    outRooms[save][7] = realSave[7];
    const taken = new Set<number>();
    for (const r of outRooms) {
        for (let cx = r[0]; cx <= r[2]; cx++) {
            for (let cy = r[1]; cy <= r[3]; cy++) {
                check(!taken.has(cx * 256 + cy), "cell taken " + cx + "," + cy);
                taken.add(cx * 256 + cy);
            }
        }
    }

    const arrays = new Map<string, CArrayOut>();
    arrays.set("g_ModShaftFg", u16Arr(newFg));
    arrays.set("g_ModShaftBg", u16Arr(newBg));
    arrays.set("g_ModSaveFg", u16Arr(saveFg));
    arrays.set("g_ModRoom0Fg", u16Arr(room0Fg));
    tdArrays(arrays, "g_ModShaftTd", td);
    arrays.set("g_ModRooms", roomsArray(outRooms));
    res.headers.set("src/st/rno3/mod_rooms.h", arrays);
    res.binaries.set("build/us/F_RNO3.BIN", fbin);
}

const TP_WRP_TO_GDN = 0x86;
const WRP_ROOM6_INDEX = 15;
const WRP_ROOM6_LAYOUT = 6;
const WARP6: Record<string, { sat: string; cell: number[]; src: number[]; load: number[]; dx: number }> = {
    wrp: { sat: "WARP", cell: [2, 45], src: [15, 38], load: [16, 38], dx: 1 },
    rwrp: { sat: "RWARP", cell: [61, 18], src: [48, 25], load: [47, 25], dx: -1 },
};

async function wrp(ctx: Ctx, res: GenResult, stage: string): Promise<void> {
    const spec = WARP6[stage];
    const up = stage.toUpperCase();
    const [st, fbin] = await loadStage(ctx.psx, up);
    const { rooms, layers, roomdefs } = st;
    check(rooms.length === WRP_ROOM6_INDEX, "warp rooms");
    check(!rooms.some((r) => r[5] !== 0xff && r[7] === WRP_ROOM6_LAYOUT), "warp layout taken");
    const src = findRoom(rooms, [...spec.src, ...spec.src]);
    const load = findRoom(rooms, [...spec.load, ...spec.load]);
    const [x, y] = spec.cell;
    const dx = spec.dx;
    const srcFg = layers[roomdefs[rooms[src][4]][0]];
    const tdAddr = srcFg.tdAddr; // <s>_tiledef_<s>
    const defs = st.tiledefs.get(tdAddr)!;
    const psx = layers.filter((l) => l.tdAddr === tdAddr).map((l) => l.tilemap);
    const head: number[] = [];
    for (let n = 0; n < 256; n++) {
        if (new Set(psx.map((t) => t[n])).size > 1 && n % 16 >= 3 && n % 16 <= 12) {
            head.push(n);
        }
    }
    const prg = await ctx.sat(spec.sat + ".PRG");
    const map = await ctx.sat(spec.sat + ".MAP");
    const pool = usedTiles([srcFg.tilemapBytes]);

    const stacked = (room: number): [Uint8Array, number, Uint8Array] => {
        const f = tilematch(prg, map, room, 0, fbin, defs, pool, true);
        const b = tilematch(prg, map, room, 2, fbin, defs, pool, true);
        const px = b.cells.slice();
        for (let i = 0; i < f.cells.length; i += 4) {
            if (f.cells[i + 3]) {
                px.set(f.cells.subarray(i, i + 4), i);
            }
        }
        const fpx = b.full.slice();
        for (let i = 0; i < f.full.length; i += 4) {
            if (f.full[i + 3]) {
                fpx.set(f.full.subarray(i, i + 4), i);
            }
        }
        return [px, f.width, fpx];
    };
    const [sat0] = stacked(0);
    let [sat5, fw5, fpx5] = stacked(5);
    const cells = head.filter((n) => bkey(sat0.subarray(n * 1024, (n + 1) * 1024)) !==
        bkey(sat5.subarray(n * 1024, (n + 1) * 1024)));
    sat5 = narrowRegion(sat5, fw5, fpx5, cells);
    const td = copyDefs(defs);
    const warpFg = srcFg.tilemap.slice();
    const usedSet = new Set<number>();
    for (const t of psx) {
        for (const v of t) {
            usedSet.add(v);
        }
    }
    const free: number[] = [];
    for (let i = 1; i < td.cols.length; i++) {
        if (!usedSet.has(i)) {
            free.push(i);
        }
    }
    const cellSet = new Set(cells);
    const err: number[] = [];
    for (let n = 0; n < 256; n++) {
        err.push(cellSet.has(n) ? 2 ** 30 : 0);
    }
    portCells(fbin, [...st.tiledefs.values()], srcFg.tilemap, td, free, warpFg, warpFg.map((t) => td.cols[t]),
              err, sat5, [], null);

    const warpDef = roomdefs.length;
    const loadDef = roomdefs.length + 1;
    const outRooms = rooms.map((r) => r.slice());
    const rs = rooms[src];
    const rl = rooms[load];
    outRooms.push([x, y, x, y, warpDef, rs[5], rs[6], WRP_ROOM6_LAYOUT]);
    outRooms.push([x + dx, y, x + dx, y, loadDef, rl[5], rl[6], rl[7]]);
    outRooms.push([x + 2 * dx, y, x + 2 * dx, y, TP_WRP_TO_GDN, 0xff, 0, 0]);
    check(!rooms.some((r) => [x, x + dx, x + 2 * dx].some((cx) => r[0] <= cx && cx <= r[2] && r[1] <= y && y <= r[3])),
          "warp room cells taken");
    check(outRooms[WRP_ROOM6_INDEX][0] === x && outRooms[WRP_ROOM6_INDEX][1] === y, "warp room index");

    const arrays = new Map<string, CArrayOut>();
    arrays.set("g_ModWarp6Fg", u16Arr(warpFg.slice(0, 256)));
    tdArrays(arrays, "g_ModWarp6Td", td);
    arrays.set("g_ModRooms", roomsArray(outRooms));
    res.headers.set(`src/st/${stage}/mod_rooms.h`, arrays);
    res.binaries.set(`build/us/F_${up}.BIN`, fbin);
}

// gen_mod_rooms.py: sat reads the Saturn disc ("STAGE_01.PRG"), psx the PS1
// disc ("ST/NP3/F_NP3.BIN")
export async function buildModRooms(sat: Reader, psx: Reader): Promise<GenResult> {
    const res: GenResult = { headers: new Map(), binaries: new Map() };
    const ctx: Ctx = { sat, psx };
    await np3(ctx, res);
    await np3(ctx, res, "no3");
    await wrp(ctx, res, "wrp");
    await rno3(ctx, res);
    await wrp(ctx, res, "rwrp");
    return res;
}
