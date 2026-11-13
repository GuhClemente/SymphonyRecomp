// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn-only stages' sprites: a byte-exact port of
// mods/sotn-ultimate/tools/gdn_assets.py sprites() / pages_only() (the Underground Garden
// "gdn", the Hell Garden "rgdn" with rgdn_assets.py's values) and
// prs_assets.py sprites() (the Cursed Prison "prs", the Soul Prison "rprs"
// with rprs_assets.py's), with what they call: psx_sprites, psx_palettes,
// stage_bin_cluts, allocate, stage_name_pixels, wight_ps1.palettes, and the
// asset tool's builds of their outputs (cmpgfx: sotn_cmpgfx.ts; the sprite
// banks: tools/sotn-assets/assets/spritebanks + spriteset BuildSpriteSet).
//
// Input: the stage_assets.ts result (main()'s files: the WRP sprite_banks /
// graphics_banks / palette_def JSONs sprites() appends to, and its
// SpriteHandoff: the tile pages, the shimmer palette), the Saturn files and
// the retail PS1 files. The PS1 art and palettes the Python reads from the
// decomp's asset PNGs (assets/st/<stage>/*.png, extracted by the asset tool)
// are read from the PS1 overlays at the offsets config/assets.us.yaml
// extracts them from (PS1_ART / PS1_PALS): a cmpgfx PNG's pixels are the
// inflated 4bpp bytes, a palette PNG round-trips to the RGB555 words (STP
// bit as alpha 127) exactly. The PS1 enemies' sprite banks (their
// sprite_banks.json animset) are read from the overlay's sprite bank table.
//
// Output: the files sprites() writes (and the asset tool generates from
// them), by repo path, and the compiled numeric arrays by symbol.
import { roundHalfEven } from "./dsp.ts";
import { pyJsonDumps } from "./pyjson.ts";
import type { PyJson } from "./pyjson.ts";
import { PsxSpriteSheet, SaturnStage, SpriteState } from "./saturn_sprites.ts";
import type { PlaceKey, PsxPart } from "./saturn_sprites.ts";
import { sotnDeflate } from "./sotn_cmpgfx.ts";
import { buildConfig, STAGES, modStageDir } from "./stage_assets.ts";
import type { Bytes, GdnConfig, PageBlock, PrsConfig, Reader, SpriteHandoff } from "./stage_assets.ts";

// a compiled array: element size in bytes, its numbers as written
export interface NumArray {
    size: number;
    values: number[];
}

export interface SpriteSources {
    // disks/saturn files by name ("STAGE_15.PRG", "0.BIN")
    saturn: Reader;
    // the retail PS1 disc's files by path ("ST/NO3/NO3.BIN")
    ps1: Reader;
}

export interface SpriteOptions {
    // config/mod_<stage>_assets.us.yaml (mod data): with it, the build
    // config build/us/<stage>_assets.us.yaml is made too
    assetConfig?: string;
    // rprs: include/game.h's MOD_WIGHT_PS1_SPIRIT / MOD_WIGHT_PS1_GLOW
    // switches (wight_ps1.switches(); default both, as the mod ships)
    wightPs1?: string[];
}

export interface SpriteResult {
    // repo path -> content (assets/st/<st>/us/sprite_banks.json,
    // graphics_banks.json, palette_def.json, mods/sotn-ultimate/src/{garden,prison}/<st>/mod_*.h,
    // src/st/<st>/gen/<block>.h, gen/us/sprites.c, gen/us/sprite_banks.h,
    // build/us/<st>_assets.us.yaml with SpriteOptions.assetConfig)
    files: Map<string, Bytes>;
    // symbol -> numbers, as the C compiler lays them out: the cmpgfx blocks
    // (u8), sprites_<st>_<bank>_<frame> (s16), g_ModBgBlockAnim<n> (u8),
    // g_ModCandleGlow (s16), g_Mod*SpritePals (u16), g_ModSpecterTex (s16)
    arrays: Map<string, NumArray>;
    // the cmpgfx block names, in the build config's order
    blocks: string[];
}

// --------------------------------------------------------------------------
// helpers

