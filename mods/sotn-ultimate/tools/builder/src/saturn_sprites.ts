// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn stage sprites -> PSX sprite banks: a byte-exact port of
// mods/sotn-ultimate/tools/saturn_sprites.py (see its header for both formats). A Saturn
// stage PRG's entity entries point to {images, palettes, 0xFFFF, 0xFFFF}
// sprite banks; the images' 4bpp pixels follow one another in the stage's
// .CHR. PsxSpriteSheet narrows them to PSX proportions (4 of every 5
// columns), packs them into 128x128 quadrants of the stage sprite pages and
// builds the PSX frames' parts.
//
// The Python keeps its tuning tables as module globals (BLEED_BANKS,
// STP_BANKS, TRIM_BANKS, ...) that every sheet shares and the stage scripts
// mutate; here they are a SpriteState, shared by the sheets of one stage
// (pass the same one to all of them; a fresh one per stage).
import { roundHalfEven } from "./dsp.ts";

// u8 width/2, u8 height/2 per image -> (w, h, CHR offset)
type ImageRec = [number, number, number];
// one Saturn frame part: u16 attr, s16 x, s16 y (the image's centre), s16 image
export type SaturnPart = [number, number, number, number];
// the Python's (bank, image, flip, narrowed width or None)
export type PlaceKey = [number, number, number, number | null];
// (quadrant index, u, v, w, h)
export type Placement = [number, number, number, number, number];
export type Rows = number[][];

// a PSX sprite part, as sprite_banks.json takes it (keys in this order)
export interface PsxPart {
    flags: number;
    x: number;
    y: number;
    width: number;
    height: number;
    clut: number;
    tileset: number;
    left: number;
    top: number;
    right: number;
    bottom: number;
}

