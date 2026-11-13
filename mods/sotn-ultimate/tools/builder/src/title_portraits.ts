// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn's New Game character select art -> src/st/sel/mod_charsel_gfx.h:
// a byte-exact port of mods/sotn-ultimate/tools/gen_title_portraits.py (see its header for
// the formats). Reads the Saturn's TITLE.PRG, TITLE.CHR and TITLE.MAP only.
//
// buildTitlePortraits() returns the header's arrays in header order. The
// compiled SEL holds them as their numbers back to back; parseCArrays finds
// the 1-D ones (g_ModCharselPix_* / g_ModCharselClut_* / g_ModCharselMap_*),
// but not g_ModCharselTiles (its Maria-only tail sits inside an #if) nor the
// 2-D g_ModCharselClut4[3][16]: those two are listed with `guarded` (the
// count of trailing MOD_SATURN_MARIA-only elements) / flattened.
import { decompressLzss } from "./saturn_lzss.ts";

export type SaturnRead = (path: string) => Promise<Uint8Array>;

export interface GfxArray {
    name: string;
    size: number; // bytes per element
    values: number[];
    maria: boolean; // the whole array is under #if defined(MOD_SATURN_MARIA)
    guarded?: number; // trailing elements under #if defined(MOD_SATURN_MARIA)
}

export interface CharselGfx {
    arrays: GfxArray[];
    win: [number, number, number, number]; // MOD_CHARSEL_WIN_X/Y/W/H
    header: string; // the generator's whole header text
}

const BASE = 0x060a5000;
const BANKS = [0x60b1116, 0x60b1328, 0x60b1438, 0x60b1666, 0x60b1870, 0x60b1a7a];
const PORTRAITS: [string, number, number][] = [
    ["alucard", 0x60b1666, 0x60b1464],
    ["richter", 0x60b1870, 0x60b166e],
    ["maria", 0x60b1a7a, 0x60b1878],
];
const W = 80;
const H = 88;
const NCOL = 128;
const MAP_DELTA = 0x4000;
const LAYER_CHARS: [number, number] = [0x664cc, 0x4d46];
const LAYER_PAL: [number, number] = [0x6b212, 0x112];
const LAYER_MAP = 0x40fc0;
const MARIA = "MOD_SATURN_MARIA";
// name, col, row, w, h (cells), guard
const LAYER_ELEMS: [string, number, number, number, number, string | null][] = [
    ["header", 2, 4, 18, 4, null],
    ["frame", 15, 8, 11, 15, null],
    ["name_alucard", 15, 23, 11, 3, null],
    ["name_richter", 3, 23, 11, 3, null],
    ["name_maria", 27, 23, 11, 3, MARIA],
];

const be16 = (d: Uint8Array, o: number): number => (d[o] << 8) | d[o + 1];
const hex = (v: number, n: number): string => "0x" + (v >>> 0).toString(16).toUpperCase().padStart(n, "0");

function check(ok: boolean, what: string): void {
    if (!ok) {
        throw new Error("title portraits: " + what);
    }
}

function ps1Colour(c: number, index: number): number {
    if (index === 0) {
        return 0;
    }
    const v = c & 0x7fff;
    return v ? v : 0x8000;
}

// CHR byte offset of each bank's first image
function walk(prg: Uint8Array): Map<number, number> {
    let cur = 0;
    const out = new Map<number, number>();
    for (const addr of BANKS) {
        const o = addr - BASE;
        out.set(addr, cur);
        for (let k = 0; ; k++) {
            if (prg[o + 4 * k] === 0) {
                cur += be16(prg, o + 4 * k + 2) * 8;
                break;
            }
        }
    }
    return out;
}

interface Layer {
    pool: [number[], string | null][];
    maps: Map<string, number[]>;
    cluts: number[][];
    win: [number, number, number, number];
}

