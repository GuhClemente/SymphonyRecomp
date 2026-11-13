// SPDX-License-Identifier: AGPL-3.0-or-later
// The PS1 assets stage_assets.ts reads (its StageInputs.ps1Asset), made from
// the player's PS1 US disc instead of the decomp's assets/ tree:
// - the shared loading / save room tile definitions and tilemaps are
//   verbatim slices of the retail stage overlays (RAW below);
// - assets/st/wrp's entity assets, which the Saturn stages copy (WRP_COPY),
//   are what the decomp's asset extractor (tools/sotn-assets, config
//   config/assets.us.yaml, target disks/us/ST/WRP/WRP.BIN) makes from
//   WRP.BIN: its sprite_banks / paldef / gfx_banks JSON, the cmpgfx images and
//   the palette (PNG + yaml), ported here byte for byte (Go's encoding/json
//   MarshalIndent layout, its PNG encoder with Go's zlib BestSpeed: go_png.ts).
import { goPngEncode, greyPalette4, paletteFromR5G5B5A1 } from "./go_png.ts";
import { pyJsonDumps } from "./pyjson.ts";
import type { PyJson } from "./pyjson.ts";

export type DiscReader = (discPath: string) => Promise<Uint8Array>;

// repo path -> (disc path, offset, length): verbatim slices
export const RAW: Record<string, [string, number, number]> = {};
function raw(stage: string, file: string, entries: [string, number, number][]): void {
    for (const [name, off, len] of entries) {
        RAW[`assets/st/${stage}/${name}.bin`] = [file, off, len];
    }
}
raw("wrp", "ST/WRP/WRP.BIN", [
    ["wrp_tiledef_load_tiles", 0x2968, 0x100], ["wrp_tiledef_load_pages", 0x2A68, 0x100],
    ["wrp_tiledef_load_cluts", 0x2B68, 0x100], ["wrp_tiledef_load_cols", 0x2C68, 0x100],
    ["wrp_tilemap_5", 0x2768, 0x200]]);
raw("np3", "ST/NP3/NP3.BIN", [
    ["np3_tiledef_save_tiles", 0x22104, 0x1000], ["np3_tiledef_save_pages", 0x23104, 0x1000],
    ["np3_tiledef_save_cluts", 0x24104, 0x1000], ["np3_tiledef_save_cols", 0x25104, 0x1000],
    ["np3_tilemap_37", 0x218F4, 0x200], ["np3_tilemap_38", 0x21AF4, 0x200]]);
raw("rwrp", "ST/RWRP/RWRP.BIN", [
    ["rwrp_tiledef_rload_tiles", 0x2BC8, 0x100], ["rwrp_tiledef_rload_pages", 0x2CC8, 0x100],
    ["rwrp_tiledef_rload_cluts", 0x2DC8, 0x100], ["rwrp_tiledef_rload_cols", 0x2EC8, 0x100],
    ["rwrp_tilemap_5", 0x29C8, 0x200]]);
// (rno4_tilemap_59's bytes occur 3 times in RNO4.BIN: 0x34824, 0x34A24,
// 0x34C24, identical; any serves)
raw("rno4", "ST/RNO4/RNO4.BIN", [
    ["rno4_tiledef_rsave_tiles", 0x35234, 0x1000], ["rno4_tiledef_rsave_pages", 0x36234, 0x1000],
    ["rno4_tiledef_rsave_cluts", 0x37234, 0x1000], ["rno4_tiledef_rsave_cols", 0x38234, 0x1000],
    ["rno4_tilemap_58", 0x34624, 0x200], ["rno4_tilemap_59", 0x34824, 0x200]]);

const WRP_BIN = "ST/WRP/WRP.BIN";
const BASE = 0x80180000;
const GAME_END = 0x801F8000;
// the names the extractor gives WRP's addresses (its config entries)
const WRP_SYMBOLS = new Map<number, string>([
    [0x80181420, "stage_title_wrp_jp"], [0x80181764, "stage_placeholder_wrp"], [0x80181D08, "D_80181D08"],
]);