const be16 = (d: Uint8Array, o: number): number => (d[o] << 8) | d[o + 1];
const bes16 = (d: Uint8Array, o: number): number => (be16(d, o) << 16) >> 16;
const be32 = (d: Uint8Array, o: number): number => ((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) >>> 0;

// Python list indexing (negative counts from the end)
function at<T>(a: T[], i: number): T {
    const v = a[i < 0 ? a.length + i : i];
    if (v === undefined) {
        throw new Error("index " + i + " out of range");
    }
    return v;
}

// Python's a % n (the sign of n)
const pyMod = (a: number, n: number): number => ((a % n) + n) % n;

const k3 = (bank: number, img: number, flip: number): string => bank + "," + img + "," + flip;
const k4 = (k: PlaceKey): string => k[0] + "," + k[1] + "," + k[2] + "," + k[3];

export class SaturnStage {
    prg: Uint8Array;
    chr: Uint8Array;
    zero: Uint8Array;
    base: number;
    zbase: number;
    // bank address -> (images, palettes address), in the order first referenced
    banks: Map<number, [ImageRec[], number]>;
    private images: Map<string, [number, number, Rows]>;

    // prg/chr: STAGE_xx.PRG / .CHR, zero: 0.BIN (loaded at 0x06004080)
    constructor(prg: Uint8Array, chr: Uint8Array, zero: Uint8Array, base: number = 0x060dc000) {
        this.prg = prg;
        this.chr = chr;
        this.zero = zero;
        this.base = base;
        this.zbase = 0x06004080;
        this.images = new Map();
        this.banks = this.walkBanks();
    }

    res(a: number): [Uint8Array, number] | null {
        if (this.base <= a && a < this.base + this.prg.length) {
            return [this.prg, a - this.base];
        }
        if (this.zbase <= a && a < this.zbase + this.zero.length) {
            return [this.zero, a - this.zbase];
        }
        return null;
    }

    private at(a: number): [Uint8Array, number] {
        const r = this.res(a);
        if (r === null) {
            throw new Error("address 0x" + a.toString(16) + " outside the PRG and 0.BIN");
        }
        return r;
    }

    u32(a: number): number {
        const [d, o] = this.at(a);
        return be32(d, o);
    }

    private walkBanks(): Map<number, [ImageRec[], number]> {
        const tab = this.u32(this.base + 4);
        let cursor = 0;
        const seen = new Set<number>();
        const banks = new Map<number, [ImageRec[], number]>();
        for (let i = 0; i < 256; i++) {
            const a = this.u32(tab + i * 4);
            const r = this.res(a);
            if (r === null) {
                break;
            }
            const [d, o] = r;
            const ia = be32(d, o);
            const pa = be32(d, o + 4);
            const al = be16(d, o + 8);
            const fl = be16(d, o + 10);
            if (al !== 0xffff || fl !== 0xffff || this.res(ia) === null || this.res(pa) === null ||
                    seen.has(ia)) {
                continue;
            }
            seen.add(ia);
            const [idd, io] = this.at(ia);
            const recs: ImageRec[] = [];
            for (let k = 0; k < 512; k++) {
                const w = idd[io + k * 4];
                const h = idd[io + k * 4 + 1];
                if (w === 0 || h === 0) {
                    break;
                }
                recs.push([w * 2, h * 2, cursor]);
                cursor += Math.ceil((w * h * 2) / 0x20) * 0x20;
            }
            banks.set(a, [recs, pa]);
        }
        return banks;
    }

    private bank(bank: number): [ImageRec[], number] {
        const b = this.banks.get(bank);
        if (b === undefined) {
            throw new Error("no sprite bank 0x" + bank.toString(16));
        }
        return b;
    }

    // the bank's 16-colour RGB555 palettes (count & 0xFF of them, at least 1)
    palettes(bank: number): number[][] {
        const [d, o] = this.at(this.bank(bank)[1]);
        const n = be16(d, o) & 0xff;
        const out: number[][] = [];
        for (let k = 0; k < Math.max(n, 1); k++) {
            const pal: number[] = [];
            for (let j = 0; j < 16; j++) {
                pal.push(be16(d, o + 2 + 32 * k + 2 * j));
            }
            out.push(pal);
        }
        return out;
    }

    // (w, h, rows of colour indices); the rows are shared: don't modify them
    image(bank: number, img: number): [number, number, Rows] {
        const key = bank + "," + img;
        const hit = this.images.get(key);
        if (hit !== undefined) {
            return hit;
        }
        const [w, h, off] = at(this.bank(bank)[0], img);
        const data = this.chr.subarray(off, off + ((w * h) >> 1));
        const px: number[] = [];
        for (const b of data) {
            px.push(b >> 4, b & 15);
        }
        const rows: Rows = [];
        for (let y = 0; y < h; y++) {
            rows.push(px.slice(y * w, (y + 1) * w));
        }
        const r: [number, number, Rows] = [w, h, rows];
        this.images.set(key, r);
        return r;
    }

    // frame i of a frame table: its parts {attr, x, y, image}
    frame(frames: number, i: number): SaturnPart[] {
        const [d, o] = this.at(this.u32(frames + 4 * i));
        const n = be16(d, o) >> 8;
        const out: SaturnPart[] = [];
        for (let k = 0; k < n; k++) {
            const p = o + 4 + 8 * k;
            out.push([be16(d, p), bes16(d, p + 2), bes16(d, p + 4), bes16(d, p + 6)]);
        }
        return out;
    }
}

export const QUAD_EDGE = 127; // usable columns/rows of a 128x128 quadrant (see alloc)

// saturn_sprites.py's module globals (see their comments there)
export class SpriteState {
    // banks of rotating entities: border columns repeating the edges parts meet
    bleedBanks: Set<number>;
    // (bank, image, flip) -> bits, 1 left, 2 right, 4 top, 8 bottom
    bleed: Map<string, number>;
    // (bank, image, flip) -> the widest narrowed width any frame asks for
    nwMax: Map<string, number>;
    bleedRows: boolean;
    // banks whose palette blocks set the STP bit on every colour
    stpBanks: Set<number>;
    // banks packed trimmed to their content -> images kept whole
    trimBanks: Map<number, Set<number>>;
    // placed key -> (dx, dy) of the stored content in the full image
    trim: Map<string, [number, number]>;

    constructor() {
        this.bleedBanks = new Set();
        this.bleed = new Map();
        this.nwMax = new Map();
        this.bleedRows = false;
        this.stpBanks = new Set();
        this.trimBanks = new Map();
        this.trim = new Map();
    }

    trimmed(bank: number, img: number): boolean {
        const keep = this.trimBanks.get(bank);
        return keep !== undefined && !keep.has(img);
    }
}

// rows -> (rows cut to their content's bounding box, dx, dy)
export function trimRows(rows: Rows): [Rows, number, number] {
    const ys: number[] = [];
    rows.forEach((r, y) => {
        if (r.some((v) => v !== 0)) {
            ys.push(y);
        }
    });
    if (ys.length === 0) {
        return [[rows[0].slice(0, 1)], 0, 0];
    }
    const xs: number[] = [];
    for (let x = 0; x < rows[0].length; x++) {
        if (rows.some((r) => r[x])) {
            xs.push(x);
        }
    }
    const x0 = xs[0];
    const x1 = xs[xs.length - 1] + 1;
    const y0 = ys[0];
    const y1 = ys[ys.length - 1] + 1;
    return [rows.slice(y0, y1).map((r) => r.slice(x0, x1)), x0, y0];
}

function sameClut(a: number[], b: number[]): boolean {
    return a.length === b.length && a.every((v, i) => v === b[i]);
}

// Packs converted sprites into 128x128 quadrants and builds PSX frames.
export class PsxSpriteSheet {
    quadrants: [number, number][]; // [(tpage, q)], filled in order
    pages: Uint8Array[]; // 128x128 colour indices per quadrant
    skylines: Map<number, number[]>; // quadrant -> filled height per column
    requests: PlaceKey[]; // placed keys, in request order
    clutBase: number;
    // converted palettes, in clut order (may be shared by several sheets)
    cluts: number[][];
    placed: Map<string, Placement>; // key -> (quad, u, v, w, h)
    blocks: Map<number, number>; // bank -> first clut of its palettes
    state: SpriteState;

    constructor(quadrants: [number, number][], clutBase: number, cluts: number[][] | null, state: SpriteState) {
        this.quadrants = quadrants.map((q) => [q[0], q[1]] as [number, number]);
        this.pages = this.quadrants.map(() => new Uint8Array(128 * 128));
        this.skylines = new Map();
        this.requests = [];
        this.clutBase = clutBase;
        this.cluts = cluts === null ? [] : cluts;
        this.placed = new Map();
        this.blocks = new Map();
        this.state = state;
    }

    private clut(colours: number[], semi: boolean = false): number {
        // bit 15 (STP) marks the texels a semi-transparent primitive blends;
        // opaque sprites only need it on black (0 would be transparent)
        const pal = colours.map((c, j) => (j === 0 || !c ? 0 : semi ? (c | 0x8000) : (c & 0x7fff) || 0x8000));
        let i = this.cluts.findIndex((p) => sameClut(p, pal));
        if (i < 0) {
            this.cluts.push(pal);
            i = this.cluts.length - 1;
        }
        return this.clutBase + i;
    }

    // narrowed to PSX proportions (to nw columns, sampled at their centres; a
    // column sampled transparent takes the content of the source columns it
    // covers), stored flipped (flip 2 left-right, 1 top-bottom)
    static pixels(stage: SaturnStage, bank: number, img: number, flip: number, nw: number | null = null): Rows {
        const [w, , src] = stage.image(bank, img);
        const n = nw === null ? Math.max(1, Math.floor((w * 4) / 5)) : nw;
        const spans: [number, number, number][] = [];
        for (let x = 0; x < n; x++) {
            const a = Math.floor((x * w) / n);
            spans.push([a, Math.max(a + 1, Math.ceil(((x + 1) * w) / n)), Math.floor(((2 * x + 1) * w) / (2 * n))]);
        }
        let rows: Rows = src.map((r) => spans.map(([a, b, c]) => {
            if (r[c]) {
                return r[c];
            }
            for (let k = a; k < Math.min(b, r.length); k++) {
                if (r[k]) {
                    return r[k];
                }
            }
            return 0;
        }));
        if (flip & 2) {
            rows = rows.map((r) => r.slice().reverse());
        }
        if (flip & 1) {
            rows = rows.slice().reverse();
        }
        return rows;
    }

    // (first usable column, columns, rows) of a quadrant: a tpage's right /
    // lower quadrants keep their last column / row free (u/v + 128 wraps),
    // its left ones their column 0 (a right-facing part reads from u - 1)
    private limits(q: number): [number, number, number] {
        const quad = this.quadrants[q][1];
        return [quad & 1 ? 0 : 1, quad & 1 ? QUAD_EDGE : 128, quad & 2 ? QUAD_EDGE : 128];
    }

    // skyline packing: where the image's top would be lowest, in the first
    // quadrant it fits
    private alloc(w: number, h: number): [number, number, number] {
        for (let q = 0; q < this.pages.length; q++) {
            const [x0, lw, lh] = this.limits(q);
            let sky = this.skylines.get(q);
            if (sky === undefined) {
                sky = new Array(128).fill(0);
                this.skylines.set(q, sky);
            }
            let best: [number, number] | null = null;
            for (let x = x0; x < lw - w + 1; x++) {
                let y = sky[x];
                for (let i = x + 1; i < x + w; i++) {
                    y = Math.max(y, sky[i]);
                }
                if (y + h <= lh && (best === null || y < best[0])) {
                    best = [y, x];
                }
            }
            if (best !== null) {
                const [y, x] = best;
                for (let i = x; i < x + w; i++) {
                    sky[i] = y + h;
                }
                return [q, x, y];
            }
        }
        throw new Error("sprite quadrants full");
    }

    private place(stage: SaturnStage, bank: number, img: number, flip: number = 0, nw: number | null = null): Placement {
        const st = this.state;
        if (nw !== null) {
            nw = Math.max(nw, st.nwMax.get(k3(bank, img, flip)) ?? nw);
        }
        const key: PlaceKey = [bank, img, flip, nw];
        const ks = k4(key);
        const bleed = st.bleed.get(k3(bank, img, flip)) ?? 0;
        const hit = this.placed.get(ks);
        if (hit !== undefined) {
            return hit;
        }
        let rows = PsxSpriteSheet.pixels(stage, bank, img, flip, nw);
        if (st.trimmed(bank, img)) {
            const [r, dx, dy] = trimRows(rows);
            rows = r;
            st.trim.set(ks, [dx, dy]);
        }
        const w = rows[0].length;
        const h = rows.length;
        // rows above and below (bleedRows) and a border column on each side
        // for the rotating banks' images
        const vb = st.bleedRows && st.bleedBanks.has(bank) ? 2 : 0;
        const hb = st.bleedBanks.has(bank) ? 1 : 0;
        let [q, x, y] = this.alloc(w + 2 * hb, h + 2 * vb);
        x += hb;
        y += vb;
        const page = this.pages[q];
        const put = (yy: number, xx: number, v: number): void => {
            // Python list indexing: -1 is the last column / row
            page[(yy < 0 ? yy + 128 : yy) * 128 + (xx < 0 ? xx + 128 : xx)] = v;
        };
        if (vb && bleed & 4) {
            rows[0].forEach((v, i) => put(y - 1, x + i, v));
        }
        if (vb && bleed & 8) {
            rows[h - 1].forEach((v, i) => put(y + h, x + i, v));
        }
        rows.forEach((r, yy) => {
            r.forEach((v, i) => put(y + yy, x + i, v));
            // the edge column copied into the part's border (bleed)
            if (bleed & 1) {
                put(y + yy, x - 1, r[0]);
            }
            if (bleed & 2) {
                put(y + yy, x + w, r[w - 1]);
            }
        });
        const p: Placement = [q, x, y, w, h];
        this.placed.set(ks, p);
        this.requests.push(key);
        return p;
    }

    // Places the given images (keys from another sheet's requests) tallest
    // first, which packs far tighter than placing them as frames ask. Throws
    // "sprite quadrants full" (the Python's AssertionError) when they don't fit.
    prepack(stage: SaturnStage, keys: PlaceKey[]): void {
        const sized: [number, number, PlaceKey][] = [];
        for (const key of keys) {
            let rows = PsxSpriteSheet.pixels(stage, key[0], key[1], key[2], key[3]);
            if (this.state.trimmed(key[0], key[1])) {
                rows = trimRows(rows)[0];
            }
            sized.push([rows.length, rows[0].length, key]);
        }
        // a stable sort, as Python's sorted()
        sized.sort((a, b) => b[0] - a[0] || b[1] - a[1]);
        for (const [, , key] of sized) {
            this.place(stage, key[0], key[1], key[2], key[3]);
        }
    }

    // Reserves all of a bank's palettes, in the Saturn's order, as consecutive
    // cluts (its parts use base + the Saturn palette index). Returns the base.
    paletteBlock(stage: SaturnStage, bank: number): number {
        const b = this.blocks.get(bank);
        if (b !== undefined) {
            return b;
        }
        this.blocks.set(bank, this.clutBase + this.cluts.length);
        for (const colours of stage.palettes(bank)) {
            const stp = this.state.stpBanks.has(bank) ? 0x8000 : 0;
            this.cluts.push(colours.map((c, j) => (j === 0 || !c ? 0 : ((c & 0x7fff) | stp) || 0x8000)));
        }
        return this.blocks.get(bank)!;
    }

    // Frame `index` of frame table `frames` of a bank, as PSX parts (the first
    // on top). palShift is added to the parts' Saturn palette numbers.
    frame(stage: SaturnStage, bank: number, frames: number, index: number, palShift: number = 0): PsxPart[] {
        const st = this.state;
        const pals = stage.palettes(bank);
        const sat = stage.frame(frames, index);
        // narrowed from their edges, not centre and width, so that parts that
        // touch on the Saturn share a PSX column edge too
        const spans: [number, number, number, number][] = [];
        for (const [, cx, cy, img] of sat) {
            const [sw, sh] = stage.image(bank, img);
            const top = cy - Math.floor(sh / 2);
            spans.push([roundHalfEven(((cx - sw / 2) * 4) / 5), roundHalfEven(((cx + sw / 2) * 4) / 5), top, top + sh]);
        }
        const parts: PsxPart[] = [];
        sat.forEach(([attr, cx, cy, img], i) => {
            const flip = (attr & 0x10 ? 2 : 0) | (attr & 0x20 ? 1 : 0);
            const [left, right, top, bottom] = spans[i];
            const nw = Math.max(1, right - left);
            const kf = k3(bank, img, flip);
            st.nwMax.set(kf, Math.max(nw, st.nwMax.get(kf) ?? 0));
            if (st.bleedBanks.has(bank)) {
                // the edges another part of the frame meets
                let bleed = 0;
                spans.forEach(([l2, r2, t2, b2], j) => {
                    if (j !== i && t2 < bottom && top < b2) {
                        if (Math.abs(r2 - left) <= 1 && l2 < left) {
                            bleed |= 1;
                        }
                        if (Math.abs(l2 - right) <= 1 && r2 > right) {
                            bleed |= 2;
                        }
                    }
                    if (j !== i && l2 < right && left < r2) {
                        if (Math.abs(b2 - top) <= 1 && t2 < top) {
                            bleed |= 4;
                        }
                        if (Math.abs(t2 - bottom) <= 1 && b2 > bottom) {
                            bleed |= 8;
                        }
                    }
                });
                st.bleed.set(kf, (st.bleed.get(kf) ?? 0) | bleed);
            }
            const [q, u, v, w, h] = this.place(stage, bank, img, flip, nw);
            const [tpage, quad] = this.quadrants[q];
            // a trimmed image: its content's place in the image
            const nwKey = Math.max(nw, st.nwMax.get(kf) ?? nw);
            const [tdx, tdy] = st.trim.get(k4([bank, img, flip, nwKey])) ?? [0, 0];
            const sh = stage.image(bank, img)[1];
            const hb = st.bleedBanks.has(bank) ? 1 : 0;
            const vb = st.bleedRows && st.bleedBanks.has(bank) ? 1 : 0;
            const pal = pyMod((attr >> 8) + palShift, pals.length);
            const block = this.blocks.get(bank);
            parts.push({
                flags: 0,
                x: left - hb + tdx,
                y: (tdx !== 0 || tdy !== 0 || st.trimBanks.has(bank) ? cy - Math.floor(sh / 2) + tdy
                    : cy - Math.floor(h / 2)) - vb,
                width: w + 2 * hb,
                height: h + 2 * vb,
                clut: block !== undefined ? block + pal : this.clut(pals[pal]),
                tileset: (tpage << 2) | quad,
                left: u - hb,
                top: v - vb,
                right: u + w + hb,
                bottom: v + h + vb,
            });
        });
        // the Saturn draws the last part on top, the PSX the first
        return parts.reverse();
    }

    // A part for one image whose top-left corner sits at (dx, dy), with the
    // given 16 colours (STP set); flip as frame()'s (stored flipped).
    imagePart(stage: SaturnStage, bank: number, img: number, dx: number, dy: number, colours: number[],
              flip: number = 0): PsxPart {
        const [q, u, v, w, h] = this.place(stage, bank, img, flip);
        const sw = stage.image(bank, img)[0];
        const [tpage, quad] = this.quadrants[q];
        return {
            flags: 0,
            x: roundHalfEven(((dx + sw / 2) * 4) / 5 - w / 2),
            y: dy,
            width: w,
            height: h,
            clut: this.clut(colours, true),
            tileset: (tpage << 2) | quad,
            left: u,
            top: v,
            right: u + w,
            bottom: v + h,
        };
    }

    // 4bpp, 128x128, as LoadTPage takes it (low nibble first)
    quadrantPixels(q: number): Uint8Array {
        const page = this.pages[q];
        const out = new Uint8Array(128 * 64);
        for (let i = 0; i < out.length; i++) {
            out[i] = page[2 * i] | (page[2 * i + 1] << 4);
        }
        return out;
    }

    usedQuadrants(): number {
        let m = -1;
        for (const p of this.placed.values()) {
            m = Math.max(m, p[0]);
        }
        return m + 1;
    }
}
