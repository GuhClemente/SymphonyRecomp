// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn's Maria boss fight in the Castle Center: a byte-exact port of
// mods/sotn-ultimate/tools/gen_cen_maria.py's numeric arrays (mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h;
// see the Python for the formats). Her body frames are composited from the
// Saturn STAGE_14 bank 0x060E9448, narrowed to PS1 proportions and
// LZSS-packed; her effects go to an fx sprite sheet; the tables (animations,
// hitboxes, speeds) are read from STAGE_14.PRG.
//
// Returns every array of plain numbers by name: the 1D ones, and the 2D ones
// (g_MbFrameDesc, g_MbFxQuad, g_MbHitbox, g_MbKick, g_MbBladeV) flattened in
// row order, as compiled. The dialogue scripts (macros, PS1 text) and the
// pointer tables (g_MbFrameTab, g_MbFxTab) are not here.
import { roundHalfEven } from "./dsp.ts";
import { PsxSpriteSheet, SaturnStage, SpriteState } from "./saturn_sprites.ts";
import type { PsxPart, SaturnPart } from "./saturn_sprites.ts";

export interface MbArray {
    size: number; // bytes per element
    values: number[];
}

const B = 0x060dc000;
const BODY_BANK = 0x060e9448;
const BODY_FRAMES = 0x060eaa2c;
const NFRAMES = 289;
const FX_BANK = 0x060e9608;
const FX_FRAMES = 0x060eb700;
const BLADE_BANK = 0x060457f0;
const BLADE_FRAMES = 0x0604501c;
const BLADE_FRAME = 0xc0;
const FIRE_BANK = 0x06046bf8;
const FIRE_FRAMES = 0x060467e0;
const MB_PAL = 0x240;
const FX_QUADS: [number, number][] = [[19, 0], [19, 1], [19, 2], [19, 3]];
const FX_FIRST_FIRE = 22;
const BODY_ANIMS = [0x60e8dfa, 0x60e8e2c, 0x60e8e3e, 0x60e8e64, 0x60e8e8e, 0x60e8ea4,
    0x60e8eca, 0x60e8ee4, 0x60e8ef2, 0x60e8f08, 0x60e8f22, 0x60e8f58,
    0x60e8f7a, 0x60e8fa8, 0x60e8fc2, 0x60e8fec, 0x60e900e, 0x60e9034,
    0x60e904e, 0x60e9060, 0x60e910c, 0x60e917e, 0x60e91a8, 0x60e923a,
    0x60e92ec, 0x60e930e, 0x60e93c0, 0x60e93e0, 0x60e940a];
const FX_ANIMS = [0x60e95d0, 0x60e95e2, 0x60e9684];
const FIRE_ANIM = 0x60e9640;

const be16 = (d: Uint8Array, o: number): number => (d[o] << 8) | d[o + 1];
const bes16 = (d: Uint8Array, o: number): number => (be16(d, o) << 16) >> 16;
const bes32 = (d: Uint8Array, o: number): number => (d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3];
const bes8 = (d: Uint8Array, o: number): number => (d[o] << 24) >> 24;

// int(round(v * 4 / 5)) (Python: round half to even)
const sx = (v: number): number => roundHalfEven((v * 4) / 5);