// repo path -> how it is made from WRP.BIN
export const EXTRACTED: Record<string, string> = {
    "assets/st/wrp/us/sprite_banks.json": "sprite_banks 0x40-0xA0",
    "assets/st/wrp/palette_def.json": "paldef 0xA0-0xB8",
    "assets/st/wrp/graphics_banks.json": "gfx_banks 0x1B8-0x23C",
    "assets/st/wrp/stage_title_wrp_jp.png": "cmpgfx 0x1420-0x1764, 128x128 4bpp",
    "assets/st/wrp/stage_placeholder_wrp.png": "cmpgfx 0x1764-0x1D08, 128x128 4bpp",
    "assets/st/wrp/D_80181D08.yaml": "palette 0x1D08-0x1D68, 16 colours (3 palettes)",
    "assets/st/wrp/D_80181D08/D_80181D08_0.png": "palette 0x1D08, 16 colours",
    "assets/st/wrp/D_80181D08/D_80181D08_1.png": "palette 0x1D28, 16 colours",
    "assets/st/wrp/D_80181D08/D_80181D08_2.png": "palette 0x1D48, 16 colours",
};

const enc = new TextEncoder();
const goJson = (v: PyJson): Uint8Array => enc.encode(pyJsonDumps(v, 2)); // == json.MarshalIndent(v, "", "  ") here
const hex8 = (a: number): string => a.toString(16).toUpperCase().padStart(8, "0");

class Ovl {
    d: Uint8Array;

    constructor(d: Uint8Array) {
        this.d = d;
    }

    // little-endian reads at a RAM address; null past the file's end (EOF)
    u16(a: number): number | null {
        const o = a - BASE;
        return o + 2 <= this.d.length ? this.d[o] | (this.d[o + 1] << 8) : null;
    }

    s16(a: number): number {
        const v = this.u16(a)!;
        return v >= 0x8000 ? v - 0x10000 : v;
    }

    u32(a: number): number {
        const o = a - BASE;
        if (o + 4 > this.d.length) {
            throw new Error("ps1 assets: read past the overlay");
        }
        return (this.d[o] | (this.d[o + 1] << 8) | (this.d[o + 2] << 16) | (this.d[o + 3] << 24)) >>> 0;
    }
}

const inRange = (a: number, b: number, e: number): boolean => a >= b && a < e;
const align = (a: number, n: number): number => Math.ceil(a / n) * n; // (RAM addresses exceed int32)
type Range = [number, number];

// datarange.Merge: contiguous ranges or an error
function merge(ranges: Range[]): Range {
    const r = ranges.slice().sort((a, b) => a[0] - b[0]);
    for (let i = 0; i < r.length - 1; i++) {
        if (r[i][1] !== r[i + 1][0]) {
            throw new Error("data ranges don't add up");
        }
    }
    return [r[0][0], r[r.length - 1][1]];
}

// assets/spriteset: ReadSpriteSet / readSpriteParts
function readSpriteSet(ovl: Ovl, addr: number): [PyJson[], Range] {
    if (!inRange(addr, BASE, GAME_END)) {
        throw new Error("invalid sprite offset");
    }
    let earliest = GAME_END;
    let cur = addr;
    const offsets: number[] = [];
    while (cur < earliest) {
        const off = ovl.u32(cur);
        cur += 4;
        offsets.push(off);
        if (off !== 0) {
            if (!inRange(off, BASE, GAME_END)) {
                throw new Error("sprite offset not valid");
            }
            earliest = Math.min(earliest, off);
        }
    }
    const ranges: Range[] = [[addr, earliest]];
    const bank: PyJson[] = offsets.map((off) => {
        if (off === 0) {
            return null;
        }
        const count = ovl.u16(off);
        if (count === null) {
            return [];
        }
        if (count >= 0x8000) {
            ranges.push([off, align(off + 8, 4)]);
            return [{ flags: count, x: ovl.s16(off + 2), y: ovl.s16(off + 4), width: ovl.u16(off + 6)!,
                height: 0, clut: 0, tileset: 0, left: 0, top: 0, right: 0, bottom: 0 }];
        }
        const parts: PyJson[] = [];
        for (let i = 0; i < count; i++) {
            const p = off + 2 + i * 22;
            parts.push({ flags: ovl.u16(p)!, x: ovl.s16(p + 2), y: ovl.s16(p + 4), width: ovl.u16(p + 6)!,
                height: ovl.u16(p + 8)!, clut: ovl.u16(p + 10)!, tileset: ovl.u16(p + 12)!, left: ovl.u16(p + 14)!,
                top: ovl.u16(p + 16)!, right: ovl.u16(p + 18)!, bottom: ovl.u16(p + 20)! });
        }
        ranges.push([off, align(off + 4 + 0x16 * count, 4)]);
        return parts;
    });
    return [bank, merge(ranges)];
}

