// SPDX-License-Identifier: AGPL-3.0-or-later
// The Cursed Prison's doors in NO4 / NO0 and the Soul Prison's in RNO4 /
// RNO0: a byte-exact port of mods/sotn-ultimate/tools/gen_prs_links.py (see its header).
//
// Inputs: the Saturn disc (STAGE_09 + STAGE_9B.MAP, STAGE_03, RSTAGE09 +
// RSTAGE9B.MAP, RSTAGE03) and the PS1 disc (each stage's overlay, for its
// rooms, layer and tile tables and retail tilemaps, and its F_<S>.BIN). The
// script reads those tables from the repo's extracted assets and runs
// mipsel objdump on the compiled header only to assert the layer tables'
// layout (layer_empty, layers[], rooms_layers back to back), which
// parseStage() finds in the retail overlay the same way.
//
// Output: per header (src/st/<s>/mod_rooms.h) its arrays of plain numbers
// (g_ModRooms); per edited tilemap (assets/st/<s>/<name>.bin, compiled in
// place into the overlay's data: see tilemapEdits) its bytes.
import { tilematch, usedTiles, type TileDefs } from "./saturn_tilematch.ts";
import {
    bkey, check, count, layersJsonTilemaps, mostCommon, parseStage, u16Bytes,
    type CArrayOut, type GenResult, type PsxStage, type Reader,
} from "./mod_rooms.ts";

const TP: Record<string, number> = { MOD_TP_NO4_TO_PRS: 0x87, MOD_TP_NO0_TO_PRS: 0x89 };

interface Link {
    room: number[];
    load: number[];
    exit: number[];
    tp: string;
    // prg, map, descriptor set, room, window (r0, r1, c0, c1), overrides "r,c" -> (fg, bg)
    saturn: [string, string, number, number, [number, number, number, number], Map<string, [number, number]>];
    edits: [number, number, number][];
}

const range4 = (f: (i: number) => [number, number, number]): [number, number, number][] => [0, 1, 2, 3].map(f);

const LINKS: [string, Link][] = [
    ["no4", {
        room: [37, 33, 37, 34], load: [36, 34], exit: [35, 34], tp: "MOD_TP_NO4_TO_PRS",
        saturn: ["STAGE_09", "STAGE_9B", 1, 4, [18, 27, 0, 16], new Map()],
        edits: range4((i) => [22 + i, 0, 0x001]),
    }],
    ["no0", {
        room: [21, 34, 26, 36], load: [27, 36], exit: [28, 36], tp: "MOD_TP_NO0_TO_PRS",
        saturn: ["STAGE_03", "STAGE_03", 0, 6, [36, 45, 84, 96], new Map([["43,95", [0x002, 0]]])],
        edits: range4((i) => [38 + i, 95, 0x597]),
    }],
    ["rno4", {
        room: [26, 29, 26, 30], load: [27, 29], exit: [28, 29], tp: "MOD_TP_NO4_TO_PRS",
        saturn: ["RSTAGE09", "RSTAGE9B", 1, 4, [5, 14, 0, 16], new Map()],
        edits: range4((i) => [6 + i, 15, 0x001]),
    }],
    ["rno0", {
        room: [37, 27, 42, 29], load: [36, 27], exit: [35, 27], tp: "MOD_TP_NO0_TO_PRS",
        saturn: ["RSTAGE03", "RSTAGE03", 0, 6, [3, 12, 0, 12], new Map([["4,0", [0x002, 0]]])],
        edits: range4((i) => [9 - i, 0, 0x597]),
    }],
];

