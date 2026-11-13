// SPDX-License-Identifier: AGPL-3.0-or-later
// A Saturn-only stage made from the player's discs, end to end: the Saturn
// map converter (tools/saturn/assets, `map convert-psx`, as WebAssembly), the
// PS1 tile swap (src/gdn_tile_swap.ts, its atlases rebuilt from the discs),
// the stage's tiles and layouts (src/stage_assets.ts) and its sprites
// (src/stage_sprites.ts). The result is what the release located in the
// compiled overlay (every generated array by name, as compiled) and the
// stage's F_TE*.BIN.
import { applyTileSwap, atlasInputs, rebuildAtlas, STAGE_PLANS, type Art } from "./gdn_tile_swap.ts";
import { ps1AssetReader } from "./ps1_assets.ts";
import { buildStage } from "./stage_assets.ts";
import { buildSprites } from "./stage_sprites.ts";

type Bytes = Uint8Array;
type Reader = (path: string) => Promise<Bytes>;

// an Emscripten build of the converter (MODULARIZE, INVOKE_RUN=0; the
// factory sets SOTN_MOD_UNCAPPED in preRun)
export interface SatConvModule {
    FS: {
        mkdir(p: string): void;
        writeFile(p: string, d: Bytes): void;
        readFile(p: string): Bytes;
        readdir(p: string): string[];
        stat(p: string): { mode: number };
        isDir(mode: number): boolean;
    };
    callMain(args: string[]): number;
}
export type SatConvFactory = () => Promise<SatConvModule>;

// the converter's area -> its Saturn stage files
const AREA_STAGE: Record<string, string> = { gdn: "STAGE_15", prs: "STAGE_16", rgdn: "RSTAGE15", rprs: "RSTAGE16" };
// read next to the PRG / MAP (tools/saturn/assets/src/psx_stage.rs)
const CONV_EXTRA = ["STAGE_15.CHR", "0.BIN", "GAME.PRG", "LOAD.MAP"];

export const STAGE_DISC: Record<string, string> = { gdn: "TE1", prs: "TE2", rgdn: "TE3", rprs: "TE4" };

// `map convert-psx` on an area: its output files by name
export async function convertArea(factory: SatConvFactory, saturn: Reader, area: string): Promise<Map<string, Bytes>> {
    const st = AREA_STAGE[area];
    const m = await factory();
    m.FS.mkdir("/in");
    m.FS.mkdir("/out");
    for (const f of [st + ".PRG", st + ".MAP", st + ".CHR", ...CONV_EXTRA]) {
        try {
            m.FS.writeFile("/in/" + f, await saturn(f));
        } catch {
            // a stage without its own CHR next to it: the converter reads what it needs
        }
    }
    const rc = m.callMain(["map", "convert-psx", "/in/" + st + ".PRG", "/in/" + st + ".MAP", "/out"]);
    if (rc !== 0) {
        throw new Error("the Saturn map converter failed on " + st + " (" + rc + ")");
    }
    const out = new Map<string, Bytes>();
    const walk = (d: string, rel: string): void => {
        for (const n of m.FS.readdir(d)) {
            if (n === "." || n === "..") {
                continue;
            }
            const p = d + "/" + n;
            if (m.FS.isDir(m.FS.stat(p).mode)) {
                walk(p, rel + n + "/");
            } else {
                out.set(rel + n, m.FS.readFile(p).slice());
            }
        }
    };
    walk("/out", "");
    return out;
}

// the numeric arrays of a generated C file (as node/c_arrays.ts reads them)
const ELEM: Record<string, number> = {
    u8: 1, s8: 1, char: 1, "unsigned char": 1, "signed char": 1, u16: 2, s16: 2, short: 2,
    "unsigned short": 2, u32: 4, s32: 4, int: 4, u_long: 4, "unsigned int": 4,
};

export function cArraysOf(text: string): Map<string, Bytes> {
    const src = text.replace(/\/\*[\s\S]*?\*\//g, "").replace(/\/\/[^\n]*/g, "");
    const out = new Map<string, Bytes>();
    const re = /(?:static\s+)?(?:const\s+)?([A-Za-z_][\w ]*?)\s+(\w+)\s*\[[^\]]*\](?:\s*\[[^\]]*\])*\s*(?:(?:__attribute__\s*\(\((?:[^()]|\([^()]*\))*\)\)|[A-Z_][A-Z0-9_]*)\s*)*=\s*\{((?:[^{}]|\{[^{}]*\})*)\}\s*;/g;
    for (const m of src.matchAll(re)) {
        const size = ELEM[m[1].trim()];
        if (!size) {
            continue;
        }
        const body = m[3].replace(/^\s*#\s*(if|ifdef|ifndef|endif)\b[^\n]*$/gm, "");
        const items = body.replace(/[{}]/g, ",").split(",").map((s) => s.trim()).filter((s) => s);
        if (!items.length || !items.every((it) => /^-?(0x[0-9a-fA-F]+|\d+)$/.test(it))) {
            continue;
        }
        out.set(m[2], numbers(size, items.map((it) => (it.startsWith("-") ? -Number(it.slice(1)) : Number(it)))));
    }
    return out;
}

