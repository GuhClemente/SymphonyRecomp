// SPDX-License-Identifier: AGPL-3.0-or-later
// PS1 art for the Saturn stages' stairs and stonework: a byte-exact port of
// mods/sotn-ultimate/tools/gdn_tile_swap.py apply() (include/game.h MOD_GDN_PS1_TILES /
// MOD_PRS_PS1_TILES; the caller checks the switch), plus rebuildAtlas(),
// which makes the hand-approved atlases (gdn_ps1_tiles/atlas*.png,
// prs_ps1_tiles/atlas*.png: game pixels, not shipped) again from the
// player's discs with gdn_tile_swap_recipe.ts.
//
// apply() runs on the Saturn map converter's output (`map convert-psx`, the
// WORK folder: file name -> bytes) between the converter and the stage's
// repack, and rewrites in it what the Python rewrites: graphics.bin (new
// graphics appended), palettes.bin (new palettes appended / filled),
// tiledefs.{gfx_page,gfx_index,clut,collision}.bin (new tiles appended) and
// the changed rooms' <fg data>.tilelayout.bin.
//
// Stages (gdn_assets.py / rgdn_assets.py / prs_assets.py / rprs_assets.py):
//   gdn   apply(WORK, False):                      set gdn
//   rgdn  apply(WORK, True):                       set gdn turned around, then rgdn
//   prs   apply(WORK, data=PRS, own="", empty=True):    set prs
//   rprs  apply(WORK, data=PRS, own="_rprs", empty=True): set rprs
//
// Python details kept: colours are (r, g, b) tuples, here r << 16 | g << 8
// | b (the same order when sorted); dicts / sets keep the Python iteration
// where it matters (pixel order for the counts, palette order for next() /
// min() / list.index(), sorted() for reduce_colours); all arithmetic is on
// integers (no float rounding involved).
import { TILE_SWAP_SETS, type RecipeTile, type TileSwapSet } from "./gdn_tile_swap_recipe.ts";

export type Bytes = Uint8Array;
// converter output: file name ("graphics.bin", "room1_fg.tilelayout.bin", ...) -> bytes
export type WorkFiles = Map<string, Bytes>;
// one atlas tile: 16x16 RGBA, row by row
export type Art = Uint8Array;

export interface SwapStats {
    cells: number;
    tiles: number;
    graphics: number;
    palettes_new: number;
    palettes_reused: number;
    nearest: number;
    max_err: number;
}

export interface SwapResult {
    stats: SwapStats;
    // the files apply() rewrote (all of them written back into the map)
    changed: string[];
}

// the four converter areas: which recipe sets their apply() uses, and how
export interface StagePlan {
    jobs: [string, boolean][]; // [recipe set, turned around]
    flip: boolean;
    empty: boolean;
}

export const STAGE_PLANS: Record<string, StagePlan> = {
    gdn: { jobs: [["gdn", false]], flip: false, empty: false },
    rgdn: { jobs: [["gdn", true], ["rgdn", false]], flip: true, empty: false },
    prs: { jobs: [["prs", false]], flip: false, empty: true },
    rprs: { jobs: [["rprs", false]], flip: false, empty: true },
};

const dec = new TextDecoder("latin1");

function get(work: WorkFiles, name: string): Bytes {
    const d = work.get(name);
    if (!d) {
        throw new Error("tile swap: no " + name + " in the converter output");
    }
    return d;
}

function u16s(d: Bytes): number[] {
    const out: number[] = [];
    for (let i = 0; i + 1 < d.length; i += 2) {
        out.push(d[i] | (d[i + 1] << 8));
    }
    return out;
}

function packU16(v: number[]): Bytes {
    const out = new Uint8Array(v.length * 2);
    for (let i = 0; i < v.length; i++) {
        if (v[i] < 0 || v[i] > 0xFFFF) {
            throw new Error("tile swap: u16 out of range");
        }
        out[2 * i] = v[i] & 0xFF;
        out[2 * i + 1] = v[i] >> 8;
    }
    return out;
}

