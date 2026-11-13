// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn-only stages' tile assets: a byte-exact port of the tile part of
// mods/sotn-ultimate/tools/gdn_assets.py main() (the Underground Garden "gdn" and, with
// rgdn_assets.py's values, the Hell Garden "rgdn") and prs_assets.py main()
// (the Cursed Prison "prs", the Soul Prison "rprs" with rprs_assets.py's).
//
// Input: the Saturn map converter's output (`map convert-psx`, the WORK
// folder) after gdn_tile_swap.apply (ported separately), the stage's Saturn
// files and the PS1 assets the stages copy (WRP's entity assets, the shared
// loading / save room tilesets: the decomp's assets/st/<stage> files, which
// the builder extracts from the player's PS1 disc).
// Output: every file main() writes but those of sprites() / pages_only()
// (keyed by repo path: assets/st/<st>/..., mods/sotn-ultimate/src/{garden,prison}/<st>/..., build/us/F_TE*.BIN)
// and the state sprites() goes on with (SpriteHandoff).
//
// Python details kept: repack()'s `for t in used` walks a set of ints, which
// CPython iterates in slot order: ascending here as long as every tile id is
// below the set's table size (checked, see setOrder()); max() returns the
// first maximal element; sorts are stable; // on non-negative ints.
import { inflateZlib } from "./inflate.ts";
import { pyJsonDumps } from "./pyjson.ts";
import type { PyJson } from "./pyjson.ts";

// the Saturn-only stages' sources (with "/"): the mod's garden/ and prison/
export const modStageDir = (stage: string): string =>
    `mods/sotn-ultimate/src/${stage.endsWith("gdn") ? "garden" : "prison"}/${stage}/`;

export type Bytes = Uint8Array;
// a file reader: the name / path -> its bytes (sync or async)
export type Reader = (name: string) => Bytes | Promise<Bytes>;

export interface StageInputs {
    // the converter's output files by name (graphics.bin, palettes.bin,
    // tiledefs.*.bin, rooms.*.json, *.tilelayout.bin,
    // shared_backdrop.preview.png), after gdn_tile_swap.apply. Not modified.
    work: Reader;
    // disks/saturn files by name (STAGE_15.PRG, STAGE_15.CHR, 0.BIN, ...)
    saturn: Reader;
    // the PS1 assets, by repo path ("assets/st/wrp/wrp_tiledef_load_tiles.bin")
    ps1Asset: Reader;
}

// an entity layout record (sprite-free)
export interface Ent {
    x: number;
    y: number;
    id: string;
    flags: number;
    slot: number;
    spawnId: number;
    params: number;
}

// tile definition tables, 0x1000 entries each ("tiles" = gfxPage, "pages" =
// gfxIndex: the asset tool's names, see gdn_assets.py)
export interface TileDefs {
    tiles: Uint8Array;
    pages: Uint8Array;
    cluts: Uint8Array;
    cols: Uint8Array;
}

// a gdn_assets.PAGE_BLOCKS entry: VRAM x, VRAM y, array name, 4bpp 128x128
export type PageBlock = [number, number, string, Uint8Array];

// what sprites() / pages_only() (gdn) and sprites() (prs) need from here
export interface SpriteHandoff {
    // gdn: PAGE_BLOCKS (backdrop()) and PAGE_BANK; prs: []
    pageBlocks: PageBlock[];
    pageBank: number;
    // gdn: SHIMMER_PALETTE (shimmer(), filled only with SHIMMER_SPRITE_PAL)
    shimmerPalette: number[];
    // VRAM tile pages 12-15 as repack() + backdrop() left them (256x256
    // halfwords; PAGE_BLOCKS are cut from it)
    vram4: Uint8Array;
    // the final tile definitions (first nTiles entries are the written tables)
    td: TileDefs;
    nTiles: number;
}

export interface StageResult {
    // repo path -> content. Includes the WRP files copied into
    // assets/st/<st> (us/sprite_banks.json, graphics_banks.json and
    // palette_def.json as copied: sprites() then rewrites them)
    files: Map<string, Bytes>;
    handoff: SpriteHandoff;
}

// --------------------------------------------------------------------------
// stage configurations (the modules' globals that this part reads)

type Entry = [number, number]; // (x, y) cell

export interface GdnConfig {
    kind: "gdn";
    stage: string;          // assets/st/<stage>, mods/sotn-ultimate/src/{garden,prison}/<stage>
    fFile: string;          // build/us/<fFile>
    sat: string;            // Saturn file stem (PRG, CHR)
    specialCells: [number, Entry][] | null;
    loadingTps: [number, number][];
    exits: [Entry, number][];
    sharedRooms: [number, string, string, string][]; // room, tilemap, tiledef, stage
    redDoors: [number, number, number, number, boolean?][];
    bossDoor: boolean;
    contextBanks: [string, number][];
    reloadBanks: [string, number][];
    roomContext: [number, string][];
    zoneWest: number;
    zoneEast: number;
    withSprites: boolean;
    reservedCluts: number[];
    shimmerBank: number | null;
    shimmerFlip: boolean;
    shimmerSpritePal: boolean;
    backdropRow0: number;
    pageBank: number;
    loadingForeground: Ent;
    saveForeground: Ent[];
    water: Ent[];
    candleRooms: number[];
    saturnIds: [number, string][];
}

export interface PrsConfig {
    kind: "prs";
    stage: string;
    fFile: string;
    sat: string;
    // room, tilemap, tiledef, stage, cell, teleport (null: the save room)
    sharedRooms: [number, string, string, string, Entry, number | null][];
    exits: [Entry, number][];
    loadingForeground: Ent;
    saveForeground: Ent[];
    fgPriority: number;
    redDoors: [number, number, number, number, boolean][];
    roomBanks: [number, number][];
    saturnIds: [number, string][];
}

const range = (a: number, b: number): number[] => Array.from({ length: b - a }, (_, i) => a + i);
const ent = (x: number, y: number, id: string, slot: number, params: number): Ent =>
    ({ x, y, id, flags: 0, slot, spawnId: 0, params });

const TP_GDN_TO_NP3 = 0x84;
const TP_GDN_TO_WRP = 0x85;
const GDN_RESERVED = [...range(163, 200), ...range(250, 256)];
const LOADING_FOREGROUND = ent(0, 56, "E_ROOM_FOREGROUND", 0x80, 0);
const SAVE_FOREGROUND = [ent(128, 48, "E_ROOM_FOREGROUND", 0x80, 1), ent(128, 192, "E_ROOM_FOREGROUND", 0x88, 2)];
const R_LOADING_FOREGROUND = ent(0, 56, "E_ROOM_FOREGROUND", 0x80, 3);
const R_SAVE_FOREGROUND = [ent(128, 208, "E_ROOM_FOREGROUND", 0x80, 4), ent(128, 64, "E_ROOM_FOREGROUND", 0x88, 5)];

function waterEnts(kinds: [string, number, number][], ys: number[]): Ent[] {
    const out: Ent[] = [];
    for (const [id, slot, params] of kinds) {
        for (const x of [128, 2176]) {
            for (const y of ys) {
                out.push(ent(x, y, id, slot, params));
            }
        }
    }
    return out;
}

export const GDN: GdnConfig = {
    kind: "gdn", stage: "gdn", fFile: "F_TE1.BIN", sat: "STAGE_15",
    specialCells: null,
    loadingTps: [[4, TP_GDN_TO_NP3], [5, TP_GDN_TO_WRP]],
    exits: [[[2, 44], TP_GDN_TO_NP3], [[2, 45], TP_GDN_TO_WRP]],
    sharedRooms: [[4, "wrp_tilemap_5", "wrp_tiledef_load", "wrp"],
        [5, "wrp_tilemap_5", "wrp_tiledef_load", "wrp"],
        [6, "np3_tilemap_37", "np3_tiledef_save", "np3"]],
    redDoors: [[0, 8, 97, 0x9D], [0, 8, 353, 0x9E]],
    bossDoor: true,
    contextBanks: [["0W", 8], ["1", 1], ["2", 2], ["3", 3], ["0E", 6]],
    reloadBanks: [["0W", 0], ["1", 9], ["2", 10], ["3", 11], ["0E", 12]],
    roomContext: [[0, "0W"], [1, "1"], [2, "2"], [3, "3"]],
    zoneWest: 1450, zoneEast: 1500,
    withSprites: true,
    reservedCluts: GDN_RESERVED,
    shimmerBank: 0x060F0B30, shimmerFlip: false, shimmerSpritePal: false,
    backdropRow0: 0,
    pageBank: 7,
    loadingForeground: LOADING_FOREGROUND,
    saveForeground: SAVE_FOREGROUND,
    water: waterEnts([["E_ALUCARD_WATER_EFFECT", 0x98, 0x0100], ["E_MOD_WATER_BODY", 0x99, 0x0100],
        ["E_MOD_WATER_SURFACE", 0x9A, 0x0100]], [128, 384]),
    candleRooms: [0, 1, 3],
    saturnIds: [[0x09, "E_STAGE_NAME_POPUP"], [0x11, "E_ID_11"], [0x1E, "E_MOD_DRIP_SPAWNER"],
        [0x20, "E_SLINGER"], [0x23, "E_MOD_GARGOYLE"], [0x2E, "E_MOD_TREE"],
        [0x35, "E_MOD_LEAPER_SPAWNER"], [0x36, "E_THORNWEED"], [0x3B, "E_MOD_MANEATER"],
        [0x3D, "E_MOD_GARDENER"], [0x44, "E_MOD_BOSS_CONTROLLER"], [0x40, "E_MOD_SKBREEDER"]],
};

