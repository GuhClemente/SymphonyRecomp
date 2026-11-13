// SPDX-License-Identifier: AGPL-3.0-or-later
// Rebuild a Saturn room layer out of an existing PSX stage's tiles: a
// byte-exact port of mods/sotn-ultimate/tools/saturn_tilematch.py (see its header), with the
// parts of tools/saturn/tile_dumper.py it uses (stage_streams, render_layer).
// Each 16x16 block of the Saturn layer (narrowed from 320 to 256 px: 4 of
// every 5 columns) gets the closest PSX tile the stage already uses; for the
// foreground (layer 0) only tiles whose collision equals the Saturn's for the
// cell, unless artOnly.
//
// The Python script's files are the result's fields: OUT = tiles, OUT.err =
// errs, OUT.full = width, height, full, OUT.px = cells, OUT.coll = coll.
import { decompressLzss } from "./saturn_lzss.ts";

export const STAGE_LOAD_ADDRESS = 0x060dc000;

const be16 = (d: Uint8Array, o: number): number => (d[o] << 8) | d[o + 1];
const be32 = (d: Uint8Array, o: number): number => ((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) >>> 0;

// PRG export field of a descriptor set (STAGE_07 / STAGE_09 have a second
// set 0x14 bytes later)
export function exportOffset(descriptorSet: number, field: number): number {
    if (descriptorSet !== 0 && descriptorSet !== 1) {
        throw new Error("descriptor set must be 0 or 1");
    }
    return field + descriptorSet * 0x14;
}

// the decompressed character, palette and map streams of one room / layer
export function stageStreams(prg: Uint8Array, mapData: Uint8Array, room: number, layer: number,
                             descriptorSet: number = 0): [Uint8Array, Uint8Array, Uint8Array] {
    if (room < 0 || layer < 0 || layer > 2) {
        throw new Error("room must be nonnegative and layer must be 0, 1, or 2");
    }
    const counts = be32(prg, exportOffset(descriptorSet, 0x14)) - STAGE_LOAD_ADDRESS;
    const offsets = be32(prg, exportOffset(descriptorSet, 0x18)) - STAGE_LOAD_ADDRESS;
    const descriptor = room * 36 + layer * 12;
    if (counts + descriptor + 12 > prg.length || offsets + descriptor + 12 > prg.length) {
        throw new Error("room descriptor lies outside the stage PRG");
    }
    const streams: Uint8Array[] = [];
    for (let i = 0; i < 3; i++) {
        const offset = be32(prg, offsets + descriptor + i * 4);
        const size = be32(prg, counts + descriptor + i * 4);
        if (!size) {
            throw new Error("room " + room + " layer " + layer + " is empty");
        }
        if (offset + size > mapData.length) {
            throw new Error("stream " + i + " exceeds MAP size");
        }
        streams.push(decompressLzss(mapData.subarray(offset, offset + size)));
    }
    return [streams[0], streams[1], streams[2]];
}

// 16-colour RGB555 banks -> [r, g, b] (5 -> 8 bits as c * 255 // 31)
function decodePalette(data: Uint8Array): [number, number, number][] {
    if (data.length < 32 || data.length % 32) {
        throw new Error("palette stream size " + data.length + " is not a whole number of 16-color banks");
    }
    const out: [number, number, number][] = [];
    for (let o = 0; o < data.length; o += 2) {
        const v = be16(data, o);
        out.push([Math.floor(((v & 0x1f) * 255) / 31), Math.floor((((v >> 5) & 0x1f) * 255) / 31),
                  Math.floor((((v >> 10) & 0x1f) * 255) / 31)]);
    }
    return out;
}

// tile_dumper.render_layer(characters, palette, map, transparent_zero=True):
// (width, height, RGBA pixels; alpha 0 for colour index 0)
export function renderLayer(characters: Uint8Array, paletteData: Uint8Array,
                            mapData: Uint8Array): [number, number, Uint8Array] {
    if (mapData.length < 4) {
        throw new Error("map stream has no width/height header");
    }
    let width = be16(mapData, 0);
    const height = be16(mapData, 2);
    let expected = 4 + width * height * 2;
    // STAGE_01 room 18 layer 0: a 720-tile header over 1280 columns
    if (mapData.length > expected && height && (mapData.length - 4) % (height * 2) === 0) {
        const payload = (mapData.length - 4) / (height * 2);
        if (payload === 0x500 && width === 720) {
            width = payload;
            expected = mapData.length;
        }
    }
    // STAGE_03 room 10's small truncations: zero-filled up to 32 bytes
    if (mapData.length < expected) {
        if (expected - mapData.length > 32) {
            throw new Error("map is " + width + "x" + height + ", needs " + expected + " bytes");
        }
        const padded = new Uint8Array(expected);
        padded.set(mapData);
        mapData = padded;
    }
    if (characters.length % 32) {
        throw new Error("4bpp character stream size is not divisible by 32");
    }
    const palette = decodePalette(paletteData);
    const tileCount = characters.length >> 5;
    const pixels = new Uint8Array(width * 8 * height * 8 * 4);
    const stride = width * 8 * 4;
    for (let my = 0; my < height; my++) {
        for (let mx = 0; mx < width; mx++) {
            const entry = be16(mapData, 4 + (my * width + mx) * 2);
            const tile = entry & 0x3ff;
            const bank = entry >> 12;
            if (tile >= tileCount) {
                throw new Error("tile " + tile + " at (" + mx + ", " + my + ") exceeds " + tileCount + " characters");
            }
            const flipX = (entry & 0x400) !== 0;
            const flipY = (entry & 0x800) !== 0;
            for (let y = 0; y < 8; y++) {
                const sy = flipY ? 7 - y : y;
                for (let x = 0; x < 8; x++) {
                    const sx = flipX ? 7 - x : x;
                    const packed = characters[tile * 32 + sy * 4 + (sx >> 1)];
                    const ci = sx % 2 === 0 ? packed >> 4 : packed & 0xf;
                    const pi = bank * 16 + ci;
                    if (pi >= palette.length) {
                        throw new Error("palette bank " + bank + " at (" + mx + ", " + my + ") exceeds " +
                                        (palette.length >> 4) + " decoded banks");
                    }
                    const c = palette[pi];
                    const o = (my * 8 + y) * stride + (mx * 8 + x) * 4;
                    pixels[o] = c[0];
                    pixels[o + 1] = c[1];
                    pixels[o + 2] = c[2];
                    pixels[o + 3] = ci === 0 ? 0 : 255;
                }
            }
        }
    }
    return [width * 8, height * 8, pixels];
}

// CPython's set of small ints, for its iteration order: the script builds
// the candidate tiles from a set (set.update over the tilemaps' u16s), and
// which of several equally close tiles wins depends on that order
// (Objects/setobject.c: linear probes, perturbation, resize at 3/5 full).
const EMPTY = -1;
const DUMMY = -2;

export class PyIntSet {
    table: Int32Array;
    mask: number;
    fill: number;
    used: number;

    constructor() {
        this.table = new Int32Array(8).fill(EMPTY);
        this.mask = 7;
        this.fill = 0;
        this.used = 0;
    }

    // keys are ints >= 0 (their hash is themselves)
    add(key: number): void {
        const mask = this.mask;
        let i = key & mask;
        let perturb = key;
        let freeslot = -1;
        for (;;) {
            let j = i;
            let probes = i + 9 <= mask ? 9 : 0;
            do {
                const e = this.table[j];
                if (e === EMPTY) {
                    if (freeslot >= 0) {
                        this.used++;
                        this.table[freeslot] = key;
                        return;
                    }
                    this.fill++;
                    this.used++;
                    this.table[j] = key;
                    if (this.fill * 5 >= mask * 3) {
                        this.resize(this.used > 50000 ? this.used * 2 : this.used * 4);
                    }
                    return;
                }
                if (e === key) {
                    return;
                }
                if (e === DUMMY && freeslot < 0) {
                    freeslot = j;
                }
                j++;
            } while (probes--);
            perturb = Math.floor(perturb / 32);
            i = (i * 5 + 1 + perturb) & mask;
        }
    }

    private resize(minused: number): void {
        let size = 8;
        while (size <= minused) {
            size *= 2;
        }
        const old = this.table;
        this.table = new Int32Array(size).fill(EMPTY);
        this.mask = size - 1;
        this.fill = this.used;
        for (const key of old) {
            if (key >= 0) {
                this.insertClean(key);
            }
        }
    }

    private insertClean(key: number): void {
        const mask = this.mask;
        let i = key & mask;
        let perturb = key;
        for (;;) {
            if (this.table[i] === EMPTY) {
                this.table[i] = key;
                return;
            }
            if (i + 9 <= mask) {
                for (let j = 1; j <= 9; j++) {
                    if (this.table[i + j] === EMPTY) {
                        this.table[i + j] = key;
                        return;
                    }
                }
            }
            perturb = Math.floor(perturb / 32);
            i = (i * 5 + 1 + perturb) & mask;
        }
    }

    discard(key: number): void {
        const j = this.table.indexOf(key);
        if (j >= 0) {
            this.table[j] = DUMMY;
            this.used--;
        }
    }

    // the keys in the order a Python for loop sees them
    values(): number[] {
        const out: number[] = [];
        for (const k of this.table) {
            if (k >= 0) {
                out.push(k);
            }
        }
        return out;
    }
}

// the PSX stage's tile definitions (<prefix>_tiles/_pages/_cluts/_cols.bin)
export interface TileDefs {
    tiles: Uint8Array; // the tile's VRAM page (the script's `pages`: names are reversed)
    pages: Uint8Array; // the tile's index in its page (the script's `gfx`)
    cluts: Uint8Array;
    cols: Uint8Array; // collision
}

export interface TileMatch {
    tiles: number[]; // per 16x16 cell, row-major (OUT, u16 LE)
    errs: number[]; // per cell, squared error // 256 (OUT.err, u32 LE)
    width: number; // the full-width Saturn layer (OUT.full: u16 width, u16 height, RGBA)
    height: number;
    full: Uint8Array;
    cells: Uint8Array; // the narrowed art per cell, 16x16 RGBA each (OUT.px)
    coll: Uint8Array | null; // layer 0: the Saturn's collision per cell (OUT.coll)
    tw: number;
    th: number;
}

// The data files of a layers.json's layers that use tiledef <prefix>.json,
// in order (the script's candidate tiles unless a pool is given).
export function layersTilemaps(layers: unknown[], prefix: string): string[] {
    const out: string[] = [];
    for (const L of layers) {
        if (L === null || typeof L !== "object" || Array.isArray(L)) {
            continue;
        }
        for (const part of Object.values(L as Record<string, unknown>)) {
            if (part !== null && typeof part === "object" && !Array.isArray(part) &&
                    (part as Record<string, unknown>).tiledef === prefix + ".json") {
                out.push((part as Record<string, string>).data);
            }
        }
    }
    return out;
}

// u16 LE tilemaps -> the script's `used` set, in its iteration order
export function usedTiles(tilemaps: Uint8Array[]): number[] {
    const s = new PyIntSet();
    for (const d of tilemaps) {
        for (let o = 0; o + 1 < d.length; o += 2) {
            s.add(d[o] | (d[o + 1] << 8));
        }
    }
    s.discard(0);
    return s.values();
}

// a tile's pixels: -1 transparent, else r << 16 | g << 8 | b
type Pix = Int32Array;
// per 4x4 block: summed r // 16, g // 16, b // 16, opaque count * 16
type Sig = Int32Array;

const rgb5 = (c: number): number => (Math.floor(((c & 31) * 255) / 31) << 16) |
    (Math.floor((((c >> 5) & 31) * 255) / 31) << 8) | Math.floor((((c >> 10) & 31) * 255) / 31);

function sig(pix: Pix): Sig {
    const s = new Int32Array(64);
    for (let by = 0; by < 4; by++) {
        for (let bx = 0; bx < 4; bx++) {
            let r = 0;
            let g = 0;
            let b = 0;
            let n = 0;
            for (let y = 0; y < 4; y++) {
                for (let x = 0; x < 4; x++) {
                    const p = pix[(by * 4 + y) * 16 + bx * 4 + x];
                    if (p >= 0) {
                        r += p >> 16;
                        g += (p >> 8) & 255;
                        b += p & 255;
                        n++;
                    }
                }
            }
            const o = (by * 4 + bx) * 4;
            s[o] = Math.floor(r / 16);
            s[o + 1] = Math.floor(g / 16);
            s[o + 2] = Math.floor(b / 16);
            s[o + 3] = n * 16;
        }
    }
    return s;
}

function dist(a: Pix, b: Pix): number {
    let d = 0;
    for (let i = 0; i < 256; i++) {
        const p = a[i];
        const q = b[i];
        if (p < 0 || q < 0) {
            if ((p < 0) !== (q < 0)) {
                d += 3 * 255 * 255;
            }
        } else {
            const dr = (p >> 16) - (q >> 16);
            const dg = ((p >> 8) & 255) - ((q >> 8) & 255);
            const db = (p & 255) - (q & 255);
            d += dr * dr + dg * dg + db * db;
        }
    }
    return d;
}

function sigdist(a: Sig, b: Sig): number {
    let d = 0;
    for (let i = 0; i < 64; i += 4) {
        const dr = a[i] - b[i];
        const dg = a[i + 1] - b[i + 1];
        const db = a[i + 2] - b[i + 2];
        const dn = a[i + 3] - b[i + 3];
        d += dr * dr + dg * dg + db * db + 4 * dn * dn;
    }
    return d;
}

const visible = (pix: Pix): number => pix.reduce((n, p) => n + (p >= 0 ? 1 : 0), 0);

// Python's min(seq, key) (the first of the smallest) / sorted(seq, key) (stable)
function minBy<T>(seq: Iterable<T>, key: (v: T) => number): T {
    let best: T | undefined;
    let bk = 0;
    for (const v of seq) {
        const k = key(v);
        if (best === undefined || k < bk) {
            best = v;
            bk = k;
        }
    }
    if (best === undefined) {
        throw new Error("min() arg is an empty sequence");
    }
    return best;
}

// saturn_tilematch.py SAT_PRG SAT_MAP ROOM LAYER F_STAGE.BIN ... [--art-only]
// [--set=N]: prg / map the Saturn stage's .PRG / .MAP, fBin the PSX stage's
// F_<STAGE>.BIN, defs its <prefix>_*.bin, used = usedTiles() of the --pool
// tilemaps, or else of layersTilemaps(layers.json, prefix).
export function tilematch(prg: Uint8Array, map: Uint8Array, room: number, layer: number, fBin: Uint8Array,
                          defs: TileDefs, used: number[], artOnly: boolean = false,
                          descriptorSet: number = 0): TileMatch {
    // --- Saturn layer, RGBA, narrowed
    const [chars, pal, tmap] = stageStreams(prg, map, room, layer, descriptorSet);
    const [w, h, px] = renderLayer(chars, pal, tmap);
    const W = Math.floor((w * 4) / 5);
    const satAt = (x: number, y: number): number => (y * w + Math.floor((x * 5) / 4)) * 4;

    // --- PSX tiles: VRAM from F_*.BIN (32 blocks of 32x128 halfwords)
    const vram = new Uint16Array(256 * 512);
    for (let i = 0; i < 32; i++) {
        const bx = (i >> 2) * 0x40 + (i & 1) * 0x20;
        const by = (i & 2) * 0x40;
        for (let y = 0; y < 128; y++) {
            for (let x = 0; x < 32; x++) {
                const o = i * 0x2000 + y * 64 + x * 2;
                vram[(by + y) * 512 + bx + x] = fBin[o] | (fBin[o + 1] << 8);
            }
        }
    }
    const { tiles: pageOf, pages: gfx, cluts, cols } = defs;
    let satcoll: Uint8Array | null = null;
    if (layer === 0) {
        const tab = be32(prg, exportOffset(descriptorSet, 0x20)) - STAGE_LOAD_ADDRESS;
        const off = be32(prg, tab + room * 8);
        const size = be32(prg, tab + room * 8 + 4);
        satcoll = decompressLzss(map.subarray(off, off + size));
    }

    const render = (t: number): Pix => {
        const page = pageOf[t];
        const ix = gfx[t];
        const k = cluts[t];
        const u = (ix & 15) * 16;
        const v = ix & 0xf0;
        const palRow = (0xf0 + (k >> 4)) * 512 + 16 * (k % 16);
        const pix = new Int32Array(256);
        for (let y = 0; y < 16; y++) {
            for (let x = 0; x < 16; x++) {
                const hw = vram[(v + y) * 512 + page * 64 + ((u + x) >> 2)];
                const i = (hw >> (((u + x) % 4) * 4)) & 15;
                pix[y * 16 + x] = i === 0 ? -1 : rgb5(vram[palRow + i]);
            }
        }
        return pix;
    };

    // the rendered tiles and their signatures, in the script's dict order
    const cat = new Map<number, Pix>();
    const catsig = new Map<number, Sig>();
    for (const t of used) {
        cat.set(t, render(t));
    }
    for (const [t, p] of cat) {
        catsig.set(t, sig(p));
    }
    const sortedUsed = [...used].sort((a, b) => a - b);
    const byColl = new Map<number, number[]>();
    for (const t of sortedUsed) {
        let l = byColl.get(cols[t]);
        if (l === undefined) {
            byColl.set(cols[t], (l = []));
        }
        l.push(t);
    }
    const allByColl = new Map<number, number[]>();
    for (let t = 1; t < cols.length; t++) {
        let l = allByColl.get(cols[t]);
        if (l === undefined) {
            allByColl.set(cols[t], (l = []));
        }
        l.push(t);
    }
    // tiles with collision c: the stage's own first, else the whole tileset
    const candidates = (c: number): number[] => {
        const own = byColl.get(c);
        const pool = own !== undefined && own.length ? own : allByColl.get(c) ?? [];
        for (const t of pool) {
            if (!cat.has(t)) {
                const p = render(t);
                cat.set(t, p);
                catsig.set(t, sig(p));
            }
        }
        return pool;
    };

    const tw = W >> 4;
    const th = h >> 4;
    if (satcoll !== null) {
        // (STAGE_03 room 6's stream has 3 bytes past its 96x48 cells)
        if (!(tw * th <= satcoll.length && satcoll.length < tw * th + 4)) {
            throw new Error("collision stream " + satcoll.length + " for " + tw + "x" + th + " cells");
        }
        satcoll = satcoll.slice(0, tw * th);
    }
    const tiles: number[] = [];
    const errs: number[] = [];
    const blank = minBy(cat.keys(), (t) => visible(cat.get(t)!) * 0x10000 + t);
    const blk = new Int32Array(256);
    for (let ty = 0; ty < th; ty++) {
        for (let tx = 0; tx < tw; tx++) {
            let empty = true;
            for (let y = 0; y < 16; y++) {
                for (let x = 0; x < 16; x++) {
                    const o = satAt(tx * 16 + x, ty * 16 + y);
                    blk[y * 16 + x] = px[o + 3] === 0 ? -1 : (px[o] << 16) | (px[o + 1] << 8) | px[o + 2];
                    if (px[o + 3] !== 0) {
                        empty = false;
                    }
                }
            }
            const c = satcoll !== null && !artOnly ? satcoll[ty * tw + tx] : null;
            let pool: number[] | null = null;
            if (c !== null) {
                pool = c ? candidates(c) : null;
            }
            if (empty) {
                if (artOnly && satcoll !== null && satcoll[ty * tw + tx]) {
                    // invisible but solid: the stage's least visible tile
                    tiles.push(blank);
                } else if (pool !== null && pool.length) {
                    // no art but solid: the least visible tile with that collision
                    tiles.push(minBy(pool, (t) => visible(cat.get(t)!)));
                } else {
                    tiles.push(0);
                }
                errs.push(0);
                continue;
            }
            if (c === 0) {
                pool = [...catsig.keys()].filter((t) => cols[t] === 0);
            }
            const s = sig(blk);
            const src = pool !== null && pool.length ? pool : [...catsig.keys()];
            const keyed = src.map((t) => [sigdist(s, catsig.get(t)!), t]);
            keyed.sort((a, b) => a[0] - b[0]);
            const cands = keyed.slice(0, 24).map((e) => e[1]);
            const best = minBy(cands, (t) => dist(blk, cat.get(t)!));
            errs.push(Math.floor(dist(blk, cat.get(best)!) / 256));
            tiles.push(best);
        }
    }
    const cells = new Uint8Array(tw * th * 1024);
    let n = 0;
    for (let ty = 0; ty < th; ty++) {
        for (let tx = 0; tx < tw; tx++) {
            for (let y = 0; y < 16; y++) {
                for (let x = 0; x < 16; x++) {
                    const o = satAt(tx * 16 + x, ty * 16 + y);
                    cells.set(px.subarray(o, o + 4), n);
                    n += 4;
                }
            }
        }
    }
    return { tiles, errs, width: w, height: h, full: px.slice(0, w * h * 4), cells, coll: satcoll, tw, th };
}