const enc = new TextEncoder();
const dec = new TextDecoder("latin1");
const text = (s: string): Bytes => enc.encode(s);
const hex = (v: number, w: number): string => v.toString(16).toUpperCase().padStart(w, "0");
const be16 = (d: Bytes, o: number): number => (d[o] << 8) | d[o + 1];
const bes16 = (d: Bytes, o: number): number => (be16(d, o) << 16) >> 16;
const be32 = (d: Bytes, o: number): number => ((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) >>> 0;
const le16 = (d: Bytes, o: number): number => d[o] | (d[o + 1] << 8);
const les16 = (d: Bytes, o: number): number => (le16(d, o) << 16) >> 16;
const le32 = (d: Bytes, o: number): number => (d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (d[o + 3] << 24)) >>> 0;
const range = (a: number, b: number): number[] => Array.from({ length: b - a }, (_, i) => a + i);
// Python's str.title()
const title = (s: string): string => s.toLowerCase().replace(/[a-z]+/g, (w) => w[0].toUpperCase() + w.slice(1));
const fail = (msg: string): never => {
    throw new Error(msg);
};

type Quad = [number, number]; // (tpage, quadrant)
type Part = PsxPart | Record<string, number>;
type Frame = Part[] | null;
type SpriteBanks = { banks: (Frame[] | null)[]; indices: number[] };

// --------------------------------------------------------------------------
// the PS1 data the Python reads from the decomp's asset PNGs

const ST_BASE = 0x80180000;
const stFile = (stage: string): string => `ST/${stage.toUpperCase()}/${stage.toUpperCase()}.BIN`;

// cmpgfx assets (config/assets.us.yaml): stage/name -> overlay offset
const PS1_ART: Record<string, number> = {
    "chi/bn_gfx_thornweed_corpseweed": 0x70F8, "chi/bn_gfx_stage_name_en": 0x9B38,
    "no0/e_slinger": 0xE87C, "no0/stage_title_no0": 0x48A0,
    "no4/D_80185400": 0x5400, "no4/D_80185994": 0x5994, "no4/D_80186260": 0x6260, "no4/D_80189ECC": 0x9ECC,
    "are/gfx_stage_name_en": 0x521C, "cat/gfx_stage_name_en": 0x80D0, "dai/gfx_stage_name_en": 0x16554,
    "rno4/D_80185E40": 0x5E40, "rno4/D_80186D50": 0x6D50, "rno4/D_801874E8": 0x74E8,
    "rnz0/D_8018DFF0": 0xDFF0, "rnz0/D_8018EF38": 0xEF38, "rnz1/D_8018AF60": 0xAF60,
    "rno3/D_8018C860": 0xC860, "rno3/D_8018D3EC": 0xD3EC, "rno3/e_jacko": 0x92FC,
    "rno3/e_nova_skeleton": 0x9D2C, "rno3/e_nova_laser": 0xAB14, "rno2/D_8018F740": 0xF740,
};
// palette assets: stage/name -> (overlay offset, palettes: the entry's size
// up to the next asset / 32)
const PS1_PALS: Record<string, [number, number]> = {
    "chi/bn_pal_thornweed_corpseweed": [0xA3C0, 4], "no0/e_slinger_pal": [0x174DC, 1],
    "no4/D_8018E094": [0xE094, 1], "no4/D_8018DA54": [0xDA54, 3], "no3/e_explosion_p": [0x18578, 22],
};

// sotn/inflate.go Inflate
export function sotnInflate(src: Bytes): Uint8Array {
    let pos = 8;
    let readHigh = true;
    const out: number[] = [];
    let half = false;
    const rd = (): number => {
        if (readHigh) {
            readHigh = false;
            return (src[pos] >> 4) & 15;
        }
        readHigh = true;
        return src[pos++] & 15;
    };
    const wr = (v: number): void => {
        if (!half) {
            out.push(v);
        } else {
            out[out.length - 1] = (out[out.length - 1] + (v << 4)) & 255;
        }
        half = !half;
    };
    for (;;) {
        const op = rd();
        if (op === 0) {
            let n = rd() << 4;
            n += rd() + 0x13;
            for (; n > 0; n--) {
                wr(0);
            }
        } else if (op === 1) {
            wr(rd());
        } else if (op === 2) {
            const c = rd();
            wr(c);
            wr(c);
        } else if (op === 3) {
            wr(rd());
            wr(rd());
        } else if (op === 4) {
            wr(rd());
            wr(rd());
            wr(rd());
        } else if (op === 5) {
            const c = rd();
            for (let n = rd() + 3; n > 0; n--) {
                wr(c);
            }
        } else if (op === 6) {
            for (let n = rd() + 3; n > 0; n--) {
                wr(0);
            }
        } else if (op === 15) {
            return Uint8Array.from(out);
        } else {
            const d = src[op - 7];
            const v = d & 15;
            if ((d & 0xf0) === 0x10) {
                wr(v);
            } else if ((d & 0xf0) === 0x20) {
                wr(v);
                wr(v);
            } else if ((d & 0xf0) === 0x60) {
                for (let n = v + 3; n > 0; n--) {
                    wr(0);
                }
            }
        }
        if (pos > src.length) {
            throw new Error("cmpgfx: stream overruns its data");
        }
    }
}

class Ps1 {
    reader: Reader;
    cache: Map<string, Bytes>;

    constructor(reader: Reader) {
        this.reader = reader;
        this.cache = new Map();
    }

    async bin(stage: string): Promise<Bytes> {
        const p = stFile(stage);
        let d = this.cache.get(p);
        if (d === undefined) {
            d = await this.reader(p);
            this.cache.set(p, d);
        }
        return d;
    }

    // a 128x128 4bpp cmpgfx asset: its packed bytes (the PNG's pixels,
    // low nibble first)
    async art(stage: string, name: string): Promise<Uint8Array> {
        const off = PS1_ART[stage + "/" + name] ?? fail(`no PS1 art ${stage}/${name}`);
        const px = sotnInflate((await this.bin(stage)).subarray(off));
        if (px.length < 128 * 64) {
            throw new Error(`${stage}/${name}: ${px.length} bytes inflated`);
        }
        return px.slice(0, 128 * 64);
    }

    // a palette asset's 16-colour palettes (gdn_assets.psx_palettes of a name)
    async pals(stage: string, name: string): Promise<number[][]> {
        const [off, n] = PS1_PALS[stage + "/" + name] ?? fail(`no PS1 palette ${stage}/${name}`);
        const d = await this.bin(stage);
        return range(0, n).map((k) => range(0, 16).map((j) => le16(d, off + k * 32 + j * 2)));
    }

    // gdn_assets.stage_bin_cluts: {clut: 16 colours} the overlay's header
    // clut table loads (PAL_BULK entries)
    async binCluts(stage: string): Promise<Map<number, number[]>> {
        const d = await this.bin(stage);
        const rd = (a: number): number => le32(d, a - ST_BASE);
        const out = new Map<number, number[]>();
        const tab = rd(ST_BASE + 0x18);
        for (let i = 0; i < 64; i++) {
            const p = rd(tab + 4 * i);
            if (!(ST_BASE <= p && p < ST_BASE + d.length) || rd(p) !== 5) {
                break;
            }
            let q = p + 4;
            while (rd(q) !== 0xFFFFFFFF) {
                const dst = rd(q);
                const n = rd(q + 4);
                const src = rd(q + 8);
                q += 12;
                for (let k = 0; k < Math.floor(n / 16); k++) {
                    out.set(Math.floor(dst / 16) + k, range(0, 16).map((j) => le16(d, src - ST_BASE + k * 32 + j * 2)));
                }
            }
        }
        return out;
    }

    // gdn_assets.psx_palettes: a palette asset's name, or (first clut, count)
    // of the overlay's clut table
    async palettes(stage: string, pal: string | [number, number]): Promise<number[][]> {
        if (typeof pal === "string") {
            return this.pals(stage, pal);
        }
        const cl = await this.binCluts(stage);
        return range(0, pal[1]).map((k) => cl.get(pal[0] + k) ?? fail(`${stage}: no clut ${pal[0] + k}`));
    }

    // the overlay's sprite bank `animset` (its sprite_banks.json
    // banks[indices[animset]]), as the asset tool reads it
    // (spriteset.ReadSpriteSet)
    async spriteSet(stage: string, animset: number): Promise<Frame[]> {
        const d = await this.bin(stage);
        const u32 = (a: number): number => le32(d, a - ST_BASE);
        const addr = u32(u32(ST_BASE + 0x14) + 4 * animset);
        let earliest = 0x801F8000;
        let cur = addr;
        const offsets: number[] = [];
        while (cur < earliest) {
            const p = u32(cur);
            cur += 4;
            offsets.push(p);
            if (p !== 0) {
                if (!(ST_BASE <= p && p < 0x801F8000)) {
                    throw new Error(`${stage}: sprite offset 0x${p.toString(16)} is not valid`);
                }
                earliest = Math.min(earliest, p);
            }
        }
        return offsets.map((p) => {
            if (p === 0) {
                return null;
            }
            const o = p - ST_BASE;
            const count = le16(d, o);
            if (count >= 0x8000) {
                // the single-sprite format {flags | 0x8000, x, y, w}
                return [{ flags: count, x: les16(d, o + 2), y: les16(d, o + 4), width: le16(d, o + 6), height: 0,
                          clut: 0, tileset: 0, left: 0, top: 0, right: 0, bottom: 0 }];
            }
            return range(0, count).map((k) => {
                const s = o + 2 + k * 22;
                return { flags: le16(d, s), x: les16(d, s + 2), y: les16(d, s + 4), width: le16(d, s + 6),
                         height: le16(d, s + 8), clut: le16(d, s + 10), tileset: le16(d, s + 12),
                         left: le16(d, s + 14), top: le16(d, s + 16), right: le16(d, s + 18), bottom: le16(d, s + 20) };
            });
        });
    }

    // gdn_assets.name_strip: a stage name image's two 116x40 halves side by side
    async nameStrip(stage: string, name: string): Promise<number[][]> {
        const b = await this.art(stage, name);
        const px = (x: number, y: number): number => (x & 1 ? b[y * 64 + (x >> 1)] >> 4 : b[y * 64 + (x >> 1)] & 15);
        return range(0, 40).map((y) => range(0, 232).map((x) => (x < 116 ? px(4 + x, y) : px(8 + x - 116, 64 + y))));
    }
}

// --------------------------------------------------------------------------
// the first-visit area name images (gdn_assets / prs_assets stage_name_pixels)

const W = 232;
const ptKey = (x: number, y: number): number => y * W + x;

// {left x: the shape's points (x, y keys), grown by a pixel}
function glyphMasks(strip: number[][]): Map<number, Set<number>> {
    const seen = new Set<number>();
    const out = new Map<number, Set<number>>();
    for (let x = 0; x < W; x++) {
        for (let y = 0; y < 40; y++) {
            if (strip[y][x] > 1 && !seen.has(ptKey(x, y))) {
                const todo: [number, number][] = [[x, y]];
                const pts: [number, number][] = [];
                seen.add(ptKey(x, y));
                while (todo.length) {
                    const [a, b] = todo.pop()!;
                    pts.push([a, b]);
                    for (const c of [a - 1, a, a + 1]) {
                        for (const d of [b - 1, b, b + 1]) {
                            if (c >= 0 && c < W && d >= 0 && d < 40 && !seen.has(ptKey(c, d)) && strip[d][c] > 1) {
                                seen.add(ptKey(c, d));
                                todo.push([c, d]);
                            }
                        }
                    }
                }
                if (pts.length > 3) {
                    const grown = new Set<number>();
                    for (const [a, b] of pts) {
                        for (const c of [a - 1, a, a + 1]) {
                            for (const d of [b - 1, b, b + 1]) {
                                if (c >= 0 && c < W && d >= 0 && d < 40) {
                                    grown.add(ptKey(c, d));
                                }
                            }
                        }
                    }
                    out.set(Math.min(...pts.map((p) => p[0])), grown);
                }
            }
        }
    }
    return out;
}

// the two halves into the 128x128 image (rows 0-40 x 4-120, rows 64-104 x
// 8-124), packed low nibble first
function nameImage(img: number[][]): Uint8Array {
    const out = new Uint8Array(128 * 64);
    const put = (x: number, y: number, v: number): void => {
        out[y * 64 + (x >> 1)] |= x & 1 ? v << 4 : v;
    };
    for (let y = 0; y < 40; y++) {
        for (let x = 0; x < 116; x++) {
            put(4 + x, y, img[y][x]);
            put(8 + x, 64 + y, img[y][116 + x]);
        }
    }
    return out;
}

// "Underground Garden" (gdn_assets.STAGE_NAME_GLYPHS)
const GDN_NAME_SOURCES: [string, string, string][] = [["no4", "no4", "D_80185400"], ["no0", "no0", "stage_title_no0"]];
const GDN_NAME_GLYPHS: [string, number, number][] = [
    ...[4, 23, 37, 49, 59, 66, 81, 91, 102, 115, 129].map((x): [string, number, number] => ["no4", x, x]),
    ["no0", 121, 152], ["no0", 139, 170], ["no0", 181, 184], ["no4", 37, 196], ["no4", 49, 208], ["no4", 23, 218],
];

async function gdnStageName(ps1: Ps1): Promise<Uint8Array> {
    const strips = new Map<string, number[][]>();
    for (const [k, st, name] of GDN_NAME_SOURCES) {
        strips.set(k, await ps1.nameStrip(st, name));
    }
    const masks = new Map([...strips].map(([k, s]) => [k, glyphMasks(s)] as const));
    const neu = range(0, 40).map(() => new Array<number>(W).fill(0));
    for (const [src, sx, nx] of GDN_NAME_GLYPHS) {
        const strip = strips.get(src)!;
        for (const p of masks.get(src)!.get(sx) ?? fail(`no glyph at ${src} ${sx}`)) {
            const x = p % W;
            const y = Math.floor(p / W);
            const v = strip[y][x];
            const tx = x - sx + nx;
            if (tx >= 0 && tx < W && v > neu[y][tx]) {
                neu[y][tx] = v;
            }
        }
    }
    return nameImage(neu);
}

// "Cursed Prison" (prs_assets.NAME_GLYPHS): source, left x, dropped pixels, gap
const PRS_NAME_SOURCES: [string, string, string][] = [
    ["no4", "no4", "D_80185400"], ["are", "are", "gfx_stage_name_en"], ["cat", "cat", "gfx_stage_name_en"],
    ["chi", "chi", "bn_gfx_stage_name_en"], ["dai", "dai", "gfx_stage_name_en"]];
type Glyph = [string, number, ((x: number, y: number) => boolean) | null, number] | null;
const PRS_NAME_GLYPHS: Glyph[] = [
    ["cat", 55, null, 1], ["no4", 102, null, 1], ["no4", 59, null, 0], ["are", 107, null, 2],
    ["no4", 49, null, 1], ["no4", 37, null, 1], null,
    ["dai", 35, (x, y) => x >= 46 && y >= 19, 1], ["no4", 59, null, 0], ["chi", 181, null, 1],
    ["are", 107, null, 2], ["no4", 91, null, 2], ["no4", 115, null, 0],
];
const NAME_SPACE = 8;

async function prsStageName(ps1: Ps1): Promise<Uint8Array> {
    const strips = new Map<string, number[][]>();
    for (const [k, st, name] of PRS_NAME_SOURCES) {
        strips.set(k, await ps1.nameStrip(st, name));
    }
    const masks = new Map([...strips].map(([k, s]) => [k, glyphMasks(s)] as const));
    const placed: [string, number[], number][] = [];
    let cursor = 0;
    for (const g of PRS_NAME_GLYPHS) {
        if (g === null) {
            cursor += NAME_SPACE;
            continue;
        }
        const [src, sx, drop, gap] = g;
        const strip = strips.get(src)!;
        const pts = [...(masks.get(src)!.get(sx) ?? fail(`no glyph at ${src} ${sx}`))]
            .filter((p) => !(drop && drop(p % W, Math.floor(p / W))));
        const right = Math.max(...pts.filter((p) => strip[Math.floor(p / W)][p % W] > 1).map((p) => p % W));
        placed.push([src, pts, cursor - sx]);
        cursor += right - sx + 1 + gap;
    }
    const shift = Math.floor((W - cursor) / 2);
    const neu = range(0, 40).map(() => new Array<number>(W).fill(0));
    for (const [src, pts, dx] of placed) {
        const strip = strips.get(src)!;
        for (const p of pts) {
            const x = p % W;
            const y = Math.floor(p / W);
            const v = strip[y][x];
            const tx = x + dx + shift;
            if (tx >= 0 && tx < W && v > neu[y][tx]) {
                neu[y][tx] = v;
            }
        }
    }
    return nameImage(neu);
}

// --------------------------------------------------------------------------
// the stages' sprite configurations (the modules' globals sprites() reads)

// (group, Saturn bank, frame table, frame[, dy[, palette shift]]) or null
type SetEntry = [string, number, number, number, number?, number?] | null;

interface GdnSprites {
    kind: "gdn";
    stage: string;
    base: GdnConfig;
    spriteGroups: [string, string[]][];
    paletteBlocks: [string, string, number][]; // name, group, bank
    spriteSets: [number, SetEntry[] | "decorations"][];
    decorations: [number, number];
    decorationGroups: string[];
    quadrants: Quad[];
    // name, stage, animset there, art blocks, palette, contexts, quadrant in
    // its tpage or null, the stage's animset
    psxSprites: [string, string, number, string[], string | [number, number], string[], number | null, number][];
    candleGlow: [number, number, number];
    glowFlip: number;
    bleedBanks: number[];
    stpBanks: number[];
    trimBanks: [number, number[]][];
    puffPalette: boolean;
    psxSpriteSymbols: boolean;
    stageNameContexts: string[];
}

interface PrsSprites {
    kind: "prs";
    stage: string;
    base: PrsConfig;
    spriteGroups: [string, number[]][];
    paletteBlocks: [string, string, number][];
    quadrants: Quad[];
    bank16: [number, number];
    candleFrames: number[];
    candleGlow: [number, number][];
    glowFlip: number;
    decorations: [number, number];
    // animset, group, (bank, frames, count): the stage's groups only
    enemies: [number, string, [number, number, number]][];
    // animset, group, (bank, frames, count), dy
    enemySets: [number, string, [number, number, number], number][];
    texFrames: [string, number, number][];
    // name, stage, animset there, art, palettes, rooms, the stage's animset[, quadrant]
    psxSprites: [string, string, number, string[], string[] | [number, number], number[], number, number?][];
    stpBanks: number[];
    stageNameRooms: number[];
    wightPal: number | null; // rprs EXTRA_PALS: the Wight's bank
}

const GDN_QUADRANTS: Quad[] = [...[18, 19].flatMap((t) => range(0, 4).map((q): Quad => [t, q])), [20, 1], [20, 2], [20, 3]];
const PRS_QUADRANTS: Quad[] = [18, 19, 20, 21, 22, 23].flatMap((t) => range(0, 4).map((q): Quad => [t, q]));
const GDN_BANK16: [number, number] = [0x060F0858, 0x060F079C];
const TREE_FRAMES = new Set([...range(1, 21), 23, ...range(26, 34), 35, 47, ...range(54, 67), ...range(68, 74), 87, 88]);
const DECORATION_GROUPS = ["candles", "candles", "scene", "scene", "scene", "scene"];

const GDN_SPRITES: GdnSprites = {
    kind: "gdn", stage: "gdn", base: STAGES.gdn as GdnConfig,
    spriteGroups: [["candles", ["0W", "0E", "1", "3"]], ["scene", ["2"]], ["drips", ["1", "3"]],
        ["gargoyles", ["0W", "0E"]], ["skbreeder", ["0W"]], ["leaper", ["0W", "0E"]], ["gardener", ["1"]],
        ["tree", ["0E"]], ["maneater", ["3"]], ["boss", ["2"]]],
    paletteBlocks: [["GARGOYLE", "gargoyles", 0x060F2ED8], ["TREE", "tree", 0x060F6DC4]],
    spriteSets: [
        [2, [0x26, 0x27, 0x28, 0x29, 0x2A].map((f): SetEntry => ["candles", ...GDN_BANK16, f])],
        [3, "decorations"],
        [4, [1, 2, 3, 0x13, 0x14, 0x15, 0x16, 0x17].map((f): SetEntry => ["drips", ...GDN_BANK16, f])],
        [7, range(1, 60).map((f): SetEntry => ["gargoyles", 0x060F2ED8, 0x060F4018, f])],
        [8, range(1, 38).map((f): SetEntry => ["skbreeder", 0x060FA7B8, 0x060FAF64, f,
            f >= 13 && f <= 31 ? 32 : f <= 12 ? 16 : 0])],
        [9, range(1, 26).map((f): SetEntry => ["leaper", 0x060F82C8, 0x060F86CC, f])],
        [10, range(1, 46).map((f): SetEntry => ["gardener", 0x060F96D8, 0x060FA59C, f])],
        [11, range(1, 89).map((f): SetEntry => (TREE_FRAMES.has(f)
            ? ["tree", 0x060F6DC4, 0x060F7EFC, f, 0, f >= 54 && f <= 66 ? -3 : 0] : null))],
        [12, range(1, 56).map((f): SetEntry => ["maneater", 0x060F8BFC, 0x060F9470, f])],
        [13, range(1, 87).map((f): SetEntry => ["boss", 0x060F43C4, 0x060F6A90, f])],
        [14, [["boss", ...GDN_BANK16, 46]]],
    ],
    decorations: [0x060F1F90, 6],
    decorationGroups: DECORATION_GROUPS,
    quadrants: GDN_QUADRANTS,
    psxSprites: [
        ["THORNWEED", "chi", 7, ["bn_gfx_thornweed_corpseweed"], "bn_pal_thornweed_corpseweed", ["1", "3"], 0, 5],
        ["SLINGER", "no0", 10, ["e_slinger"], "e_slinger_pal", ["0W", "0E"], null, 6],
    ],
    candleGlow: [0x060F0858, 0x060F1F16, 4],
    glowFlip: 0,
    bleedBanks: [0x060F82C8, 0x060F6DC4],
    stpBanks: [],
    trimBanks: [],
    puffPalette: true,
    psxSpriteSymbols: false,
    stageNameContexts: ["0W", "0E"],
};

const RBANK16: [number, number] = [0x060ECC88, 0x060ECBCC];
const IFRIT_BANK = 0x060F04E4;
const RGDN_SPRITES: GdnSprites = {
    kind: "gdn", stage: "rgdn", base: STAGES.rgdn as GdnConfig,
    spriteGroups: [["candles", ["0", "1", "3"]], ["scene", ["2"]], ["skguardian", ["3"]], ["copper", ["0"]],
        ["ifrit", ["0"]]],
    paletteBlocks: [["IFRIT", "ifrit", IFRIT_BANK]],
    spriteSets: [
        [2, [0x26, 0x27, 0x28, 0x29, 0x2A].map((f): SetEntry => ["candles", ...RBANK16, f])],
        [3, "decorations"],
        [8, range(1, 27).map((f): SetEntry => ["skguardian", 0x060F2E08, 0x060F33B4, f, f <= 18 ? 32 : 0])],
        [9, range(1, 61).map((f): SetEntry => ["copper", 0x060EF3F4, 0x060F0240, f, 0])],
        [10, range(1, 82).map((f): SetEntry => (f === 70 || f === 81 ? null : ["ifrit", IFRIT_BANK, 0x060F1648, f, 0]))],
    ],
    decorations: [0x060EE478, 6],
    decorationGroups: DECORATION_GROUPS,
    quadrants: GDN_QUADRANTS,
    psxSprites: [
        ["CAVE_TROLL", "rno4", 4, ["D_80185E40", "D_80186D50", "D_801874E8"], [0x227, 2], ["0"], null, 4],
        ["BITTERFLY", "rnz0", 10, ["D_8018DFF0", "D_8018EF38"], [0x247, 5], ["0", "3"], null, 5],
        ["CLOAKED_KNIGHT", "rnz1", 6, ["D_8018AF60"], [0x210, 5], ["1"], null, 6],
        ["VENUS_WEED", "rno3", 7, ["D_8018C860", "D_8018D3EC"], [0x240, 41], ["1", "3"], 0, 7],
    ],
    candleGlow: [RBANK16[0], 0x060EE416, 4],
    glowFlip: 3,
    bleedBanks: [],
    stpBanks: [IFRIT_BANK],
    trimBanks: [[IFRIT_BANK, [70]]],
    puffPalette: true,
    psxSpriteSymbols: true,
    stageNameContexts: [],
};

const PRS_SPECTER: [number, number, number] = [0x060ECB58, 0x060ECE10, 29];
const PRS_WISP: [number, number, number] = [0x060EEC7C, 0x060EEFBC, 41];
const CANDLE_FRAMES = [1, 2, 3, 4, 6, 7, 8, 9, 0x0C, 0x0A, 0x05];
const PRS_SPRITES: PrsSprites = {
    kind: "prs", stage: "prs", base: STAGES.prs as PrsConfig,
    spriteGroups: [["objects", [0, 1]], ["gargoyles", [1]], ["specters", [1]], ["wisps", [1]], ["switch", [0]]],
    paletteBlocks: [["GARGOYLE", "gargoyles", 0x060ECEF0], ["SPECTER", "specters", PRS_SPECTER[0]],
        ["WISP", "wisps", PRS_WISP[0]]],
    quadrants: PRS_QUADRANTS,
    bank16: [0x060E8F60, 0x060E8F08],
    candleFrames: CANDLE_FRAMES,
    candleGlow: [...range(0, 4).map((f): [number, number] => [0x060EA13C, f]),
        ...range(0, 4).map((f): [number, number] => [0x060EA114, f])],
    glowFlip: 0,
    decorations: [0x060EA1CC, 6],
    enemies: [[7, "gargoyles", [0x060ECEF0, 0x060EE030, 59]], [10, "specters", PRS_SPECTER],
        [11, "wisps", PRS_WISP], [12, "switch", [0x060E900C, 0x060E9000, 2]]],
    enemySets: [],
    texFrames: [],
    psxSprites: [
        ["SLINGER", "no0", 10, ["e_slinger"], ["e_slinger_pal"], [0], 6],
        ["BONE_ARCHER", "no4", 8, ["D_80189ECC"], ["D_8018E094"], [0], 8],
        ["SPEAR_GUARD", "no4", 2, ["D_80185994", "D_80186260"], ["D_8018DA54"], [0], 9],
    ],
    // prs_assets.py's __main__ (the Specter's and the Wisps' additive colours)
    stpBanks: [PRS_SPECTER[0], PRS_WISP[0]],
    stageNameRooms: [0, 1],
    wightPal: null,
};

const RPRS_WIGHT: [number, number, number] = [0x060EA3BC, 0x060ECA7C, 100];
const RPRS_WRAITH: [number, number, number] = [0x060ECFA0, 0x060ED788, 45];
const RPRS_SPRITES: PrsSprites = {
    kind: "prs", stage: "rprs", base: STAGES.rprs as PrsConfig,
    spriteGroups: [["objects", [0, 1]], ["gargoyles", [0]], ["wights", [0, 1]], ["wraiths", [1]]],
    paletteBlocks: [["GARGOYLE", "gargoyles", 0x060EDE90], ["WIGHT", "wights", RPRS_WIGHT[0]]],
    quadrants: PRS_QUADRANTS,
    bank16: [0x060E88F0, 0x060E8898],
    candleFrames: CANDLE_FRAMES,
    candleGlow: [...range(0, 4).map((f): [number, number] => [0x060E99A0, f]),
        ...range(0, 4).map((f): [number, number] => [0x060E9978, f])],
    glowFlip: 3,
    decorations: [0x060E9A30, 6],
    enemies: [[7, "gargoyles", [0x060EDE90, 0x060EEFD0, 59]]],
    enemySets: [[8, "wights", RPRS_WIGHT, 32], [9, "wraiths", RPRS_WRAITH, 32]],
    texFrames: [["WIGHT_SPIRIT", 8, 71]],
    psxSprites: [
        ["FLYING_ZOMBIE", "rno2", 7, ["D_8018F740"], [0x21E, 2], [0], 4],
        ["JACK_O_BONES", "rno3", 4, ["e_jacko"], [0x20C, 2], [0], 5],
        ["NOVA_SKELETON", "rno3", 5, ["e_nova_skeleton", "e_nova_laser"], [0x214, 5], [0], 6, 0],
    ],
    stpBanks: [RPRS_WIGHT[0], RPRS_WRAITH[0]],
    stageNameRooms: [],
    wightPal: RPRS_WIGHT[0],
};

export const SPRITE_STAGES: Record<string, GdnSprites | PrsSprites> = {
    gdn: GDN_SPRITES, rgdn: RGDN_SPRITES, prs: PRS_SPRITES, rprs: RPRS_SPRITES,
};

const SPRITE_CLUT_BASE = 0x210;
const GLOW_COLOURS = [...new Array<number>(10).fill(0), 0x8044, 0x8067, 0x808A, 0x80CE, 0x8112, 0];

// --------------------------------------------------------------------------
// shared pieces

// the three passes: the images each group's frames ask for, measured packed
// tallest first, then packed into the quadrants the allocation gives
// (growing a group that doesn't fit and allocating again)
function pack<K extends string>(st: SaturnStage, state: SpriteState, groups: K[], quadrants: Quad[],
                                fill: (sheets: Map<K, PsxSpriteSheet>) => void,
                                allocate: (sizes: Map<K, number>) => Map<K, Quad[]>):
                                { sheets: Map<K, PsxSpriteSheet>; cluts: number[][] } {
    const many: Quad[] = new Array(16).fill(0).map((): Quad => [0, 0]);
    const seen = new Map(groups.map((g) => [g, new PsxSpriteSheet(many, SPRITE_CLUT_BASE, null, state)] as const));
    fill(seen);
    const sizes = new Map<K, number>();
    for (const g of groups) {
        const probe = new PsxSpriteSheet([...quadrants, ...quadrants], SPRITE_CLUT_BASE, null, state);
        probe.prepack(st, seen.get(g)!.requests);
        sizes.set(g, probe.usedQuadrants());
    }
    for (;;) {
        const given = allocate(sizes);
        const cluts: number[][] = [];
        const sheets = new Map(groups.map((g) => [g, new PsxSpriteSheet(given.get(g)!, SPRITE_CLUT_BASE, cluts, state)] as const));
        let short: K | null = null;
        for (const g of groups) {
            try {
                sheets.get(g)!.prepack(st, seen.get(g)!.requests as PlaceKey[]);
            } catch (e) {
                if (!(e instanceof Error) || e.message !== "sprite quadrants full") {
                    throw e;
                }
                short = g;
                break;
            }
        }
        if (short === null) {
            return { sheets, cluts };
        }
        sizes.set(short, sizes.get(short)! + 1);
    }
}

function newState(bleed: number[], stp: number[], trim: [number, number[]][]): SpriteState {
    const s = new SpriteState();
    bleed.forEach((b) => s.bleedBanks.add(b));
    stp.forEach((b) => s.stpBanks.add(b));
    trim.forEach(([b, keep]) => s.trimBanks.set(b, new Set(keep)));
    return s;
}

const shifted = (parts: PsxPart[], dy: number): PsxPart[] => parts.map((p) => ({ ...p, y: p.y + dy }));

// the PSX quadrant of a part: (u, v) in its tpage
const tpageUV = (p: PsxPart): [number, number] => [p.left + ((p.tileset & 3) & 1) * 128, p.top + ((p.tileset & 3) >> 1) * 128];

function glowHeader(gen: string, what: string, glow: PsxPart[]): [string, number[]] {
    let s = gen + "// clang-format off\n" + what + "static const s16 g_ModCandleGlow[][8] = {\n";
    const vals: number[] = [];
    for (const p of glow) {
        const [u, v] = tpageUV(p);
        s += `    {${p.x}, ${p.y}, ${p.width}, ${p.height}, ${u}, ${v}, 0x${p.clut.toString(16).toUpperCase()}, ${p.tileset >> 2}},\n`;
        vals.push(p.x, p.y, p.width, p.height, u, v, p.clut, p.tileset >> 2);
    }
    return [s + "};\n// clang-format on\n", vals];
}

function bgBlocks(gen: string, zs: number[], arrays: Map<string, NumArray>): string {
    let s = gen + "// clang-format off\n// the Saturn's decorations (entity 0x11): ANIMSET_OVL(3) frame n + 1\n";
    zs.forEach((_, v) => {
        s += `static u8 g_ModBgBlockAnim${v}[] = {0x40, ${v + 1}, 0xFF, 0x00};\n`;
        arrays.set(`g_ModBgBlockAnim${v}`, { size: 1, values: [0x40, v + 1, 0xFF, 0x00] });
    });
    s += "ObjInit2 BackgroundBlockInit[] = {\n";
    zs.forEach((z, v) => {
        s += `    {.animSet = ANIMSET_OVL(3), .zPriority = 0x${hex(z, 2)}, .unk5A = 0, .palette = 0,\n` +
            "     .drawFlags = ENTITY_DEFAULT, .blendMode = BLEND_NO, .flags = 0,\n" +
            `     .animFrames = g_ModBgBlockAnim${v}},\n`;
    });
    return s + "};\n// clang-format on\n";
}

function palsHeader(gen: string, what: string, sym: string, pals: number[][], arrays: Map<string, NumArray>): string {
    let s = gen + "// clang-format off\n" + what + `u16 ${sym}[] = {\n`;
    for (const p of pals) {
        s += "    " + p.map((c) => "0x" + hex(c, 4)).join(", ") + ",\n";
    }
    arrays.set(sym, { size: 2, values: pals.flat() });
    return s + "};\n// clang-format on\n";
}

function gfxHeader(gen: string, what: string, names: string[]): string {
    let s = gen + "// clang-format off\n" + what;
    for (const n of names) {
        s += `u8 ${n}[] __attribute__((aligned(4))) = {\n#include "gen/${n}.h"\n};\n`;
    }
    return s + "// clang-format on\n";
}

// the cmpgfx blocks: their gen/<name>.h (util.WriteBytesAsHex) and arrays
function cmpgfxBlocks(stage: string, blocks: [string, Uint8Array][], files: Map<string, Bytes>,
                      arrays: Map<string, NumArray>): void {
    const H = "0123456789ABCDEF";
    for (const [name, data] of blocks) {
        const c = sotnDeflate(data);
        let s = "// clang-format off\n";
        c.forEach((b, i) => {
            s += "0x" + H[b >> 4] + H[b & 15] + ",";
            if ((i & 15) === 15) {
                s += "\n";
            }
        });
        files.set(`${modStageDir(stage)}gen/${name}.h`, text(s + "\n"));
        arrays.set(name, { size: 1, values: [...c] });
    }
}

// the asset tool's sprite_banks build (spritebanks.buildSprites,
// spriteset.BuildSpriteSet): gen/us/sprites.c and gen/us/sprite_banks.h
function spriteBanksC(stage: string, sb: SpriteBanks, files: Map<string, Bytes>, arrays: Map<string, NumArray>): void {
    if (sb.indices.length !== 24 && sb.indices.length !== 25) {
        throw new Error(`the number of banks must be exactly 24, got ${sb.indices.length}`);
    }
    let h = "// clang-format off\n";
    let c = "// clang-format off\n#include \"common.h\"\n";
    const symbols: string[] = [];
    sb.banks.forEach((set, i) => {
        if (set === null || set.length === 0) {
            symbols.push("");
            return;
        }
        const sym = `sprites_${stage}_${i}`;
        h += `extern s16* ${sym}[];\n`;
        const player = (g: Part[]): boolean => g.length === 1 && g[0].flags >= 0x8000;
        const names = set.map((g, j) => (g === null ? "" : `${sym}_${j}`));
        set.forEach((g, j) => {
            if (g !== null) {
                const size = player(g) ? 4 : g.length * 11 + 1 + (g.length & 1 ? 2 : 1);
                c += `static s16 ${names[j]}[${size}];\n`;
            }
        });
        c += `s16* ${sym}[] = {\n` + names.map((n) => `    ${n || "0"},\n`).join("") + "};\n";
        set.forEach((g, j) => {
            if (g === null) {
                return;
            }
            if (player(g)) {
                const s = g[0];
                c += `static s16 ${names[j]}[] = {0x${hex(s.flags & 0x7FFF, 2)} | 0x8000, ${s.x}, ${s.y}, ${s.width}};\n`;
                return;
            }
            const vals = [g.length];
            c += `static s16 ${names[j]}[] = {\n    ${g.length},\n`;
            for (const s of g) {
                const v = [s.flags, s.x, s.y, s.width, s.height, s.clut, s.tileset, s.left, s.top, s.right, s.bottom];
                c += "    " + v.join(", ") + ",\n";
                vals.push(...v);
            }
            c += g.length & 1 ? "    0, 0\n" : "    0\n";
            vals.push(...(g.length & 1 ? [0, 0] : [0]));
            c += "};\n";
            arrays.set(names[j], { size: 2, values: vals });
        });
        symbols.push(sym);
    });
    h += "s16** spriteBanks[] = {\n";
    for (const index of sb.indices) {
        h += index >= 0 ? `    ${symbols[index]},\n` : "    0,\n";
    }
    h += "};\n";
    files.set(`${modStageDir(stage)}gen/us/sprites.c`, text(c));
    files.set(`${modStageDir(stage)}gen/us/sprite_banks.h`, text(h));
}

const parse = (files: Map<string, Bytes>, path: string): unknown =>
    JSON.parse(dec.decode(files.get(path) ?? fail(`main()'s ${path} is missing`)));
const dumps = (v: unknown): Bytes => text(pyJsonDumps(v as PyJson, 2));

async function saturnStage(sat: Reader, stem: string): Promise<SaturnStage> {
    return new SaturnStage(await sat(stem + ".PRG"), await sat(stem + ".CHR"), await sat("0.BIN"));
}

// GFX_ENTRY: VRAM y first, then x
const gfxEntry = ([tpage, quad]: Quad, name: string): Record<string, PyJson> =>
    ({ x: (tpage >> 4) * 256 + (quad >> 1) * 128, y: (tpage & 15) * 64 + (quad & 1) * 32, width: 128, height: 128, name });

// --------------------------------------------------------------------------
// gdn_assets.py sprites()

const GEN_GDN = "// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n";

async function gdnSprites(cfg: GdnSprites, src: SpriteSources, mainFiles: Map<string, Bytes>,
                          handoff: SpriteHandoff, opts: SpriteOptions): Promise<SpriteResult> {
    const base = cfg.base;
    const out = `assets/st/${cfg.stage}/`;
    const srcDir = modStageDir(cfg.stage);
    const files = new Map<string, Bytes>();
    const arrays = new Map<string, NumArray>();
    const ps1 = new Ps1(src.ps1);
    const st = await saturnStage(src.saturn, base.sat);
    const prg = st.prg;
    const pb = (a: number): number => a - st.base;

    // the decorations' variants: (group, bank, frames, frame) and z
    const decos: [SetEntry, number][] = [];
    for (let v = 0; v < cfg.decorations[1]; v++) {
        const o = pb(cfg.decorations[0]) + v * 28;
        const anim = be32(prg, o + 24);
        const z = be16(prg, o + 10);
        decos.push([[cfg.decorationGroups[v], be32(prg, o), be32(prg, o + 4), prg[pb(anim) + 1]],
                    z === 0x20 ? 0x67 + prg[o + 12] : z]);
    }
    const sets = cfg.spriteSets.map(([n, f]): [number, SetEntry[]] => [n, f === "decorations" ? decos.map((d) => d[0]) : f]);
    const [glowBank, glowRecs, glowN] = cfg.candleGlow;
    const glowImgs = range(0, glowN).map((f) => {
        const o = pb(glowRecs) + f * 10;
        return [bes16(prg, o), bes16(prg, o + 2), bes16(prg, o + 4)];
    });
    const state = newState(cfg.bleedBanks, cfg.stpBanks, cfg.trimBanks);

    let banks: [number, (PsxPart[] | null)[]][] = [];
    let glow: PsxPart[] = [];
    const fill = (sheets: Map<string, PsxSpriteSheet>): void => {
        for (const [, g, bank] of cfg.paletteBlocks) {
            sheets.get(g)!.paletteBlock(st, bank);
        }
        banks = sets.map(([n, frames]) => [n, frames.map((e) => (e === null ? null
            : shifted(sheets.get(e[0])!.frame(st, e[1], e[2], e[3], e[5] ?? 0), e[4] ?? 0)))]);
        glow = glowImgs.map(([dx, dy, img]) => sheets.get("candles")!.imagePart(st, glowBank, img, dx, dy, GLOW_COLOURS, cfg.glowFlip));
    };
    const groups = cfg.spriteGroups.map((g) => g[0]);
    const contexts = new Map(cfg.spriteGroups);
    let slots = new Map<string, number[]>();
    // gdn_assets.allocate: whole quadrants, never shared within a context;
    // a PS1 enemy's blocks take consecutive tilesets
    const allocate = (items: [string, number, string[], number | null, boolean][]): Map<string, number[]> => {
        const used = new Map(base.contextBanks.map(([c]) => [c, new Set<number>()] as const));
        const res = new Map<string, number[]>();
        const order = items.slice().sort((a, b) => b[2].length - a[2].length || b[1] - a[1]);
        for (const [key, n, ctx, quad, consecutive] of order) {
            const usedIn = (i: number): boolean => ctx.some((c) => (used.get(c) ?? fail(`context ${c}`)).has(i));
            let free = range(0, cfg.quadrants.length).filter((i) => !usedIn(i) && (quad === null || cfg.quadrants[i][1] === quad));
            if (consecutive && n > 1) {
                const tiles = cfg.quadrants.map(([t, q]) => (t << 2) | q);
                const at = (tile: number): number => cfg.quadrants.findIndex(([t, q]) => t === tile >> 2 && q === (tile & 3));
                const runs = free.filter((i) => range(0, n).every((k) => at(tiles[i] + k) >= 0))
                    .map((i) => range(0, n).map((k) => at(tiles[i] + k)))
                    .filter((r) => r.every((j) => !usedIn(j)));
                if (!runs.length) {
                    throw new Error(`no consecutive sprite quadrants for ${key}`);
                }
                free = runs[0];
            }
            if (free.length < n) {
                throw new Error(`sprite quadrants full: ${key}`);
            }
            res.set(key, free.slice(0, n));
            for (const c of ctx) {
                free.slice(0, n).forEach((i) => used.get(c)!.add(i));
            }
        }
        return res;
    };
    const { sheets, cluts } = pack(st, state, groups, cfg.quadrants, fill, (sizes) => {
        slots = allocate([...groups.map((g): [string, number, string[], number | null, boolean] => [g, sizes.get(g)!, contexts.get(g)!, null, false]),
            ...cfg.psxSprites.map((p): [string, number, string[], number | null, boolean] => [p[0], p[3].length, p[5], p[6], true])]);
        return new Map(groups.map((g) => [g, slots.get(g)!.map((i) => cfg.quadrants[i])] as const));
    });
    fill(sheets);

    // the sprite banks: the converted sets, then the PS1 enemies' (psx_sprites)
    const sb = parse(mainFiles, out + "us/sprite_banks.json") as SpriteBanks;
    for (const [animset, frames] of banks) {
        sb.banks.push([null, ...frames]);
        sb.indices[animset] = sb.banks.length - 1;
    }
    const psx: { name: string; quadrants: Quad[]; animset: number; bank: number; blocks: Uint8Array[]; palettes: number[][] }[] = [];
    for (const [name, stage, animset, art, pal, , , gdnAnimset] of cfg.psxSprites) {
        sb.banks.push(await ps1.spriteSet(stage, animset));
        sb.indices[gdnAnimset] = sb.banks.length - 1;
        const blocks: Uint8Array[] = [];
        for (const a of art) {
            blocks.push(await ps1.art(stage, a));
        }
        psx.push({ name, quadrants: slots.get(name)!.map((i) => cfg.quadrants[i]), animset: gdnAnimset,
                   bank: sb.banks.length - 1, blocks, palettes: await ps1.palettes(stage, pal) });
    }
    files.set(out + "us/sprite_banks.json", dumps(sb));
    files.set(srcDir + "mod_bg_blocks.h", text(bgBlocks(GEN_GDN, decos.map((d) => d[1]), arrays)));

    // the art blocks, then one graphics bank per context
    const blocks: [Quad | null, string, Uint8Array, string[]][] = [];
    for (const g of groups) {
        const sh = sheets.get(g)!;
        for (let k = 0; k < sh.usedQuadrants(); k++) {
            blocks.push([sh.quadrants[k], `g_ModGdn_${g}${k}`, sh.quadrantPixels(k), contexts.get(g)!]);
        }
    }
    psx.forEach((p, n) => {
        p.blocks.forEach((px, k) => {
            blocks.push([p.quadrants[k], `g_ModGdn${title(p.name)}Gfx${p.blocks.length > 1 ? k : ""}`, px,
                         cfg.psxSprites[n][5]]);
        });
    });
    if (cfg.stageNameContexts.length) {
        blocks.push([[17, 0], "g_ModGdnStageName", await gdnStageName(ps1), cfg.stageNameContexts]);
    }
    const ctxBanks = new Map(base.contextBanks);
    if (ctxBanks.has("0W") && ctxBanks.has("0E")) {
        let s = GEN_GDN;
        const names = new Map<string, [string, Quad][]>();
        for (const [side, other] of [["0W", "0E"], ["0E", "0W"]]) {
            names.set(side, blocks.filter((b) => b[3].includes(side) && !b[3].includes(other)).map((b) => [b[1], b[0]!]));
        }
        for (const [n] of [...names.get("0W")!, ...names.get("0E")!]) {
            s += `extern u8 ${n}[];\n`;
        }
        for (const [side, label] of [["0W", "West"], ["0E", "East"]]) {
            s += `static const ModZoneBlock g_ModZone${label}[] = {\n`;
            for (const [n, [tpage, quad]] of names.get(side)!) {
                s += `    {${n}, ${(tpage & 15) * 64 + (quad & 1) * 32}, ${(tpage >> 4) * 256 + (quad >> 1) * 128}},\n`;
            }
            s += "};\n";
        }
        files.set(srcDir + "mod_zone_stream.h", text(s));
    }
    const gb = parse(mainFiles, out + "graphics_banks.json") as { banks: { kind: number; entries: PyJson[] }[]; indices: number[] };
    for (const [c, index] of base.contextBanks) {
        gb.banks.push({ kind: 4, entries: blocks.filter((b) => b[3].includes(c)).map((b) => gfxEntry(b[0]!, b[1])) });
        gb.indices[index] = gb.banks.length - 1;
    }
    const pageBlocks: PageBlock[] = handoff.pageBlocks;
    if (!pageBlocks.length) {
        throw new Error("backdrop() runs first");
    }
    const pages = pageBlocks.map(([vx, vy, name]) => ({ x: vy, y: vx, width: 128, height: 128, name }));
    gb.banks.push({ kind: 4, entries: pages });
    gb.indices[handoff.pageBank] = gb.banks.length - 1;
    for (const [c, index] of base.reloadBanks) {
        gb.banks.push({ kind: 4, entries: [...gb.banks[gb.indices[ctxBanks.get(c)!]].entries, ...pages] });
        gb.indices[index] = gb.banks.length - 1;
    }
    files.set(out + "graphics_banks.json", dumps(gb));
    const all: [string, Uint8Array][] = [...blocks.map((b): [string, Uint8Array] => [b[1], b[2]]),
        ...pageBlocks.map(([, , name, data]): [string, Uint8Array] => [name, data])];
    cmpgfxBlocks(cfg.stage, all, files, arrays);
    if (opts.assetConfig !== undefined) {
        files.set(`build/us/${cfg.stage}_assets.us.yaml`, text(buildConfig(opts.assetConfig, all.map((b) => b[0]))));
    }

    const psxPals = psx.flatMap((p) => p.palettes);
    let puffClut: number | null = null;
    if (cfg.puffPalette) {
        // NO3's opaque puff palettes (e_explosion_p): the fire is the fifth
        psxPals.push((await ps1.pals("no3", "e_explosion_p"))[4]);
        puffClut = SPRITE_CLUT_BASE + cluts.length + psxPals.length - 1;
    }
    let shimmerClut: number | null = null;
    if (base.shimmerSpritePal && handoff.shimmerPalette.length) {
        psxPals.push(handoff.shimmerPalette.slice());
        shimmerClut = SPRITE_CLUT_BASE + cluts.length + psxPals.length - 1;
    }
    const pd = parse(mainFiles, out + "palette_def.json") as { defs: { entries: PyJson[] }[] };
    pd.defs[0].entries.push({ destination: SPRITE_CLUT_BASE * 16, length: 16 * (cluts.length + psxPals.length),
                              name: "g_ModGdnSpritePals.palbin" });
    files.set(out + "palette_def.json", dumps(pd));

    files.set(srcDir + "mod_sprite_gfx.h", text(gfxHeader(GEN_GDN,
        "// the Garden's sprites, 4bpp 128x128 quadrants of tpages 18-19,\n" +
        "// compressed by the asset tool from assets/st/gdn/<name>.png\n", all.map((b) => b[0]))));
    files.set(srcDir + "mod_sprite_pals.h", text(palsHeader(GEN_GDN,
        `// the Garden's sprite palettes, g_ClutIds 0x${SPRITE_CLUT_BASE.toString(16).toUpperCase()} on\n`,
        "g_ModGdnSpritePals", [...cluts, ...psxPals], arrays)));
    let h = GEN_GDN + "// the Saturn enemies' palette blocks (PALETTE_BLOCKS)\n";
    for (const [name, g, bank] of cfg.paletteBlocks) {
        h += `#define MOD_${name}_PAL 0x${hex(sheets.get(g)!.blocks.get(bank)!, 3)}\n`;
    }
    if (puffClut !== null) {
        h += "// the boss's death fire palette (PAL_PUFF_OPAQUE_OFFSET + 4)\n" +
            `#define MOD_PUFF_OPAQUE_OFFSET 0x${hex(puffClut - 4, 3)}\n`;
    }
    if (shimmerClut !== null) {
        h += `// the water shimmer's palette (SHIMMER_SPRITE_PAL)\n#define MOD_SHIMMER_PAL 0x${hex(shimmerClut, 3)}\n`;
    }
    h += "// the PS1 enemies' sprites in the Garden (PSX_SPRITES)\n";
    let clut = SPRITE_CLUT_BASE + cluts.length;
    for (const p of psx) {
        const [tpage, quad] = p.quadrants[0];
        h += `#define MOD_${p.name}_ANIMSET ANIMSET_OVL(${p.animset})\n` +
            `#define MOD_${p.name}_UNK5A 0x${hex((tpage << 2) | quad, 2)}\n` +
            `#define MOD_${p.name}_TPAGE 0x${hex(tpage, 2)}\n` +
            `#define MOD_${p.name}_PAL 0x${hex(clut, 3)}\n`;
        if (cfg.psxSpriteSymbols) {
            h += `#define MOD_${p.name}_SPRITES sprites_${cfg.stage}_${p.bank}\n`;
        }
        clut += p.palettes.length;
    }
    files.set(srcDir + "mod_psx_sprites.h", text(h));
    const [gh, gv] = glowHeader(GEN_GDN, "// per candle flame frame: x, y, w, h, u, v, clut, tpage\n", glow);
    files.set(srcDir + "mod_candle_glow.h", text(gh));
    arrays.set("g_ModCandleGlow", { size: 2, values: gv });
    spriteBanksC(cfg.stage, sb, files, arrays);
    return { files, arrays, blocks: all.map((b) => b[0]) };
}

// gdn_assets.py pages_only() (WITH_SPRITES off): the tile pages alone in
// graphics bank PAGE_BANK
export function pagesOnly(stage: string, mainFiles: Map<string, Bytes>, handoff: SpriteHandoff,
                          opts: SpriteOptions = {}): SpriteResult {
    const out = `assets/st/${stage}/`;
    const files = new Map<string, Bytes>();
    const arrays = new Map<string, NumArray>();
    if (!handoff.pageBlocks.length) {
        throw new Error("backdrop() runs first");
    }
    const gb = parse(mainFiles, out + "graphics_banks.json") as { banks: PyJson[]; indices: number[] };
    gb.banks.push({ kind: 4, entries: handoff.pageBlocks.map(([vx, vy, name]) => ({ x: vy, y: vx, width: 128, height: 128, name })) });
    gb.indices[handoff.pageBank] = gb.banks.length - 1;
    files.set(out + "graphics_banks.json", dumps(gb));
    const all = handoff.pageBlocks.map(([, , name, data]): [string, Uint8Array] => [name, data]);
    cmpgfxBlocks(stage, all, files, arrays);
    if (opts.assetConfig !== undefined) {
        files.set(`build/us/${stage}_assets.us.yaml`, text(buildConfig(opts.assetConfig, all.map((b) => b[0]))));
    }
    files.set(`${modStageDir(stage)}mod_sprite_gfx.h`, text(gfxHeader(GEN_GDN,
        "// tile pages 12-15, 4bpp 128x128 blocks compressed by the asset tool\n", all.map((b) => b[0]))));
    return { files, arrays, blocks: all.map((b) => b[0]) };
}

// --------------------------------------------------------------------------
// prs_assets.py sprites()

const GEN_PRS = "// Generated by mods/sotn-ultimate/tools/prs_assets.py, do not edit\n";

// wight_ps1.palettes: the Wight's PS1-style body / glow / spirit palettes
function yellow(c: number, gain: number): number {
    if (!(c & 0x7FFF)) {
        return 0;
    }
    const lum = ((c & 31) * 30 + ((c >> 5) & 31) * 59 + ((c >> 10) & 31) * 11) / 100.0;
    const v = Math.min(31, roundHalfEven(lum * gain));
    const w = Math.min(31, roundHalfEven(lum * gain * 0.28));
    return 0x8000 | v | (v << 5) | (w << 10);
}

function wightPalettes(block: number[][], on: string[]): [string, number[][]][] {
    const out: [string, number[][]][] = [];
    if (on.includes("MOD_WIGHT_PS1_GLOW")) {
        out.push(["WIGHT_BODY", block.map((p) => p.map((c, i) => (i === 14 || i === 15 ? 0 : c)))]);
        out.push(["WIGHT_GLOW", block.map((p) => p.map((c, i) => (i === 14 || i === 15 ? yellow(c, 2.2) : 0)))]);
    }
    if (on.includes("MOD_WIGHT_PS1_SPIRIT")) {
        out.push(["WIGHT_SPIRIT", [1.25, 0.95, 1.55].map((g) => block[0].map((c) => yellow(c, g)))]);
    }
    return out;
}

async function prsSprites(cfg: PrsSprites, src: SpriteSources, mainFiles: Map<string, Bytes>,
                          opts: SpriteOptions): Promise<SpriteResult> {
    const base = cfg.base;
    const out = `assets/st/${cfg.stage}/`;
    const srcDir = modStageDir(cfg.stage);
    const files = new Map<string, Bytes>();
    const arrays = new Map<string, NumArray>();
    const ps1 = new Ps1(src.ps1);
    const st = await saturnStage(src.saturn, base.sat);
    const prg = st.prg;
    const pb = (a: number): number => a - st.base;
    const [bank, frames] = cfg.bank16;
    const decos: [number, number][] = [];
    for (let v = 0; v < cfg.decorations[1]; v++) {
        const o = pb(cfg.decorations[0]) + v * 28;
        if (be32(prg, o) !== bank || be32(prg, o + 4) !== frames) {
            throw new Error(`decoration ${v}: not bank 16`);
        }
        decos.push([prg[pb(be32(prg, o + 24)) + 1], be16(prg, o + 10)]);
    }
    const glowRecs = cfg.candleGlow.map(([rec, f]) => {
        const o = pb(rec) + f * 10;
        return [bes16(prg, o), bes16(prg, o + 2), bes16(prg, o + 4)];
    });
    const state = newState([], cfg.stpBanks, []);
    const groups = cfg.spriteGroups.map((g) => g[0]);
    const rooms = new Map(cfg.spriteGroups);

    let sets: [number, PsxPart[][]][] = [];
    let glow: PsxPart[] = [];
    const fill = (sheets: Map<string, PsxSpriteSheet>): void => {
        for (const [, g, pbank] of cfg.paletteBlocks) {
            sheets.get(g)!.paletteBlock(st, pbank);
        }
        const obj = sheets.get("objects")!;
        sets = [[2, cfg.candleFrames.map((f) => obj.frame(st, bank, frames, f))],
                [3, decos.map(([f]) => obj.frame(st, bank, frames, f))]];
        for (const [n, g, [ebank, eframes, count]] of cfg.enemies) {
            if (sheets.has(g)) {
                sets.push([n, range(1, count + 1).map((f) => sheets.get(g)!.frame(st, ebank, eframes, f))]);
            }
        }
        for (const [n, g, [ebank, eframes, count], dy] of cfg.enemySets) {
            sets.push([n, range(1, count + 1).map((f) => shifted(sheets.get(g)!.frame(st, ebank, eframes, f), dy))]);
        }
        glow = glowRecs.map(([dx, dy, img]) => obj.imagePart(st, bank, img, dx, dy, GLOW_COLOURS, cfg.glowFlip));
    };
    // prs_assets.allocate: runs of consecutive quadrants, never shared within
    // a room; -> the first index
    const allocate = (items: [string, number, number[], number | null][]): Map<string, number> => {
        const used = new Map(base.roomBanks.map(([r]) => [r, new Set<number>()] as const));
        const res = new Map<string, number>();
        const order = items.slice().sort((a, b) => b[2].length - a[2].length || b[1] - a[1]);
        const Q = cfg.quadrants;
        for (const [key, n, rs, quad] of order) {
            let found = -1;
            for (let i = 0; i < Q.length - n + 1; i++) {
                if (quad !== null && (Q[i][1] !== quad || Q[i + n - 1][0] !== Q[i][0])) {
                    continue;
                }
                if (!rs.some((r) => range(i, i + n).some((j) => used.get(r)!.has(j)))) {
                    found = i;
                    break;
                }
            }
            if (found < 0) {
                throw new Error(`sprite quadrants full: ${key}`);
            }
            res.set(key, found);
            for (const r of rs) {
                range(found, found + n).forEach((j) => used.get(r)!.add(j));
            }
        }
        return res;
    };
    let first = new Map<string, number>();
    const { sheets, cluts } = pack(st, state, groups, cfg.quadrants, fill, (sizes) => {
        first = allocate([...groups.map((g): [string, number, number[], number | null] => [g, sizes.get(g)!, rooms.get(g)!, null]),
            ...cfg.psxSprites.map((p): [string, number, number[], number | null] => [p[0], p[3].length, p[5], p[7] ?? null])]);
        return new Map(groups.map((g) => [g, cfg.quadrants.slice(first.get(g)!, first.get(g)! + sizes.get(g)!)] as const));
    });
    fill(sheets);

    const sb = parse(mainFiles, out + "us/sprite_banks.json") as SpriteBanks;
    for (const [animset, parts] of sets) {
        sb.banks.push([null, ...parts]);
        sb.indices[animset] = sb.banks.length - 1;
    }
    const blocks: [Quad, string, Uint8Array, number[]][] = [];
    for (const g of groups) {
        const sh = sheets.get(g)!;
        for (let k = 0; k < sh.usedQuadrants(); k++) {
            blocks.push([sh.quadrants[k], `g_ModPrs_${g}${k}`, sh.quadrantPixels(k), rooms.get(g)!]);
        }
    }
    const psxPals: number[][] = [];
    const psxDefs: [string, number, number, number, number | null][] = [];
    const clut = SPRITE_CLUT_BASE + cluts.length;
    for (const [name, stage, animset, art, pal, rs, prsAnimset, quadReq] of cfg.psxSprites) {
        sb.banks.push(await ps1.spriteSet(stage, animset));
        sb.indices[prsAnimset] = sb.banks.length - 1;
        for (let k = 0; k < art.length; k++) {
            blocks.push([cfg.quadrants[first.get(name)! + k], `g_ModPrs${title(name).replace(/_/g, "")}Gfx${k}`,
                         await ps1.art(stage, art[k]), rs]);
        }
        const palettes: number[][] = [];
        if (Array.isArray(pal) && typeof pal[0] === "number") {
            palettes.push(...(await ps1.palettes(stage, pal as [number, number])));
        } else {
            for (const p of pal as string[]) {
                palettes.push(...(await ps1.pals(stage, p)));
            }
        }
        const [tpage, quad] = cfg.quadrants[first.get(name)!];
        psxDefs.push([name, prsAnimset, (tpage << 2) | quad, clut + psxPals.length, quadReq !== undefined ? tpage : null]);
        psxPals.push(...palettes);
    }
    files.set(out + "us/sprite_banks.json", dumps(sb));
    if (cfg.stageNameRooms.length) {
        blocks.push([[17, 0], "g_ModPrsStageName", await prsStageName(ps1), cfg.stageNameRooms]);
    }
    const gb = parse(mainFiles, out + "graphics_banks.json") as { banks: PyJson[]; indices: number[] };
    for (const [room, index] of base.roomBanks) {
        gb.banks.push({ kind: 4, entries: blocks.filter((b) => b[3].includes(room)).map((b) => gfxEntry(b[0], b[1])) });
        gb.indices[index] = gb.banks.length - 1;
    }
    files.set(out + "graphics_banks.json", dumps(gb));
    const names = blocks.map((b) => b[1]);
    cmpgfxBlocks(cfg.stage, blocks.map((b) => [b[1], b[2]]), files, arrays);
    if (opts.assetConfig !== undefined) {
        files.set(`build/us/${cfg.stage}_assets.us.yaml`, text(buildConfig(opts.assetConfig, names)));
    }

    const allPals = [...cluts.map((c) => c.slice()), ...psxPals];
    const extraDefs: [string, number][] = [];
    if (cfg.wightPal !== null) {
        const firstPal = sheets.get("wights")!.blocks.get(cfg.wightPal)! - SPRITE_CLUT_BASE;
        for (const [name, pals] of wightPalettes(allPals.slice(firstPal, firstPal + 5),
                                                  opts.wightPs1 ?? ["MOD_WIGHT_PS1_SPIRIT", "MOD_WIGHT_PS1_GLOW"])) {
            extraDefs.push([name, SPRITE_CLUT_BASE + allPals.length]);
            allPals.push(...pals.map((c) => c.slice()));
        }
    }
    const pd = parse(mainFiles, out + "palette_def.json") as { defs: { entries: PyJson[] }[] };
    pd.defs[0].entries.push({ destination: SPRITE_CLUT_BASE * 16, length: 16 * allPals.length,
                              name: "g_ModPrsSpritePals.palbin" });
    files.set(out + "palette_def.json", dumps(pd));

    files.set(srcDir + "mod_sprite_gfx.h", text(gfxHeader(GEN_PRS,
        "// the Prison's sprites, 4bpp 128x128 quadrants, compressed by the\n" +
        "// asset tool from assets/st/prs/<name>.png\n", names)));
    files.set(srcDir + "mod_sprite_pals.h", text(palsHeader(GEN_PRS,
        `// the Prison's sprite palettes, g_ClutIds 0x${SPRITE_CLUT_BASE.toString(16).toUpperCase()} on\n`,
        "g_ModPrsSpritePals", allPals, arrays)));
    let h = GEN_PRS + "// the Saturn enemies' palette blocks (PALETTE_BLOCKS)\n";
    for (const [name, g, pbank] of cfg.paletteBlocks) {
        h += `#define MOD_${name}_PAL 0x${hex(sheets.get(g)!.blocks.get(pbank)!, 3)}\n`;
    }
    h += "// the PS1 enemies' sprites in the Prison (PSX_SPRITES)\n";
    for (const [name, animset, unk5a, pal, tpage] of psxDefs) {
        h += `#define MOD_${name}_ANIMSET ANIMSET_OVL(${animset})\n#define MOD_${name}_UNK5A 0x${hex(unk5a, 2)}\n` +
            `#define MOD_${name}_PAL 0x${hex(pal, 3)}\n`;
        if (tpage !== null) {
            h += `#define MOD_${name}_TPAGE 0x${hex(tpage, 2)}\n`;
        }
    }
    if (extraDefs.length) {
        h += "// extra palettes (EXTRA_PALS)\n";
    }
    for (const [name, c] of extraDefs) {
        h += `#define MOD_${name}_PAL 0x${hex(c, 3)}\n`;
    }
    if (cfg.texFrames.length) {
        h += "// frames drawn as polygons (TEX_FRAMES)\n";
    }
    const setMap = new Map(sets);
    for (const [name, animset, frame] of cfg.texFrames) {
        const parts = setMap.get(animset)![frame - 1];
        if (parts.length !== 1) {
            throw new Error(`${name}: frame ${frame} has ${parts.length} parts`);
        }
        const p = parts[0];
        const [u, v] = tpageUV(p);
        for (const [k, val] of [["U", u], ["V", v], ["W", p.width], ["H", p.height], ["TPAGE", p.tileset >> 2],
                                ["CLUT", p.clut]] as [string, number][]) {
            h += `#define MOD_${name}_TEX_${k} 0x${val.toString(16).toUpperCase()}\n`;
        }
    }
    files.set(srcDir + "mod_psx_sprites.h", text(h));
    // the Specter's texture per frame (ANIMSET_OVL(10)), for stages with it
    const specter = setMap.get(10) ?? [];
    if (specter.length) {
        let s = GEN_PRS + "// clang-format off\n// the Specter's frames (ANIMSET_OVL(10)): u, v, w, h, tpage\n" +
            "static const s16 g_ModSpecterTex[][5] = {\n    {0, 0, 0, 0, 0},\n";
        const vals = [0, 0, 0, 0, 0];
        for (const parts of specter) {
            if (!parts.length) {
                s += "    {0, 0, 0, 0, 0},\n";
                vals.push(0, 0, 0, 0, 0);
                continue;
            }
            if (parts.length !== 1) {
                throw new Error("a Specter frame with several parts");
            }
            const p = parts[0];
            const [u, v] = tpageUV(p);
            s += `    {${u}, ${v}, ${p.width}, ${p.height}, ${p.tileset >> 2}},\n`;
            vals.push(u, v, p.width, p.height, p.tileset >> 2);
        }
        files.set(srcDir + "mod_specter_tex.h", text(s + "};\n// clang-format on\n"));
        arrays.set("g_ModSpecterTex", { size: 2, values: vals });
    }
    const [gh, gv] = glowHeader(GEN_PRS, "// per candle flame frame (ANIMSET_OVL(2) 1-8): x, y, w, h, u, v, clut, tpage\n", glow);
    files.set(srcDir + "mod_candle_glow.h", text(gh));
    arrays.set("g_ModCandleGlow", { size: 2, values: gv });
    files.set(srcDir + "mod_bg_blocks.h", text(bgBlocks(GEN_PRS, decos.map((d) => d[1]), arrays)));
    spriteBanksC(cfg.stage, sb, files, arrays);
    return { files, arrays, blocks: names };
}

// --------------------------------------------------------------------------

// The stage's sprites (gdn_assets / prs_assets sprites()), after
// stage_assets.ts buildStage(): `main` is its result (files + handoff).
export async function buildSprites(stage: string, src: SpriteSources,
                                   main: { files: Map<string, Bytes>; handoff: SpriteHandoff },
                                   opts: SpriteOptions = {}): Promise<SpriteResult> {
    const cfg = SPRITE_STAGES[stage] ?? fail("unknown stage " + stage);
    return cfg.kind === "gdn" ? gdnSprites(cfg, src, main.files, main.handoff, opts)
        : prsSprites(cfg, src, main.files, opts);
}
