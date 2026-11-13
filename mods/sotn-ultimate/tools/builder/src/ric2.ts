// SPDX-License-Identifier: AGPL-3.0-or-later
// Richter's Saturn alternate costume (the long blue coat of RICHTER2.PRG/CHR +
// RIC_W2.CHR): a byte-exact port of mods/sotn-ultimate/tools/build_ric2.py (RIC2.BIN),
// build_bo6ric2.py (BO6RIC2.BIN), build_topric2.py (TOP's g_ModTopRic2) and
// build_selric2.py (SELRIC2.BIN). See those scripts' headers for the art
// mapping and the formats.
//
// Inputs: the Saturn disc (RICHTER.PRG/.CHR, RICHTER2.PRG/.CHR, RIC_W2.CHR),
// the PS1 disc (BIN/F_GAME2.BIN, retail palettes) and the mod's own built
// overlays as they are on the patched disc (BIN/RIC.BIN, BOSS/BO6/BO6.BIN,
// ST/TOP/TOP.BIN with g_ModTopRic2 blank, ST/SEL/SEL.BIN), plus the link
// map symbols the scripts look up (Ric2Syms: mod build data).
//
// Output: per disc file, the byte ranges these scripts write (everything else
// of RIC2.BIN / BO6RIC2.BIN / SELRIC2.BIN is the patch's: the copies of
// RIC.BIN / BO6.BIN / SEL.BIN), and the whole new files.
import { decompressLzss } from "./saturn_lzss.ts";

export type DiscRead = (path: string) => Promise<Uint8Array>;

export interface Range {
    offset: number;
    bytes: Uint8Array;
}

export interface NumArray {
    size: number; // bytes per element
    values: number[];
}

// link map addresses (RAM) the scripts use
export interface Ric2Syms {
    // build/us/ric.map (build_ric2.py's ric_syms: the last definition of a name)
    ric: { richter_sprites: number; spritebank0: number; spritebank1: number };
    // build/us/bobo6.map (build_bo6ric2.syms: the first)
    bo6: { richter_sprites: number; D_us_801A39B0: number; D_us_801A43A4: number; D_us_8019E9A0: number };
    // build/us/sttop.map
    top: { richter_sprites: number; g_ModTopRic2: number };
    // build/us/stsel.map
    sel: { D_8018C718: number; D_8018C92C: number; D_8018CB4C: number; D_8018D5D4: number };
}

export interface Ric2Result {
    // disc path -> the ranges to write over the patched file
    ranges: Map<string, Range[]>;
    // disc path -> the whole new file (RIC2.BIN, BO6RIC2.BIN, SELRIC2.BIN and
    // TOP.BIN with g_ModTopRic2 filled)
    files: Map<string, Uint8Array>;
    // header -> arrays (g_ModTopRic2's 231 words, the same bytes as TOP's range)
    arrays: Map<string, Map<string, NumArray>>;
}

export const RIC2_PATHS = {
    ric: "BIN/RIC.BIN", bo6: "BOSS/BO6/BO6.BIN", top: "ST/TOP/TOP.BIN", sel: "ST/SEL/SEL.BIN",
    ric2: "BIN/RIC2.BIN", bo6ric2: "BIN/BO6RIC2.BIN", selric2: "BIN/SELRIC2.BIN",
};

const RAM = 0x8013c000;
const SAT = 0x060a5000;
const MAGIC = 0x32434952; // "RIC2"
const SHEET_END = 0x170ac;
const MAX_W = 64;
const MAX_H = 64;
const BO6_RAM = 0x80180000;
const TOP_RAM = 0x80180000;
const SEL_RAM = 0x80180000;
const RIC2_HDR = SHEET_END - 8;
const TOP_WORDS = 231;

interface Profile {
    imageTable: number;
    frameTable: number;
    paletteTable: number;
    prg: string;
    chr: string;
}

// tools/saturn/character_viewer.py CHARACTER_PROFILES["richter"] and the
// RICHTER2 overrides of build_ric2.py
const P1: Profile = { imageTable: 0x060c12ec, frameTable: 0x060c0f98, paletteTable: 0x060c0240,
    prg: "RICHTER.PRG", chr: "RICHTER.CHR" };
const P2: Profile = { imageTable: 0x060c120c, frameTable: 0x060c0ea0, paletteTable: 0x060c0240,
    prg: "RICHTER2.PRG", chr: "RICHTER2.CHR" };