// (row, col, fg tile, bg tile) for the Saturn's cells in the window; used:
// the tilematch candidates (the tilemaps as the assets folder has them)
function saturnGraft(sat: Link["saturn"], prg: Uint8Array, map: Uint8Array, fbin: Uint8Array, td: TileDefs,
                     fg: number[], bg: number[], width: number, used: number[]): [number, number, number, number][] {
    const [, , dset, sroom, [r0, r1, c0, c1], overrides] = sat;
    const m0 = tilematch(prg, map, sroom, 0, fbin, td, used, false, dset);
    const m2 = tilematch(prg, map, sroom, 2, fbin, td, used, false, dset);
    const coll = m0.coll!;
    const fpx = m0.cells;
    const bpx = m2.cells;
    const nCells = fg.length;
    check(coll.length === nCells && bg.length === nCells && fpx.length / 1024 === nCells, "graft sizes");
    const cols = td.cols;
    const hw = (X: number, Y: number): number => {
        const o = ((((X >> 6) << 2) | ((Y >> 7) << 1) | ((X % 64) >> 5)) * 0x2000) + ((Y % 128) * 32 + (X % 32)) * 2;
        return fbin[o] | (fbin[o + 1] << 8);
    };
    const tiles = new Map<number, Int32Array>(); // -1 transparent, else r << 16 | g << 8 | b
    const render = (t: number): Int32Array => {
        let pix = tiles.get(t);
        if (pix === undefined) {
            pix = new Int32Array(256).fill(-1);
            if (t) {
                const page = td.tiles[t];
                const ix = td.pages[t];
                const k = td.cluts[t];
                const u = (ix & 15) * 16;
                const v = ix & 0xf0;
                for (let y = 0; y < 16; y++) {
                    for (let x = 0; x < 16; x++) {
                        const j = (hw(page * 64 + ((u + x) >> 2), v + y) >> (((u + x) % 4) * 4)) & 15;
                        if (j) {
                            const c = hw(16 * (k % 16) + j, 0xf0 + (k >> 4));
                            pix[y * 16 + x] = (Math.floor(((c & 31) * 255) / 31) << 16) |
                                (Math.floor((((c >> 5) & 31) * 255) / 31) << 8) | Math.floor((((c >> 10) & 31) * 255) / 31);
                        }
                    }
                }
            }
            tiles.set(t, pix);
        }
        return pix;
    };
    const pairPx = (f: number, b: number): Int32Array => {
        const p = render(f);
        const q = render(b);
        return p.map((v, i) => (v >= 0 ? v : q[i]));
    };
    const satCell = (n: number): Uint8Array => {
        const f = fpx.slice(n * 1024, (n + 1) * 1024);
        for (let q = 0; q < 1024; q += 4) {
            if (!f[q + 3]) {
                f.set(bpx.subarray(n * 1024 + q, n * 1024 + q + 4), q);
            }
        }
        return f;
    };
    const dist = (cell: Uint8Array, pix: Int32Array): number => {
        let d = 0;
        for (let i = 0; i < 256; i++) {
            const a = cell[i * 4 + 3];
            const q = pix[i];
            if (!a || q < 0) {
                d += (!a) === (q < 0) ? 0 : 3 * 255 * 255;
            } else {
                d += (cell[i * 4] - (q >> 16)) ** 2 + (cell[i * 4 + 1] - ((q >> 8) & 255)) ** 2 +
                    (cell[i * 4 + 2] - (q & 255)) ** 2;
            }
        }
        return Math.floor(d / 256);
    };
    const inside = (n: number): boolean => {
        const r = Math.floor(n / width);
        const c = n % width;
        return r0 <= r && r < r1 && c0 <= c && c < c1;
    };
    const learned = new Map<string, Map<number, number>>(); // cell -> (fg << 16 | bg) -> count
    for (let n = 0; n < nCells; n++) {
        if (!inside(n)) {
            const k = bkey(satCell(n));
            let c = learned.get(k);
            if (c === undefined) {
                learned.set(k, (c = new Map()));
            }
            count(c, fg[n] * 0x10000 + bg[n]);
        }
    }
    const rows = nCells / width;
    const near = new Set<number>();
    for (let r = Math.max(0, r0 - 4); r < Math.min(rows, r1 + 4); r++) {
        for (let c = Math.max(0, c0 - 4); c < Math.min(width, c1 + 4); c++) {
            near.add(fg[r * width + c] * 0x10000 + bg[r * width + c]);
        }
    }
    const out = new Map<number, [number, number] | null>();
    for (let r = r0; r < r1; r++) {
        for (let c = c0; c < c1; c++) {
            const n = r * width + c;
            const cell = satCell(n);
            const ov = overrides.get(r + "," + c);
            if (ov !== undefined) {
                out.set(n, ov);
                continue;
            }
            let holes = 0;
            for (let q = 3; q < 1024; q += 4) {
                holes += cell[q] ? 0 : 1;
            }
            if (holes > 64) {
                check(coll[n] === 0, "solid hole");
                out.set(n, null);
                continue;
            }
            const lc = learned.get(bkey(cell));
            if (lc !== undefined) {
                const p = mostCommon(lc);
                const f = Math.floor(p / 0x10000);
                if (cols[f] === coll[n]) {
                    out.set(n, [f, p % 0x10000]);
                    continue;
                }
            }
            // min by (dist, (fg, bg)): order-independent
            const cands = new Set(near);
            cands.add(m0.tiles[n] * 0x10000 + m2.tiles[n]);
            let best = -1;
            let bd = 0;
            for (const p of cands) {
                if (cols[Math.floor(p / 0x10000)] !== coll[n]) {
                    continue;
                }
                const d = dist(cell, pairPx(Math.floor(p / 0x10000), p % 0x10000));
                if (best < 0 || d < bd || (d === bd && p < best)) {
                    best = p;
                    bd = d;
                }
            }
            check(best >= 0, "no candidate");
            let pair: [number, number] = [Math.floor(best / 0x10000), best % 0x10000];
            if (cols[fg[n]] === coll[n] && dist(cell, pairPx(fg[n], bg[n])) <= bd) {
                pair = [fg[n], bg[n]];
            }
            out.set(n, pair);
        }
    }
    const holeCells = [...out.entries()].filter(([, v]) => v === null).map(([k]) => k).sort((a, b) => a - b);
    for (const n of holeCells) {
        const c = n % width;
        for (const d of [1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6]) {
            const m = n + d;
            if (!(0 <= c + d && c + d < width)) {
                continue;
            }
            const v = inside(m) ? out.get(m) ?? null : [fg[m], bg[m]] as [number, number];
            if (v && v[0] === 0 && v[1] && !render(v[1]).some((p) => p < 0)) {
                out.set(n, [0, v[1]]);
                break;
            }
        }
        check(out.get(n) !== null, "hole");
    }
    for (const [n, v] of out) {
        check(cols[v![0]] === coll[n] || overrides.has(Math.floor(n / width) + "," + (n % width)), "graft collision");
    }
    return [...out.entries()].sort((a, b) => a[0] - b[0]).map(([n, v]) => [Math.floor(n / width), n % width, v![0], v![1]]);
}

