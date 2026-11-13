// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn's playable Maria (BIN/MARIA.BIN, src/ric with -DMOD_MARIA_SAT):
// a byte-exact port of mods/sotn-ultimate/tools/gen_mar_assets.py's generated data
// (build/us/mod/mar/mar_sprites.h, mar_spritebank0.h, mar_anims.h,
// mar_fx.h, mar_sp.h, mar_pal.h; see the Python for the formats). Her images
// come from MARIA.PRG / MARIA.CHR, her attack effects from MAR_W.CHR, the
// shared sprite banks her specials borrow from 0.BIN (the packages the
// decomp's Saturn tooling extracts to src/saturn/zero/gen/stgsprN.h, read
// here from the disc) with their pixels from STAGE_01.CHR; the animations
// her missing ones fall back to are Richter's, from the PS1's BIN/RIC.BIN.
//
// marAssets returns, per header, the arrays of plain numbers as
// node/c_arrays.ts parseCArrays reads them (2D arrays flat, in row order).
// marAssetsExtra returns the other arrays whose bytes are game data, as
// numbers in their compiled layout: the AnimationFrame ({u16, u16}) and
// FrameProperty ({s8 x4}) tables of mar_anims.h, the spritebank0_N frames
// (their first value is written `0xJ | 0x8000`) and mar_sp_group_pal
// (3D, which parseCArrays skips). Not here: pointer tables (richter_sprites,
// mar_lz, spritebank0, mar_fxbank_*, D_8015538C), mar_sp_chunks (a struct
// of pointers and VRAM layout numbers) and the #defines.
import { mbLzss } from "./cen_maria.ts";
import { roundHalfEven } from "./dsp.ts";
import { decompressLzss } from "./saturn_lzss.ts";

export interface MarArray {
    size: number; // bytes per element
    values: number[];
}

export type MarHeaders = Map<string, Map<string, MarArray>>;
type Reader = (path: string) => Promise<Uint8Array>;

const OUT = "build/us/mod/mar/";
const SAT = 0x060a5000;
const PHASE = 4;
const PROF = {
    imageTable: 0x060c51a8,
    paletteTable: 0x060c398c,
    animationStart: 0x060c28d8,
    animationEnd: 0x060c37cc,
    frameTable: 0x060c4bb0,
};
const HITBOX_TABLE = 0x060c37cc;
const STAND = 0x060c28e8;
const RIC_RAM = 0x8013c000;

// pl_anims.c's tables in order: [name, Saturn Maria animation] (ANIM_MAP),
// [name, RIC.BIN offset, pairs] (Richter's timing, her standing pose) or
// the hitbox table (FrameProperty)
type AnimSrc = [string, number] | [string, number, number] | [string];
const PL_ANIMS: AnimSrc[] = [
    ["ric_anim_press_up", 103316, 2],
    ["ric_anim_stop_run", 103324, 8],
    ["ric_anim_stand", 0x060c28e8],
    ["ric_anim_stand_relax", 0x060c28e8],
    ["ric_anim_walk", 0x060c29b0],
    ["ric_anim_crouch_from_stand2", 0x060c2aa8],
    ["ric_anim_crouch", 0x060c2ab0],
    ["ric_anim_land_from_air_run", 103624, 2],
    ["ric_anim_crouch_from_stand", 103632, 4],
    ["D_801554E0", 0x060c2bec],
    ["D_801554F0", 0x060c2b18],
    ["D_8015550C", 0x060c2adc],
    ["D_80155528", 0x060c2b38],
    ["D_80155534", 0x060c2b38],
    ["D_80155544", 0x060c2db8],
    ["D_8015555C", 0x060c3704],
    ["D_80155588", 0x060c3760],
    ["D_801555A8", 0x060c30b0],
    ["D_801555C8", 0x060c30ec],
    ["D_801555E8", 0x060c2ed8],
    ["ric_anim_throw_daggers", 103944, 12],
    ["D_80155638", 0x060c2f00],
    ["ric_anim_stand_in_air", 104024, 6],
    ["ric_anim_run", 0x060c2a18],
    ["ric_anim_stun", 0x060c2c28],
    ["D_8015569C", 0x060c2d84],
    ["D_801556C4", 0x060c2c28],
    ["D_80155704", 0x060c2d84],
    ["ric_anim_brandish_whip", 0x060c28d8],
    ["D_80155738", 104248, 2],
    ["D_80155740", 0x060c2b38],
    ["D_80155748", 104264, 2],
    ["D_80155750", 0x060c2e4c],
    ["D_8015577C", 0x060c2f8c],
    ["D_80155788", 0x060c3720],
    ["ric_anim_high_jump", 0x060c2bb8],
    ["ric_anim_point_finger_up", 104364, 11],
    ["ric_anim_blade_dash", 0x060c2adc],
    ["ric_anim_flame_whip", 104512, 29],
    ["D_801558B4", 104628, 8],
    ["D_801558D4", 104660, 2],
    ["D_801558DC", 104668, 2],
    ["D_801558E4", 104676, 14],
    ["D_8015591C", 0x060c3788],
    ["D_80155950", 0x060c37b8],
    ["D_80155964"],
];
// ANIM_MAP in its (dict) order, for the names of her POSE_JUMP group
const ANIM_MAP_ORDER: [string, number][] = [
    ["ric_anim_stand", 0x060c28e8], ["ric_anim_stand_relax", 0x060c28e8],
    ["ric_anim_walk", 0x060c29b0], ["ric_anim_crouch_from_stand2", 0x060c2aa8],
    ["ric_anim_crouch", 0x060c2ab0], ["D_801554E0", 0x060c2bec], ["D_80155544", 0x060c2db8],
    ["D_8015555C", 0x060c3704], ["D_80155588", 0x060c3760], ["D_801555A8", 0x060c30b0],
    ["D_801555C8", 0x060c30ec], ["D_801555E8", 0x060c2ed8], ["D_80155638", 0x060c2f00],
    ["ric_anim_run", 0x060c2a18], ["ric_anim_stun", 0x060c2c28], ["D_8015569C", 0x060c2d84],
    ["D_801556C4", 0x060c2c28], ["D_80155704", 0x060c2d84], ["ric_anim_brandish_whip", 0x060c28d8],
    ["D_80155740", 0x060c2b38], ["D_80155788", 0x060c3720], ["ric_anim_high_jump", 0x060c2bb8],
    ["ric_anim_blade_dash", 0x060c2adc], ["D_801554F0", 0x060c2b18], ["D_8015550C", 0x060c2adc],
    ["D_80155528", 0x060c2b38], ["D_80155534", 0x060c2b38], ["D_80155750", 0x060c2e4c],
    ["D_8015577C", 0x060c2f8c], ["D_8015591C", 0x060c3788], ["D_80155950", 0x060c37b8],
];
const EXTRA_ANIMS: [string, number][] = [
    ["mar_anim_atk_air_down", 0x060c3114], ["mar_anim_kick", 0x060c2fa4],
    ["mar_anim_kick_crouch", 0x060c3080], ["mar_anim_kick_air_fall", 0x060c2f18],
    ["mar_anim_kick_air_rise", 0x060c3734], ["mar_anim_atk_end", 0x060c3164],
    ["mar_anim_atk_end_b", 0x060c3184], ["mar_anim_kick_end", 0x060c2fc8],
    ["mar_anim_kick_end_b", 0x060c2ff0], ["mar_anim_kick_crouch_end", 0x060c309c],
    ["mar_anim_dive", 0x060c2f48], ["mar_anim_dive_down", 0x060c2f54],
    ["mar_anim_uppercut_diag", 0x060c2b04], ["mar_anim_roll", 0x060c2e64],
    ["mar_anim_stand_edge", 0x060c292c], ["mar_anim_backdash", 0x060c2e04],
    ["mar_anim_backdash_end", 0x060c2e34], ["mar_anim_backdash_end_b", 0x060c2e40],
    ["mar_anim_throw", 0x060c2e78], ["mar_anim_throw_b", 0x060c2ea8],
    ["mar_anim_sp_knuckle", 0x060c3518], ["mar_anim_sp_genbu", 0x060c31ac],
    ["mar_anim_sp_genbu2", 0x060c31e8], ["mar_anim_sp_byakko", 0x060c33d8],
    ["mar_anim_sp_byakko2", 0x060c3438], ["mar_anim_sp_byakko3", 0x060c34d4],
    ["mar_anim_sp_byakko4", 0x060c34f8], ["mar_anim_sp_seiryu", 0x060c32bc],
    ["mar_anim_sp_seiryu2", 0x060c32f8], ["mar_anim_sp_seiryu3", 0x060c3390],
    ["mar_anim_sp_seiryu4", 0x060c33b0], ["mar_anim_sp_summon", 0x060c3258],
    ["mar_anim_sp_summon2", 0x060c3284],
];
const ANIM_GROUPS = 0x060c28a0;