// rgb(): the converter previews' colour (5-bit * 255 // 31) as r << 16 | g << 8 | b
export function rgb(c: number): number {
    const f = (v: number): number => Math.floor((v & 31) * 255 / 31);
    return (f(c) << 16) | (f(c >> 5) << 8) | f(c >> 10);
}

const NONE = -1; // prgb() of raw 0 (Python None): matches no colour

function prgb(v: number): number {
    return v ? rgb(v) : NONE;
}

const FIVE = new Map<number, number>();
for (let c = 0; c < 32; c++) {
    FIVE.set(Math.floor(c * 255 / 31), c);
}

function to555(col: number): number {
    const ch = (v: number): number => {
        const c = FIVE.get(v);
        if (c === undefined) {
            throw new Error("tile swap: colour off the 5-bit grid");
        }
        return c;
    };
    return ch(col >> 16) | (ch((col >> 8) & 0xFF) << 5) | (ch(col & 0xFF) << 10);
}

function dist(a: number, b: number): number {
    const dr = (a >> 16) - (b >> 16);
    const dg = ((a >> 8) & 0xFF) - ((b >> 8) & 0xFF);
    const db = (a & 0xFF) - (b & 0xFF);
    return dr * dr + dg * dg + db * db;
}

function maxChannelDiff(a: number, b: number): number {
    return Math.max(Math.abs((a >> 16) - (b >> 16)), Math.abs(((a >> 8) & 0xFF) - ((b >> 8) & 0xFF)),
                    Math.abs((a & 0xFF) - (b & 0xFF)));
}

// reduce_colours(): merge the closest colours (the rarer into the commoner)
// until at most `limit` remain; colour -> kept colour
function reduceColours(cols: number[], counts: Map<number, number>, limit = 15): Map<number, number> {
    const keep = new Map<number, number>();
    for (const c of cols) {
        keep.set(c, c);
    }
    const live = [...cols].sort((a, b) => a - b);
    while (live.length > limit) {
        let best: [number, number, number] | null = null;
        for (let i = 0; i < live.length; i++) {
            for (let j = i + 1; j < live.length; j++) {
                const d = dist(live[i], live[j]);
                if (best === null || d < best[0]) {
                    best = [d, live[i], live[j]];
                }
            }
        }
        const [, a, b] = best!;
        const [lose, win] = counts.get(a)! < counts.get(b)! ? [a, b] : [b, a];
        live.splice(live.indexOf(lose), 1);
        for (const [k, v] of keep) {
            if (v === lose) {
                keep.set(k, win);
            }
        }
    }
    return keep;
}

function slotPlace(s: number): [number, number] {
    let page = 0;
    while (s >= (page < 4 ? 240 : 256)) {
        s -= page < 4 ? 240 : 256;
        page++;
    }
    return [page, (Math.floor(s / 16) << 4) | (s % 16)];
}

function slotOf(page: number, index: number): number {
    let s = 0;
    for (let p = 0; p < page; p++) {
        s += p < 4 ? 240 : 256;
    }
    return s + (index >> 4) * 16 + (index & 15);
}

interface FgLayer {
    data: string;
    left: number;
    right: number;
    top: number;
    bottom: number;
}

function turnAround(a: Art): Art {
    const out = new Uint8Array(1024);
    for (let p = 0; p < 256; p++) {
        out.set(a.subarray((255 - p) * 4, (255 - p) * 4 + 4), p * 4);
    }
    return out;
}

const TD_KEYS = ["gfx_page", "gfx_index", "clut", "collision"] as const;