// assets/spritebanks: ReadSpritesBanks (with recoverGaps)
function spriteBanks(ovl: Ovl, start: number, numBanks: number): PyJson {
    const offBanks: number[] = [];
    let p = BASE + start;
    for (;;) {
        const a = ovl.u32(p);
        p += 4;
        if ((a !== 0 && !inRange(a, BASE, GAME_END)) || offBanks.length === numBanks) {
            break;
        }
        offBanks.push(a);
    }
    const pool = new Map<number, PyJson[]>();
    const known: Range[] = [];
    for (const a of offBanks) {
        if (a === 0 || pool.has(a)) {
            continue;
        }
        const [bank, r] = readSpriteSet(ovl, a);
        pool.set(a, bank);
        known.push(r);
    }
    // recoverGaps: unreferenced banks between the known ones
    for (;;) {
        const sorted = known.slice().sort((x, y) => x[0] - y[0]);
        let end = 0;
        let gap: Range | null = null;
        for (const cur of sorted) {
            if (end !== 0 && end < cur[0]) {
                gap = [end, cur[0]];
                break;
            }
            if (cur[1] > end) {
                end = cur[1];
            }
        }
        if (gap === null) {
            break;
        }
        let got: [PyJson[], Range];
        try {
            got = readSpriteSet(ovl, gap[0]);
        } catch {
            break;
        }
        const [bank, r] = got;
        if (r[0] !== gap[0] || r[1] <= gap[0] || r[1] > gap[1]) {
            break;
        }
        pool.set(gap[0], bank);
        known.push(r);
    }
    const sortedOffsets = [...pool.keys()].sort((x, y) => x - y);
    return {
        banks: sortedOffsets.map((o) => pool.get(o)!),
        indices: offBanks.map((o) => (o === 0 ? -1 : sortedOffsets.indexOf(o))),
    };
}

// assets/paldef: readPaletteDefs / readClutsArray / resolveSymbols
function paletteDefs(ovl: Ovl, start: number, end: number): PyJson {
    interface Entry { destination: number; length: number; addr: number }
    interface Def { kind: number; flags: number; freq: number; destination: number; count: number; addr: number;
        entries: Entry[]; data: Entry[] }
    const defs: Def[] = [];
    let off = start;
    while (off < end) {
        const addr = BASE + off;
        const op = ovl.u32(addr);
        if (op === 0) {
            off += 4;
            continue;
        }
        const flags = op & 0xC000;
        const kind = op & 0xFFFF & ~0xC000;
        const freq = op >>> 16;
        if (kind < 1 || kind > 5 || freq > 0xFF) {
            break;
        }
        const def: Def = { kind, flags, freq, destination: 0, count: 0, addr, entries: [], data: [] };
        const triplets = kind === 1 || kind === 5;
        let q = addr + 4;
        if (!triplets) {
            def.destination = ovl.u32(q);
            def.count = ovl.u32(q + 4);
            q += 8;
        }
        for (;;) {
            const dst = ovl.u32(q);
            q += 4;
            if (dst === 0xFFFFFFFF) {
                break;
            }
            let length = 0;
            let a = dst;
            if (triplets) {
                length = ovl.u32(q);
                a = ovl.u32(q + 4);
                q += 8;
            }
            if (!inRange(a, BASE, GAME_END)) {
                throw new Error("palette entry out of the stage");
            }
            if (triplets) {
                def.entries.push({ destination: dst, length, addr: a });
            } else {
                def.data.push({ destination: 0, length: 0, addr: a });
            }
        }
        off = q - BASE;
        defs.push(def);
    }
    const cluts: number[] = [];
    for (let pos = off; pos + 4 <= end; pos += 4) {
        const a = ovl.u32(BASE + pos);
        if (a === 0) {
            continue;
        }
        const i = defs.findIndex((d) => d.addr === a);
        if (i < 0) {
            throw new Error("cluts entry points to no palette def");
        }
        cluts.push(i);
    }
    const ref = (e: Entry): PyJson => {
        let name = `D_${hex8(e.addr)}.palbin`;
        let offset = 0;
        for (let o = 0; o <= 0x2000 && e.addr - o >= BASE; o += 2) {
            const s = WRP_SYMBOLS.get(e.addr - o);
            if (s !== undefined) {
                name = s + ".palbin";
                offset = o / 2;
                break;
            }
        }
        const out: Record<string, PyJson> = {};
        if (e.destination) {
            out.destination = e.destination;
        }
        if (e.length) {
            out.length = e.length;
        }
        out.name = name;
        if (offset) {
            out.offset = offset;
        }
        return out;
    };
    return {
        cluts,
        defs: defs.map((d) => {
            const o: Record<string, PyJson> = { kind: d.kind };
            for (const k of ["flags", "freq", "destination", "count"] as const) {
                if (d[k]) {
                    o[k] = d[k];
                }
            }
            if (d.entries.length) {
                o.entries = d.entries.map(ref);
            }
            if (d.data.length) {
                o.data = d.data.map(ref);
            }
            return o;
        }),
    };
}