function layerArt(prg: Uint8Array, mp: Uint8Array): Layer {
    const stream = (off: number, size: number): Uint8Array =>
        decompressLzss(mp.subarray(off - MAP_DELTA, off - MAP_DELTA + size));
    const chars = stream(...LAYER_CHARS);
    const palBytes = stream(...LAYER_PAL);
    check(palBytes.length >= 512, "layer palette");
    const pal: number[] = [];
    for (let i = 0; i < 256; i++) {
        pal.push(be16(palBytes, i * 2));
    }
    const cells: number[] = [];
    for (let i = 0; i < 1280; i++) {
        cells.push(be16(mp, LAYER_MAP - MAP_DELTA + i * 2));
    }
    const tile = (v: number): number[] => {
        const t = v & 0x3ff;
        const fx = v & 0x400;
        const fy = v & 0x800;
        const out: number[] = [];
        for (let y = 0; y < 8; y++) {
            const sy = fy ? 7 - y : y;
            for (let x = 0; x < 8; x++) {
                const sx = fx ? 7 - x : x;
                const c = chars[t * 32 + sy * 4 + (sx >> 1)];
                check(c !== undefined, "tile out of range");
                out.push(sx % 2 === 0 ? c >> 4 : c & 15);
            }
        }
        return out;
    };
    const pool: [number[], string | null][] = [];
    const index = new Map<string, number>();
    const maps = new Map<string, number[]>();
    const banks = new Map<string, Set<number>>();
    for (const [name, col, row, w, h, guard] of LAYER_ELEMS) {
        const m: number[] = [];
        for (let y = row; y < row + h; y++) {
            for (let x = col; x < col + w; x++) {
                const v = cells[y * 40 + x];
                const t = tile(v);
                if (!t.some((p) => p)) {
                    m.push(0);
                    continue;
                }
                if (!banks.has(name)) {
                    banks.set(name, new Set());
                }
                banks.get(name)!.add(v >> 12);
                const key = t.join(",");
                if (!index.has(key)) {
                    index.set(key, pool.length);
                    pool.push([t, guard]);
                }
                m.push(index.get(key)! + 1);
            }
        }
        maps.set(name, m);
    }
    const hb = banks.get("header");
    check(hb !== undefined && hb.size === 1 && hb.has(6), "header bank");
    for (const [k, b] of banks) {
        if (k !== "header") {
            check([...b].every((v) => [2, 7, 13, 14].includes(v)), "frame banks");
        }
    }
    const first = pool.findIndex(([, g]) => g === MARIA);
    check(first >= 0, "no Maria tiles");
    check(pool.slice(first).every(([, g]) => g), "shared tile after Maria's");
    check(pool.length < 256, "too many tiles");
    const cluts: number[][] = [];
    for (const b of [6, 13]) {
        const c: number[] = [];
        for (let i = 0; i < 16; i++) {
            c.push(ps1Colour(pal[b * 16 + i], i));
        }
        cluts.push(c);
    }
    const ro = 0x60acb10 + 6 * 32 - BASE;
    const ramp: number[] = [];
    for (let i = 0; i < 16; i++) {
        ramp.push(ps1Colour(be16(prg, ro + i * 2), i));
    }
    cluts.push(ramp);
    for (const b of [2, 7, 14]) {
        for (let i = 1; i < 16; i++) {
            check(pal[b * 16 + i] === pal[13 * 16 + i], "frame bank colours");
        }
    }
    // the frame's inner window: flood fill of the transparent pixels
    const w = 11 * 8;
    const h = 15 * 8;
    const px = new Array<number>(w * h).fill(0);
    const m = maps.get("frame")!;
    m.forEach((v, k) => {
        if (v) {
            const t = pool[v - 1][0];
            for (let j = 0; j < 64; j++) {
                px[(Math.floor(k / 11) * 8 + (j >> 3)) * w + (k % 11) * 8 + (j & 7)] = t[j];
            }
        }
    });
    const seen = new Uint8Array(w * h);
    let count = 0;
    let x0 = Infinity;
    let y0 = Infinity;
    let x1 = -Infinity;
    let y1 = -Infinity;
    const todo: [number, number][] = [[w / 2 - 4, h / 2]];
    while (todo.length) {
        const [x, y] = todo.pop()!;
        if (!(x >= 0 && x < w && y >= 0 && y < h) || seen[y * w + x] || px[y * w + x]) {
            continue;
        }
        seen[y * w + x] = 1;
        count++;
        x0 = Math.min(x0, x);
        y0 = Math.min(y0, y);
        x1 = Math.max(x1, x);
        y1 = Math.max(y1, y);
        todo.push([x + 1, y], [x - 1, y], [x, y + 1], [x, y - 1]);
    }
    const win: [number, number, number, number] = [x0, y0, x1 + 1 - x0, y1 + 1 - y0];
    check(count * 100 >= 90 * win[2] * win[3], "window fill");
    return { pool, maps, cluts, win };
}

const tileWords = (t: number[]): number[] => {
    const words: number[] = [];
    for (let y = 0; y < 8; y++) {
        let v = 0;
        for (let x = 0; x < 8; x++) {
            v += t[y * 8 + x] * 2 ** (4 * x);
        }
        words.push(v);
    }
    return words;
};