// apply(): jobs are [cell [room, y, x], art, turned around]; flip / empty /
// freePalettes as the Python's flip / empty / free_palettes
export function applyJobs(work: WorkFiles, jobs: [[number, number, number], Art, boolean][], flip: boolean,
                          empty: boolean, freePalettes: number | null = null): SwapResult {
    const graphics: number[] = Array.from(get(work, "graphics.bin"));
    const palRaw = get(work, "palettes.bin");
    const palettes: number[][] = [];
    for (let i = 0; i < palRaw.length; i += 32) {
        palettes.push(u16s(palRaw.subarray(i, i + 32)));
    }
    const nOldPals = palettes.length;
    const td: Record<string, number[]> = {};
    for (const k of TD_KEYS) {
        td[k] = Array.from(get(work, `tiledefs.${k}.bin`));
    }
    const layers = JSON.parse(dec.decode(get(work, "rooms.layers.json"))) as { fg: FgLayer }[];
    // raw values (STP bit included) the stage already uses per colour
    const rawOf = new Map<number, number>();
    for (const p of palettes) {
        for (const c of p.slice(1)) {
            if (c && !rawOf.has(rgb(c))) {
                rawOf.set(rgb(c), c);
            }
        }
    }
    const raw = (col: number): number => {
        const r = rawOf.get(col);
        if (r !== undefined) {
            return r;
        }
        const c = to555(col);
        return c ? c : 0x8000; // opaque black needs the STP bit
    };

    const maps = new Map<string, number[]>();
    const newGraphic = new Map<string, number>();
    const newTile = new Map<string, number>();
    const stats: SwapStats = { cells: 0, tiles: 0, graphics: 0, palettes_new: 0, palettes_reused: 0, nearest: 0,
                               max_err: 0 };
    for (const [cell, art0, turn] of jobs) {
        const [room] = cell;
        let [, y, x] = cell;
        let a = art0;
        const fg = layers[room].fg;
        const w = (fg.right - fg.left + 1) * 16;
        const h = (fg.bottom - fg.top + 1) * 16;
        if (turn) {
            y = h - 1 - y;
            x = w - 1 - x;
            a = turnAround(a);
        }
        const path = fg.data + ".tilelayout.bin";
        if (!maps.has(path)) {
            maps.set(path, u16s(get(work, path)));
        }
        const tm = maps.get(path)!;
        const old = tm[y * w + x];
        if (!(old || empty)) {
            throw new Error(`tile swap: cell ${room} ${y} ${x} is empty`);
        }
        if (old >= td.collision.length) {
            throw new Error("tile swap: tile past the tileset");
        }
        const coll = td.collision[old];

        // opaque pixels, row by row, and their counts (first-seen order)
        const pos: number[] = [];
        const pix: number[] = [];
        const counts = new Map<number, number>();
        for (let p = 0; p < 256; p++) {
            if (a[p * 4 + 3] > 0) {
                const c = (a[p * 4] << 16) | (a[p * 4 + 1] << 8) | a[p * 4 + 2];
                pos.push(p);
                pix.push(c);
                counts.set(c, (counts.get(c) ?? 0) + 1);
            }
        }
        const cols = [...counts.keys()];
        // an existing palette with every colour
        let k: number | null = null;
        for (let i = 0; i < palettes.length; i++) {
            const have = new Set(palettes[i].slice(1).map(prgb));
            if (cols.every((c) => have.has(c))) {
                k = i;
                break;
            }
        }
        let remap = new Map<number, number>(cols.map((c) => [c, c]));
        if (k === null && !flip) {
            remap = reduceColours(cols, counts);
            const want = new Set(remap.values());
            for (let i = nOldPals; i < palettes.length; i++) {
                const u = new Set(want);
                for (const v of palettes[i].slice(1)) {
                    if (v) {
                        u.add(rgb(v));
                    }
                }
                if (u.size <= 15) {
                    k = i;
                    break;
                }
            }
            if (k === null) {
                if (!(freePalettes === null || palettes.length - nOldPals < freePalettes)) {
                    throw new Error("no palette slot left for the PS1 tiles");
                }
                palettes.push(new Array<number>(16).fill(0));
                k = palettes.length - 1;
            }
            const have = palettes[k].slice(1).filter((v) => v);
            for (const col of [...want].sort((p, q) => p - q)) {
                if (!have.includes(raw(col))) {
                    have.push(raw(col));
                }
            }
            if (have.length > 15) {
                throw new Error("tile swap: palette overflow"); // (the Python's struct.pack fails too)
            }
            palettes[k] = [0, ...have, ...new Array<number>(15 - have.length).fill(0)];
            let e = 0;
            for (const col of cols) {
                e = Math.max(e, maxChannelDiff(col, remap.get(col)!));
            }
            stats.max_err = Math.max(stats.max_err, e);
        } else if (k === null) {
            // no free palette slot: the existing palette nearest to the art
            const err = (p: number[]): number => {
                const pc = p.slice(1).filter((v) => v).map(prgb);
                if (!pc.length) {
                    return Infinity;
                }
                let s = 0;
                for (const col of cols) {
                    let m = Infinity;
                    for (const q of pc) {
                        m = Math.min(m, dist(col, q));
                    }
                    s += counts.get(col)! * m;
                }
                return s;
            };
            let bk = 0;
            let be = err(palettes[0]);
            for (let i = 1; i < palettes.length; i++) {
                const e = err(palettes[i]);
                if (e < be) {
                    bk = i;
                    be = e;
                }
            }
            k = bk;
            const pc = palettes[k].slice(1).filter((v) => v).map(prgb);
            for (const col of cols) {
                let best = pc[0];
                let bd = dist(col, best);
                for (const q of pc.slice(1)) {
                    const d = dist(col, q);
                    if (d < bd) {
                        best = q;
                        bd = d;
                    }
                }
                remap.set(col, best);
            }
            stats.nearest++;
            let e = 0;
            for (const col of cols) {
                e = Math.max(e, maxChannelDiff(col, remap.get(col)!));
            }
            stats.max_err = Math.max(stats.max_err, e);
        } else {
            stats.palettes_reused++;
        }
        const palCols = [NONE, ...palettes[k].slice(1).map(prgb)];
        const idx = new Uint8Array(256);
        for (let i = 0; i < pos.length; i++) {
            const j = palCols.indexOf(remap.get(pix[i])!, 1);
            if (j < 0) {
                throw new Error("tile swap: colour not in the palette");
            }
            idx[pos[i]] = j;
        }
        const ikey = idx.join(",");
        if (!newGraphic.has(ikey)) {
            newGraphic.set(ikey, Math.floor(graphics.length / 256));
            for (const v of idx) {
                graphics.push(v);
            }
            stats.graphics++;
        }
        const s = newGraphic.get(ikey)!;
        const tkey = `${ikey}|${k}|${coll}`;
        if (!newTile.has(tkey)) {
            if (td.clut.length >= 0x1000) {
                throw new Error("tileset full");
            }
            const [page, index] = slotPlace(s);
            for (const [kk, v] of [["gfx_page", page], ["gfx_index", index], ["clut", k], ["collision", coll]] as const) {
                if (v > 255) {
                    throw new Error("tile swap: byte out of range"); // (bytearray.append)
                }
                td[kk].push(v);
            }
            newTile.set(tkey, td.clut.length - 1);
            stats.tiles++;
        }
        tm[y * w + x] = newTile.get(tkey)!;
        stats.cells++;
    }

    stats.palettes_new = palettes.length - nOldPals;
    const changed: string[] = [];
    const put = (name: string, d: Bytes): void => {
        work.set(name, d);
        changed.push(name);
    };
    for (const [path, tm] of maps) {
        put(path, packU16(tm));
    }
    put("graphics.bin", Uint8Array.from(graphics));
    put("palettes.bin", packU16(palettes.flat()));
    for (const k of TD_KEYS) {
        put(`tiledefs.${k}.bin`, Uint8Array.from(td[k]));
    }
    return { stats, changed };
}

