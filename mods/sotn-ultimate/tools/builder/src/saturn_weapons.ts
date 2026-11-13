// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn's new weapons 60-63 and weapon 21's two new foods: a byte-exact
// port of mods/sotn-ultimate/tools/port_saturn_weapon.py (mods/sotn-ultimate/src/items/weapon/mod/w_0NN_data.h and
// the art page assets/weapon/f_0NN.png that png2bin puts in each WEAPON2.BIN
// slot, see build_saturn_weapon.sh / build_saturn_weapons.py) and of
// mods/sotn-ultimate/tools/port_saturn_food.py (the right half of assets/weapon/f_021.png,
// compiled into WEAPON0.BIN and, by build_weapon1.sh, WEAPON1.BIN).
//
// Inputs: the Saturn disc only (WEAPON0.PRG/.CHR; 0.BIN, STAGE_00.PRG/.CHR).
// Outputs:
// - ranges: BIN/WEAPON2.BIN's art (0x4000 bytes at the start of each of its 8
//   0x7000-byte slots: weapon w, hand h at slot (w - 60) * 2 + h; both hands
//   get the same page), BIN/WEAPON0.BIN and BIN/WEAPON1.BIN's two food boxes
//   (slot 21's art, 16 rows of 16 bytes);
// - arrays: per header (mods/sotn-ultimate/src/items/weapon/mod/w_0NN_data.h) the arrays of plain
//   numbers it defines, as mods/sotn-ultimate/tools/builder/node/c_arrays.ts parseCArrays
//   finds them (s16 frames, the u16 cluts, u16 frame lists, s8 hitboxes);
// - headers: the whole header text (what the generator writes), whose other
//   definitions (pointer tables, WeaponAnimation records, the
//   W_0NN_IMG<n>_U/V/W/H defines) are not plain numbers.
import { PsxSpriteSheet, SaturnStage, SpriteState } from "./saturn_sprites.ts";
import type { PlaceKey, Placement, PsxPart } from "./saturn_sprites.ts";

export type DiscRead = (path: string) => Promise<Uint8Array>;

export interface Range {
    offset: number;
    bytes: Uint8Array;
}

export interface NumArray {
    size: number; // bytes per element
    values: number[];
}

export interface SaturnWeaponsResult {
    ranges: Map<string, Range[]>; // disc path -> ranges
    arrays: Map<string, Map<string, NumArray>>; // header path -> name -> numbers
    headers: Map<string, string>; // header path -> its whole text
    art: Map<number, Uint8Array>; // weapon id -> its 0x4000-byte page (png2bin)
}

const PRG_SLOT = 0x3000;
const CHR_SLOT = 0x4000;
const BASE = 0x060d6000; // hand 0
const N_WEAPON_PAL = 24;
export const WEAPON_SLOT = 0x7000;
export const FIRST_WEAPON2 = 60;
export const LAST_WEAPON2 = 63;

// Saturn Alucard animation ids -> PSX ones
const FRAME_START = new Map([[0x40, 0x48], [0x60, 0x68], [0x77, 0x7d], [0x7e, 0x84], [0xe0, 247]]);

// The frames of the PSX animsets the weapons borrow their layout from
// (assets/weapon/<front_ref>.animset.json) that have a part with flag 0x100
// (drawn in front of Alucard); node/test_saturn_weapons.ts checks them
// against those files
const W051_FRONT = [1, 2, 3, 4, 5, 14, 15, 16, 17, 18, 23, 24, 25, 33, 34, 35, 36, 37, 46, 47, 48, 49, 50, 55, 56, 57];
const W032_FRONT = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 23, 24, 25, 26, 27, 28, 31, 32,
    36, 37, 38, 39, 40, 41, 45, 46, 47, 50, 51, 54];
const W008_FRONT = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 55, 56, 57, 58, 59, 60, 61];

interface WeaponCfg {
    anims: [number, number][]; // (Saturn address, entries) of each WeaponAnimation table
    frontRef: string;
    front: number[];
    semiPals: number[];
    cluts: number;
    primImages: number[];
}