// assets/gfxbanks: Extract / ReadGraphics
function gfxBanks(ovl: Ovl, start: number): PyJson {
    const baseStart = BASE + start;
    let actualStart = baseStart;
    for (let p = baseStart; ; p += 4) {
        const a = ovl.u32(p);
        if (inRange(a, baseStart, actualStart)) {
            break;
        }
        actualStart += 4;
    }
    const addrs: number[] = [];
    for (let p = actualStart; ; p += 4) {
        const a = ovl.u32(p);
        if (a === 0) {
            addrs.push(0);
            continue;
        }
        if (!inRange(a, BASE, actualStart)) {
            break;
        }
        addrs.push(a);
    }
    if (addrs[addrs.length - 1] === 0) {
        addrs.pop();
    }
    const sorted = [...new Set(addrs)].filter((a) => a !== 0).sort((x, y) => x - y);
    const pool = new Map<number, number>([[0, -1]]);
    const banks: PyJson[] = [];
    const last = sorted[sorted.length - 1];
    for (let a = sorted[0]; a <= last;) {
        const kind = ovl.u32(a);
        let next: number;
        const bank: Record<string, PyJson> = { kind };
        if (kind === 0xFFFFFFFF) {
            next = a + 4;
        } else {
            const entries: PyJson[] = [];
            let q = a + 4;
            for (;;) {
                const x = ovl.u16(q)!;
                const y = ovl.u16(q + 2)!;
                if (x === 0xFFFF && y === 0xFFFF) {
                    break;
                }
                const w = ovl.u16(q + 4)!;
                const h = ovl.u16(q + 6)!;
                const ea = ovl.u32(q + 8);
                q += 12;
                const e: Record<string, PyJson> = {};
                if (x) {
                    e.x = x;
                }
                if (y) {
                    e.y = y;
                }
                if (w) {
                    e.width = w;
                }
                if (h) {
                    e.height = h;
                }
                if (x || y || w || h) {
                    e.name = WRP_SYMBOLS.get(ea) ?? `D_${hex8(ea)}`;
                }
                entries.push(e);
            }
            if (entries.length) {
                bank.entries = entries;
            }
            next = align(a + 4 + entries.length * 12 + 4, 4);
        }
        pool.set(a, banks.length);
        banks.push(bank);
        a = next;
    }
    return { banks, indices: addrs.map((a) => pool.get(a) ?? 0) };
}