// the stage's apply() with its atlases (rebuildAtlas() output by recipe set)
export function applyTileSwap(stage: string, work: WorkFiles, atlases: Record<string, Art[]>): SwapResult {
    const plan = STAGE_PLANS[stage];
    if (!plan) {
        throw new Error("tile swap: no plan for " + stage);
    }
    const jobs: [[number, number, number], Art, boolean][] = [];
    for (const [set, turn] of plan.jobs) {
        const art = atlases[set];
        if (!art) {
            throw new Error("tile swap: no atlas " + set);
        }
        for (const [room, y, x, tile] of TILE_SWAP_SETS[set].cells) {
            jobs.push([[room, y, x], art[tile], turn]);
        }
    }
    return applyJobs(work, jobs, plan.flip, plan.empty);
}

// ---- the atlases, rebuilt from the discs -----------------------------------

// what rebuildAtlas() needs for a set: converter areas and PS1 stages
// (their ST/<STAGE>/F_<STAGE>.BIN)
export function atlasInputs(set: string): { areas: string[]; stages: string[] } {
    const s = TILE_SWAP_SETS[set];
    const areas = new Set<string>([s.area]);
    const stages = new Set<string>();
    for (const t of s.tiles) {
        for (const src of t.src) {
            if (src[0] === "p") {
                stages.add(src[1]);
            } else {
                areas.add(src[1]);
            }
        }
    }
    return { areas: [...areas].sort(), stages: [...stages].sort() };
}