// mods/sotn-ultimate/tools/rgdn_assets.py
export const RGDN: GdnConfig = {
    ...GDN,
    stage: "rgdn", fFile: "F_TE3.BIN", sat: "RSTAGE15",
    pageBank: 0,
    contextBanks: [["0", 1], ["1", 2], ["2", 3], ["3", 4]],
    reloadBanks: [["0", 6], ["1", 7], ["2", 8], ["3", 9]],
    roomContext: [[0, "0"], [1, "1"], [2, "2"], [3, "3"]],
    specialCells: [[4, [60, 18]], [5, [60, 19]], [6, [50, 18]]],
    loadingTps: [[4, TP_GDN_TO_WRP], [5, TP_GDN_TO_NP3]],
    exits: [[[61, 18], TP_GDN_TO_WRP], [[61, 19], TP_GDN_TO_NP3]],
    sharedRooms: [[4, "rwrp_tilemap_5", "rwrp_tiledef_rload", "rwrp"],
        [5, "rwrp_tilemap_5", "rwrp_tiledef_rload", "rwrp"],
        [6, "rno4_tilemap_59", "rno4_tiledef_rsave", "rno4"]],
    loadingForeground: R_LOADING_FOREGROUND,
    saveForeground: R_SAVE_FOREGROUND,
    redDoors: [[0, 2296, 97, 0x9E, true], [0, 2296, 353, 0x9D, true]],
    bossDoor: false,
    candleRooms: [0, 1, 3],
    saturnIds: [[0x11, "E_ID_11"], [0x0C, "E_PERSISTENT_ITEM_DROP"], [0x1E, "E_CAVE_TROLL"],
        [0x33, "E_BITTERFLY"], [0x34, "E_CLOAKED_KNIGHT"], [0x2C, "E_VENUS_WEED"],
        [0x38, "E_MOD_SKGUARDIAN"], [0x21, "E_MOD_COPPER_ARMOR"], [0x23, "E_MOD_IFRIT"]],
    shimmerBank: 0x060ECEF0, shimmerFlip: true,
    reservedCluts: GDN_RESERVED.filter((k) => k !== 250),
    shimmerSpritePal: true,
    water: waterEnts([["E_MOD_CEILING_WATER", 0x98, 0], ["E_ALUCARD_WATER_EFFECT", 0x99, 0x0100],
        ["E_MOD_CEILING_SHIMMER", 0x9B, 0]], [384, 128]),
    backdropRow0: 16,
};

const TP_PRS_TO_NO4 = 0x88;
const TP_PRS_TO_NO0 = 0x8A;

export const PRS: PrsConfig = {
    kind: "prs", stage: "prs", fFile: "F_TE2.BIN", sat: "STAGE_16",
    sharedRooms: [[2, "wrp_tilemap_5", "wrp_tiledef_load", "wrp", [27, 36], TP_PRS_TO_NO0],
        [3, "wrp_tilemap_5", "wrp_tiledef_load", "wrp", [36, 34], TP_PRS_TO_NO4],
        [4, "np3_tilemap_38", "np3_tiledef_save", "np3", [32, 34], null]],
    exits: [[[26, 36], TP_PRS_TO_NO0], [[37, 34], TP_PRS_TO_NO4]],
    loadingForeground: LOADING_FOREGROUND,
    saveForeground: SAVE_FOREGROUND,
    fgPriority: 0x60,
    redDoors: [[0, 760, 97, 0x9D, true], [1, 8, 97, 0x9E, false]],
    roomBanks: [[0, 1], [1, 2]],
    saturnIds: [[0x01, "E_BREAKABLE"], [0x09, "E_STAGE_NAME_POPUP"], [0x11, "E_ID_11"],
        [0x18, "E_BONE_ARCHER"], [0x1A, "E_SPEAR_GUARD"], [0x1D, "E_SLINGER"],
        [0x21, "E_MOD_GARGOYLE"], [0x16, "E_MOD_PRS_SWITCH"], [0x17, "E_MOD_PRS_BLOCK"],
        [0x20, "E_MOD_SPECTER"], [0x28, "E_MOD_WISP_SPAWNER"]],
};

// mods/sotn-ultimate/tools/rprs_assets.py
export const RPRS: PrsConfig = {
    ...PRS,
    stage: "rprs", fFile: "F_TE4.BIN", sat: "RSTAGE16",
    sharedRooms: [[2, "rwrp_tilemap_5", "rwrp_tiledef_rload", "rwrp", [27, 29], TP_PRS_TO_NO4],
        [3, "rwrp_tilemap_5", "rwrp_tiledef_rload", "rwrp", [36, 27], TP_PRS_TO_NO0],
        [4, "rno4_tilemap_58", "rno4_tiledef_rsave", "rno4", [31, 29], null]],
    exits: [[[26, 29], TP_PRS_TO_NO4], [[37, 27], TP_PRS_TO_NO0]],
    redDoors: [[0, 8, 609, 0x80, false], [1, 1272, 97, 0x80, true]],
    loadingForeground: R_LOADING_FOREGROUND,
    saveForeground: R_SAVE_FOREGROUND,
    saturnIds: [[0x01, "E_BREAKABLE"], [0x11, "E_ID_11"], [0x1C, "E_FLYING_ZOMBIE"],
        [0x20, "E_MOD_GARGOYLE"], [0x23, "E_JACK_O_BONES"], [0x26, "E_NOVA_SKELETON"],
        [0x15, "E_MOD_WIGHT"], [0x18, "E_MOD_WRAITH"]],
};

export const STAGES: Record<string, GdnConfig | PrsConfig> = { gdn: GDN, rgdn: RGDN, prs: PRS, rprs: RPRS };

// assets/st/wrp, copied whole into the stage's assets but for wrp_tile*,
// layers.json, rooms.json and entity_layouts.json (shutil.copytree ignore)
export const WRP_COPY = [
    "D_80181D08/D_80181D08_0.png", "D_80181D08/D_80181D08_1.png", "D_80181D08/D_80181D08_2.png",
    "D_80181D08.yaml", "graphics_banks.json", "palette_def.json", "stage_placeholder_wrp.png",
    "stage_title_wrp_jp.png", "us/sprite_banks.json",
];

// --------------------------------------------------------------------------
// helpers