// sotn.Inflate: the games' nibble-RLE image compression
function sotnInflate(src: Uint8Array): Uint8Array {
    let srcPos = 8;
    const dict = src.subarray(0, 8);
    let readFlag = 0;
    let writeFlag = 0;
    const dst: number[] = [];
    const rn = (): number => {
        if (readFlag === 0) {
            readFlag = 1;
            return (src[srcPos] >> 4) & 15;
        }
        readFlag = 0;
        return src[srcPos++] & 15;
    };
    const wn = (ch: number): void => {
        if (writeFlag === 0) {
            writeFlag = 1;
            dst.push(ch);
        } else {
            writeFlag = 0;
            dst[dst.length - 1] = (dst[dst.length - 1] + (ch << 4)) & 0xFF;
        }
    };
    for (;;) {
        const op = rn();
        if (op === 0) {
            let n = (rn() << 4) & 0xFF;
            n += (rn() + 0x13) & 0xFF;
            for (; n > 0; n--) {
                wn(0);
            }
        } else if (op === 1) {
            wn(rn());
        } else if (op === 2) {
            const ch = rn();
            wn(ch);
            wn(ch);
        } else if (op === 3) {
            wn(rn());
            wn(rn());
        } else if (op === 4) {
            wn(rn());
            wn(rn());
            wn(rn());
        } else if (op === 5) {
            const ch = rn();
            for (let n = rn() + 3; n > 0; n--) {
                wn(ch);
            }
        } else if (op === 6) {
            for (let n = rn() + 3; n > 0; n--) {
                wn(0);
            }
        } else if (op === 15) {
            return Uint8Array.from(dst);
        } else {
            const kind = dict[op - 7] & 0xF0;
            const amount = dict[op - 7] & 0x0F;
            if (kind === 0x10) {
                wn(amount);
            } else if (kind === 0x20) {
                wn(amount);
                wn(amount);
            } else if (kind === 0x60) {
                for (let n = amount + 3; n > 0; n--) {
                    wn(0);
                }
            }
        }
    }
}

function extractWrp(d: Uint8Array, path: string): Uint8Array {
    const ovl = new Ovl(d);
    const p = path.slice("assets/st/wrp/".length);
    switch (p) {
        case "us/sprite_banks.json":
            return goJson(spriteBanks(ovl, 0x40, (0xA0 - 0x40) / 4));
        case "palette_def.json":
            return goJson(paletteDefs(ovl, 0xA0, 0xB8));
        case "graphics_banks.json":
            return goJson(gfxBanks(ovl, 0x1B8));
        case "stage_title_wrp_jp.png":
            return goPngEncode(sotnInflate(d.subarray(0x1420, 0x1764)), 128, 128, greyPalette4());
        case "stage_placeholder_wrp.png":
            return goPngEncode(sotnInflate(d.subarray(0x1764, 0x1D08)), 128, 128, greyPalette4());
        case "D_80181D08.yaml":
            return enc.encode("colors_per_palette: 16\npalettes:\n" +
                [0, 1, 2].map((i) => `- D_80181D08/D_80181D08_${i}.png\n`).join(""));
        default: {
            const m = /^D_80181D08\/D_80181D08_([0-2])\.png$/.exec(p);
            if (m) {
                const o = 0x1D08 + Number(m[1]) * 32;
                return goPngEncode(Uint8Array.from({ length: 16 }, (_, i) => i), 16, 1,
                                   paletteFromR5G5B5A1(d.subarray(o, o + 32)));
            }
        }
    }
    throw new Error("ps1 assets: no recipe for " + path);
}

// the reader stage_assets.ts takes as StageInputs.ps1Asset
export function ps1AssetReader(readPs1: DiscReader): (repoPath: string) => Promise<Uint8Array> {
    const cache = new Map<string, Promise<Uint8Array>>();
    const disc = (p: string): Promise<Uint8Array> => {
        let f = cache.get(p);
        if (f === undefined) {
            f = readPs1(p);
            cache.set(p, f);
        }
        return f;
    };
    return async (repoPath: string): Promise<Uint8Array> => {
        const r = RAW[repoPath];
        if (r !== undefined) {
            const [file, off, len] = r;
            const d = await disc(file);
            if (off + len > d.length) {
                throw new Error(`ps1 assets: ${file} too short for ${repoPath}`);
            }
            return d.slice(off, off + len);
        }
        if (EXTRACTED[repoPath] !== undefined) {
            return extractWrp(await disc(WRP_BIN), repoPath);
        }
        throw new Error("ps1 assets: no recipe for " + repoPath);
    };
}