export async function buildTitlePortraits(saturn: SaturnRead): Promise<CharselGfx> {
    const prg = await saturn("TITLE.PRG");
    const chr = await saturn("TITLE.CHR");
    const mp = await saturn("TITLE.MAP");
    const offs = walk(prg);
    const arrays: GfxArray[] = [];
    const lines = [
        "// Generated by mods/sotn-ultimate/tools/gen_title_portraits.py from the Saturn's",
        "// TITLE.PRG/TITLE.CHR: the New Game character select portraits, 80x88",
        "// 8bpp (two pixels per u16, left pixel in the low byte) and their",
        "// 128-colour CLUTs. Do not edit.",
        "#define MOD_CHARSEL_TEX_W " + W,
        "#define MOD_CHARSEL_TEX_H " + H,
        "#define MOD_CHARSEL_CLUT_LEN " + NCOL,
    ];
    const rows = (vals: string[], per: number): void => {
        for (let j = 0; j < vals.length; j += per) {
            lines.push("    " + vals.slice(j, j + per).join(", ") + ",");
        }
    };
    for (const [name, bank, palAddr] of PORTRAITS) {
        const o = bank - BASE;
        check(prg[o] * 2 === W && prg[o + 1] * 2 === H && be16(prg, o + 2) === 0, name + " bank");
        const start = offs.get(bank)!;
        const px = chr.subarray(start, start + W * H);
        check(px.length === W * H && px.every((v) => v < NCOL), name + " pixels");
        check(be16(prg, palAddr - BASE) === 0x4010, name + " palette header");
        const clut: number[] = [];
        for (let i = 0; i < NCOL; i++) {
            clut.push(ps1Colour(be16(prg, palAddr - BASE + 2 + i * 2), i));
        }
        const words: number[] = [];
        for (let j = 0; j < W * H; j += 2) {
            words.push(px[j] | (px[j + 1] << 8));
        }
        const maria = name === "maria";
        if (maria) {
            lines.push("#if defined(MOD_SATURN_MARIA)");
        }
        lines.push("const u16 g_ModCharselPix_" + name + "[" + words.length + "] MOD_RO = {");
        rows(words.map((v) => hex(v, 4)), 12);
        lines.push("};");
        lines.push("const u16 g_ModCharselClut_" + name + "[" + NCOL + "] MOD_RO = {");
        rows(clut.map((v) => hex(v, 4)), 12);
        lines.push("};");
        if (maria) {
            lines.push("#endif");
        }
        arrays.push({ name: "g_ModCharselPix_" + name, size: 2, values: words, maria });
        arrays.push({ name: "g_ModCharselClut_" + name, size: 2, values: clut, maria });
    }
    const { pool, maps, cluts, win } = layerArt(prg, mp);
    lines.push(
        "// The Player select screen's VDP2 layer (TITLE.MAP): 8x8 4bpp tiles",
        "// (PS1 nibble order, one u32 per row) and per-element cell maps",
        "// (0 = empty, n = tile n - 1); CLUT 0 = header, 1 = frame/names,",
        "// 2 = frame/names of the chosen character. WIN = the frame's inner",
        "// window (Saturn pixels from the frame's top left).",
        "#define MOD_CHARSEL_WIN_X " + win[0],
        "#define MOD_CHARSEL_WIN_Y " + win[1],
        "#define MOD_CHARSEL_WIN_W " + win[2],
        "#define MOD_CHARSEL_WIN_H " + win[3],
    );
    for (const [name, , , w, h] of LAYER_ELEMS) {
        lines.push("#define MOD_CHARSEL_" + name.toUpperCase() + "_W " + w);
        lines.push("#define MOD_CHARSEL_" + name.toUpperCase() + "_H " + h);
    }
    lines.push("const u32 g_ModCharselTiles[] MOD_RO = {");
    let open: string | null = null;
    const tiles: number[] = [];
    let guarded = 0;
    for (const [t, guard] of pool) {
        if (guard !== open) {
            lines.push("#if defined(" + guard + ")");
            open = guard;
        }
        const words = tileWords(t);
        tiles.push(...words);
        if (guard) {
            guarded += 8;
        }
        lines.push("    " + words.map((v) => hex(v, 8)).join(", ") + ",");
    }
    if (open) {
        lines.push("#endif");
    }
    lines.push("};");
    arrays.push({ name: "g_ModCharselTiles", size: 4, values: tiles, maria: false, guarded });
    for (const [name, , , w, h, guard] of LAYER_ELEMS) {
        if (guard) {
            lines.push("#if defined(" + guard + ")");
        }
        lines.push("const u8 g_ModCharselMap_" + name + "[" + w * h + "] MOD_RO = {");
        const m = maps.get(name)!;
        rows(m.map(String), w);
        lines.push("};");
        if (guard) {
            lines.push("#endif");
        }
        arrays.push({ name: "g_ModCharselMap_" + name, size: 1, values: m, maria: guard !== null });
    }
    lines.push("const u16 g_ModCharselClut4[" + cluts.length + "][16] MOD_RO = {");
    for (const c of cluts) {
        lines.push("    {" + c.map((v) => hex(v, 4)).join(", ") + "},");
    }
    lines.push("};");
    arrays.push({ name: "g_ModCharselClut4", size: 2, values: cluts.flat(), maria: false });
    return { arrays, win, header: lines.join("\n") + "\n" };
}