function numbers(size: number, values: number[]): Bytes {
    const out = new Uint8Array(values.length * size);
    const v = new DataView(out.buffer);
    values.forEach((x, i) => {
        if (size === 1) {
            v.setUint8(i, x & 0xFF);
        } else if (size === 2) {
            v.setUint16(i * 2, x & 0xFFFF, true);
        } else {
            v.setUint32(i * 4, x >>> 0, true);
        }
    });
    return out;
}

// the asset tool's rooms handler (gen/rooms.c): 8 bytes a room, then 0x40
function roomsBytes(json: Bytes): Bytes {
    const rooms = JSON.parse(new TextDecoder().decode(json)) as Record<string, number>[];
    const out: number[] = [];
    for (const r of rooms) {
        out.push(r.left, r.top, r.right, r.bottom, r.layerId, r.tileDefId, r.entityGfxId, r.entityLayoutId);
    }
    out.push(0x40);
    return Uint8Array.from(out, (x) => x & 0xFF);
}

export interface StageBuild {
    arrays: Map<string, Bytes>; // symbol -> compiled bytes
    fte: Bytes; // F_TE*.BIN
}

// one stage (gdn, prs, rgdn, rprs) from the discs
export async function buildStageAll(stage: string, factory: SatConvFactory, saturn: Reader, ps1: Reader,
                                    works = new Map<string, Map<string, Bytes>>()): Promise<StageBuild> {
    const plan = STAGE_PLANS[stage];
    const sets = plan.jobs.map(([set]) => set);
    const areas = new Set<string>([stage, ...sets.flatMap((s) => atlasInputs(s).areas)]);
    for (const a of areas) {
        if (!works.has(a)) {
            works.set(a, await convertArea(factory, saturn, a));
        }
    }
    const atlases: Record<string, Art[]> = {};
    for (const set of sets) {
        const fbins: Record<string, Bytes> = {};
        for (const s of atlasInputs(set).stages) {
            fbins[s] = await ps1("ST/" + s + "/F_" + s + ".BIN");
        }
        atlases[set] = rebuildAtlas(set, Object.fromEntries(works), fbins);
    }
    const work = new Map([...works.get(stage)!].map(([k, v]) => [k, v.slice()]));
    applyTileSwap(stage, work, atlases);
    const main = await buildStage(stage, {
        work: (n) => {
            const d = work.get(n);
            if (!d) {
                throw new Error("stage " + stage + ": no " + n + " in the converter output");
            }
            return d;
        },
        saturn, ps1Asset: ps1AssetReader(ps1),
    });
    const sp = await buildSprites(stage, { saturn, ps1 }, main);
    const arrays = new Map<string, Bytes>();
    const st = stage;
    // the asset files the tool compiles as they are (tilemaps, tile definitions)
    for (const [p, d] of main.files) {
        const m = new RegExp("^assets/st/" + st + "/(\\w+)\\.bin$").exec(p);
        if (m) {
            arrays.set(m[1], d);
        }
    }
    const rooms = main.files.get("assets/st/" + st + "/rooms.json");
    if (rooms) {
        arrays.set("rooms", roomsBytes(rooms));
    }
    // the generated headers' numbers (the main part's, then the sprites')
    for (const files of [main.files, sp.files]) {
        for (const [p, d] of files) {
            if (/\.(h|c)$/.test(p)) {
                for (const [n, b] of cArraysOf(new TextDecoder("latin1").decode(d))) {
                    arrays.set(n, b);
                }
            }
        }
    }
    for (const [n, a] of sp.arrays) {
        arrays.set(n, numbers(a.size, a.values));
    }
    const fte = main.files.get("build/us/F_" + STAGE_DISC[stage] + ".BIN");
    if (!fte) {
        throw new Error("stage " + stage + ": no F_" + STAGE_DISC[stage] + ".BIN");
    }
    return { arrays, fte };
}