// gen_cen_maria.lzss: flag byte (LSB first): 1 = literal, 0 = match
// u16 (off-1) | (len-3)<<12, hash chains of 3-byte keys, newest first
export function mbLzss(data: Uint8Array): Uint8Array {
    const out: number[] = [];
    const n = data.length;
    const heads = new Map<number, number[]>();
    const key = (p: number): number => (data[p] << 16) | (data[p + 1] << 8) | data[p + 2];
    let i = 0;
    while (i < n) {
        const flagPos = out.length;
        out.push(0);
        let flags = 0;
        for (let bit = 0; bit < 8; bit++) {
            if (i >= n) {
                break;
            }
            let bestLen = 0;
            let bestOff = 0;
            if (i + 2 < n) {
                const chain = heads.get(key(i)) ?? [];
                for (let c = chain.length - 1; c >= 0; c--) {
                    const j = chain[c];
                    if (i - j > 4096) {
                        break;
                    }
                    let k = 0;
                    while (k < 18 && i + k < n && data[j + k] === data[i + k]) {
                        k++;
                    }
                    if (k > bestLen) {
                        bestLen = k;
                        bestOff = i - j;
                        if (k === 18) {
                            break;
                        }
                    }
                }
            }
            let step: number;
            if (bestLen >= 3) {
                const v = (bestOff - 1) | ((bestLen - 3) << 12);
                out.push(v & 0xff, v >> 8);
                step = bestLen;
            } else {
                flags |= 1 << bit;
                out.push(data[i]);
                step = 1;
            }
            for (let k = 0; k < step; k++) {
                if (i + k + 2 < n) {
                    const h = key(i + k);
                    const l = heads.get(h);
                    if (l === undefined) {
                        heads.set(h, [i + k]);
                    } else {
                        l.push(i + k);
                    }
                }
            }
            i += step;
        }
        out[flagPos] = flags;
    }
    return Uint8Array.from(out);
}

const ps1Colour = (c: number, j: number): number => (j === 0 ? 0 : (c & 0x7fff) || 0x8000);

// saturn_sprites._pixels' sampling: w -> round(w * 4 / 5) columns
function narrow(rows: Uint8Array[], w: number): Uint8Array[] {
    const nw = Math.max(1, roundHalfEven((w * 4) / 5));
    const out = rows.map(() => new Uint8Array(nw));
    for (let x = 0; x < nw; x++) {
        const a = Math.floor((x * w) / nw);
        const b = Math.max(a + 1, Math.ceil(((x + 1) * w) / nw));
        const c = Math.floor(((2 * x + 1) * w) / (2 * nw));
        rows.forEach((r, y) => {
            let v = r[c];
            for (let s = a; s < b && v === 0; s++) {
                v = r[s];
            }
            out[y][x] = v;
        });
    }
    return out;
}

type Image = [Uint8Array, number, number, number, number]; // packed, raw size, x, y, clut

const sameTuple = (a: number[], b: number[]): boolean => a.length === b.length && a.every((v, i) => v === b[i]);