const FX_RES = 0x060c6d68;
const FX_ORDER = [1, 2, 3, 8, 4, 6, 5, 7, 9, 10, 11, 12];
const FX_NFRAMES: Record<number, number> = { 1: 55, 2: 2, 3: 4, 4: 13, 5: 38, 6: 5, 7: 9, 8: 17, 9: 20, 10: 10, 11: 14, 12: 19 };
const FX_VRAM_X = 992;
const FX_VRAM_Y = 384;
const FX_VRAM_W = 32;
const SP_PERM_VRAM: [number, number] = [960, 384];
const SP_SLOT_VRAM: [number, number] = [800, 384];
const SP_SLOT_ROWS = 504 - 384;
const SP_FLAME_VRAM: [number, number] = [992, 458];
const SP_PUFF_VRAM: [number, number] = [896, 385];
const tileset = (x: number, y: number): number => (Math.floor(x / 64) + Math.floor(y / 256) * 16) << 2;
const FX_TILESET = tileset(FX_VRAM_X, FX_VRAM_Y);
const U_FIRST = 1; // gen_mar_assets.u_first
const FX_BANKS: [string, number, number[]][] = [
    ["mar_fxbank_shot", 7, range(1, 9)],
    ["mar_fxbank_ball", 1, [...range(1, 13), 16]],
];
const FX_PALS: [number, number, number][] = [[7, 0, 0x140], [1, 0, 0x139], [1, 1, 0x13a], [1, 3, 0x13b]];
// the mod's own colours (her holy water flames, recoloured)
const FLAME_PAL: [number, number[]] = [0x13f, [0x0000, 0xd84c, 0xc84c, 0xd84c, 0xe44c, 0xf04c, 0xe84c, 0xd84c,
    0xcc4c, 0xc84c, 0xd04c, 0xd84c, 0xdc4c, 0xe44c, 0xe84c, 0xf44c]];
const CHARGE_PALS = 0x060c16d0;

type PkKey = number | string;
type SpBank = [string, PkKey, number[], [number, number], number[]];
const SP_PERM: SpBank[] = [
    ["mar_fxbank_perm", 11, range(1, 14), [0, 0], [0x141]],
    ["mar_fxbank_perm", 12, range(13, 19), [1, 1], [0x142]],
    ["mar_fxbank_perm", "S4", range(1, 11), [19, 4], [0x145]],
];
const SP_PUFF: SpBank = ["mar_fxbank_perm", "S9", range(1, 15), [29, 5], [0x146]];
const SP_GROUPS: [string, number, number[]][] = [
    ["mar_fxbank_knuckle", 8, range(1, 16)],
    ["mar_fxbank_genbu", 9, range(1, 19)],
    ["mar_fxbank_byakko", 10, range(1, 9)],
    ["mar_fxbank_seiryu", 12, [10, 11]],
];
const SP_TRANSPOSE = new Set(["12,3"]);
const SP_CHUNK = 4816;