// port_saturn_weapon.py WEAPONS
export const WEAPONS = new Map<number, WeaponCfg>([
    [63, { anims: [[0x060d6580, 1]], frontRef: "w_051_1", front: W051_FRONT, semiPals: [1], cluts: 1, primImages: [] }],
    [62, { anims: [[0x060d6a80, 2]], frontRef: "w_032_1", front: W032_FRONT, semiPals: [], cluts: 1, primImages: [3] }],
    [61, { anims: [], frontRef: "w_008_1", front: W008_FRONT, semiPals: [], cluts: 1,
        primImages: [3, 10, 11, 12, 13, 14, 15] }],
    [60, { anims: [[0x060d6a50, 3]], frontRef: "w_051_1", front: W051_FRONT, semiPals: [1, 2], cluts: 1, primImages: [] }],
]);

const be16 = (d: Uint8Array, o: number): number => (d[o] << 8) | d[o + 1];
const bes16 = (d: Uint8Array, o: number): number => (be16(d, o) << 16) >> 16;
const be32 = (d: Uint8Array, o: number): number => ((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) >>> 0;
const s8 = (v: number): number => (v << 24) >> 24;

function check(ok: boolean, what: string): void {
    if (!ok) {
        throw new Error("saturn weapons: " + what);
    }
}

type Rows = number[][];
type SaturnPart = [number, number, number, number];
// WeaponAnimation: frames, hitboxes, palette, sound, frameStart, soundFrame, unused
type Anim = [number, number, number, number, number, number, number];

// One Saturn weapon slot, with SaturnStage's image / palettes / frame (what
// PsxSpriteSheet calls)
class SaturnWeapon {
    base: number;
    prg: Uint8Array;
    chr: Uint8Array;
    banks: Map<number, [[number, number, number][], number]>;
    bankList: [number, number, number][]; // (bank, frame table, frames)
    private images: Map<string, [number, number, Rows]>;

    constructor(prgFile: Uint8Array, chrFile: Uint8Array, wid: number, hand: number = 0) {
        this.base = BASE + hand * 0x3000;
        this.prg = prgFile.subarray(wid * PRG_SLOT, (wid + 1) * PRG_SLOT);
        this.chr = chrFile.subarray(wid * CHR_SLOT, (wid + 1) * CHR_SLOT);
        this.banks = new Map();
        this.bankList = [];
        this.images = new Map();
        let cursor = 0;
        const tab = this.u32(this.base + 0x40);
        for (let i = 0; i < 16; i++) {
            const bank = this.u32(tab + 4 * i);
            if (!this.ok(bank)) {
                break;
            }
            const o = bank - this.base;
            const ia = be32(this.prg, o);
            const pa = be32(this.prg, o + 4);
            check(be16(this.prg, o + 8) === 0xffff && be16(this.prg, o + 10) === 0xffff, "bad sprite bank");
            const recs: [number, number, number][] = [];
            for (let k = 0; k < 512; k++) {
                const w = this.prg[ia - this.base + 4 * k];
                const h = this.prg[ia - this.base + 4 * k + 1];
                if (w === 0 || h === 0) {
                    break;
                }
                recs.push([w * 2, h * 2, cursor]);
                cursor += Math.ceil((w * h * 2) / 0x20) * 0x20;
            }
            check(cursor <= CHR_SLOT, "CHR slot overflow");
            this.banks.set(bank, [recs, pa]);
            // the frame table: the pointers to frames right before the bank
            let a = bank;
            while (this.ok(this.u32(a - 4)) && this.isFrame(this.u32(a - 4))) {
                a -= 4;
            }
            const n = (bank - a) / 4;
            check(n > 0, "no frame table before a bank");
            this.bankList.push([bank, a, n]);
        }
    }

    ok(a: number): boolean {
        return this.base <= a && a < this.base + PRG_SLOT;
    }

    u32(a: number): number {
        return be32(this.prg, a - this.base);
    }

    u16(a: number): number {
        return be16(this.prg, a - this.base);
    }

    private isFrame(p: number): boolean {
        const n = this.u16(p);
        return (n & 0xff) === 0 && (n >> 8) <= 24;
    }

    palettes(bank: number): number[][] {
        const pa = this.banks.get(bank)![1];
        const n = this.u16(pa) & 0xff;
        const out: number[][] = [];
        for (let k = 0; k < Math.max(n, 1); k++) {
            const pal: number[] = [];
            for (let j = 0; j < 16; j++) {
                pal.push(be16(this.prg, pa - this.base + 2 + 32 * k + 2 * j));
            }
            out.push(pal);
        }
        return out;
    }

    image(bank: number, img: number): [number, number, Rows] {
        const key = bank + "," + img;
        const hit = this.images.get(key);
        if (hit !== undefined) {
            return hit;
        }
        const [w, h, off] = this.banks.get(bank)![0][img];
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

    frame(frames: number, i: number): SaturnPart[] {
        const p = this.u32(frames + 4 * i);
        const n = this.u16(p) >> 8;
        const out: SaturnPart[] = [];
        for (let k = 0; k < n; k++) {
            const o = p - this.base + 4 + 8 * k;
            out.push([be16(this.prg, o), bes16(this.prg, o + 2), bes16(this.prg, o + 4), bes16(this.prg, o + 6)]);
        }
        return out;
    }

    anim(a: number): Anim {
        const o = a - this.base;
        return [be32(this.prg, o), be32(this.prg, o + 4), be16(this.prg, o + 8), be16(this.prg, o + 10),
            this.prg[o + 12], this.prg[o + 13], bes16(this.prg, o + 14)];
    }

    // the u16 lists of a frames table (pointers until one isn't, or an
    // address in stop)
    frameLists(table: number, stop: Set<number>): number[][] {
        const lists: number[][] = [];
        let a = table;
        while (this.ok(this.u32(a)) && !stop.has(a)) {
            let p = this.u32(a);
            const seq: number[] = [];
            for (;;) {
                const v = this.u16(p);
                seq.push(v);
                p += 2;
                if (v === 0xffff) {
                    break;
                }
                check(seq.length < 256, "frame list too long");
            }
            lists.push(seq);
            a += 4;
        }
        return lists;
    }
}

// C-style (toward zero)
const trunc45 = (v: number): number => Math.trunc((v * 4) / 5);

function psxColour(c: number, semi: boolean): number {
    c &= 0x7fff;
    if (semi) {
        return c ? c | 0x8000 : 0;
    }
    return c || 0x8000;
}

const hex4 = (v: number): string => "0x" + v.toString(16).toUpperCase().padStart(4, "0");
const pad3 = (n: number): string => String(n).padStart(3, "0");

function parseKey(k: string): PlaceKey {
    const f = k.split(",");
    return [Number(f[0]), Number(f[1]), Number(f[2]), f[3] === "null" ? null : Number(f[3])];
}

function sameList(a: number[], b: number[]): boolean {
    return a.length === b.length && a.every((v, i) => v === b[i]);
}

export interface WeaponPort {
    art: Uint8Array; // the 0x4000-byte page (png2bin of f_0NN.png)
    header: string; // mods/sotn-ultimate/src/items/weapon/mod/w_0NN_data.h
    arrays: Map<string, NumArray>;
}

// port_saturn_weapon.py convert(wid): prgFile / chrFile are the Saturn's
// whole WEAPON0.PRG / WEAPON0.CHR
export function convertWeapon(prgFile: Uint8Array, chrFile: Uint8Array, wid: number): WeaponPort {
    const cfg = WEAPONS.get(wid)!;
    const sw = new SaturnWeapon(prgFile, chrFile, wid);
    const st = sw as unknown as SaturnStage; // the interface PsxSpriteSheet uses
    const name = "w_" + pad3(wid);
    const front = new Set(cfg.front);
    const arrays = new Map<string, NumArray>();
    // saturn_sprites.py's module globals, fresh per weapon (a process each)
    const state = new SpriteState();

    const fill = (sheet: PsxSpriteSheet): (PsxPart[] | null)[][] => {
        const banks: (PsxPart[] | null)[][] = [];
        for (const [bank, table, n] of sw.bankList) {
            sheet.paletteBlock(st, bank);
            const frames: (PsxPart[] | null)[] = [];
            for (let i = 0; i < n; i++) {
                const f = sheet.frame(st, bank, table, i);
                frames.push(f.length ? f : null);
            }
            banks.push(frames);
        }
        return banks;
    };

    // the first pass learns each image's widest narrowed copy and which images
    // are asked for; the second packs them tallest first
    const quads: [number, number][] = [];
    for (let i = 0; i < 8; i++) {
        quads.push([0, 0], [0, 1]);
    }
    const probe = new PsxSpriteSheet(quads, 0, null, state);
    fill(probe);
    const sheet = new PsxSpriteSheet([[0, 0], [0, 1]], 0, null, state);
    sheet.prepack(st, probe.requests);
    const banks = fill(sheet);
    // images the C draws as primitives
    const primImages: [number, number, number, number, number][] = [];
    for (const img of cfg.primImages) {
        const bank = sw.bankList[0][0];
        let best: Placement | null = null;
        for (const [k, p] of sheet.placed) {
            const key = parseKey(k);
            if (key[0] === bank && key[1] === img && key[2] === 0 && (best === null || p[3] > best[3])) {
                best = p;
            }
        }
        if (best === null) {
            // (PsxSpriteSheet.place is private in its type only)
            best = (sheet as unknown as { place(s: SaturnStage, b: number, i: number): Placement }).place(st, bank, img);
        }
        const [q, u, v, iw, ih] = best;
        const quad = sheet.quadrants[q][1];
        primImages.push([img, u + 128 * (quad & 1), v + 64 * (quad & 2), iw, ih]);
    }
    check(sheet.usedQuadrants() <= 2, "art doesn't fit a 256x128 page");
    for (const frames of banks) {
        frames.forEach((parts, i) => {
            for (const p of parts ?? []) {
                if (front.has(i)) {
                    p.flags |= 0x100;
                }
            }
        });
    }

    // art: quadrant 0 = columns 0-127, quadrant 1 = 128-255; png2bin packs
    // two pixels a byte, the left one in the low nibble
    const art = new Uint8Array(0x4000);
    for (let y = 0; y < 128; y++) {
        for (let x = 0; x < 256; x += 2) {
            const page = sheet.pages[x >> 7];
            const xx = x & 127;
            art[y * 128 + (x >> 1)] = page[y * 128 + xx] | (page[y * 128 + xx + 1] << 4);
        }
    }

    // palettes
    const anims = cfg.anims;
    const records: Anim[][] = anims.map(([a, n]) => {
        const t: Anim[] = [];
        for (let k = 0; k < n; k++) {
            t.push(sw.anim(a + 16 * k));
        }
        return t;
    });
    const palOffsets = new Set<number>();
    for (const t of records) {
        for (const r of t) {
            palOffsets.add(r[2]);
        }
    }
    const pal0 = palOffsets.size ? Math.min(...palOffsets) : 0;
    const semi = new Set(cfg.semiPals);
    const cluts: number[][] = [];
    for (let i = 0; i < N_WEAPON_PAL; i++) {
        cluts.push(new Array(16).fill(0));
    }
    let k = 0;
    for (const [bank] of sw.bankList) {
        for (const colours of sw.palettes(bank)) {
            check(pal0 + k < N_WEAPON_PAL, "too many palettes");
            cluts[pal0 + k] = colours.map((c, j) => (j === 0 ? 0 : psxColour(c, semi.has(k))));
            k++;
        }
    }
    check(k === sheet.cluts.length, "clut count");

    const out: string[] = [];
    const w = (s: string): void => {
        out.push(s);
    };
    w("// Generated by mods/sotn-ultimate/tools/port_saturn_weapon.py from the Saturn's weapon " + wid + "\n");
    w("// (disks/saturn/WEAPON0.PRG/.CHR slot " + wid + "); do not edit\n");
    w("// clang-format off\n");
    banks.forEach((frames, bi) => {
        const sym = name + "_" + (bi + 1);
        const sizes = new Map<number, number>();
        frames.forEach((parts, i) => {
            if (parts) {
                const n = parts.length;
                sizes.set(i, n * 11 + 1 + (n & 1 ? 2 : 1));
                w("static s16 " + sym + "_" + i + "[" + sizes.get(i) + "];\n");
            }
        });
        w("static s16* " + sym + "[] = {\n");
        frames.forEach((parts, i) => {
            w("    " + (parts ? sym + "_" + i : "0") + ",\n");
        });
        w("};\n");
        frames.forEach((parts, i) => {
            if (!parts) {
                return;
            }
            const size = sizes.get(i)!;
            const vals = [parts.length];
            for (const p of parts) {
                vals.push(p.flags, p.x, p.y, p.width, p.height, p.clut, p.tileset, p.left, p.top, p.right, p.bottom);
            }
            while (vals.length < size) {
                vals.push(0);
            }
            arrays.set(sym + "_" + i, { size: 2, values: vals.slice() });
            w("static s16 " + sym + "_" + i + "[] = {\n    " + vals[0] + ",\n");
            for (let k2 = 0; k2 < parts.length; k2++) {
                w("    " + vals.slice(1 + 11 * k2, 12 + 11 * k2).join(", ") + ",\n");
            }
            if (size > parts.length * 11 + 1) {
                w("    " + vals.slice(1 + 11 * parts.length).join(", ") + "\n");
            }
            w("};\n");
        });
    });
    w("\nstatic u16 " + name + "_cluts[N_WEAPON_PAL][COLORS_PER_PAL] = {\n");
    for (const c of cluts) {
        w("    {" + c.map(hex4).join(", ") + "},\n");
    }
    w("};\n");
    arrays.set(name + "_cluts", { size: 2, values: cluts.flat() });
    w("static u16* g_WeaponCluts[] = {" + new Array(cfg.cluts).fill(name + "_cluts").join(", ") + "};\n");
    for (const [img, u, v, iw, ih] of primImages) {
        const vals = [u, v, iw, ih];
        "UVWH".split("").forEach((c, i) => {
            w("#define " + name.toUpperCase() + "_IMG" + img + "_" + c + " " + vals[i] + "\n");
        });
    }
    // frame lists, hitboxes, WeaponAnimation tables
    const tables = new Map<number, number[][]>();
    const props = new Map<number, number>();
    const stop = new Set<number>();
    for (const [a, n] of anims) {
        for (let x = a; x < a + 16 * n; x += 16) {
            stop.add(x);
        }
    }
    for (const t of records) {
        for (const r of t) {
            stop.add(r[1]);
        }
    }
    for (const t of records) {
        for (const [fr, fp] of t) {
            if (!tables.has(fr)) {
                tables.set(fr, sw.frameLists(fr, stop));
            }
            if (!props.has(fp)) {
                props.set(fp, 0);
            }
            for (const seq of tables.get(fr)!) {
                for (const v of seq.slice(0, -1)) {
                    props.set(fp, Math.max(props.get(fp)!, ((v >> 9) & 0x7f) + 1));
                }
            }
        }
    }
    const tableKeys = [...tables.keys()];
    const propKeys = [...props.keys()];
    tableKeys.forEach((fr, ti) => {
        const lists = tables.get(fr)!;
        const uniq: number[][] = [];
        for (const seq of lists) {
            if (!uniq.some((u) => sameList(u, seq))) {
                uniq.push(seq);
            }
        }
        w("\n");
        uniq.forEach((seq, li) => {
            const nm = name + "_frames_" + ti + "_" + li;
            w("static u16 " + nm + "[] = {\n    " + seq.map(hex4).join(", ") + "};\n");
            arrays.set(nm, { size: 2, values: seq.slice() });
        });
        w("static u16* " + name + "_frames_" + ti + "[] = {\n    " +
            lists.map((s) => name + "_frames_" + ti + "_" + uniq.findIndex((u) => sameList(u, s))).join(", ") + "};\n");
    });
    propKeys.forEach((fp, pi) => {
        const nm = name + "_hitboxes_" + pi;
        w("\nstatic s8 " + nm + "[][4] = {\n");
        const vals: number[] = [];
        for (let k2 = 0; k2 < props.get(fp)!; k2++) {
            const o = fp - sw.base + 4 * k2;
            const box = [trunc45(s8(sw.prg[o])), s8(sw.prg[o + 1]), trunc45(s8(sw.prg[o + 2])), s8(sw.prg[o + 3])];
            vals.push(...box);
            w("    {" + box.join(", ") + "},\n");
        }
        w("};\n");
        if (vals.length) {
            arrays.set(nm, { size: 1, values: vals });
        }
    });
    records.forEach((t, ai) => {
        w("\nstatic WeaponAnimation " + name + "_anims_" + ai + "[] = {\n");
        for (const [fr, fp, pal, snd, stt, sf] of t) {
            const fs = FRAME_START.get(stt);
            check(fs !== undefined, name + ": no PSX animation for Saturn frameStart 0x" + stt.toString(16));
            w("    {" + name + "_frames_" + tableKeys.indexOf(fr) + ", " + name + "_hitboxes_" + propKeys.indexOf(fp) +
                ", " + pal + ", 0x" + snd.toString(16).toUpperCase().padStart(3, "0") + ", 0x" +
                fs!.toString(16).toUpperCase().padStart(2, "0") + ", " + sf + "},\n");
        }
        w("};\n");
    });
    return { art, header: out.join(""), arrays };
}

// --- weapon 21's foods (port_saturn_food.py) --------------------------------

// (Saturn image, PSX clut, box left, food name)
const FOODS: [number, number, number][] = [[41, 6, 128], [42, 4, 144]];
const BOX = 16;
const FOOD_IMAGES = 0x06047120;
export const FOOD_WEAPON = 21;

function cropFood(rows: Rows): Rows {
    const r = rows.filter((row) => row.some((v) => v));
    const cols: number[] = [];
    for (let x = 0; x < r[0].length; x++) {
        if (r.some((row) => row[x])) {
            cols.push(x);
        }
    }
    return r.map((row) => row.slice(cols[0], cols[cols.length - 1] + 1));
}

// drop every 5th column, starting where the dropped columns look most like
// their left neighbours
function narrowFood(rows: Rows): Rows {
    const w = rows[0].length;
    let best = 0;
    let bestScore = -1;
    for (let p = 0; p < 5; p++) {
        let s = 0;
        for (let x = Math.max(p, 1); x < w; x += 5) {
            for (const r of rows) {
                if (r[x] === r[x - 1]) {
                    s++;
                }
            }
        }
        if (s > bestScore) {
            best = p;
            bestScore = s;
        }
    }
    const keep: number[] = [];
    for (let x = 0; x < w; x++) {
        if (!(x >= Math.max(best, 1) && (x - best) % 5 === 0)) {
            keep.push(x);
        }
    }
    return rows.map((r) => keep.map((x) => r[x]));
}

// The food boxes of weapon 21's art page: [(byte offset in the 0x4000-byte
// page, 16 bytes)] for rows 0-15 of columns 128-159
export function foodArt(zero: Uint8Array, prg: Uint8Array, chr: Uint8Array): Range[] {
    const stage = new SaturnStage(prg, chr, zero);
    let food: number | null = null;
    for (const a of stage.banks.keys()) {
        const [d, o] = stage.res(a)!;
        if (be32(d, o) === FOOD_IMAGES) {
            food = a;
        }
    }
    check(food !== null && stage.banks.get(food)![0].length >= 43, "Saturn food bank not found");
    const page: Rows = [];
    for (let y = 0; y < BOX; y++) {
        page.push(new Array(32).fill(0));
    }
    for (const [img, , left] of FOODS) {
        const art = narrowFood(cropFood(stage.image(food!, img)[2]));
        const h = art.length;
        const w = art[0].length;
        check(w <= BOX && h <= BOX, "food too big");
        const ox = Math.floor((BOX - w) / 2);
        const oy = BOX - h;
        for (let y = 0; y < h; y++) {
            for (let x = 0; x < w; x++) {
                page[oy + y][left - 128 + ox + x] = art[y][x];
            }
        }
    }
    const out: Range[] = [];
    for (let y = 0; y < BOX; y++) {
        const b = new Uint8Array(16);
        for (let i = 0; i < 16; i++) {
            b[i] = page[y][2 * i] | (page[y][2 * i + 1] << 4);
        }
        out.push({ offset: y * 128 + 64, bytes: b });
    }
    return out;
}

// Everything, from the Saturn disc
export async function buildSaturnWeapons(saturn: DiscRead): Promise<SaturnWeaponsResult> {
    const prg = await saturn("WEAPON0.PRG");
    const chr = await saturn("WEAPON0.CHR");
    const ranges = new Map<string, Range[]>();
    const arrays = new Map<string, Map<string, NumArray>>();
    const headers = new Map<string, string>();
    const art = new Map<number, Uint8Array>();
    const w2: Range[] = [];
    for (let wid = FIRST_WEAPON2; wid <= LAST_WEAPON2; wid++) {
        const r = convertWeapon(prg, chr, wid);
        const hdr = "mods/sotn-ultimate/src/items/weapon/mod/w_" + pad3(wid) + "_data.h";
        arrays.set(hdr, r.arrays);
        headers.set(hdr, r.header);
        art.set(wid, r.art);
        for (let hand = 0; hand < 2; hand++) {
            w2.push({ offset: ((wid - FIRST_WEAPON2) * 2 + hand) * WEAPON_SLOT, bytes: r.art });
        }
    }
    ranges.set("BIN/WEAPON2.BIN", w2);
    const boxes = foodArt(await saturn("0.BIN"), await saturn("STAGE_00.PRG"), await saturn("STAGE_00.CHR"));
    for (const f of ["BIN/WEAPON0.BIN", "BIN/WEAPON1.BIN"]) {
        ranges.set(f, boxes.map((b) => ({ offset: FOOD_WEAPON * WEAPON_SLOT + b.offset, bytes: b.bytes })));
    }
    return { ranges, arrays, headers, art };
}