// gen_cen_maria.composite
function composite(st: SaturnStage, parts: SaturnPart[], npals: number, cluts: number[][]): Image | null {
    const imgs: [number, number, number, number, Uint8Array[], number][] = []; // x, y, w, h, rows, pal
    for (const [attr0, cx0, cy, img] of parts) {
        const cx = -cx0;
        const attr = attr0 ^ 0x10;
        const [w, h, rows] = st.image(BODY_BANK, img);
        let a = rows.map((r) => Uint8Array.from(r));
        if (attr & 0x10) {
            a = a.map((r) => r.slice().reverse());
        }
        if (attr & 0x20) {
            a = a.slice().reverse();
        }
        imgs.push([cx - Math.floor(w / 2), cy - Math.floor(h / 2), w, h, a, (attr >> 8) % npals]);
    }
    const x0 = Math.min(...imgs.map((m) => m[0]));
    const y0 = Math.min(...imgs.map((m) => m[1]));
    const x1 = Math.max(...imgs.map((m) => m[0] + m[2]));
    const y1 = Math.max(...imgs.map((m) => m[1] + m[3]));
    const cw = x1 - x0;
    const ch = y1 - y0;
    const canvas = new Uint32Array(cw * ch); // 0 = transparent
    for (const [x, y, w, h, a, p] of imgs) {
        for (let yy = 0; yy < h; yy++) {
            for (let xx = 0; xx < w; xx++) {
                const k = a[yy][xx];
                if (k) {
                    canvas[(y - y0 + yy) * cw + x - x0 + xx] = cluts[p][k] | 0x10000;
                }
            }
        }
    }
    // Python iterates the small-int set in ascending order; max() keeps the first
    const usedPals = [...new Set(imgs.map((m) => m[5]))].sort((p, q) => p - q);
    const counts = usedPals.map((p) => imgs.filter((m) => m[5] === p)
        .reduce((s, m) => s + m[4].reduce((t, r) => t + r.reduce((u, v) => u + (v ? 1 : 0), 0), 0), 0));
    let main = usedPals[0];
    let best = counts[0];
    usedPals.forEach((p, i) => {
        if (counts[i] > best) {
            best = counts[i];
            main = p;
        }
    });
    const colours = [...new Set(canvas)].filter((v) => v).sort((p, q) => p - q);
    let pal: number[];
    let clut: number;
    if (usedPals.length === 1) {
        pal = cluts[main];
        clut = main;
    } else {
        if (colours.length <= 15) {
            pal = [0, ...colours.map((c) => c & 0xffff), ...new Array(15 - colours.length).fill(0)];
        } else {
            pal = cluts[main];
        }
        clut = cluts.findIndex((p) => sameTuple(p, pal));
        if (clut < 0) {
            cluts.push(pal);
            clut = cluts.length - 1;
        }
    }
    const lut = new Map<number, number>();
    for (const c of colours) {
        const c16 = c & 0xffff;
        const hit = pal.indexOf(c16, 1);
        if (hit >= 1) {
            lut.set(c, hit);
        } else {
            let bk = 1;
            let bd = Infinity;
            for (let k = 1; k < 16; k++) {
                const dr = (pal[k] & 31) - (c16 & 31);
                const dg = ((pal[k] >> 5) & 31) - ((c16 >> 5) & 31);
                const db = ((pal[k] >> 10) & 31) - ((c16 >> 10) & 31);
                const d = dr * dr + dg * dg + db * db;
                if (d < bd) {
                    bd = d;
                    bk = k;
                }
            }
            lut.set(c, bk);
        }
    }
    const idx: Uint8Array[] = [];
    for (let y = 0; y < ch; y++) {
        const r = new Uint8Array(cw);
        for (let x = 0; x < cw; x++) {
            const v = canvas[y * cw + x];
            r[x] = v ? lut.get(v)! : 0;
        }
        idx.push(r);
    }
    const nar = narrow(idx, cw);
    let cy0 = Infinity;
    let cy1 = -1;
    let cx0 = Infinity;
    let cx1 = -1;
    nar.forEach((r, y) => r.forEach((v, x) => {
        if (v) {
            cy0 = Math.min(cy0, y);
            cy1 = Math.max(cy1, y + 1);
            cx0 = Math.min(cx0, x);
            cx1 = Math.max(cx1, x + 1);
        }
    }));
    if (cy1 < 0) {
        return null;
    }
    const cwid = cx1 - cx0;
    const w = (cwid + 3) & ~3;
    const h = cy1 - cy0;
    if (w > 128 || h > 77) {
        throw new Error("frame too big for a g_PlayerDraw slot: " + w + "x" + h);
    }
    const raw = new Uint8Array(4 + (w * h) / 2);
    raw[0] = w;
    raw[1] = h;
    let o = 4;
    for (let y = cy0; y < cy1; y++) {
        for (let x = 0; x < w; x += 2) {
            const lo = x < cwid ? nar[y][cx0 + x] : 0;
            const hi = x + 1 < cwid ? nar[y][cx0 + x + 1] : 0;
            raw[o++] = lo | (hi << 4);
        }
    }
    const packed = mbLzss(raw);
    const px0 = roundHalfEven((x0 * 4) / 5) + cx0;
    return [packed, raw.length, px0, y0 + cy0, clut];
}

// gen_cen_maria.body
function body(st: SaturnStage): [(Image | null)[], Image[], number[], number[][], number] {
    const pals = st.palettes(BODY_BANK);
    const cluts = pals.map((p) => p.map((c, j) => ps1Colour(c, j)));
    const frames: (Image | null)[] = [];
    const overlays: Image[] = [];
    const overlayOf: number[] = [];
    for (let f = 0; f < NFRAMES; f++) {
        const parts = st.frame(BODY_FRAMES, f);
        if (!parts.length) {
            frames.push(null);
            overlayOf.push(0);
            continue;
        }
        const palOf = parts.map((p) => (p[0] >> 8) % pals.length);
        if (palOf.includes(0) && new Set(palOf).size > 1) {
            frames.push(composite(st, parts.filter((_p, i) => palOf[i] === 0), pals.length, cluts));
            const ov = composite(st, parts.filter((_p, i) => palOf[i] !== 0), pals.length, cluts);
            if (ov === null) {
                overlayOf.push(0);
            } else {
                overlays.push(ov);
                overlayOf.push(overlays.length);
            }
        } else {
            frames.push(composite(st, parts, pals.length, cluts));
            overlayOf.push(0);
        }
    }
    let maxraw = 0;
    for (const fr of [...frames, ...overlays]) {
        if (fr) {
            maxraw = Math.max(maxraw, fr[1]);
        }
    }
    return [frames, overlays, overlayOf, cluts, maxraw];
}