const enc = new TextEncoder();
const dec = new TextDecoder("latin1");
const text = (s: string): Bytes => enc.encode(s);
const json = (v: PyJson): Bytes => enc.encode(pyJsonDumps(v, 2));
const be16 = (d: Bytes, o: number): number => (d[o] << 8) | d[o + 1];
const be32 = (d: Bytes, o: number): number => ((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) >>> 0;
const hex = (v: number, w: number): string => v.toString(16).toUpperCase().padStart(w, "0");
const u16s = (d: Bytes): number[] => {
    const out = new Array<number>(d.length >> 1);
    for (let i = 0; i < out.length; i++) {
        out[i] = d[2 * i] | (d[2 * i + 1] << 8);
    }
    return out;
};
const packU16 = (v: number[]): Bytes => {
    const out = new Uint8Array(v.length * 2);
    for (let i = 0; i < v.length; i++) {
        out[2 * i] = v[i] & 0xFF;
        out[2 * i + 1] = (v[i] >> 8) & 0xFF;
    }
    return out;
};
const fdiv = Math.floor;

// a Python set of ints iterated: CPython's set(dict) of n keys has a table of
// the smallest power of two above 2n (at least 8), each int in slot
// (value & mask): ascending order when every value is below the table size.
function setOrder(values: Iterable<number>): number[] {
    const v = [...values];
    let size = 8;
    if (v.length >= 5) {
        while (size <= v.length * 2) {
            size <<= 1;
        }
    }
    const sorted = v.slice().sort((a, b) => a - b);
    if (sorted.length && sorted[sorted.length - 1] >= size) {
        throw new Error("stage_assets: set iteration order would need CPython's probing (" +
                        `${v.length} values, max ${sorted[sorted.length - 1]}, table ${size})`);
    }
    return sorted;
}

// F_*.BIN file offset of VRAM halfword (0x200 + X, Y)
export function fteOff(X: number, Y: number): number {
    const blk = (fdiv(X / 64) << 2) | (fdiv(Y / 128) << 1) | fdiv((X % 64) / 32);
    return blk * 0x2000 + ((Y % 128) * 32 + (X % 32)) * 2;
}

// 4bpp pixel (x, y) of a little-endian halfword image width_hw wide
export function vramPut(buf: Uint8Array, widthHw: number, x: number, y: number, value: number): void {
    const o = (y * widthHw + (x >> 2)) * 2;
    let hw = buf[o] | (buf[o + 1] << 8);
    const shift = (x % 4) * 4;
    hw = (hw & ~(15 << shift)) | (value << shift);
    buf[o] = hw & 0xFF;
    buf[o + 1] = (hw >> 8) & 0xFF;
}

function putClut(fte: Uint8Array, k: number, j: number, c: number): void {
    const o = fteOff(16 * (k % 16) + j, 0xF0 + (k >> 4));
    fte[o] = c & 0xFF;
    fte[o + 1] = (c >> 8) & 0xFF;
}

// converter slot of (page, index): pages 0-3 hold 15 rows (palettes below)
function convSlot(page: number, index: number): number {
    let s = 0;
    for (let p = 0; p < page; p++) {
        s += p < 4 ? 240 : 256;
    }
    return s + (index >> 4) * 16 + (index & 15);
}

interface ConvCell {
    left: number;
    top: number;
    right: number;
    bottom: number;
    data?: string;
}
type ConvLayer = { fg?: ConvCell; bg?: ConvCell };
interface ConvRoom {
    left: number;
    top: number;
    right: number;
    bottom: number;
    tileLayoutId: number;
}

// --------------------------------------------------------------------------
// repack(): tile placement into the F_ file (8 pages) and the extra VRAM tile
// pages 13-15, merging near-identical background tiles if they don't fit

const EXTRA_PAGES = [13, 14, 15];
const MIST_CLUT = 251;
const SHIMMER_CLUT = 250;

export interface RepackResult {
    fte: Uint8Array;   // 0x40000 bytes, the F_ file
    vram4: Uint8Array; // pages 12-15: 256 rows of 256 halfwords
    merged: number;
    graphicsPlaced: number;
}

// layouts: tilemap data name -> part ("fg"/"bg"); rewritten in `work`
async function repack(work: Map<string, Bytes>, td: TileDefs, nTiles: number, convLayers: ConvLayer[],
                      shared: Set<number>, reserved: Set<number>): Promise<RepackResult> {
    const graphics = work.get("graphics.bin")!;
    const palRaw = work.get("palettes.bin")!;
    const palettes: number[][] = [];
    for (let i = 0; i + 32 <= palRaw.length; i += 32) {
        palettes.push(u16s(palRaw.subarray(i, i + 32)));
    }
    const slot = range(0, nTiles).map((t) => convSlot(td.tiles[t], td.pages[t]));

    // a tile's 256 pixels as r, g, b (x8) per pixel, -1 for transparent
    const pxCache = new Map<number, Int16Array>();
    const px = (t: number): Int16Array => {
        let p = pxCache.get(t);
        if (p === undefined) {
            p = new Int16Array(768);
            const pal = palettes[td.cluts[t]];
            const base = slot[t] * 256;
            for (let i = 0; i < 256; i++) {
                const g = graphics[base + i];
                if (g === 0) {
                    p[3 * i] = p[3 * i + 1] = p[3 * i + 2] = -1;
                } else {
                    const c = pal[g];
                    p[3 * i] = (c & 31) * 8;
                    p[3 * i + 1] = ((c >> 5) & 31) * 8;
                    p[3 * i + 2] = ((c >> 10) & 31) * 8;
                }
            }
            pxCache.set(t, p);
        }
        return p;
    };

    // tiles the stage's own rooms use (the shared-tileset rooms don't)
    const layouts = new Map<string, [string, number[]]>();
    convLayers.forEach((cl, i) => {
        if (shared.has(i)) {
            return;
        }
        for (const part of ["fg", "bg"] as const) {
            const name = cl[part]!.data! + ".tilelayout.bin";
            layouts.set(name, [part, u16s(work.get(name)!)]);
        }
    });
    const uses = new Map<number, number>();
    const fg = new Set<number>();
    for (const [part, tm] of layouts.values()) {
        for (const t of tm) {
            if (t) {
                uses.set(t, (uses.get(t) ?? 0) + 1);
                if (part === "fg") {
                    fg.add(t);
                }
            }
        }
    }
    const used = setOrder(uses.keys());

    const fSlots: [number, number][] = [];
    for (let p = 0; p < 8; p++) {
        for (let r = 0; r < (p < 4 ? 15 : 16); r++) {
            for (let c = 0; c < 16; c++) {
                fSlots.push([p, r * 16 + c]);
            }
        }
    }
    fSlots.shift(); // (0, 0) is the empty graphic
    const xSlots: [number, number][] = [];
    for (const p of EXTRA_PAGES) {
        for (let i = 0; i < 256; i++) {
            xSlots.push([p, i]);
        }
    }
    const cap = fSlots.length + xSlots.length;

    const remap = new Map<number, number>();
    const refs = new Map<number, number>();
    for (const t of used) {
        refs.set(slot[t], (refs.get(slot[t]) ?? 0) + 1);
    }
    const needed = (): number => {
        let n = 0;
        for (const [s, k] of refs) {
            if (k && s !== 0) {
                n++;
            }
        }
        return n;
    };
    const cost = (a: number, b: number): number => {
        const pa = px(a);
        const pb = px(b);
        let e = 0;
        for (let i = 0; i < 768; i += 3) {
            const na = pa[i] < 0;
            const nb = pb[i] < 0;
            if (na !== nb) {
                e += 3 * 255 * 255;
            } else if (!na) {
                const d0 = pa[i] - pb[i];
                const d1 = pa[i + 1] - pb[i + 1];
                const d2 = pa[i + 2] - pb[i + 2];
                e += d0 * d0 + d1 * d1 + d2 * d2;
            }
        }
        return e / 256;
    };
    const signature = (t: number, shift: number): string => {
        const out: number[] = [];
        const pix = px(t);
        for (let by = 0; by < 4; by++) {
            for (let bx = 0; bx < 4; bx++) {
                let a0 = 0;
                let a1 = 0;
                let a2 = 0;
                let a3 = 0;
                for (let y = 0; y < 4; y++) {
                    for (let x = 0; x < 4; x++) {
                        const i = ((by * 4 + y) * 16 + bx * 4 + x) * 3;
                        if (pix[i] >= 0) {
                            a0 += pix[i];
                            a1 += pix[i + 1];
                            a2 += pix[i + 2];
                            a3 += 1;
                        }
                    }
                }
                const n = a3 || 1;
                out.push(fdiv(a0 / n) >> shift, fdiv(a1 / n) >> shift, fdiv(a2 / n) >> shift, a3 >> 2);
            }
        }
        return out.join(",");
    };

    let merged = 0;
    const targets = new Set<number>();
    let shift = 4;
    while (needed() > cap) {
        if (shift > 8) {
            throw new Error("tiles don't fit even merging coarsely");
        }
        const buckets = new Map<string, number[]>();
        for (const t of used) {
            if (!remap.has(t)) {
                const k = signature(t, shift);
                const g = buckets.get(k);
                if (g) {
                    g.push(t);
                } else {
                    buckets.set(k, [t]);
                }
            }
        }
        const cands: [number, number, number][] = [];
        for (const group of buckets.values()) {
            if (group.length < 2) {
                continue;
            }
            // max(): the first of the maximal (in fg, uses)
            let rep = group[0];
            for (const t of group) {
                const kt = fg.has(t) ? 1 : 0;
                const kr = fg.has(rep) ? 1 : 0;
                if (kt > kr || (kt === kr && uses.get(t)! > uses.get(rep)!)) {
                    rep = t;
                }
            }
            for (const a of group) {
                if (a !== rep && !fg.has(a)) {
                    const u = uses.get(a)!;
                    cands.push([cost(a, rep) * (u * u), a, rep]);
                }
            }
        }
        cands.sort((p, q) => p[0] - q[0] || p[1] - q[1] || p[2] - q[2]);
        for (const [, a, rep] of cands) {
            if (needed() <= cap) {
                break;
            }
            if (remap.has(a) || remap.has(rep) || targets.has(a) || refs.get(slot[a]) !== 1) {
                continue;
            }
            remap.set(a, rep);
            targets.add(rep);
            refs.set(slot[a], refs.get(slot[a])! - 1);
            merged++;
        }
        shift++;
    }
    const kept = used.filter((t) => !remap.has(t));
    for (const [name, [, tm]] of layouts) {
        work.set(name, packU16(tm.map((t) => remap.get(t) ?? t)));
    }

    // graphics into slots (foreground first, so it stays in the F_ file),
    // palettes into ids
    const fgSlots = new Set([...fg].map((t) => slot[t]));
    const order = [...new Set(kept.map((t) => slot[t]))].filter((s) => s !== 0)
        .sort((a, b) => ((fgSlots.has(a) ? 0 : 1) - (fgSlots.has(b) ? 0 : 1)) || a - b);
    if (order.length > cap) {
        throw new Error("tile graphics don't fit");
    }
    const place = new Map<number, [number, number]>([[0, [0, 0]]]);
    const dsts = fSlots.concat(xSlots);
    order.forEach((s, i) => place.set(s, dsts[i]));
    const freeCluts = range(1, 256).filter((k) => !reserved.has(k));
    const pals = [...new Set(kept.map((t) => td.cluts[t]))].filter((c) => c !== 0).sort((a, b) => a - b);
    if (pals.length > freeCluts.length) {
        throw new Error(`${pals.length} palettes, ${freeCluts.length} slots`);
    }
    const clutOf = new Map<number, number>([[0, 0]]);
    pals.forEach((p, i) => clutOf.set(p, freeCluts[i]));

    const fte = new Uint8Array(0x40000);
    const vram4 = new Uint8Array(256 * 2 * 256);
    for (const [s, [page, index]] of place) {
        const base = s * 256;
        const u = (index & 15) * 16;
        const v = index & 0xF0;
        for (let y = 0; y < 16; y++) {
            for (let x = 0; x < 16; x++) {
                const val = graphics[base + y * 16 + x];
                if (page < 8) {
                    const o = fteOff(page * 64 + ((u + x) >> 2), v + y);
                    let hw = fte[o] | (fte[o + 1] << 8);
                    const sh = ((u + x) % 4) * 4;
                    hw = (hw & ~(15 << sh)) | (val << sh);
                    fte[o] = hw & 0xFF;
                    fte[o + 1] = (hw >> 8) & 0xFF;
                } else {
                    vramPut(vram4, 256, (page - 12) * 256 + u + x, v + y, val);
                }
            }
        }
    }
    for (const [old, nw] of clutOf) {
        palettes[old].forEach((c, j) => putClut(fte, nw, j, c));
    }
    const usedSet = new Set(used);
    for (let t = 0; t < nTiles; t++) {
        if (remap.has(t) || !usedSet.has(t)) {
            td.tiles[t] = td.pages[t] = td.cluts[t] = 0;
            continue;
        }
        [td.tiles[t], td.pages[t]] = place.get(slot[t])!;
        td.cluts[t] = clutOf.get(td.cluts[t])!;
    }
    return { fte, vram4, merged, graphicsPlaced: order.length };
}

// --------------------------------------------------------------------------
// the red mist backdrop and the water shimmer (gdn_assets.backdrop/shimmer)

function readPngRgba(d: Bytes): [number, number, Uint8Array] {
    let p = 8;
    let w = 0;
    let h = 0;
    const idat: Bytes[] = [];
    while (p < d.length) {
        const n = be32(d, p);
        const t = String.fromCharCode(d[p + 4], d[p + 5], d[p + 6], d[p + 7]);
        if (t === "IHDR") {
            w = be32(d, p + 8);
            h = be32(d, p + 12);
            if (d[p + 16] !== 8 || d[p + 17] !== 6 || d[p + 20] !== 0) {
                throw new Error("preview png: not 8-bit RGBA, non-interlaced");
            }
        } else if (t === "IDAT") {
            idat.push(d.subarray(p + 8, p + 8 + n));
        }
        p += 12 + n;
    }
    const all = new Uint8Array(idat.reduce((a, b) => a + b.length, 0));
    let k = 0;
    for (const b of idat) {
        all.set(b, k);
        k += b.length;
    }
    const raw = inflateZlib(all);
    const stride = w * 4;
    const bpp = 4;
    const out = new Uint8Array(stride * h);
    let prev = new Uint8Array(stride);
    let o = 0;
    for (let y = 0; y < h; y++) {
        const f = raw[o];
        const line = raw.slice(o + 1, o + 1 + stride);
        o += 1 + stride;
        for (let i = 0; i < stride; i++) {
            const a = i >= bpp ? line[i - bpp] : 0;
            const b = prev[i];
            const c = i >= bpp ? prev[i - bpp] : 0;
            if (f === 1) {
                line[i] = (line[i] + a) & 0xFF;
            } else if (f === 2) {
                line[i] = (line[i] + b) & 0xFF;
            } else if (f === 3) {
                line[i] = (line[i] + ((a + b) >> 1)) & 0xFF;
            } else if (f === 4) {
                const pa = Math.abs(b - c);
                const pb = Math.abs(a - c);
                const pc = Math.abs(a + b - 2 * c);
                const pr = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
                line[i] = (line[i] + pr) & 0xFF;
            }
        }
        out.set(line, y * stride);
        prev = line;
    }
    return [w, h, out];
}

type Rgb = [number, number, number];
const dist2 = (c: Rgb, d: Rgb): number =>
    (c[0] - d[0]) ** 2 + (c[1] - d[1]) ** 2 + (c[2] - d[2]) ** 2;
const nearest = (c: Rgb, centres: Rgb[]): number => {
    let best = 0;
    let bd = dist2(c, centres[0]);
    for (let j = 1; j < centres.length; j++) {
        const e = dist2(c, centres[j]);
        if (e < bd) {
            best = j;
            bd = e;
        }
    }
    return best;
};

// k-means over the distinct colours (weighted), 16 RGB centres
function quantize16(colors: Rgb[]): Rgb[] {
    const cnt = new Map<number, number>();
    for (const c of colors) {
        const k = (c[0] << 16) | (c[1] << 8) | c[2];
        cnt.set(k, (cnt.get(k) ?? 0) + 1);
    }
    const unkey = (k: number): Rgb => [k >> 16, (k >> 8) & 255, k & 255];
    const pts: [Rgb, number][] = [...cnt].map(([k, n]) => [unkey(k), n]);
    const lum = (c: Rgb): number => c[0] * 3 + c[1] * 6 + c[2];
    const byLum = [...cnt.keys()].map(unkey).sort((a, b) => lum(a) - lum(b));
    let centres: Rgb[] = range(0, 16).map((i) => byLum[fdiv(i * (byLum.length - 1) / 15)]);
    for (let it = 0; it < 12; it++) {
        const acc = centres.map(() => [0, 0, 0, 0]);
        for (const [c, n] of pts) {
            const a = acc[nearest(c, centres)];
            a[0] += c[0] * n;
            a[1] += c[1] * n;
            a[2] += c[2] * n;
            a[3] += n;
        }
        centres = acc.map((a, j): Rgb => a[3] ? [fdiv(a[0] / a[3]), fdiv(a[1] / a[3]), fdiv(a[2] / a[3])] : centres[j]);
    }
    return centres;
}

const MIST_W = 128;
const MIST_H = 104;
const BACKDROP_TPAGE = 20;
const BACKDROP_VRAM: [number, number] = [256, 256];
const SHIMMER_SEGMENTS: [number, number][] = [[0, 128], [1, 128], [2, 128], [0, 128], [3, 20]];
const GEN_GDN = "// Generated by mods/sotn-ultimate/tools/gdn_assets.py, do not edit\n";
const GEN_PRS = "// Generated by mods/sotn-ultimate/tools/prs_assets.py, do not edit\n";

// the shimmer strips into page 12 rows 104-127 and their palette (into the
// F_ file's SHIMMER_CLUT, or returned for the sprite palettes); the segments
async function shimmer(cfg: GdnConfig, inp: StageInputs, vram4: Uint8Array, fte: Uint8Array,
                       shimmerPalette: number[]): Promise<[number, number, number][][]> {
    const prg = await inp.saturn(cfg.sat + ".PRG");
    const zero = await inp.saturn("0.BIN");
    const chr = await inp.saturn(cfg.sat + ".CHR");
    const zb = 0x06004080;
    const lb = 0x060DC000;
    const res = (a: number): [Bytes, number] | null => {
        if (lb <= a && a < lb + prg.length) {
            return [prg, a - lb];
        }
        if (zb <= a && a < zb + zero.length) {
            return [zero, a - zb];
        }
        return null;
    };
    const tab = be32(prg, 4) - lb;
    let cursor = 0;
    const seen = new Set<number>();
    let bank: [[number, number, number][], number] | null = null;
    for (let i = 0; i < 256; i++) {
        const a = be32(prg, tab + i * 4);
        const r = res(a);
        if (r === null) {
            break;
        }
        const [d, o] = r;
        const ia = be32(d, o);
        const pa = be32(d, o + 4);
        const al = be16(d, o + 8);
        const fl = be16(d, o + 10);
        if (al !== 0xFFFF || fl !== 0xFFFF || res(ia) === null || res(pa) === null || seen.has(ia)) {
            continue;
        }
        seen.add(ia);
        const [idd, io] = res(ia)!;
        const recs: [number, number, number][] = [];
        for (let k = 0; k < 512; k++) {
            const w = idd[io + k * 4];
            const h = idd[io + k * 4 + 1];
            if (w === 0 || h === 0) {
                break;
            }
            recs.push([w * 2, h * 2, cursor]);
            cursor += Math.ceil((w * h * 2) / 0x20) * 0x20;
        }
        if (a === cfg.shimmerBank) {
            bank = [recs, pa];
        }
    }
    if (bank === null || bank[0].length !== 12) {
        throw new Error("shimmer bank not found");
    }
    const [recs, pa] = bank;
    const [pd, po] = res(pa)!;
    shimmerPalette.length = 0;
    for (let j = 0; j < 16; j++) {
        let c = be16(pd, po + 2 + j * 2);
        c = j === 0 ? 0 : (c & 0x7FFF) || 0x8000;
        if (cfg.shimmerSpritePal) {
            shimmerPalette.push(c);
            continue;
        }
        putClut(fte, SHIMMER_CLUT, j, c);
    }
    const strip = (rec: [number, number, number]): number[][] => {
        const [w, h, off] = rec;
        if (h !== 2) {
            throw new Error("shimmer strip height");
        }
        const px: number[] = [];
        for (let i = off; i < off + fdiv(w * h / 2); i++) {
            px.push(chr[i] >> 4, chr[i] & 15);
        }
        const nw = fdiv(w * 4 / 5);
        let rows = range(0, h).map((y) => range(0, nw).map((x) => px[y * w + fdiv(x * 5 / 4)]));
        if (cfg.shimmerFlip) {
            rows = rows.reverse().map((row) => row.slice().reverse());
        }
        return rows;
    };
    const out: [number, number, number][][] = [];
    for (let f = 0; f < 3; f++) {
        const strips = range(0, 4).map((i) => strip(recs[f * 4 + i]));
        const place: [number, number, number][] = [];
        for (let i = 0; i < 4; i++) {
            const u = 0;
            const v = MIST_H + f * 8 + i * 2;
            if (v + 2 > 128 || strips[i][0].length > 128) {
                throw new Error("shimmer placement");
            }
            strips[i].forEach((row, y) => row.forEach((val, x) => vramPut(vram4, 256, u + x, v + y, val)));
            place.push([u, v, strips[i][0].length]);
        }
        out.push(SHIMMER_SEGMENTS.map(([i, w]): [number, number, number] =>
            [place[i][0], place[i][1], cfg.shimmerFlip ? place[i][2] : Math.min(place[i][2], fdiv(w * 4 / 5))]));
    }
    return out;
}

async function backdrop(cfg: GdnConfig, inp: StageInputs, work: Map<string, Bytes>, fte: Uint8Array,
                        vram4: Uint8Array, files: Map<string, Bytes>, handoff: SpriteHandoff): Promise<void> {
    const [w, h, rgba] = readPngRgba(work.get("shared_backdrop.preview.png")!);
    if (w !== 256 || h !== 256) {
        throw new Error("backdrop size");
    }
    const pix: Rgb[] = [];
    for (let i = 0; i < rgba.length; i += 4) {
        pix.push([rgba[i], rgba[i + 1], rgba[i + 2]]);
    }
    const centres = quantize16(pix);
    const pal = centres.map(([r, g, b]) => ((r >> 3) | ((g >> 3) << 5) | ((b >> 3) << 10)) || 0x8000);
    const k = MIST_CLUT;
    pal.forEach((c, j) => putClut(fte, k, j, c));
    for (let y = 0; y < 256; y++) {
        vram4.fill(0, y * 512, y * 512 + 128);
    }
    for (let y = 0; y < MIST_H; y++) {
        let y0 = fdiv(y * 240 / MIST_H);
        let y1 = Math.max(y0 + 1, fdiv((y + 1) * 240 / MIST_H));
        y0 += cfg.backdropRow0;
        y1 += cfg.backdropRow0;
        for (let x = 0; x < MIST_W; x++) {
            const box: Rgb[] = [];
            for (let sy = y0; sy < y1; sy++) {
                for (const sx of [2 * x, 2 * x + 1]) {
                    box.push(pix[sy * 256 + sx]);
                }
            }
            const avg: Rgb = [0, 1, 2].map((c) => fdiv(box.reduce((s, p) => s + p[c], 0) / box.length)) as Rgb;
            vramPut(vram4, 256, x, y, nearest(avg, centres));
        }
    }
    const segs = cfg.shimmerBank !== null ? await shimmer(cfg, inp, vram4, fte, handoff.shimmerPalette) : null;
    handoff.pageBlocks.length = 0;
    for (let by = 0; by < 2; by++) {
        for (let bx = 0; bx < 8; bx++) {
            if (bx < 2 && (bx !== 0 || by !== 0)) {
                continue; // page 12 beyond the mist's quadrant: sprite space
            }
            const data = new Uint8Array(128 * 64);
            for (let y = 0; y < 128; y++) {
                const o = (by * 128 + y) * 512 + bx * 64;
                data.set(vram4.subarray(o, o + 64), y * 64);
            }
            handoff.pageBlocks.push([BACKDROP_VRAM[0] + bx * 32, BACKDROP_VRAM[1] + by * 128,
                `g_ModGdnPage${by}_${bx}`, data]);
        }
    }
    const src = modStageDir(cfg.stage);
    files.set(src + "mod_backdrop.h", text(GEN_GDN + "// clang-format off\n" +
        "// tile pages 12 (the mist) to 15 (Garden tiles), VRAM x 256-511, y 256-511\n" +
        `#define MOD_BACKDROP_VRAM_X ${BACKDROP_VRAM[0]}\n#define MOD_BACKDROP_VRAM_Y ${BACKDROP_VRAM[1]}\n` +
        `#define MOD_BACKDROP_VRAM_W ${64 * (1 + EXTRA_PAGES.length)}\n` +
        `#define MOD_BACKDROP_TPAGE ${BACKDROP_TPAGE}\n#define MOD_BACKDROP_CLUT ${k}\n` +
        "// stored at half resolution, drawn stretched over the screen\n" +
        `#define MOD_BACKDROP_W ${MIST_W}\n#define MOD_BACKDROP_H ${MIST_H}\n` +
        "// the pages' compressed graphics bank (PAGE_BANK)\n" +
        `#define MOD_BACKDROP_GFX ANIMSET_OVL(${cfg.pageBank})\n// clang-format on\n`));
    if (segs === null) {
        return;
    }
    let s = GEN_GDN + "// clang-format off\n" +
        `#define MOD_SHIMMER_TPAGE ${BACKDROP_TPAGE}\n#define MOD_SHIMMER_CLUT ${cfg.shimmerSpritePal ? "MOD_SHIMMER_PAL" : SHIMMER_CLUT}\n` +
        `// per frame, 5 segments {u, v, width}, 2px tall${cfg.shimmerFlip ? ", stored rotated 180 degrees" : ""}\n` +
        `static const u8 g_ModShimmerSegs[3][${SHIMMER_SEGMENTS.length}][3] = {\n`;
    for (const frame of segs) {
        s += "    {" + frame.map((g) => `{${g[0]}, ${g[1]}, ${g[2]}}`).join(", ") + "},\n";
    }
    files.set(src + "mod_shimmer.h", text(s + "};\n// clang-format on\n"));
}

// --------------------------------------------------------------------------
// Saturn layout lists, layer priorities

// the Saturn room's layout records (list room + 1): [x, y, eid, slot, params]
function layoutRecords(prg: Bytes, room: number): number[][] {
    const base = 0x060DC000;
    const lists = be32(prg, 8) - base;
    let p = be32(prg, lists + 4 * (room + 1)) - base;
    const out: number[][] = [];
    for (;;) {
        const r = [0, 2, 4, 6, 8].map((k) => be16(prg, p + k));
        p += 10;
        if (r[0] === 0xFFFF) {
            break;
        }
        if (r[0] !== 0xFFFE) {
            out.push(r);
        }
    }
    return out;
}

const satEnt = (r: number[], id: string): Ent => ({
    x: fdiv(r[0] * 4 / 5), y: r[1], id, flags: (r[2] & 0xE000) >> 8,
    slot: r[3] & 0xFF, spawnId: r[3] >> 8, params: r[4],
});

function saturnCandles(prg: Bytes, room: number): Ent[] {
    return layoutRecords(prg, room).filter((r) => (r[2] & 0x1FFF) === 1).map((r) => satEnt(r, "E_BREAKABLE"));
}

function saturnEntities(prg: Bytes, room: number, ids: [number, string][]): Ent[] {
    const m = new Map(ids);
    return layoutRecords(prg, room).filter((r) => m.has(r[2] & 0x1FFF)).map((r) => satEnt(r, m.get(r[2] & 0x1FFF)!));
}

function fgPriority(prg: Bytes, room: number): number {
    const table = be32(prg, 0x24) - 0x060DC000;
    const rec = prg.subarray(table + room * 12, table + room * 12 + 12);
    return rec[4] < rec[1] ? 0xA0 : 0x60;
}

// the entity layout list: a head and tail around the entities (yOrder = index)
function layout(ents: Ent[]): PyJson {
    const out: PyJson[] = [{ x: -2, y: -2, id: "E_NONE", flags: 0, slot: 0, spawnId: 0, params: 0, yOrder: 0 }];
    ents.forEach((e, n) => out.push({ ...e, yOrder: n + 1 }));
    out.push({ x: -1, y: -1, id: "E_NONE", flags: 0, slot: 0, spawnId: 0, params: 0, yOrder: ents.length + 1 });
    return out;
}

// --------------------------------------------------------------------------
// shared steps

async function readTileDefs(work: Map<string, Bytes>): Promise<[TileDefs, number]> {
    let n: number | null = null;
    const td: Record<string, Uint8Array> = {};
    for (const [src, dst] of [["gfx_page", "tiles"], ["gfx_index", "pages"], ["clut", "cluts"], ["collision", "cols"]]) {
        const data = work.get(`tiledefs.${src}.bin`)!;
        if (data.length > 0x1000 || (n !== null && n !== data.length)) {
            throw new Error("tiledefs size");
        }
        n = data.length;
        const t = new Uint8Array(0x1000);
        t.set(data);
        td[dst] = t;
    }
    return [{ tiles: td.tiles, pages: td.pages, cluts: td.cluts, cols: td.cols }, n!];
}

const tiledefJson = (p: string): Bytes => json({ tiles: `${p}_tiles.bin`, pages: `${p}_pages.bin`,
    cluts: `${p}_cluts.bin`, collisions: `${p}_cols.bin` });

async function loadWork(work: Reader, names: string[]): Promise<Map<string, Bytes>> {
    const m = new Map<string, Bytes>();
    for (const n of names) {
        m.set(n, (await work(n)).slice());
    }
    return m;
}

async function copyWrp(inp: StageInputs, out: string, files: Map<string, Bytes>): Promise<void> {
    for (const f of WRP_COPY) {
        files.set(out + f, (await inp.ps1Asset("assets/st/wrp/" + f)).slice());
    }
}

// keep only the entries of the shared tilesets that their tilemaps use
function trimSharedTiledefs(layers: Record<string, Record<string, PyJson>>[], out: string, own: string,
                            files: Map<string, Bytes>): void {
    const used = new Map<string, number>();
    for (const entry of layers) {
        for (const lay of Object.values(entry)) {
            if (lay.tiledef === own) {
                continue;
            }
            const top = Math.max(...u16s(files.get(out + (lay.data as string))!));
            used.set(lay.tiledef as string, Math.max(used.get(lay.tiledef as string) ?? 0, top + 1));
        }
    }
    for (const [tdef, n] of used) {
        const names = JSON.parse(dec.decode(files.get(out + tdef)!)) as Record<string, string>;
        for (const f of Object.values(names)) {
            files.set(out + f, files.get(out + f)!.slice(0, n));
        }
    }
}

// red doors: closed = the doorway's own tiles made solid (new entries),
// open = as they are
function redDoorRows(doors: [number, number, number, number, boolean?][], layers: Record<string, Record<string, PyJson>>[],
                     out: string, files: Map<string, Bytes>, td: TileDefs, nTiles: number): [number[][], number] {
    const rows: number[][] = [];
    const solid = new Map<number, number>();
    for (const door of doors) {
        const [room, x, y] = door;
        const fgl = layers[room].fg;
        const tm = u16s(files.get(out + (fgl.data as string))!);
        const w = ((fgl.right as number) - (fgl.left as number) + 1) * 16;
        const col = (x + (door[4] ? -4 : 4)) >> 4;
        const row = (y + 31 - 24) >> 4;
        const opened = range(0, 4).map((i) => tm[(row + i) * w + col]);
        const closed: number[] = [];
        for (const t of opened) {
            if (td.cols[t] !== 3 && !solid.has(t)) {
                if (nTiles >= 0x1000) {
                    throw new Error("tileset full");
                }
                td.tiles[nTiles] = td.tiles[t];
                td.pages[nTiles] = td.pages[t];
                td.cluts[nTiles] = td.cluts[t];
                td.cols[nTiles] = 3;
                solid.set(t, nTiles);
                nTiles++;
            }
            closed.push(td.cols[t] === 3 ? t : solid.get(t)!);
        }
        rows.push(closed.concat(opened));
    }
    return [rows, nTiles];
}

function redDoorHeader(gen: string, rows: number[][]): Bytes {
    let s = gen + "// [params][0..3] closed (solid), [4..7] open: the doorway's own tiles\n" +
        "u16 g_RedDoorTiles[][8] = {\n";
    for (const r of rows) {
        s += "    {" + r.map((v) => "0x" + hex(v, 3)).join(", ") + "},\n";
    }
    return text(s + "};\n");
}

// a blank (tile 0's art), solid tile appended
function blankSolid(td: TileDefs, nTiles: number): number {
    if (nTiles >= 0x1000) {
        throw new Error("tileset full");
    }
    td.tiles[nTiles] = td.tiles[0];
    td.pages[nTiles] = td.pages[0];
    td.cluts[nTiles] = td.cluts[0];
    td.cols[nTiles] = 3;
    return nTiles;
}

function writeTileDefs(files: Map<string, Bytes>, prefix: string, td: TileDefs, nTiles: number): void {
    for (const k of ["tiles", "pages", "cluts", "cols"] as const) {
        files.set(`${prefix}_${k}.bin`, td[k].slice(0, nTiles));
    }
}

// the converter output files main() reads
function workNames(convLayers: ConvLayer[], skip: Set<number>, backdropPng: boolean): string[] {
    const names = ["graphics.bin", "palettes.bin", "tiledefs.gfx_page.bin", "tiledefs.gfx_index.bin",
        "tiledefs.clut.bin", "tiledefs.collision.bin"];
    convLayers.forEach((cl, i) => {
        if (!skip.has(i)) {
            names.push(cl.fg!.data! + ".tilelayout.bin", cl.bg!.data! + ".tilelayout.bin");
        }
    });
    if (backdropPng) {
        names.push("shared_backdrop.preview.png");
    }
    return names;
}

const parseJson = async (r: Reader, name: string): Promise<unknown> => JSON.parse(dec.decode(await r(name)));

// asset config lines for the sprite blocks (sprites() / pages_only()):
// config text (must end with the sprites skip) + one cmpgfx entry per block
export function buildConfig(config: string, blockNames: string[]): string {
    let conf = config.replace(/\n+$/, "") + "\n";
    if (!conf.endsWith("          - [0x6D88, skip] # sprites\n")) {
        throw new Error("the asset config must end with the sprites skip");
    }
    blockNames.forEach((name, k) => {
        conf += `          - [0x${hex(0x6D90 + k * 4, 1)}, cmpgfx, ${name}, 128, 128, 4]\n`;
    });
    return conf;
}

// --------------------------------------------------------------------------
// gdn_assets.main() without sprites() / pages_only()

export async function buildGdn(cfg: GdnConfig, inp: StageInputs): Promise<StageResult> {
    const out = `assets/st/${cfg.stage}/`;
    const src = modStageDir(cfg.stage);
    const files = new Map<string, Bytes>();
    const handoff: SpriteHandoff = { pageBlocks: [], pageBank: cfg.pageBank, shimmerPalette: [],
        vram4: new Uint8Array(0), td: null as unknown as TileDefs, nTiles: 0 };
    const sharedMap = new Map(cfg.sharedRooms.map((r) => [r[0], r] as const));
    const loadingTps = new Map(cfg.loadingTps);

    let convRooms = ((await parseJson(inp.work, "rooms.roomdef.json")) as ConvRoom[]).slice(0, 7);
    let convLayers = ((await parseJson(inp.work, "rooms.layers.json")) as ConvLayer[]).slice(0, 7);
    if (cfg.specialCells !== null) {
        convRooms = convRooms.slice(0, 4);
        convLayers = convLayers.slice(0, 4);
        for (const [room, [x, y]] of cfg.specialCells.slice().sort((a, b) => a[0] - b[0])) {
            if (room !== convRooms.length) {
                throw new Error("special cells out of order");
            }
            const cell = { left: x, top: y, right: x, bottom: y };
            convRooms.push({ ...cell, tileLayoutId: room });
            convLayers.push({ fg: cell });
        }
    }
    if (convRooms.map((r) => r.tileLayoutId).join() !== range(0, 7).join()) {
        throw new Error("converter rooms");
    }
    const sharedSet = new Set(sharedMap.keys());
    const work = await loadWork(inp.work, workNames(convLayers, sharedSet, true));

    await copyWrp(inp, out, files);

    let [td, nTiles] = await readTileDefs(work);
    const reserved = new Set(cfg.reservedCluts);
    const { fte, vram4 } = await repack(work, td, nTiles, convLayers, sharedSet, reserved);
    files.set(out + "gdn_tiledef_gdn.json", tiledefJson("gdn_tiledef_gdn"));

    const shared = new Map<number, [string, string]>();
    for (const [room, tmap, tdef, st] of cfg.sharedRooms) {
        const kind = tdef.slice(tdef.lastIndexOf("_") + 1);
        const newTdef = `gdn_tiledef_${kind}`;
        const newTmap = `gdn_tilemap_${kind}`;
        if (![...shared.values()].some((v) => v[1] === newTdef + ".json")) {
            for (const part of ["tiles", "pages", "cluts", "cols"]) {
                files.set(`${out}${newTdef}_${part}.bin`, (await inp.ps1Asset(`assets/st/${st}/${tdef}_${part}.bin`)).slice());
            }
            files.set(out + newTdef + ".json", tiledefJson(newTdef));
            files.set(out + newTmap + ".bin", (await inp.ps1Asset(`assets/st/${st}/${tmap}.bin`)).slice());
        }
        shared.set(room, [newTmap + ".bin", newTdef + ".json"]);
    }

    const layers: Record<string, Record<string, PyJson>>[] = [];
    const tilemaps: string[] = [];
    const prg = await inp.saturn(cfg.sat + ".PRG");
    for (let i = 0; i < convLayers.length; i++) {
        const cl = convLayers[i];
        const sh = shared.get(i);
        if (sh !== undefined) {
            const [data, tdef] = sh;
            const c = cl.fg!;
            const loading = i === 4 || i === 5;
            const exitRight = cfg.exits.some(([ex]) => ex[0] === c.right + 1 && ex[1] === c.top);
            layers.push({ fg: {
                data, tiledef: tdef, left: c.left, top: c.top, right: c.right, bottom: c.bottom,
                scrollMode: loading ? (exitRight ? 1 : 0) : 2,
                hideOnMap: false, isSaveRoom: !loading, isLoadingRoom: loading, unusedFlag: false,
                zPriority: loading ? loadingTps.get(i)! : 96, flags: 515 } });
            if (!tilemaps.includes(data)) {
                tilemaps.push(data);
            }
            continue;
        }
        const entry: Record<string, Record<string, PyJson>> = {};
        for (const part of ["fg", "bg"] as const) {
            const c = cl[part]!;
            const name = `gdn_tilemap_${tilemaps.length}.bin`;
            files.set(out + name, work.get(c.data + ".tilelayout.bin")!.slice());
            tilemaps.push(name);
            const loading = i === 4 || i === 5;
            const save = i === 6;
            entry[part] = {
                data: name, tiledef: "gdn_tiledef_gdn.json",
                left: c.left, top: c.top, right: c.right, bottom: c.bottom,
                scrollMode: loading || save ? 0 : 1,
                hideOnMap: false, isSaveRoom: save && part === "fg",
                isLoadingRoom: loading && part === "fg", unusedFlag: false,
                zPriority: part === "fg" ? (loading ? loadingTps.get(i)! : fgPriority(prg, i)) : 32,
                flags: (loading || save) && part === "fg" ? 515 : 3,
            };
        }
        layers.push(entry);
    }
    files.set(out + "layers.json", json(layers as PyJson));
    trimSharedTiledefs(layers, out, "gdn_tiledef_gdn.json", files);

    // red doors, the boss room's door
    let doorRows: number[][];
    [doorRows, nTiles] = redDoorRows(cfg.redDoors, layers, out, files, td, nTiles);
    if (cfg.bossDoor) {
        const boss = blankSolid(td, nTiles);
        nTiles++;
        files.set(src + "mod_boss_door.h", text(GEN_GDN + "// the boss room's door: blank art, solid\n" +
            `#define MOD_BOSS_DOOR_TILE 0x${hex(boss, 3)}\n`));
    }
    files.set(src + "mod_red_door_tiles.h", redDoorHeader(GEN_GDN, doorRows));
    handoff.vram4 = vram4;
    await backdrop(cfg, inp, work, fte, vram4, files, handoff);
    writeTileDefs(files, out + "gdn_tiledef_gdn", td, nTiles);
    handoff.td = td;
    handoff.nTiles = nTiles;

    const contextBanks = new Map(cfg.contextBanks);
    const roomContext = new Map(cfg.roomContext);
    const rooms: PyJson[] = [];
    convRooms.forEach((r, i) => {
        rooms.push({ left: r.left, top: r.top, right: r.right, bottom: r.bottom, layerId: i, tileDefId: 0,
            entityGfxId: roomContext.has(i) ? contextBanks.get(roomContext.get(i)!)! + 1 : 0,
            entityLayoutId: i + 1 });
    });
    for (const [[x, y], tp] of cfg.exits) {
        rooms.push({ left: x, top: y, right: x, bottom: y, layerId: tp > 127 ? tp - 256 : tp,
            tileDefId: -1, entityGfxId: 0, entityLayoutId: 0 });
    }
    files.set(out + "rooms.json", json(rooms));
    if (cfg.withSprites) {
        files.set(src + "mod_sprite_zones.h", spriteZones(cfg, convRooms));
    }

    const lists: PyJson[] = [layout([])];
    for (let i = 0; i < 7; i++) {
        const doors = cfg.redDoors.map((d, n) => [n, d] as const).filter(([, d]) => d[0] === i)
            .sort((a, b) => a[1][2] - b[1][2]);
        const ents: Ent[] = doors.map(([n, d]) => ({ x: d[1], y: d[2], id: "E_RED_DOOR", flags: 0, slot: d[3],
            spawnId: 0, params: n | (d[4] ? 0x100 : 0) }));
        if (i === 4 || i === 5) {
            ents.push({ ...cfg.loadingForeground });
        }
        if (i === 6) {
            ents.push(...cfg.saveForeground.map((e) => ({ ...e })));
        }
        if (i === 0) {
            ents.push(...cfg.water.map((e) => ({ ...e })));
        }
        if (cfg.candleRooms.includes(i)) {
            ents.push(...saturnCandles(prg, i));
        }
        ents.push(...saturnEntities(prg, i, cfg.saturnIds));
        lists.push(layout(ents));
    }
    files.set(out + "entity_layouts.json", json({ entities: lists, indices: range(0, lists.length) }));

    // (sprites() / pages_only() run here in the Python)

    let h = GEN_GDN;
    for (const t of tilemaps) {
        h += `#include <gen/${t.slice(0, -4)}.h>\n`;
    }
    const tdefs = [...new Set([...shared.values()].map((v) => v[1].slice(0, -5)))].sort();
    for (const t of ["gdn_tiledef_gdn", ...tdefs]) {
        h += `#include <gen/${t}.h>\n`;
    }
    files.set(src + "mod_tilemaps.h", text(h));
    files.set("build/us/" + cfg.fFile, fte);
    return { files, handoff };
}

function spriteZones(cfg: GdnConfig, convRooms: ConvRoom[]): Bytes {
    const banks = new Map(cfg.contextBanks);
    const reload = new Map(cfg.reloadBanks);
    let s = GEN_GDN;
    if (banks.has("0W") && banks.has("0E")) {
        s += "// room 0's two ends take turns in the sprite pages (ZONE_WEST/EAST)\n" +
            `#define MOD_ZONE_ROOM_LEFT ${convRooms[0].left}\n#define MOD_ZONE_ROOM_TOP ${convRooms[0].top}\n` +
            `#define MOD_ZONE_WEST ${cfg.zoneWest}\n#define MOD_ZONE_EAST ${cfg.zoneEast}\n` +
            `#define MOD_ZONE_WEST_GFX ANIMSET_OVL(${banks.get("0W")})\n` +
            `#define MOD_ZONE_EAST_GFX ANIMSET_OVL(${banks.get("0E")})\n`;
    }
    s += "// a room's sprite bank -> the same with the tile pages (RELOAD_BANKS),\n" +
        "// for the menu's reload; rooms without sprites reload the pages alone\n" +
        "static const u16 g_ModReloadBanks[][2] = {\n";
    for (const [c, index] of cfg.contextBanks) {
        s += `    {ANIMSET_OVL(${index}), ANIMSET_OVL(${reload.get(c)})},\n`;
    }
    return text(s + `};\n#define MOD_PAGES_GFX ANIMSET_OVL(${cfg.pageBank})\n`);
}

// --------------------------------------------------------------------------
// prs_assets.main() without sprites() and the build config

export async function buildPrs(cfg: PrsConfig, inp: StageInputs): Promise<StageResult> {
    const out = `assets/st/${cfg.stage}/`;
    const src = modStageDir(cfg.stage);
    const files = new Map<string, Bytes>();
    const convRooms = ((await parseJson(inp.work, "rooms.roomdef.json")) as ConvRoom[]).slice(0, 2);
    const convLayers = ((await parseJson(inp.work, "rooms.layers.json")) as ConvLayer[]).slice(0, 2);
    if (convRooms.map((r) => r.tileLayoutId).join() !== "0,1") {
        throw new Error("converter rooms");
    }
    const work = await loadWork(inp.work, workNames(convLayers, new Set(), false));
    await copyWrp(inp, out, files);

    let [td, nTiles] = await readTileDefs(work);
    const { fte, vram4 } = await repack(work, td, nTiles, convLayers, new Set(), new Set(GDN_RESERVED));
    if (vram4.some((b) => b !== 0)) {
        throw new Error("the Prison's tiles must fit its F_ file");
    }
    files.set(out + "prs_tiledef_prs.json", tiledefJson("prs_tiledef_prs"));

    const shared = new Map<number, [string, string]>();
    for (const [room, tmap, tdef, st] of cfg.sharedRooms) {
        const kind = tdef.slice(tdef.lastIndexOf("_") + 1);
        const newTdef = `prs_tiledef_${kind}`;
        const newTmap = `prs_tilemap_${kind}`;
        for (const part of ["tiles", "pages", "cluts", "cols"]) {
            files.set(`${out}${newTdef}_${part}.bin`, (await inp.ps1Asset(`assets/st/${st}/${tdef}_${part}.bin`)).slice());
        }
        files.set(out + newTdef + ".json", tiledefJson(newTdef));
        files.set(out + newTmap + ".bin", (await inp.ps1Asset(`assets/st/${st}/${tmap}.bin`)).slice());
        shared.set(room, [newTmap + ".bin", newTdef + ".json"]);
    }

    const layers: Record<string, Record<string, PyJson>>[] = [];
    const tilemaps: string[] = [];
    for (const cl of convLayers) {
        const entry: Record<string, Record<string, PyJson>> = {};
        for (const part of ["fg", "bg"] as const) {
            const c = cl[part]!;
            const name = `prs_tilemap_${tilemaps.length}.bin`;
            files.set(out + name, work.get(c.data + ".tilelayout.bin")!.slice());
            tilemaps.push(name);
            entry[part] = {
                data: name, tiledef: "prs_tiledef_prs.json",
                left: c.left, top: c.top, right: c.right, bottom: c.bottom,
                scrollMode: 1, hideOnMap: false, isSaveRoom: false,
                isLoadingRoom: false, unusedFlag: false,
                zPriority: part === "fg" ? cfg.fgPriority : 32, flags: 3,
            };
        }
        layers.push(entry);
    }
    for (const [room, , , , [x, y], tp] of cfg.sharedRooms) {
        const [data, tdef] = shared.get(room)!;
        const loading = tp !== null;
        const exitRight = cfg.exits.some(([ex]) => ex[0] === x + 1 && ex[1] === y);
        layers.push({ fg: {
            data, tiledef: tdef, left: x, top: y, right: x, bottom: y,
            scrollMode: loading ? (exitRight ? 1 : 0) : 2,
            hideOnMap: false, isSaveRoom: !loading, isLoadingRoom: loading,
            unusedFlag: false, zPriority: loading ? tp : 96, flags: 515 } });
        if (!tilemaps.includes(data)) {
            tilemaps.push(data);
        }
    }
    files.set(out + "layers.json", json(layers as PyJson));
    trimSharedTiledefs(layers, out, "prs_tiledef_prs.json", files);

    let doorRows: number[][];
    [doorRows, nTiles] = redDoorRows(cfg.redDoors, layers, out, files, td, nTiles);
    files.set(src + "mod_red_door_tiles.h", redDoorHeader(GEN_PRS, doorRows));
    // the sinking block's wall: tile 0's (blank) art, solid
    const block = blankSolid(td, nTiles);
    files.set(src + "mod_block_tile.h", text(GEN_PRS + "// the sinking block's wall: blank art, solid\n" +
        `#define MOD_BLOCK_SOLID_TILE 0x${hex(block, 3)}\n`));
    nTiles++;
    writeTileDefs(files, out + "prs_tiledef_prs", td, nTiles);

    const roomBanks = new Map(cfg.roomBanks);
    const rooms: PyJson[] = [];
    convRooms.forEach((r, i) => {
        rooms.push({ left: r.left, top: r.top, right: r.right, bottom: r.bottom, layerId: i, tileDefId: 0,
            entityGfxId: roomBanks.get(i)! + 1, entityLayoutId: i + 1 });
    });
    for (const [room, , , , [x, y]] of cfg.sharedRooms) {
        rooms.push({ left: x, top: y, right: x, bottom: y, layerId: room, tileDefId: 0, entityGfxId: 0,
            entityLayoutId: room + 1 });
    }
    for (const [[x, y], tp] of cfg.exits) {
        rooms.push({ left: x, top: y, right: x, bottom: y, layerId: tp > 127 ? tp - 256 : tp,
            tileDefId: -1, entityGfxId: 0, entityLayoutId: 0 });
    }
    files.set(out + "rooms.json", json(rooms));

    const prg = await inp.saturn(cfg.sat + ".PRG");
    const lists: PyJson[] = [layout([])];
    for (let i = 0; i < 5; i++) {
        const ents: Ent[] = [];
        cfg.redDoors.forEach(([room, x, y, slot, right], n) => {
            if (room === i) {
                ents.push({ x, y, id: "E_RED_DOOR", flags: 0, slot, spawnId: 0, params: n | (right ? 0x100 : 0) });
            }
        });
        if (i === 2 || i === 3) {
            ents.push({ ...cfg.loadingForeground });
        }
        if (i === 4) {
            ents.push(...cfg.saveForeground.map((e) => ({ ...e })));
        }
        if (i === 0 || i === 1) {
            ents.push(...saturnEntities(prg, i, cfg.saturnIds));
        }
        lists.push(layout(ents));
    }
    files.set(out + "entity_layouts.json", json({ entities: lists, indices: range(0, lists.length) }));

    let h = GEN_PRS;
    for (const t of tilemaps) {
        h += `#include <gen/${t.slice(0, -4)}.h>\n`;
    }
    const tdefs = [...new Set([...shared.values()].map((v) => v[1].slice(0, -5)))].sort();
    for (const t of ["prs_tiledef_prs", ...tdefs]) {
        h += `#include <gen/${t}.h>\n`;
    }
    files.set(src + "mod_tilemaps.h", text(h));
    files.set("build/us/" + cfg.fFile, fte);
    return { files, handoff: { pageBlocks: [], pageBank: 0, shimmerPalette: [], vram4, td, nTiles } };
}

export async function buildStage(stage: string, inp: StageInputs): Promise<StageResult> {
    const cfg = STAGES[stage];
    if (cfg === undefined) {
        throw new Error("unknown stage " + stage);
    }
    return cfg.kind === "gdn" ? buildGdn(cfg, inp) : buildPrs(cfg, inp);
}