// where an edited tilemap sits in the retail overlay (the mod's overlay
// keeps its data section byte-identical to retail, edits aside)
export interface TilemapEdit {
    overlay: string; // e.g. "ST/NO4/NO4.BIN"
    offset: number;
    name: string; // assets/st/<s>/<name>.bin
}

export interface PrsResult extends GenResult {
    tilemapEdits: Map<string, TilemapEdit>; // by binaries key
    passes: Map<string, number>; // graft runs until the edited tilemaps no longer change them
}

async function build(sat: Reader, psx: Reader, stage: string, spec: Link, res: PrsResult): Promise<void> {
    const up = stage.toUpperCase();
    const ovPath = `ST/${up}/${up}.BIN`;
    const st: PsxStage = parseStage(await psx(ovPath));
    const fbin = await psx(`ST/${up}/F_${up}.BIN`);
    const { rooms, layers, roomdefs } = st;
    const room = rooms.findIndex((r) => r.slice(0, 4).join() === spec.room.join());
    check(room >= 0, "prs room");
    const [fgI, bgI] = roomdefs[rooms[room][4]];
    check(bgI >= 0, "prs room bg");
    const fg = layers[fgI];
    const bg = layers[bgI];
    check(fg.tdAddr === bg.tdAddr, "prs room tiledefs");
    const width = (fg.right - fg.left + 1) * 16;
    const td = st.tiledefs.get(fg.tdAddr)!;
    const prg = await sat(spec.saturn[0] + ".PRG");
    const map = await sat(spec.saturn[1] + ".MAP");

    // The script's tilematch candidates come from the tilemaps in the assets
    // folder, which hold its own earlier edits: run it until they settle.
    const edit = (base: number[], ed: [number, number, number][]): number[] => {
        const tm = base.slice();
        for (const [r, c, t] of ed) {
            tm[r * width + c] = t;
        }
        return tm;
    };
    const override = new Map<number, Uint8Array>();
    let fgNew: number[] = [];
    let bgNew: number[] = [];
    let passes = 0;
    for (;;) {
        passes++;
        check(passes <= 8, "graft does not settle");
        const used = usedTiles(layersJsonTilemaps(st, fg.tdAddr, override));
        const graft = saturnGraft(spec.saturn, prg, map, fbin, td, fg.tilemap, bg.tilemap, width, used);
        const f = edit(fg.tilemap, [...graft.map(([r, c, t]): [number, number, number] => [r, c, t]), ...spec.edits]);
        const b = edit(bg.tilemap, graft.map(([r, c, , t]): [number, number, number] => [r, c, t]));
        const same = f.join() === fgNew.join() && b.join() === bgNew.join();
        fgNew = f;
        bgNew = b;
        if (same) {
            break;
        }
        override.set(fgI, u16Bytes(f));
        override.set(bgI, u16Bytes(b));
    }
    res.passes.set(stage, passes);
    for (const [i, tm] of [[fgI, fgNew], [bgI, bgNew]] as [number, number[]][]) {
        const name = `${stage}_tilemap_${tilemapNumber(st, i)}`;
        const key = `assets/st/${stage}/${name}.bin`;
        res.binaries.set(key, u16Bytes(tm));
        res.tilemapEdits.set(key, { overlay: ovPath, offset: layers[i].tilemapAddr - 0x80180000, name });
    }

    const [lx, ly] = spec.load;
    const [ex, ey] = spec.exit;
    const side = ex > lx ? 0x41 : 0x40;
    // the stage's loading rooms are the layers with params 0x40 / 0x41 (and
    // only they use its "load" tiledef)
    const src = rooms.findIndex((r) => r[5] !== 0xff && r[0] === r[2] && r[1] === r[3] &&
        roomdefs[r[4]][0] >= 0 && layers[roomdefs[r[4]][0]].params === side);
    check(src >= 0, "no loading room");
    check(rooms.every((r) => r[5] === 0xff || [[lx, ly], [ex, ey]].every(([x, y]) =>
        !(r[0] <= x && x <= r[2] && r[1] <= y && y <= r[3]))), "cells taken");
    check(!rooms.some((r) => r[5] === 0xff && r[0] === ex && r[1] === ey), "exit taken");
    const outRooms = rooms.map((r) => r.slice());
    const rs = rooms[src];
    outRooms.push([lx, ly, lx, ly, roomdefs.length, rs[5], rs[6], rs[7]]);
    outRooms.push([ex, ey, ex, ey, TP[spec.tp], 0xff, 0, 0]);
    const v: number[] = [];
    for (const r of outRooms) {
        v.push(...r);
    }
    v.push(0x40);
    res.headers.set(`src/st/${stage}/mod_rooms.h`, new Map<string, CArrayOut>([["g_ModRooms", { size: 1, values: v }]]));
}

// the asset tool numbers a stage's tilemaps in layers[] order, a tilemap
// shared by several layers once (<stage>_tilemap_<n>)
export function tilemapNumber(st: PsxStage, i: number): number {
    const seen: number[] = [];
    for (const l of st.layers) {
        if (!seen.includes(l.tilemapAddr)) {
            seen.push(l.tilemapAddr);
        }
    }
    return seen.indexOf(st.layers[i].tilemapAddr);
}

// gen_prs_links.py: sat reads the Saturn disc ("STAGE_09.PRG"), psx the PS1
// disc ("ST/NO4/NO4.BIN")
export async function buildPrsLinks(sat: Reader, psx: Reader): Promise<PrsResult> {
    const res: PrsResult = { headers: new Map(), binaries: new Map(), tilemapEdits: new Map(), passes: new Map() };
    for (const [stage, spec] of LINKS) {
        await build(sat, psx, stage, spec, res);
    }
    return res;
}