// 0.BIN (loaded at 0x06004080): the shared sprite banks' resources
// {images, palette, -1, 0xFFFF} (config/assets.saturn.yaml's packages
// src/saturn/zero/gen/stgspr0-14.h), and the frame counts of the banks
// whose frame tables (right before the images) are used
const ZERO = 0x06004080;
const SHARED_RES = [0x060457f0, 0x06045fa0, 0x0604604c, 0x0604622c, 0x060464c4, 0x060466e0, 0x0604672c,
    0x06046bf8, 0x06046d08, 0x06046e5c, 0x06047414, 0x06047518, 0x06047570, 0x06047614, 0x06047694];
const SHARED_FRAMES: Record<number, number> = { 9: 15, 11: 8 };

function range(a: number, b: number): number[] {
    const out: number[] = [];
    for (let i = a; i < b; i++) {
        out.push(i);
    }
    return out;
}

const be16 = (d: Uint8Array, o: number): number => (d[o] << 8) | d[o + 1];
const bes16 = (d: Uint8Array, o: number): number => (be16(d, o) << 16) >> 16;
const be32 = (d: Uint8Array, o: number): number => ((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) >>> 0;
const s8 = (d: Uint8Array, o: number): number => (d[o] << 24) >> 24;
const pyMod = (a: number, n: number): number => ((a % n) + n) % n;
const fdiv = (a: number, b: number): number => Math.floor(a / b);

// an h x w array of 4bpp indices, row major (numpy uint8 2D)
interface Img {
    w: number;
    h: number;
    px: Uint8Array;
}

const zeros = (h: number, w: number): Img => ({ w, h, px: new Uint8Array(w * h) });

function sub(im: Img, y0: number, y1: number, x0: number, x1: number): Img {
    const o = zeros(y1 - y0, x1 - x0);
    for (let y = 0; y < o.h; y++) {
        o.px.set(im.px.subarray((y0 + y) * im.w + x0, (y0 + y) * im.w + x1), y * o.w);
    }
    return o;
}

function cols(im: Img, keep: number[]): Img {
    const o = zeros(im.h, keep.length);
    for (let y = 0; y < im.h; y++) {
        for (let k = 0; k < keep.length; k++) {
            o.px[y * o.w + k] = im.px[y * im.w + keep[k]];
        }
    }
    return o;
}

function transpose(im: Img): Img {
    const o = zeros(im.w, im.h);
    for (let y = 0; y < im.h; y++) {
        for (let x = 0; x < im.w; x++) {
            o.px[x * o.w + y] = im.px[y * im.w + x];
        }
    }
    return o;
}

// numpy's dst[top:top + h, left:left + w] = im (inside the destination)
function blit(dst: Img, im: Img, top: number, left: number, opaqueOnly = false): void {
    if (top < 0 || left < 0 || top + im.h > dst.h || left + im.w > dst.w) {
        throw new Error("blit outside the atlas");
    }
    for (let y = 0; y < im.h; y++) {
        for (let x = 0; x < im.w; x++) {
            const v = im.px[y * im.w + x];
            if (!opaqueOnly || v) {
                dst.px[(top + y) * dst.w + left + x] = v;
            }
        }
    }
}

// 4bpp nibbles, high first (the Saturn's)
function nibbles(d: Uint8Array, w: number, h: number): Img {
    if (d.length < (w * h) >> 1) {
        throw new Error("short image data");
    }
    const im = zeros(h, w);
    for (let i = 0; i < (w * h) >> 1; i++) {
        im.px[2 * i] = d[i] >> 4;
        im.px[2 * i + 1] = d[i] & 15;
    }
    return im;
}

// build_ric2.pack_psx: low nibble first
function packPsx(im: Img): Uint8Array {
    const n = im.px.length;
    if (n & 1) {
        throw new Error("odd pixel count");
    }
    const out = new Uint8Array(n >> 1);
    for (let i = 0; i < out.length; i++) {
        out[i] = im.px[2 * i] | (im.px[2 * i + 1] << 4);
    }
    return out;
}

// build_ric2.narrow: drop the columns whose absolute x (left + c) % 5 ==
// phase; a dropped pixel with no neighbour in its row keeps its place
function narrow(im: Img, left: number, phase: number): Img {
    const keep = range(0, im.w).filter((c) => pyMod(left + c, 5) !== phase);
    const out = cols(im, keep);
    for (let c = 0; c < im.w; c++) {
        if (pyMod(left + c, 5) !== phase) {
            continue;
        }
        let dst = -1;
        for (let y = 0; y < im.h; y++) {
            const v = im.px[y * im.w + c];
            const nl = c > 0 ? im.px[y * im.w + c - 1] : 0;
            const nr = c + 1 < im.w ? im.px[y * im.w + c + 1] : 0;
            if (v !== 0 && nl === 0 && nr === 0) {
                if (dst < 0) {
                    let d = 0;
                    for (let cc = 0; cc < c; cc++) {
                        if (pyMod(left + cc, 5) !== phase) {
                            d++;
                        }
                    }
                    dst = Math.min(Math.max(d - 1, 0), out.w - 1);
                    if (dst < 0) {
                        throw new Error("narrow: no column left");
                    }
                }
                out.px[y * out.w + dst] = v;
            }
        }
    }
    return out;
}

// build_ric2.crop -> (image cropped to its pixels, width padded to 4, x0, y0)
function crop(im: Img): [Img, number, number] {
    let y0 = Infinity;
    let y1 = -1;
    let x0 = Infinity;
    let x1 = -1;
    for (let y = 0; y < im.h; y++) {
        for (let x = 0; x < im.w; x++) {
            if (im.px[y * im.w + x]) {
                y0 = Math.min(y0, y);
                y1 = Math.max(y1, y);
                x0 = Math.min(x0, x);
                x1 = Math.max(x1, x);
            }
        }
    }
    if (y1 < 0) {
        return [zeros(Math.min(1, im.h), Math.min(4, im.w)), 0, 0];
    }
    const c = sub(im, y0, y1 + 1, x0, x1 + 1);
    const w = (c.w + 3) & ~3;
    if (w === c.w) {
        return [c, x0, y0];
    }
    const p = zeros(c.h, w);
    blit(p, c, 0, 0);
    return [p, x0, y0];
}

// PS1 column of a kept absolute Saturn column (PHASE 4)
const mapcol = (c: number): number => c - fdiv(c + 1, 5);
const keptCols = (left: number, w: number): number[] => range(left, left + w).filter((x) => pyMod(x, 5) !== PHASE);
const ps1Color = (c: number, i: number): number => (i === 0 ? 0 : (c & 0x7fff) || 0x8000);
// int(round(v * 0.8))
const r08 = (v: number): number => roundHalfEven(v * 0.8);

// build_ric2.Saturn with the maria profile (character_viewer readers)
class MariaSaturn {
    prg: Uint8Array;
    chr: Uint8Array;
    recs: [number, number, number][] = [];
    streams: [number, number][] = [];
    cache = new Map<number, Img>();

    constructor(prg: Uint8Array, chr: Uint8Array) {
        this.prg = prg;
        this.chr = chr;
        let cur = PROF.imageTable - SAT;
        let prev = -1;
        while (cur + 4 <= prg.length) {
            const dims = be16(prg, cur);
            const units = be16(prg, cur + 2);
            const w = dims >> 8;
            const h = dims & 0xff;
            if (!w || !h || w > 128 || h > 128 || units < prev) {
                break;
            }
            this.recs.push([w, h, units * 8]);
            prev = units;
            cur += 4;
        }
        const dir = be32(prg, 0x40) - SAT;
        const backing = be32(prg, dir);
        for (let i = 0; i < this.recs.length; i++) {
            this.streams.push([be32(prg, dir + i * 8) - backing, be32(prg, dir + i * 8 + 4)]);
        }
    }

    image(j: number): Img {
        let im = this.cache.get(j);
        if (im === undefined) {
            const [wf, hf] = this.recs[j];
            const [o, s] = this.streams[j];
            im = nibbles(decompressLzss(this.chr.subarray(o, o + s)), wf * 2, hf * 2);
            this.cache.set(j, im);
        }
        return im;
    }

    // (flag byte, x, y, image) of every one-part frame, null otherwise
    frames(): ([number, number, number, number] | null)[] {
        const ft = PROF.frameTable - SAT;
        const it = PROF.imageTable - SAT;
        const out: ([number, number, number, number] | null)[] = [];
        for (let i = 0; i < fdiv(it - ft, 4); i++) {
            const o = be32(this.prg, ft + i * 4) - SAT;
            const pc = (be16(this.prg, o) >> 8) & 0x3f;
            if (pc !== 1) {
                out.push(null);
                continue;
            }
            out.push([be16(this.prg, o + 4) >> 8, bes16(this.prg, o + 6), bes16(this.prg, o + 8), be16(this.prg, o + 10)]);
        }
        return out;
    }

    palette(bank: number): number[] {
        const t = PROF.paletteTable - SAT + 2 + bank * 32;
        return range(0, 16).map((i) => be16(this.prg, t + i * 2));
    }
}

// a Saturn sprite frame part: attr, x, y (image centre), image
type Part = [number, number, number, number];
// (images, palettes, frames)
type Package = [Img[], number[][], Part[][]];

// gen_mar_assets.fx_packages: MAR_W.CHR's sprite packages
function fxPackages(prg: Uint8Array, chr: Uint8Array): Map<PkKey, Package> {
    const u16 = (a: number): number => be16(prg, a - SAT);
    const out = new Map<PkKey, Package>();
    let base = 0;
    for (const r of FX_ORDER) {
        const ra = FX_RES - SAT + r * 12;
        const im = be32(prg, ra);
        const pal = be32(prg, ra + 4);
        let o = im - SAT;
        const recs: [number, number, number][] = [];
        let size: number;
        for (;;) {
            const w = prg[o];
            const h = prg[o + 1];
            const u = be16(prg, o + 2);
            o += 4;
            if (w === 0 && h === 0) {
                size = u * 8;
                break;
            }
            recs.push([w * 2, h * 2, base + u * 8]);
        }
        const nb = u16(pal) & 0x3fff;
        const pals = range(0, nb).map((k) => range(0, 16).map((i) => u16(pal + 2 + (k * 16 + i) * 2)));
        const ftab = pal - FX_NFRAMES[r] * 4;
        const frames: Part[][] = [];
        for (let f = 0; f < FX_NFRAMES[r]; f++) {
            const fa = be32(prg, ftab + f * 4 - SAT);
            const parts: Part[] = [];
            for (let p = 0; p < ((u16(fa) >> 8) & 0x3f); p++) {
                const q = fa + 4 + p * 8 - SAT;
                parts.push([be16(prg, q), bes16(prg, q + 2), bes16(prg, q + 4), be16(prg, q + 6)]);
            }
            frames.push(parts);
        }
        const imgs = recs.map(([w, h, off]) => nibbles(chr.subarray(off, off + ((w * h) >> 1)), w, h));
        out.set(r, [imgs, pals, frames]);
        base += size;
    }
    if (base !== chr.length) {
        throw new Error("MAR_W.CHR size");
    }
    return out;
}

// gen_mar_assets.shared_bank: (images, frames by number (empty: none), palettes)
function sharedBank(zero: Uint8Array, stage: Uint8Array, i: number): [Img[], Part[][], number[][]] {
    const z = (a: number): number => a - ZERO;
    const recsOf = (k: number): [number, number, number][] => {
        let a = z(be32(zero, z(SHARED_RES[k])));
        const recs: [number, number, number][] = [];
        for (;;) {
            const rec: [number, number, number] = [zero[a], zero[a + 1], be16(zero, a + 2)];
            a += 4;
            recs.push(rec);
            if (rec[0] === 0 && rec[1] === 0) {
                return recs;
            }
        }
    };
    let base = 0;
    for (let k = 0; k < i; k++) {
        const r = recsOf(k);
        base += r[r.length - 1][2] * 8;
    }
    const recs = recsOf(i);
    const imgs = recs.slice(0, -1).map(([w, h, o]) =>
        nibbles(stage.subarray(base + o * 8, base + o * 8 + w * h * 2), w * 2, h * 2));
    const frames: Part[][] = [];
    const nf = SHARED_FRAMES[i] ?? 0;
    const images = be32(zero, z(SHARED_RES[i]));
    for (let f = 0; f < nf; f++) {
        const fa = z(be32(zero, z(images - 4 * nf + 4 * f)));
        const parts: Part[] = [];
        for (let p = 0; p < ((be16(zero, fa) >> 8) & 0x3f); p++) {
            const q = fa + 4 + p * 8;
            parts.push([be16(zero, q), bes16(zero, q + 2), bes16(zero, q + 4), be16(zero, q + 6)]);
        }
        frames.push(parts);
    }
    const pa = z(be32(zero, z(SHARED_RES[i]) + 4));
    const pals = range(0, be16(zero, pa) & 0x3fff).map((k) => range(0, 16).map((c) => be16(zero, pa + 2 + (k * 16 + c) * 2)));
    return [imgs, frames, pals];
}

class Headers {
    m: MarHeaders = new Map();
    extra: MarHeaders = new Map();

    put(hdr: string, name: string, size: number, values: number[], extra = false): void {
        const t = extra ? this.extra : this.m;
        let h = t.get(OUT + hdr);
        if (h === undefined) {
            h = new Map();
            t.set(OUT + hdr, h);
        }
        if (h.has(name)) {
            throw new Error("duplicate " + name);
        }
        h.set(name, { size, values });
    }
}

// gen_mar_assets.fx_frame
function fxFrame(imgs: Img[], parts: Part[]): [Img, number, number, number] {
    const boxes = parts.map(([, x, y, j]): [number, number, number, number] =>
        [x - fdiv(imgs[j].w, 2), y - fdiv(imgs[j].h, 2), imgs[j].w, imgs[j].h]);
    const x0 = Math.min(...boxes.map((b) => b[0]));
    const y0 = Math.min(...boxes.map((b) => b[1]));
    const x1 = Math.max(...boxes.map((b) => b[0] + b[2]));
    const y1 = Math.max(...boxes.map((b) => b[1] + b[3]));
    const out = zeros(y1 - y0, x1 - x0);
    const banks = new Set<number>();
    parts.forEach(([attr, , , j], k) => {
        let img = imgs[j];
        if (attr & 0x10) {
            img = cols(img, range(0, img.w).reverse());
        }
        if (attr & 0x20) {
            const f = zeros(img.h, img.w);
            for (let y = 0; y < img.h; y++) {
                f.px.set(img.px.subarray((img.h - 1 - y) * img.w, (img.h - y) * img.w), y * img.w);
            }
            img = f;
        }
        blit(out, img, boxes[k][1] - y0, boxes[k][0] - x0, true);
        banks.add(attr >> 8);
    });
    if (banks.size !== 1) {
        throw new Error("fx frame banks");
    }
    const [c, cx, cy] = crop(narrow(out, x0, PHASE));
    const kept = keptCols(x0, x1 - x0);
    return [c, mapcol(kept[0]) + cx, y0 + cy, [...banks][0]];
}

const keyOf = (im: Img): string => im.w + "/" + im.px.join(",");

// gen_mar_assets.gen_fx
function genFx(H: Headers, prg: Uint8Array, pk: Map<PkKey, Package>): void {
    const W = FX_VRAM_W * 4;
    const atlas = zeros(256, W);
    let shelfX = 0;
    let shelfY = 0;
    let shelfH = 0;
    const placed = new Map<string, [number, number]>();
    for (const [name, r, frames] of FX_BANKS) {
        const [imgs, , sframes] = pk.get(r)!;
        for (let f = 0; f <= Math.max(...frames); f++) {
            if (!frames.includes(f) || !sframes[f].length) {
                continue;
            }
            const [img, x, y, bank] = fxFrame(imgs, sframes[f]);
            const clut = FX_PALS.findIndex(([pr, pb]) => pr === r && pb === bank);
            if (clut < 0) {
                throw new Error("fx clut " + r + " " + f);
            }
            let w = img.w;
            let h = img.h;
            const key = keyOf(img);
            if (!placed.has(key)) {
                if (shelfX + w > W) {
                    shelfX = 0;
                    shelfY += shelfH;
                    shelfH = 0;
                }
                if ((FX_VRAM_Y & 255) + shelfY + h > 255) {
                    throw new Error("fx atlas v");
                }
                blit(atlas, img, shelfY, shelfX);
                placed.set(key, [shelfX, shelfY]);
                shelfX += w;
                shelfH = Math.max(shelfH, h);
            }
            let [u, v] = placed.get(key)!;
            u += (FX_VRAM_X & 63) * 4;
            v += FX_VRAM_Y & 255;
            w = Math.min(w, 255 - u);
            h = Math.min(h, 255 - v);
            H.put("mar_fx.h", name + "_" + f, 2, [1, 0, x, y, w, h, clut, FX_TILESET, u, v, u + w, v + h]);
        }
    }
    const px = packPsx(sub(atlas, 0, shelfY + shelfH, 0, W));
    H.put("mar_fx.h", "mar_fx_lz", 1, [...mbLzss(px)]);
    H.put("mar_fx.h", "mar_fx_pal_ids", 2, [...FX_PALS.map((p) => p[2]), FLAME_PAL[0]]);
    const rows: number[] = [];
    for (const [r, k] of FX_PALS) {
        pk.get(r)![1][k].forEach((c, i) => rows.push(ps1Color(c, i)));
    }
    rows.push(...FLAME_PAL[1]);
    H.put("mar_fx.h", "mar_fx_pal", 2, rows);
    const charge: number[] = [];
    for (let k = 5; k < 20; k++) {
        const a = CHARGE_PALS + k * 32 - SAT;
        range(0, 16).forEach((i) => charge.push(ps1Color(be16(prg, a + i * 2), i)));
    }
    H.put("mar_fx.h", "mar_charge_pal", 2, charge);
}

const spNarrow = (im: Img): Img => cols(im, range(0, im.w).filter((c) => c % 5 !== 4));

// gen_mar_assets.gen_sp
function genSp(H: Headers, pk: Map<PkKey, Package>, zero: Uint8Array, stage: Uint8Array): void {
    const HDR = "mar_sp.h";
    const [dImgs, , dPals] = sharedBank(zero, stage, 4);
    const [pImgs, pFrames] = sharedBank(zero, stage, 9);
    const b7 = sharedBank(zero, stage, 7)[2];
    pk.set("S9", [pImgs, [b7[0x1c]], range(0, 15).map((f) => pFrames[f] ?? [])]);
    pk.set("S4", [dImgs, [dPals[9]], [[], ...range(1, 11).map((k): Part[] => [[0, 0, 0, 3 + k]])]]);
    const W = FX_VRAM_W * 4;
    // (name, x, y, rows, raw size)
    type Chunk = [string, number, number, number, number];

    const layout = (banks: [string, PkKey, number[], [number, number]][], block: [number, number]): [Img, number] => {
        const [bx, by] = block;
        const ytop = by & 255;
        if (!ytop) {
            throw new Error("v 0");
        }
        const used = new Map<string, Img>();
        for (const [, r, frames] of banks) {
            const [imgs, , sframes] = pk.get(r)!;
            for (const f of frames) {
                for (const [, , , j] of sframes[f]) {
                    let im = spNarrow(imgs[j]);
                    if (SP_TRANSPOSE.has(r + "," + j)) {
                        im = transpose(im);
                    }
                    used.set(r + "," + j, im);
                }
            }
        }
        const order = [...used.keys()].map((k, i): [string, number] => [k, i]).sort((a, b) => {
            const A = used.get(a[0])!;
            const B = used.get(b[0])!;
            return B.h - A.h || B.w - A.w || a[1] - b[1];
        }).map((e) => e[0]);
        const atlas = zeros(256, W);
        const where = new Map<string, [number, number]>();
        const sky = new Array<number>(W - 1).fill(1);
        for (const k of order) {
            const im = used.get(k)!;
            let best: [number, number] | null = null;
            for (let x = U_FIRST; x < W - im.w; x++) {
                const top = Math.max(...sky.slice(x, x + im.w));
                if (best === null || top < best[0]) {
                    best = [top, x];
                }
            }
            if (best === null) {
                throw new Error("sp image too wide");
            }
            const [top, x] = best;
            blit(atlas, im, top, x);
            where.set(k, [x, top]);
            for (let c = x; c < x + im.w; c++) {
                sky[c] = top + im.h;
            }
        }
        const Hh = Math.max(...sky);
        if (ytop + Hh > 255) {
            throw new Error("sp block rows");
        }
        for (const [name, r, frames, [foff, coff]] of banks) {
            const [imgs, , sframes] = pk.get(r)!;
            for (const f of frames) {
                if (!sframes[f].length) {
                    continue;
                }
                const vals = [sframes[f].length];
                for (const [attr, x, y, j] of sframes[f]) {
                    const w0 = imgs[j].w;
                    const im = used.get(r + "," + j)!;
                    const left = r08(x - w0 / 2);
                    const top = y - fdiv(im.h, 2);
                    let [u, v] = where.get(r + "," + j)!;
                    u += (bx & 63) * 4;
                    v += ytop;
                    const flags = (attr & 0x10 ? 2 : 0) | (attr & 0x20 ? 1 : 0);
                    vals.push(flags, left, top, im.w, im.h, (attr >> 8) + coff, tileset(bx, by), u, v, u + im.w, v + im.h);
                }
                H.put(HDR, name + "_" + (f + foff), 2, vals);
            }
        }
        return [sub(atlas, 0, Hh, 0, W), Hh];
    };

    const chunks = (tag: string, atlas: Img, block: [number, number]): Chunk[] => {
        const rows = fdiv(SP_CHUNK * 2, W);
        const res: Chunk[] = [];
        for (let k = 0, r0 = 0; r0 < atlas.h; k++, r0 += rows) {
            const part = sub(atlas, r0, Math.min(r0 + rows, atlas.h), 0, W);
            const px = packPsx(part);
            const cname = "mar_sp_lz_" + tag + "_" + k;
            H.put(HDR, cname, 1, [...mbLzss(px)]);
            res.push([cname, block[0], block[1] + r0, part.h, px.length]);
        }
        return res;
    };

    const strip = (b: SpBank): [string, PkKey, number[], [number, number]] => [b[0], b[1], b[2], b[3]];
    let [atlas] = layout(SP_PERM.map(strip), SP_PERM_VRAM);
    const perm = chunks("perm", atlas, SP_PERM_VRAM);
    [atlas] = layout([strip(SP_PUFF)], SP_PUFF_VRAM);
    perm.push(...chunks("puff", atlas, SP_PUFF_VRAM));
    const groups: Chunk[][] = [];
    SP_GROUPS.forEach(([name, r, frames], g) => {
        const [a, hg] = layout([[name, r, frames, [0, 0]]], SP_SLOT_VRAM);
        if (hg > SP_SLOT_ROWS) {
            throw new Error("sp slot rows " + name);
        }
        groups.push(chunks(String(g), a, SP_SLOT_VRAM));
    });
    // Byakko's flames
    const [fimgs, fframes] = sharedBank(zero, stage, 11);
    const used = new Map<number, Img>();
    for (let f = 1; f < 8; f++) {
        for (const [, , , j] of fframes[f]) {
            used.set(j, spNarrow(fimgs[j]));
        }
    }
    const bins: [[number, number], number][] = [[SP_SLOT_VRAM, SP_SLOT_ROWS], [SP_FLAME_VRAM, 255 - (SP_FLAME_VRAM[1] & 255)]];
    const atl = bins.map(([, n]) => zeros(n, W));
    const where = new Map<number, [number, number, number]>();
    const shelves: number[][][] = bins.map(() => []);
    const order = [...used.keys()].map((k, i): [number, number] => [k, i]).sort((a, b) => {
        const A = used.get(a[0])!;
        const B = used.get(b[0])!;
        return B.h - A.h || B.w - A.w || a[1] - b[1];
    }).map((e) => e[0]);
    for (const j of order) {
        const im = used.get(j)!;
        let spot: [number, number[]] | null = null;
        for (let bi = 0; bi < bins.length; bi++) {
            for (const sh of shelves[bi]) {
                if (sh[1] >= im.h && sh[2] + im.w <= W - 1) {
                    spot = [bi, sh];
                    break;
                }
            }
            if (spot) {
                break;
            }
            const top = 1 + shelves[bi].reduce((s, q) => s + q[1], 0);
            if (top + im.h <= bins[bi][1]) {
                shelves[bi].push([top, im.h, U_FIRST]);
                spot = [bi, shelves[bi][shelves[bi].length - 1]];
                break;
            }
        }
        if (!spot) {
            throw new Error("flames do not fit");
        }
        const [bi, sh] = spot;
        blit(atl[bi], im, sh[0], sh[2]);
        where.set(j, [bi, sh[2], sh[0]]);
        sh[2] += im.w;
    }
    for (let f = 1; f < 8; f++) {
        const vals = [fframes[f].length];
        for (const [attr, x, y, j] of fframes[f]) {
            const h = fimgs[j].h;
            const w0 = fimgs[j].w;
            const w = used.get(j)!.w;
            let [bi, u, v] = where.get(j)!;
            const [fx, fy] = bins[bi][0];
            u += (fx & 63) * 4;
            v += fy & 255;
            const flags = (attr & 0x10 ? 2 : 0) | (attr & 0x20 ? 1 : 0);
            vals.push(flags, r08(x - w0 / 2), y - fdiv(h, 2), w, h, 0, tileset(fx, fy), u, v, u + w, v + h);
        }
        H.put(HDR, "mar_fxbank_flame_" + f, 2, vals);
    }
    const fl: Chunk[] = [];
    bins.forEach(([blk], k) => {
        let rowsUsed = 0;
        for (const [j, [bi, , v]] of where) {
            if (bi === k) {
                rowsUsed = Math.max(rowsUsed, v + used.get(j)!.h);
            }
        }
        if (rowsUsed) {
            fl.push(...chunks("flame" + k, sub(atl[k], 0, rowsUsed, 0, W), blk));
        }
    });
    groups.push(fl);
    const first = [0, perm.length];
    for (const g of groups) {
        first.push(first[first.length - 1] + g.length);
    }
    H.put(HDR, "mar_sp_first", 1, first);
    const ids: number[] = [];
    const rows: number[] = [];
    for (const [, r, , , pals] of [...SP_PERM, SP_PUFF]) {
        pals.forEach((pid, k) => {
            ids.push(pid);
            pk.get(r)![1][k].forEach((c, i) => rows.push(ps1Color(c, i)));
        });
    }
    H.put(HDR, "mar_sp_perm_pal_ids", 2, ids);
    H.put(HDR, "mar_sp_perm_pal", 2, rows);
    const gp: number[] = [];
    for (const [, r] of SP_GROUPS) {
        const p = pk.get(r)![1];
        for (const q of [...p, ...p].slice(0, 2)) {
            q.forEach((c, i) => gp.push(ps1Color(c, i)));
        }
    }
    const fpal = sharedBank(zero, stage, 7)[2];
    for (const q of [fpal[12], fpal[5]]) {
        q.forEach((c, i) => gp.push(ps1Color(c, i)));
    }
    H.put(HDR, "mar_sp_group_pal", 2, gp, true); // [5][2][16]: parseCArrays skips it
}

// gen_mar_assets.sat_anims: {address: [(duration, pose)...]} with the control entry
function satAnims(prg: Uint8Array): Map<number, [number, number][]> {
    let o = PROF.animationStart - SAT;
    const end = PROF.animationEnd - SAT;
    const out = new Map<number, [number, number][]>();
    while (o + 4 <= end) {
        const start = o;
        const pairs: [number, number][] = [];
        while (o + 4 <= end) {
            const d = be16(prg, o);
            const q = be16(prg, o + 2);
            o += 4;
            pairs.push([d, q]);
            if (d === 0 || d === 0xfffe || d === 0xffff) {
                break;
            }
        }
        out.set(SAT + start, pairs);
    }
    return out;
}

async function generate(readSaturn: Reader, readPs1?: Reader): Promise<Headers> {
    const H = new Headers();
    const prg = await readSaturn("MARIA.PRG");
    const s = new MariaSaturn(prg, await readSaturn("MARIA.CHR"));
    const frames = s.frames();
    const nimg = s.recs.length;
    const first = new Map<number, [number, number, number, number]>();
    for (const fr of frames) {
        if (fr && !first.has(fr[3])) {
            first.set(fr[3], fr);
        }
    }
    // sprites: (header + packed bytes, raw size, x0, y0)
    const sprites: [number[], number, number, number][] = [];
    let maxpix = 0;
    for (let j = 0; j < nimg; j++) {
        const img = s.image(j);
        const fr = first.get(j);
        const left = fr ? fr[1] - fdiv(img.w, 2) : 0;
        const [c, x0, y0] = crop(narrow(img, left, PHASE));
        const px = packPsx(c);
        sprites.push([[c.w, c.h, 0, 0, ...mbLzss(px)], px.length, x0, y0]);
        maxpix = Math.max(maxpix, px.length);
        if (c.w > 252 || c.h > 255) {
            throw new Error("sprite size");
        }
    }
    const frOut: ([number, number, number, number, number] | null)[] = frames.map((fr) => {
        if (!fr) {
            return null;
        }
        const [fl, x, y, j] = fr;
        const im = s.image(j);
        const left = x - fdiv(im.w, 2);
        const [, , x0, y0] = sprites[j];
        // SAT_Y (gen_mar_assets.py): the Saturn frame y is 1 below the PS1's
        return [j, mapcol(keptCols(left, im.w)[0]) + x0, y - fdiv(im.h, 2) + y0 - 1, 0, fl];
    });
    // hitboxes
    const hb: number[] = [];
    for (let o = HITBOX_TABLE - SAT; o + 4 <= PROF.paletteTable - SAT; o += 4) {
        hb.push(r08(s8(prg, o)), s8(prg, o + 1), r08(s8(prg, o + 2)), s8(prg, o + 3));
    }
    // animations (mar_anims.h: AnimationFrame / FrameProperty, extra)
    const anims = satAnims(prg);
    const standPose = anims.get(STAND)![0][1];
    const ric = readPs1 ? await readPs1("BIN/RIC.BIN") : null;
    const flat = (pairs: [number, number][]): number[] => pairs.flatMap((p) => [p[0], p[1]]);
    const groups = range(0, 14).map((k) => be32(prg, ANIM_GROUPS - SAT + k * 4));
    const rev = new Map<number, string>();
    for (const [name, addr] of [...ANIM_MAP_ORDER, ...EXTRA_ANIMS]) {
        if (!rev.has(addr)) {
            rev.set(addr, name);
        }
    }
    const extra: [string, number][] = [...EXTRA_ANIMS];
    for (const addr of groups) {
        if (!rev.has(addr)) {
            const name = "mar_anim_grp_" + addr.toString(16).toUpperCase();
            rev.set(addr, name);
            extra.push([name, addr]);
        }
    }
    for (const src of PL_ANIMS) {
        if (src.length === 1) {
            H.put("mar_anims.h", src[0], 1, hb, true);
        } else if (src.length === 2) {
            H.put("mar_anims.h", src[0], 2, flat(anims.get(src[1])!), true);
        } else if (ric) {
            const [name, off, n] = src;
            const pairs: [number, number][] = range(0, n).map((k) => {
                const d = ric[off + 4 * k] | (ric[off + 4 * k + 1] << 8);
                const q = ric[off + 4 * k + 2] | (ric[off + 4 * k + 3] << 8);
                return [d, d === 0xfffe ? 2 : d === 0 || d === 0xffff ? q : standPose];
            });
            H.put("mar_anims.h", name, 2, flat(pairs), true);
        }
    }
    const jumpEnds = new Set(EXTRA_ANIMS.map((e) => e[0]).filter((n) => n.startsWith("mar_anim_sp_")));
    for (const [name, addr] of extra) {
        let pairs = anims.get(addr)!;
        if (jumpEnds.has(name)) {
            pairs = pairs.map(([d, q]): [number, number] => (d === 0xfffe ? [0xffff, 0] : [d, q]));
        }
        H.put("mar_anims.h", name, 2, flat(pairs), true);
    }
    // effects and specials
    const pk = fxPackages(prg, await readSaturn("MAR_W.CHR"));
    genFx(H, prg, pk);
    genSp(H, pk, await readSaturn("0.BIN"), await readSaturn("STAGE_01.CHR"));
    // sprites
    H.put("mar_sprites.h", "mar_lz_size", 2, sprites.map((sp) => sp[1]));
    sprites.forEach((sp, j) => H.put("mar_sprites.h", "mar_lz_" + j, 1, sp[0]));
    frOut.forEach((fr, f) => {
        if (fr) {
            H.put("mar_spritebank0.h", "spritebank0_" + f, 2, [fr[0] | 0x8000, fr[1], fr[2], fr[3]], true);
        }
    });
    const ps1Pal = (bank: number): number[] => s.palette(bank).map((c, i) => ps1Color(c, i));
    const light = new Array<number>(fdiv(frOut.length + 7, 8)).fill(0);
    frOut.forEach((fr, f) => {
        if (fr && fr[4] === 1) {
            light[f >> 3] |= 1 << (f & 7);
        }
    });
    H.put("mar_pal.h", "mar_pal", 2, ps1Pal(0));
    H.put("mar_pal.h", "mar_pal_light", 2, ps1Pal(1));
    H.put("mar_pal.h", "mar_frame_light", 1, light);
    return H;
}

// header path (build/us/mod/mar/...) -> array name -> numbers: what
// parseCArrays reads from gen_mar_assets.py's headers. Saturn files by
// their disc names (MARIA.PRG, MARIA.CHR, MAR_W.CHR, 0.BIN, STAGE_01.CHR);
// readPs1 is not needed for these
export async function marAssets(readSaturn: Reader, readPs1?: Reader): Promise<MarHeaders> {
    return (await generate(readSaturn, readPs1)).m;
}

// the game-data arrays parseCArrays doesn't read (see the top): with
// readPs1 (BIN/RIC.BIN) also the animations with Richter's timing
export async function marAssetsExtra(readSaturn: Reader, readPs1?: Reader): Promise<MarHeaders> {
    return (await generate(readSaturn, readPs1)).extra;
}

// both at once
export async function marAssetsAll(readSaturn: Reader, readPs1?: Reader): Promise<[MarHeaders, MarHeaders]> {
    const h = await generate(readSaturn, readPs1);
    return [h.m, h.extra];
}