// gen_cen_maria.recolour: the four guardian spirits' tints of the blue orb
function recolour(pal: number[], mode: number): number[] {
    const out = [0];
    for (const c of pal.slice(1)) {
        let r = c & 31;
        let g = (c >> 5) & 31;
        let b = (c >> 10) & 31;
        const lum = Math.max(r, g, b);
        if (mode === 0) {
            [r, g, b] = [lum, Math.floor((lum * 2) / 5), Math.floor(lum / 8)];
        } else if (mode === 1) {
            [r, g, b] = [lum, lum, Math.floor(lum / 4)];
        } else if (mode === 3) {
            [r, g, b] = [Math.floor(lum / 3), lum, Math.floor(lum / 3)];
        }
        out.push((r | (g << 5) | (b << 10)) || 0x8000);
    }
    return out;
}

function fx(st: SaturnStage): [PsxSpriteSheet, PsxPart[][]] {
    const state = new SpriteState(); // the Python's module globals, shared
    const fill = (sheet: PsxSpriteSheet): PsxPart[][] => {
        const frames: PsxPart[][] = [];
        sheet.paletteBlock(st, FX_BANK);
        for (let f = 1; f < 21; f++) {
            frames.push(sheet.frame(st, FX_BANK, FX_FRAMES, f));
        }
        frames.push(sheet.frame(st, BLADE_BANK, BLADE_FRAMES, BLADE_FRAME));
        for (let f = 1; f < 14; f++) {
            frames.push(sheet.frame(st, FIRE_BANK, FIRE_FRAMES, f));
        }
        return frames;
    };
    const probe = new PsxSpriteSheet([...FX_QUADS, ...FX_QUADS], 0, null, state);
    fill(probe);
    const sheet = new PsxSpriteSheet(FX_QUADS, 0, null, state);
    sheet.prepack(st, probe.requests);
    return [sheet, fill(sheet)];
}

function anim(prg: Uint8Array, a: number): number[] {
    const out: number[] = [];
    let k = a - B;
    for (;;) {
        const d = be16(prg, k);
        out.push(d, be16(prg, k + 2));
        k += 4;
        if (d === 0 || d === 0xff) {
            return out;
        }
    }
}