// a converter tile: its 16x16 palette indices and its palette (raw values)
function satTile(work: WorkFiles, t: number): [Uint8Array, number[]] {
    const page = get(work, "tiledefs.gfx_page.bin");
    const index = get(work, "tiledefs.gfx_index.bin");
    const clut = get(work, "tiledefs.clut.bin");
    if (t >= clut.length) {
        throw new Error("tile swap recipe: no converter tile " + t);
    }
    const s = slotOf(page[t], index[t]);
    const g = get(work, "graphics.bin");
    if ((s + 1) * 256 > g.length) {
        throw new Error("tile swap recipe: graphic past graphics.bin");
    }
    return [g.slice(s * 256, (s + 1) * 256), workPalette(work, clut[t])];
}

function workPalette(work: WorkFiles, k: number): number[] {
    const d = get(work, "palettes.bin");
    if ((k + 1) * 32 > d.length) {
        throw new Error("tile swap recipe: no palette " + k);
    }
    return u16s(d.subarray(k * 32, (k + 1) * 32));
}

// F_<STAGE>.BIN: 32 blocks of 32 x 128 halfwords filling VRAM 512 x 256 (as
// mods/sotn-ultimate/tools/saturn_tilematch.py / the stage loader); CLUTs from row 0xF0
function vramHw(f: Bytes, row: number, col: number): number {
    const i = ((col >> 6) << 2) | (row >= 128 ? 2 : 0) | ((col >> 5) & 1);
    const o = i * 0x2000 + (((row & 127) * 32 + (col & 31)) * 2);
    return f[o] | (f[o + 1] << 8);
}

function ps1Tile(f: Bytes, page: number, ix: number, k: number): [Uint8Array, number[]] {
    if (f.length < 0x40000) {
        throw new Error("tile swap recipe: F_ file too short");
    }
    const u = (ix & 15) * 16;
    const v = ix & 0xF0;
    const idx = new Uint8Array(256);
    for (let y = 0; y < 16; y++) {
        for (let x = 0; x < 16; x++) {
            const hw = vramHw(f, v + y, page * 64 + ((u + x) >> 2));
            idx[y * 16 + x] = (hw >> (((u + x) % 4) * 4)) & 15;
        }
    }
    const pal: number[] = [];
    for (let e = 0; e < 16; e++) {
        pal.push(vramHw(f, 0xF0 + (k >> 4), 16 * (k % 16) + e));
    }
    return [idx, pal];
}

// flip (bit 0 x, bit 1 y), then move dy pixels down; -1: nothing there
function place(idx: Uint8Array, flip: number, dy: number): Int16Array {
    const out = new Int16Array(256).fill(-1);
    for (let y = 0; y < 16; y++) {
        const sy = y - dy;
        if (sy < 0 || sy > 15) {
            continue;
        }
        const fy = flip & 2 ? 15 - sy : sy;
        for (let x = 0; x < 16; x++) {
            const fx = flip & 1 ? 15 - x : x;
            out[y * 16 + x] = idx[fy * 16 + fx];
        }
    }
    return out;
}