// (PS1 rect (u0, v0, u1, v1), RIC_W2 offset, stride, phase, x offset[, y
// offset, image height])
const ARMS: number[][] = [
    [112, 16, 128, 32, 21350, 24, 3, -2],
    [112, 0, 128, 16, 21508, 24, 1, -5],
    [64, 48, 88, 64, 21660, 24, 0, 5],
    [88, 48, 112, 64, 0x5540, 32, 3, 2, 4, 9],
    [80, 32, 96, 48, 21970, 24, 3, -2],
    [112, 32, 128, 56, 22136, 16, 3, 2],
    [96, 32, 112, 48, 22286, 16, 3, -2],
    [96, 16, 112, 32, 22414, 16, 1, -2],
    [96, 0, 112, 16, 22504, 16, 3, 1],
];
const ARM_VRAM = [0x240, 0x180];

const be16 = (d: Uint8Array, o: number): number => (d[o] << 8) | d[o + 1];
const bes16 = (d: Uint8Array, o: number): number => (be16(d, o) << 16) >> 16;
const be32 = (d: Uint8Array, o: number): number => ((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) >>> 0;
const le16 = (d: Uint8Array, o: number): number => d[o] | (d[o + 1] << 8);
const les16 = (d: Uint8Array, o: number): number => (le16(d, o) << 16) >> 16;
const le32 = (d: Uint8Array, o: number): number => (d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (d[o + 3] << 24)) >>> 0;
const put16 = (d: Uint8Array, o: number, v: number): void => {
    d[o] = v & 0xff;
    d[o + 1] = (v >> 8) & 0xff;
};
const put32 = (d: Uint8Array, o: number, v: number): void => {
    put16(d, o, v & 0xffff);
    put16(d, o + 2, (v >>> 16) & 0xffff);
};
const pyMod = (a: number, n: number): number => ((a % n) + n) % n;

function check(ok: boolean, what: string): void {
    if (!ok) {
        throw new Error("ric2: " + what);
    }
}

function sameBytes(a: Uint8Array, b: Uint8Array): boolean {
    if (a.length !== b.length) {
        return false;
    }
    for (let i = 0; i < a.length; i++) {
        if (a[i] !== b[i]) {
            return false;
        }
    }
    return true;
}

// a 4bpp image as colour indices, row-major
class Img {
    w: number;
    h: number;
    d: Uint8Array;

    constructor(w: number, h: number, d: Uint8Array | null = null) {
        this.w = w;
        this.h = h;
        this.d = d === null ? new Uint8Array(w * h) : d;
    }

    at(y: number, x: number): number {
        return this.d[y * this.w + x];
    }

    // the given columns, in order (numpy img[:, cols])
    cols(cols: number[]): Img {
        const o = new Img(cols.length, this.h);
        for (let y = 0; y < this.h; y++) {
            for (let k = 0; k < cols.length; k++) {
                o.d[y * o.w + k] = this.d[y * this.w + cols[k]];
            }
        }
        return o;
    }

    nonzero(): number {
        let n = 0;
        for (const v of this.d) {
            if (v) {
                n++;
            }
        }
        return n;
    }
}

// PS1 4bpp, low nibble first
function unpackPsx(data: Uint8Array, o: number, w: number, h: number): Img {
    const img = new Img(w, h);
    const n = (w * h) >> 1;
    for (let i = 0; i < n; i++) {
        img.d[2 * i] = data[o + i] & 15;
        img.d[2 * i + 1] = data[o + i] >> 4;
    }
    return img;
}

function packPsx(img: Img): Uint8Array {
    const out = new Uint8Array(img.d.length >> 1);
    for (let i = 0; i < out.length; i++) {
        out[i] = (img.d[2 * i] | (img.d[2 * i + 1] << 4)) & 0xff;
    }
    return out;
}

class Saturn {
    p: Profile;
    prg: Uint8Array;
    chr: Uint8Array;
    recs: [number, number, number][];
    streams: [number, number][];
    cache: Map<number, Img>;

    constructor(p: Profile, prg: Uint8Array, chr: Uint8Array) {
        this.p = p;
        this.prg = prg;
        this.chr = chr;
        this.cache = new Map();
        // character_viewer.read_image_records
        this.recs = [];
        let cursor = p.imageTable - SAT;
        let prev = -1;
        while (cursor + 4 <= prg.length) {
            const dims = be16(prg, cursor);
            const units = be16(prg, cursor + 2);
            const w = dims >> 8;
            const h = dims & 0xff;
            if (!w || !h || w > 128 || h > 128 || units < prev) {
                break;
            }
            this.recs.push([w, h, units * 8]);
            prev = units;
            cursor += 4;
        }
        // read_stream_records (stream_entry_base 0)
        const dir = be32(prg, 0x40) - SAT;
        const backing = be32(prg, dir);
        this.streams = [];
        for (let i = 0; i < this.recs.length; i++) {
            this.streams.push([be32(prg, dir + i * 8) - backing, be32(prg, dir + i * 8 + 4)]);
        }
    }

    image(j: number): Img {
        const hit = this.cache.get(j);
        if (hit !== undefined) {
            return hit;
        }
        const [wf, hf] = this.recs[j];
        const [o, s] = this.streams[j];
        const d = decompressLzss(this.chr.subarray(o, o + s));
        const w = wf * 2;
        const h = hf * 2;
        const n = (w * h) >> 1;
        check(d.length >= n, "short Saturn image " + j);
        const img = new Img(w, h);
        for (let i = 0; i < n; i++) {
            img.d[2 * i] = d[i] >> 4;
            img.d[2 * i + 1] = d[i] & 15;
        }
        this.cache.set(j, img);
        return img;
    }

    // per frame: (flags >> 8, x, y, image) of its single part, or null
    frames(): ([number, number, number, number] | null)[] {
        const ft = this.p.frameTable - SAT;
        const it = this.p.imageTable - SAT;
        const out: ([number, number, number, number] | null)[] = [];
        for (let i = 0; i < Math.floor((it - ft) / 4); i++) {
            const o = be32(this.prg, ft + i * 4) - SAT;
            const pc = (be16(this.prg, o) >> 8) & 0x3f;
            if (pc !== 1) {
                out.push(null);
                continue;
            }
            out.push([be16(this.prg, o + 4) >> 8, bes16(this.prg, o + 6), bes16(this.prg, o + 8),
                be16(this.prg, o + 10)]);
        }
        return out;
    }

    palette(bank: number): number[] {
        const t = this.p.paletteTable - SAT + 2 + bank * 32;
        const out: number[] = [];
        for (let i = 0; i < 16; i++) {
            out.push(be16(this.prg, t + 2 * i));
        }
        return out;
    }
}

// drop the columns whose absolute x (left + c) % 5 == phase; rescue: a
// dropped pixel with no neighbour in its row keeps its place
function narrow(img: Img, left: number, phase: number, rescue: boolean): Img {
    const w = img.w;
    const cols: number[] = [];
    for (let c = 0; c < w; c++) {
        if (pyMod(left + c, 5) !== phase) {
            cols.push(c);
        }
    }
    const out = img.cols(cols);
    if (rescue) {
        for (let c = 0; c < w; c++) {
            if (pyMod(left + c, 5) !== phase) {
                continue;
            }
            let dst = -1;
            for (let y = 0; y < img.h; y++) {
                const v = img.at(y, c);
                const nl = c > 0 ? img.at(y, c - 1) : 0;
                const nr = c + 1 < w ? img.at(y, c + 1) : 0;
                if (v !== 0 && nl === 0 && nr === 0) {
                    if (dst < 0) {
                        let k = 0;
                        for (let cc = 0; cc < c; cc++) {
                            if (pyMod(left + cc, 5) !== phase) {
                                k++;
                            }
                        }
                        dst = Math.min(Math.max(k - 1, 0), out.w - 1);
                        check(dst >= 0, "narrow: empty image");
                    }
                    out.d[y * out.w + dst] = v;
                }
            }
        }
    }
    return out;
}

// kept columns from absolute x a to b (signed)
function droppedBetween(a: number, b: number, phase: number): number {
    if (b >= a) {
        let n = 0;
        for (let x = a; x < b; x++) {
            if (pyMod(x, 5) === phase) {
                n++;
            }
        }
        return b - a - n;
    }
    return -droppedBetween(b, a, phase);
}

// pixels differing between ps at (0, 0) and nr at (ox, oy) on their union
function placeScore(ps: Img, nr: Img, nrCount: number, ox: number, oy: number): number {
    let mism = 0;
    let inside = 0;
    for (let y = 0; y < ps.h; y++) {
        const ny = y - oy;
        const rowIn = ny >= 0 && ny < nr.h;
        for (let x = 0; x < ps.w; x++) {
            const nx = x - ox;
            const b = rowIn && nx >= 0 && nx < nr.w ? nr.d[ny * nr.w + nx] : 0;
            if (b) {
                inside++;
            }
            if (ps.d[y * ps.w + x] !== b) {
                mism++;
            }
        }
    }
    return mism + nrCount - inside;
}

// best (mismatch, phase relative to the image, ox, oy)
function align(ps: Img, sat: Img): [number, number, number, number] {
    let best: [number, number, number, number] | null = null;
    for (let ph = 0; ph < 5; ph++) {
        const cols: number[] = [];
        for (let c = 0; c < sat.w; c++) {
            if (c % 5 !== ph) {
                cols.push(c);
            }
        }
        const nr = sat.cols(cols);
        const cnt = nr.nonzero();
        for (let oy = -4; oy < 5; oy++) {
            for (let ox = -10; ox < 11; ox++) {
                const s = placeScore(ps, nr, cnt, ox, oy);
                if (best === null || s < best[0]) {
                    best = [s, ph, ox, oy];
                }
            }
        }
    }
    return best!;
}

// cut to the content, width padded to a multiple of 4: (image, x0, y0)
function crop(img: Img): [Img, number, number] {
    let x0 = Infinity;
    let x1 = -1;
    let y0 = Infinity;
    let y1 = -1;
    for (let y = 0; y < img.h; y++) {
        for (let x = 0; x < img.w; x++) {
            if (img.at(y, x)) {
                x0 = Math.min(x0, x);
                x1 = Math.max(x1, x);
                y0 = Math.min(y0, y);
                y1 = Math.max(y1, y);
            }
        }
    }
    if (y1 < 0) {
        return [new Img(Math.min(4, img.w), Math.min(1, img.h)), 0, 0];
    }
    const cw = x1 + 1 - x0;
    const w = (cw + 3) & ~3;
    const c = new Img(w, y1 + 1 - y0);
    for (let y = 0; y < c.h; y++) {
        for (let x = 0; x < cw; x++) {
            c.d[y * w + x] = img.at(y0 + y, x0 + x);
        }
    }
    return [c, x0, y0];
}

// the largest 8-connected component (the first found of the largest size)
function largestComponent(img: Img): Img {
    const { w, h } = img;
    const seen = new Uint8Array(w * h);
    let best: number[] = [];
    for (let y = 0; y < h; y++) {
        for (let x = 0; x < w; x++) {
            if (img.at(y, x) && !seen[y * w + x]) {
                const comp: number[] = [];
                const stack = [y * w + x];
                seen[y * w + x] = 1;
                while (stack.length) {
                    const p = stack.pop()!;
                    comp.push(p);
                    const cy = Math.floor(p / w);
                    const cx = p % w;
                    for (let dy = -1; dy <= 1; dy++) {
                        for (let dx = -1; dx <= 1; dx++) {
                            const ny = cy + dy;
                            const nx = cx + dx;
                            if (ny >= 0 && ny < h && nx >= 0 && nx < w && img.at(ny, nx) && !seen[ny * w + nx]) {
                                seen[ny * w + nx] = 1;
                                stack.push(ny * w + nx);
                            }
                        }
                    }
                }
                if (comp.length > best.length) {
                    best = comp;
                }
            }
        }
    }
    const out = new Img(w, h);
    for (const p of best) {
        out.d[p] = img.d[p];
    }
    return out;
}

function retailPal(g2: Uint8Array, k: number): number[] {
    const out: number[] = [];
    for (let i = 0; i < 16; i++) {
        out.push(le16(g2, 0x40000 + (k - 0x100) * 32 + 2 * i));
    }
    return out;
}

function concat(parts: Uint8Array[]): Uint8Array {
    let n = 0;
    for (const p of parts) {
        n += p.length;
    }
    const out = new Uint8Array(n);
    let o = 0;
    for (const p of parts) {
        out.set(p, o);
        o += p.length;
    }
    return out;
}

const keyOf = (b: Uint8Array): string => {
    let s = "";
    for (const v of b) {
        s += String.fromCharCode(v);
    }
    return s;
};

// --- RIC2.BIN (build_ric2.py main) ------------------------------------------

export interface Ric2Bin {
    out: Uint8Array;
    ranges: Range[];
}

export function makeRic2(ric: Uint8Array, syms: Ric2Syms["ric"], s1: Saturn, s2: Saturn, g2: Uint8Array,
                         w2: Uint8Array): Ric2Bin {
    check(syms.spritebank0 - RAM === SHEET_END, "RIC's sprite sheet moved");
    const table = syms.richter_sprites - RAM;
    // the PS1 sheet
    const ptrs: number[] = [];
    let minPtr = Infinity;
    for (let o = table; ; o += 4) {
        const p = le32(ric, o);
        if (!(RAM <= p && p < RAM + SHEET_END) || (ptrs.length && o >= minPtr - RAM)) {
            break;
        }
        ptrs.push(p);
        minPtr = Math.min(minPtr, p);
    }
    const n = ptrs.length;
    const dataStart = table + 4 * n;
    // (w, h, px, py, image, data offset)
    const sprites: [number, number, number, number, Img, number][] = ptrs.map((p) => {
        const q = p - RAM;
        const [w, h, px, py] = [ric[q], ric[q + 1], ric[q + 2], ric[q + 3]];
        return [w, h, px, py, unpackPsx(ric, q + 4, w, h), q];
    });
    // PS1 frames: (sprite, x, y, clut, offset)
    const frames: ([number, number, number, number, number] | null)[] = [];
    {
        let o = syms.spritebank0 - RAM;
        let lim = syms.spritebank1 - RAM;
        while (o < lim) {
            const p = le32(ric, o);
            if (p === 0) {
                frames.push(null);
            } else {
                const q = p - RAM;
                lim = Math.min(lim, q);
                frames.push([le16(ric, q) & 0x7fff, les16(ric, q + 2), les16(ric, q + 4), les16(ric, q + 6), q]);
            }
            o += 4;
        }
    }
    const f1 = s1.frames();
    const f2 = s2.frames();
    check(frames.length === f1.length, "frame count " + frames.length + " " + f1.length);
    // Saturn placement per image: left/top edges of both versions
    const delta = new Map<number, [[number, number, number, number], [number, number]]>();
    const cand = new Map<number, Set<number>>();
    frames.forEach((fr, f) => {
        if (fr === null) {
            return;
        }
        const a = f1[f];
        const b = f2[f];
        check(a !== null && b !== null && a[3] === b[3], "frame " + f);
        const j = a![3];
        let cs = cand.get(fr[0]);
        if (cs === undefined) {
            cs = new Set();
            cand.set(fr[0], cs);
        }
        cs.add(j);
        const i1 = s1.image(j);
        const i2 = s2.image(j);
        const e: [number, number, number, number] = [a![1] - Math.floor(i1.w / 2), a![2] - Math.floor(i1.h / 2),
            b![1] - Math.floor(i2.w / 2), b![2] - Math.floor(i2.h / 2)];
        const k: [number, number] = [e[2] - e[0], e[3] - e[1]];
        const old = delta.get(j);
        if (old !== undefined) {
            check(old[1][0] === k[0] && old[1][1] === k[1], "inconsistent " + j);
        } else {
            delta.set(j, [e, k]);
        }
    });
    // group sprites by shared data (placeholders share one image)
    const groups = new Map<number, number[]>();
    sprites.forEach((s, i) => {
        const g = groups.get(s[5]);
        if (g === undefined) {
            groups.set(s[5], [i]);
        } else {
            g.push(i);
        }
    });
    const newImg = new Map<number, Uint8Array>();
    const frameFix = new Map<number, [number, number]>();
    for (const off of [...groups.keys()].sort((a, b) => a - b)) {
        const members = groups.get(off)!;
        const [, h, px, py, ps] = sprites[members[0]];
        const js = new Set<number>();
        for (const i of members) {
            for (const j of cand.get(i) ?? []) {
                js.add(j);
            }
        }
        let best: [[number, number, number, number], number] | null = null;
        for (const j of [...js].sort((a, b) => a - b)) {
            const r = align(ps, s1.image(j));
            if (best === null || r[0] < best[0][0]) {
                best = [r, j];
            }
        }
        if (best === null || best[0][0] > 0.15 * ps.nonzero()) {
            // placeholder art: the Saturn image it is a copy of
            for (let j = 0; j < s1.recs.length; j++) {
                if (Math.abs(s1.image(j).h - h) > 4) {
                    continue;
                }
                const r = align(ps, s1.image(j));
                if (best === null || r[0] < best[0][0]) {
                    best = [r, j];
                }
            }
        }
        const [[, ph, ox, oy], j] = best!;
        const dj = delta.get(j);
        check(dj !== undefined, "no Saturn placement for image " + j);
        const [left1, top1, left2, top2] = dj![0];
        const phase = pyMod(left1 + ph, 5);
        const img2 = narrow(s2.image(j), left2, phase, true);
        const dx = droppedBetween(left1, left2, phase);
        const dy = top2 - top1;
        const [c, cx, cy] = crop(img2);
        check(c.w <= MAX_W && c.h <= MAX_H, "sprite too big " + members);
        let npx = px + ox + dx + cx;
        let npy = py + oy + dy + cy;
        let fx = 0;
        let fy = 0;
        if (!(0 <= npx && npx <= 255)) {
            fx = npx < 0 ? npx : npx - 255;
            npx -= fx;
        }
        if (!(0 <= npy && npy <= 255)) {
            fy = npy < 0 ? npy : npy - 255;
            npy -= fy;
        }
        if (fx || fy) {
            for (const fr of frames) {
                if (fr && members.includes(fr[0])) {
                    frameFix.set(fr[4], [fx, fy]);
                }
            }
        }
        newImg.set(off, concat([new Uint8Array([c.w, c.h, npx, npy]), packPsx(c)]));
    }
    // palettes
    const base = retailPal(g2, 0x120);
    const r1b0 = s1.palette(0);
    const r2b0 = s2.palette(0);
    for (let i = 1; i < 16; i++) {
        check((base[i] & 0x7fff) === (r1b0[i] & 0x7fff), "PAL_RICHTER is not RICHTER's bank 0");
    }
    const satBank = new Map([[0x12b, 2], [0x12c, 3], [0x12d, 4], [0x12e, 6]]);
    const pals: [number, number[]][] = [];
    for (let k = 0x100; k < 0x200; k++) {
        const p = retailPal(g2, k);
        const same: number[] = [];
        for (let i = 1; i < 16; i++) {
            if ((p[i] & 0x7fff) === (base[i] & 0x7fff)) {
                same.push(i);
            }
        }
        if (same.length < 10) {
            continue;
        }
        const q = p.slice();
        for (const i of same) {
            q[i] = (r2b0[i] & 0x7fff) | (p[i] & 0x8000);
        }
        if (q.some((v, i) => v !== p[i])) {
            const sb = satBank.get(k);
            pals.push([k, sb === undefined ? q : [q[0], ...s2.palette(sb).slice(1)]]);
        }
    }
    // whip-brandish arms
    const arms: [[number, number, number, number], Img][] = [];
    for (const arm of ARMS) {
        const [u0, v0, u1, v1, o, stride, ph, ox] = arm;
        const h = v1 - v0;
        const w = u1 - u0;
        const [oy, ih] = arm.length > 8 ? [arm[8], arm[9]] : [0, h];
        const n2 = (ih * stride) >> 1;
        const sat = new Img(stride, ih);
        for (let i = 0; i < n2; i++) {
            sat.d[2 * i] = w2[o + i] >> 4;
            sat.d[2 * i + 1] = w2[o + i] & 15;
        }
        const nr = largestComponent(narrow(sat, 0, ph, true));
        const out = new Img(w, h);
        for (let y = 0; y < ih; y++) {
            for (let x = 0; x < nr.w; x++) {
                const v = nr.at(y, x);
                if (v && 0 <= x + ox && x + ox < w && 0 <= y + oy && y + oy < h) {
                    out.d[(y + oy) * w + x + ox] = v;
                }
            }
        }
        arms.push([[ARM_VRAM[0] + Math.floor(u0 / 4), ARM_VRAM[1] + v0, w >> 2, h], out]);
    }
    // write RIC2.BIN
    const out = ric.slice();
    const end = SHEET_END - 8;
    let pos = dataStart;
    const written = new Map<string, number>();
    const addr = new Map<number, number>();
    for (const off of [...newImg.keys()].sort((a, b) => a - b)) {
        const b = newImg.get(off)!;
        const key = keyOf(b);
        if (!written.has(key)) {
            written.set(key, pos);
            out.set(b, pos);
            pos = (pos + b.length + 3) & ~3;
        }
        addr.set(off, written.get(key)!);
    }
    sprites.forEach((s, i) => put32(out, table + 4 * i, RAM + addr.get(s[5])!));
    const extra = pos;
    const blob: number[] = [];
    const b32 = (v: number): void => {
        blob.push(v & 0xff, (v >> 8) & 0xff, (v >> 16) & 0xff, (v >>> 24) & 0xff);
    };
    const b16 = (v: number): void => {
        blob.push(v & 0xff, (v >> 8) & 0xff);
    };
    b32(pals.length);
    for (const [k, q] of pals) {
        b32(k);
        q.forEach(b16);
    }
    b32(arms.length);
    for (const [[x, y, w, h], img] of arms) {
        [x, y, w, h].forEach(b16);
        blob.push(...packPsx(img));
        while (blob.length % 4) {
            blob.push(0);
        }
    }
    check(pos + blob.length <= end, "costume sheet too big");
    out.set(blob, pos);
    pos += blob.length;
    out.fill(0, pos, end);
    put32(out, end, MAGIC);
    put32(out, end + 4, RAM + extra);
    const ranges: Range[] = [{ offset: table, bytes: out.slice(table, SHEET_END) }];
    for (const [q, [fx, fy]] of frameFix) {
        put16(out, q + 2, les16(out, q + 2) + fx);
        put16(out, q + 4, les16(out, q + 4) + fy);
        ranges.push({ offset: q + 2, bytes: out.slice(q + 2, q + 6) });
    }
    return { out, ranges };
}

// --- shared by the BO6 / TOP / SEL copies (build_bo6ric2.py) ----------------

function sheet(d: Uint8Array, base: number, table: number): number[] {
    const ptrs: number[] = [];
    let minPtr = Infinity;
    for (let o = table - base; ; o += 4) {
        const p = le32(d, o);
        if (!(base <= p && p < base + d.length) || (ptrs.length && o >= minPtr - base)) {
            break;
        }
        ptrs.push(p);
        minPtr = Math.min(minPtr, p);
    }
    return ptrs;
}

function sprite(d: Uint8Array, base: number, p: number): Uint8Array {
    const q = p - base;
    return d.slice(q, q + 4 + ((d[q] * d[q + 1]) >> 1));
}

function frameOffsets(d: Uint8Array, base: number, start: number, end: number): (number | null)[] {
    const out: (number | null)[] = [];
    let o = start - base;
    let lim = end - base;
    while (o < lim) {
        const p = le32(d, o);
        if (p) {
            lim = Math.min(lim, p - base);
        }
        out.push(p ? p - base : null);
        o += 4;
    }
    return out;
}

// RIC2's extras blob: [(palette id, offset of its 32 colour bytes)]
function ric2Pals(ric2: Uint8Array): [number, number][] {
    check(le32(ric2, RIC2_HDR) === MAGIC, "RIC2 magic");
    let o = le32(ric2, RIC2_HDR + 4) - RAM;
    const n = le32(ric2, o);
    o += 4;
    const out: [number, number][] = [];
    for (let i = 0; i < n; i++) {
        out.push([le32(ric2, o), o + 4]);
        o += 36;
    }
    return out;
}

export function makeBo6Ric2(ric: Uint8Array, ric2: Uint8Array, bo6: Uint8Array, syms: Ric2Syms,
                            g2: Uint8Array): Ric2Bin {
    const rs = syms.ric;
    const bs = syms.bo6;
    check(ric.length === ric2.length, "RIC2 size");
    const rp = sheet(ric, RAM, rs.richter_sprites);
    const r2p = sheet(ric2, RAM, rs.richter_sprites);
    const bp = sheet(bo6, BO6_RAM, bs.richter_sprites);
    check(rp.length === bp.length && bp.length === r2p.length, "sheet sizes");
    rp.forEach((a, i) => check(sameBytes(sprite(ric, RAM, a), sprite(bo6, BO6_RAM, bp[i])), "BO6 sheet"));
    const start = Math.min(...bp) - BO6_RAM;
    let end = 0;
    for (const b of bp) {
        end = Math.max(end, b - BO6_RAM + sprite(bo6, BO6_RAM, b).length);
    }
    const tab = bs.richter_sprites - BO6_RAM;
    check(start === tab + 4 * bp.length, "BO6 sheet start");
    const out = bo6.slice();
    let pos = start;
    const placed = new Map<number, number>();
    r2p.forEach((p, i) => {
        if (!placed.has(p)) {
            const s = sprite(ric2, RAM, p);
            placed.set(p, pos);
            out.set(s, pos);
            pos = (pos + s.length + 3) & ~3;
        }
        put32(out, tab + 4 * i, BO6_RAM + placed.get(p)!);
    });
    check(pos <= ((end + 3) & ~3), "coat sheet too big");
    if (end > pos) {
        out.fill(0, pos, end);
    }
    const ranges: Range[] = [{ offset: tab, bytes: out.slice(tab, Math.max(pos, end)) }];
    // frames: BO6 spritebank 1 == RIC spritebank0; RIC2's moved pivots
    const rf = frameOffsets(ric, RAM, rs.spritebank0, rs.spritebank1);
    const bf = frameOffsets(bo6, BO6_RAM, bs.D_us_801A39B0, bs.D_us_801A43A4);
    check(rf.length === bf.length, "frame counts");
    const done = new Set<number>();
    rf.forEach((a, i) => {
        const b = bf[i];
        if (a === null) {
            check(b === null, "BO6 frame");
            return;
        }
        check(b !== null && sameBytes(ric.subarray(a, a + 8), bo6.subarray(b, b + 8)), "BO6 frame");
        if (!sameBytes(ric2.subarray(a, a + 8), ric.subarray(a, a + 8)) && !done.has(b!)) {
            out.set(ric2.subarray(a, a + 8), b!);
            done.add(b!);
            ranges.push({ offset: b!, bytes: ric2.slice(a, a + 8) });
        }
    });
    // palettes (PAL_BULK 0x2200: 0x220-0x24F)
    const pal = bs.D_us_8019E9A0 - BO6_RAM;
    for (const [k, co] of ric2Pals(ric2)) {
        check(0x120 <= k && k < 0x150, "palette id");
        const q = pal + (k - 0x120) * 32;
        const retail = g2.subarray(0x40000 + (k - 0x100) * 32, 0x40000 + (k - 0xff) * 32);
        check(sameBytes(bo6.subarray(q, q + 32), retail), "BO6 palette");
        out.set(ric2.subarray(co, co + 32), q);
        ranges.push({ offset: q, bytes: ric2.slice(co, co + 32) });
    }
    return { out, ranges };
}

// TOP's g_ModTopRic2 words (build_topric2.py). TOP.BIN's g_ModTopRic2 is not
// read (blank on the patched disc, filled in a full build).
export function makeTopRic2(ric: Uint8Array, ric2: Uint8Array, top: Uint8Array, syms: Ric2Syms): Ric2Bin {
    const rp = sheet(ric, RAM, syms.ric.richter_sprites);
    const r2p = sheet(ric2, RAM, syms.ric.richter_sprites);
    const tab = syms.top.richter_sprites - TOP_RAM;
    const parts: Uint8Array[] = [];
    for (const i of [16, 17]) {
        const tp = le32(top, tab + 4 * i);
        const retail = sprite(top, TOP_RAM, tp);
        check(sameBytes(retail, sprite(ric, RAM, rp[i])), "TOP sprite " + i);
        const coat = sprite(ric2, RAM, r2p[i]);
        check(coat[0] === retail[0] && coat[1] === retail[1], "RIC2 sprite size " + i);
        parts.push(coat);
    }
    const pals: Uint8Array[] = [];
    for (const [k, co] of ric2Pals(ric2)) {
        // only his own palette (PAL_RICHTER)
        if (k === 0x120) {
            const p = new Uint8Array(36);
            put16(p, 0, 0x230 + k - 0x120);
            p.set(ric2.subarray(co, co + 32), 4);
            pals.push(p);
        }
    }
    const cnt = new Uint8Array(4);
    put32(cnt, 0, pals.length);
    const blob = concat([...parts, cnt, ...pals]);
    check(blob.length <= TOP_WORDS * 4, "g_ModTopRic2 too small");
    const words = new Uint8Array(TOP_WORDS * 4);
    words.set(blob);
    const at = syms.top.g_ModTopRic2 - TOP_RAM;
    const out = top.slice();
    out.set(words, at);
    return { out, ranges: [{ offset: at, bytes: words }] };
}

// SELRIC2.BIN (build_selric2.py) from the final SEL.BIN
export function makeSelRic2(ric: Uint8Array, ric2: Uint8Array, sel: Uint8Array, syms: Ric2Syms,
                            g2: Uint8Array): Ric2Bin {
    const rp = sheet(ric, RAM, syms.ric.richter_sprites);
    const r2p = sheet(ric2, RAM, syms.ric.richter_sprites);
    const out = sel.slice();
    const ranges: Range[] = [];
    const sprites: [number, number][] = [[0, syms.sel.D_8018C718], [6, syms.sel.D_8018C92C], [37, syms.sel.D_8018CB4C]];
    for (const [i, sym] of sprites) {
        const at = sym - SEL_RAM;
        const retail = sprite(ric, RAM, rp[i]);
        check(sameBytes(sel.subarray(at, at + retail.length), retail), "SEL image " + i);
        const coat = sprite(ric2, RAM, r2p[i]);
        check(coat[0] === retail[0] && coat[1] === retail[1] && coat.length === retail.length, "SEL coat " + i);
        out.set(coat, at);
        ranges.push({ offset: at, bytes: coat });
    }
    const base = syms.sel.D_8018D5D4 - SEL_RAM;
    for (const [k, co] of ric2Pals(ric2)) {
        check(0x120 <= k && k < 0x150, "palette id");
        const q = base + (k - 0x120) * 32;
        const retail = g2.subarray(0x40000 + (k - 0x100) * 32, 0x40000 + (k - 0xff) * 32);
        check(sameBytes(sel.subarray(q, q + 32), retail), "SEL palette");
        if (k === 0x120) {
            out.set(ric2.subarray(co, co + 32), q);
            ranges.push({ offset: q, bytes: ric2.slice(co, co + 32) });
        }
    }
    return { out, ranges };
}

// Everything: saturn reads RICHTER*.PRG/CHR and RIC_W2.CHR, ps1 the retail
// BIN/F_GAME2.BIN, built the mod's overlays (RIC2_PATHS.ric/bo6/top/sel).
export async function buildRic2(saturn: DiscRead, ps1: DiscRead, built: DiscRead, syms: Ric2Syms): Promise<Ric2Result> {
    const s1 = new Saturn(P1, await saturn(P1.prg), await saturn(P1.chr));
    const s2 = new Saturn(P2, await saturn(P2.prg), await saturn(P2.chr));
    const w2 = await saturn("RIC_W2.CHR");
    const g2 = await ps1("BIN/F_GAME2.BIN");
    const ric = await built(RIC2_PATHS.ric);
    const r2 = makeRic2(ric, syms.ric, s1, s2, g2, w2);
    const bo6 = makeBo6Ric2(ric, r2.out, await built(RIC2_PATHS.bo6), syms, g2);
    const top = makeTopRic2(ric, r2.out, await built(RIC2_PATHS.top), syms);
    const sel = makeSelRic2(ric, r2.out, await built(RIC2_PATHS.sel), syms, g2);
    const words: number[] = [];
    for (let i = 0; i < TOP_WORDS; i++) {
        words.push(le32(top.ranges[0].bytes, 4 * i));
    }
    return {
        ranges: new Map([[RIC2_PATHS.ric2, r2.ranges], [RIC2_PATHS.bo6ric2, bo6.ranges],
            [RIC2_PATHS.top, top.ranges], [RIC2_PATHS.selric2, sel.ranges]]),
        files: new Map([[RIC2_PATHS.ric2, r2.out], [RIC2_PATHS.bo6ric2, bo6.out], [RIC2_PATHS.top, top.out],
            [RIC2_PATHS.selric2, sel.out]]),
        arrays: new Map([["mods/sotn-ultimate/src/richter_saturn/mod_ric2.h", new Map([["g_ModTopRic2", { size: 4, values: words }]])]]),
    };
}