// the Saturn files: STAGE_14.PRG, STAGE_14.CHR, 0.BIN
export async function cenMaria(readSaturn: (path: string) => Promise<Uint8Array>): Promise<Map<string, MbArray>> {
    const prg = await readSaturn("STAGE_14.PRG");
    const st = new SaturnStage(prg, await readSaturn("STAGE_14.CHR"), await readSaturn("0.BIN"));
    const [frames, overlays, overlayOf, cluts] = body(st);
    const images = [...frames, ...overlays];
    const [sheet, fxf] = fx(st);
    const fxBase = MB_PAL + cluts.length;
    const fxCluts = sheet.cluts.slice();
    const spirits = [0, 1, 2, 3].map((m) => recolour(fxCluts[0], m));
    if (fxBase + fxCluts.length + 4 > 0x2c0) {
        throw new Error("palettes");
    }
    const nq = sheet.usedQuadrants();
    const out = new Map<string, MbArray>();
    const put = (name: string, size: number, values: number[]): void => {
        out.set(name, { size, values });
    };

    const lz: number[] = [];
    const offs: number[] = [];
    const sizes: number[] = [];
    for (const fr of images) {
        if (fr === null) {
            offs.push(0);
            sizes.push(0);
            continue;
        }
        offs.push(lz.length);
        sizes.push(fr[1]);
        for (const b of fr[0]) {
            lz.push(b);
        }
    }
    put("g_MbLz", 1, lz);
    put("g_MbLzOff", 4, offs);
    put("g_MbRawSize", 2, sizes);
    put("g_MbFrameDesc", 2, images.flatMap((fr, f) => fr === null ? [0x8000 | f, 0, 0, 0] : [0x8000 | f, fr[2], fr[3], fr[4]]));
    put("g_MbOverlayOf", 2, overlayOf.map((k) => (k ? NFRAMES + k - 1 : 0)));
    put("g_MbPals", 2, cluts.flat());
    const pix: number[] = [];
    for (let q = 0; q < nq; q++) {
        for (const b of sheet.quadrantPixels(q)) {
            pix.push(b);
        }
    }
    put("g_MbFxPix", 1, pix);
    const quad: number[] = [];
    for (let q = 0; q < nq; q++) {
        const [tpage, qd] = sheet.quadrants[q];
        quad.push((tpage & 15) * 64 + (qd & 1) * 32, (tpage >> 4) * 256 + (qd >> 1) * 128);
    }
    put("g_MbFxQuad", 2, quad);
    fxf.forEach((parts, n) => {
        const vals = [parts.length];
        for (const p of parts) {
            vals.push(p.flags, p.x, p.y, p.width, p.height, p.clut + fxBase, p.tileset, p.left, p.top, p.right, p.bottom);
        }
        vals.push(0);
        if (vals.length & 1) {
            vals.push(0);
        }
        put("g_MbFx" + (n + 1), 2, vals);
    });
    put("g_MbFxPals", 2, [...fxCluts, ...spirits].flat());
    const hex = (a: number): string => (a & 0xffff).toString(16).toUpperCase();
    for (const a of [...BODY_ANIMS, ...FX_ANIMS]) {
        put("g_MbAnim" + hex(a), 2, anim(prg, a));
    }
    const fire = anim(prg, FIRE_ANIM);
    put("g_MbAnim" + hex(FIRE_ANIM), 2, fire.map((x, i) =>
        i % 2 === 0 || fire[i - 1] === 0 || fire[i - 1] === 0xff ? x : x + FX_FIRST_FIRE - 1));

    const idx = Array.from(prg.subarray(0x060e8cd8 - B, 0x060e8cd8 - B + NFRAMES));
    put("g_MbHitboxIdx", 1, idx);
    const hb: number[] = [];
    for (let i = 0; i <= Math.max(...idx); i++) {
        const o = 0x060e8b50 - B + 4 * i;
        hb.push(sx(bes8(prg, o)), bes8(prg, o + 1), sx(prg[o + 2]), prg[o + 3]);
    }
    put("g_MbHitbox", 1, hb);
    const kick: number[] = [];
    for (let i = 0; i < 5; i++) {
        const o = 0x060e95a8 - B + 8 * i;
        kick.push(sx(bes16(prg, o)), bes16(prg, o + 2), sx(be16(prg, o + 4)), be16(prg, o + 6));
    }
    put("g_MbKick", 2, kick);
    const s2 = [0, 1, 2, 3].map((i) => bes16(prg, 0x060e8b24 - B + 2 * i));
    put("g_MbSensors2", 2, [sx(s2[0]), s2[1], sx(s2[2]), s2[3]]);
    const s3 = [0, 1, 2, 3, 4, 5, 6, 7].map((i) => bes16(prg, 0x060e8b2c - B + 2 * i));
    put("g_MbSensors3", 2, s3.map((v, i) => (i % 2 === 0 ? sx(v) : v)));
    put("g_MbBladeX", 2, [0, 1, 2].map((i) => sx(bes16(prg, 0x060e8b44 - B + 4 * i))));
    put("g_MbBladeV", 4, [0, 1, 2].flatMap((i) => {
        const o = 0x060e961c - B + 8 * i;
        return [sx(bes32(prg, o)), bes32(prg, o + 4)];
    }));
    put("g_MbRays", 4, Array.from({ length: 32 }, (_v, i) => bes32(prg, 0x060e945c - B + 4 * i)));
    put("g_MbSparkSpeed", 4, Array.from({ length: 8 }, (_v, i) => bes32(prg, 0x060e955c - B + 4 * i)));
    return out;
}