function unrle(map: string): number[] {
    const out: number[] = [];
    let i = 0;
    while (i < map.length) {
        const ch = map[i++];
        let n = "";
        while (i < map.length && map[i] >= "0" && map[i] <= "9") {
            n += map[i++];
        }
        const v = ch === "." ? -1 : ch.charCodeAt(0) - 97;
        for (let r = n ? parseInt(n, 10) : 1; r > 0; r--) {
            out.push(v);
        }
    }
    if (out.length !== 256) {
        throw new Error("tile swap recipe: map of " + out.length + " pixels");
    }
    return out;
}

function rebuildTile(t: RecipeTile, area: string, works: Record<string, WorkFiles>,
                     fbins: Record<string, Bytes>): Art {
    const work = (a: string): WorkFiles => {
        const w = works[a];
        if (!w) {
            throw new Error("tile swap recipe: no converter output for " + a);
        }
        return w;
    };
    // per source: pixel -> colour (r << 16 | g << 8 | b) or -1 (clear)
    const layers: Int32Array[] = t.src.map((src) => {
        const out = new Int32Array(256).fill(-1);
        if (src[0] === "c") {
            const v = workPalette(work(src[1]), src[2])[src[3]];
            if (!v) {
                throw new Error("tile swap recipe: empty palette entry");
            }
            return out.fill(rgb(v));
        }
        let idx: Uint8Array;
        let pal: number[];
        let flip: number;
        let dy: number;
        let lut: number[] | undefined;
        if (src[0] === "s") {
            [idx, pal] = satTile(work(src[1]), src[2]);
            [, , , flip, dy, lut] = src;
        } else {
            const f = fbins[src[1]];
            if (!f) {
                throw new Error("tile swap recipe: no F_" + src[1] + ".BIN");
            }
            [idx, pal] = ps1Tile(f, src[2], src[3], src[4]);
            [, , , , , flip, dy, lut] = src;
        }
        const pi = place(idx, flip, dy);
        const map = new Map<number, number>();
        if (lut) {
            for (let i = 0; i < lut.length; i += 3) {
                const v = workPalette(work(area), lut[i + 1])[lut[i + 2]];
                map.set(lut[i], rgb(v));
            }
        }
        for (let p = 0; p < 256; p++) {
            const i = pi[p];
            if (i <= 0) {
                continue; // index 0: transparent
            }
            if (lut) {
                const c = map.get(i);
                if (c !== undefined) {
                    out[p] = c;
                }
            } else {
                out[p] = rgb(pal[i]);
            }
        }
        return out;
    });
    const art = new Uint8Array(1024);
    unrle(t.map).forEach((s, p) => {
        if (s < 0) {
            return;
        }
        const c = layers[s]?.[p] ?? -1;
        if (c < 0) {
            throw new Error("tile swap recipe: pixel from a clear source pixel");
        }
        art.set([c >> 16, (c >> 8) & 0xFF, c & 0xFF, 255], p * 4);
    });
    return art;
}

// a recipe set's atlas tiles (16x16 RGBA each, as the atlas PNG's columns),
// from the converter output of the set's areas (works: area -> files, before
// apply(); apply() only appends, so after it works too) and the PS1 disc's
// ST/<STAGE>/F_<STAGE>.BIN (fbins: STAGE -> bytes); see atlasInputs()
export function rebuildAtlas(set: string, works: Record<string, WorkFiles>, fbins: Record<string, Bytes>): Art[] {
    const s: TileSwapSet | undefined = TILE_SWAP_SETS[set];
    if (!s) {
        throw new Error("tile swap recipe: no set " + set);
    }
    return s.tiles.map((t) => rebuildTile(t, s.area, works, fbins));
}
