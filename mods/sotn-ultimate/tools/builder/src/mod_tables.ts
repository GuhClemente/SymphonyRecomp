// SPDX-License-Identifier: AGPL-3.0-or-later
// The mod's small generated tables, regenerated from the player's discs: a
// byte-exact port of mods/sotn-ultimate/tools/gen_mod_layouts.py, gen_np3_sconce.py,
// gen_no3_horse.py, gen_saturn_items.py (equipment_mod.h), gen_saturn_enemies.py,
// gen_saturn_bestiary.py and gen_mar2_items.py (see their headers).
//
// Every function returns Map<header path, Map<name, ModArray>>, the arrays in
// header order. A plain numeric array (kind "array") is what parseCArrays
// reads from the header; the others are the compiled bytes of arrays it
// can't parse:
//   - kind "struct": an array of structs, whole, as its numbers back to back
//     (LayoutEntity = 5 u16; ModHorseStrip = u32 off + 4 u16, given as 6 u16
//     little-endian halves, the same bytes);
//   - kind "run": part of a struct array, named "symbol+0xOFFSET" (the byte
//     offset in the symbol), as bytes: the numeric fields of g_EquipDefs /
//     g_EnemyDefs records (each record's name / description pointers, bytes
//     0-7 / 0-3, are the mod's and stay in the patch).
// Not covered here (mod-authored or tiny): gen_saturn_balance.py (typed-in
// values the script only checks), accessories_mod.h (the retail table + typed
// entries), TT_005 / TT_002 (mod code; see the report), the sconce's
// g_ModSconceBank / g_ModPalDef / g_ModGfxBanks / g_ModSpriteBanks (pointers,
// retail PS1 addresses and the art's row / palette counts), the bestiary's
// descriptions (a fan translation).
import { mbLzss } from "./cen_maria.ts";
import { PsxSpriteSheet, SaturnStage, SpriteState } from "./saturn_sprites.ts";
import type { PsxPart } from "./saturn_sprites.ts";

export type DiscRead = (path: string) => Promise<Uint8Array>;

export interface ModArray {
    size: number; // bytes per element
    values: number[];
    kind: "array" | "struct" | "run";
}

export type ModTables = Map<string, Map<string, ModArray>>;

const be16 = (d: Uint8Array, o: number): number => (d[o] << 8) | d[o + 1];
const bes16 = (d: Uint8Array, o: number): number => (be16(d, o) << 16) >> 16;
const be32 = (d: Uint8Array, o: number): number => ((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) >>> 0;
const le16 = (d: Uint8Array, o: number): number => d[o] | (d[o + 1] << 8);
const les16 = (d: Uint8Array, o: number): number => (le16(d, o) << 16) >> 16;
const le32 = (d: Uint8Array, o: number): number => (d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | (d[o + 3] << 24)) >>> 0;

function check(ok: boolean, what: string): void {
    if (!ok) {
        throw new Error("mod tables: " + what);
    }
}

function put(out: ModTables, header: string, name: string, a: ModArray): void {
    let m = out.get(header);
    if (m === undefined) {
        m = new Map();
        out.set(header, m);
    }
    m.set(name, a);
}

export function mergeTables(...all: ModTables[]): ModTables {
    const out: ModTables = new Map();
    for (const t of all) {
        for (const [h, m] of t) {
            for (const [n, a] of m) {
                put(out, h, n, a);
            }
        }
    }
    return out;
}

// --- gen_mod_layouts.py -------------------------------------------------------

interface LayoutAdd {
    x: number;
    y: number;
    id: string;
    flags?: number;
    slot: number;
    params: number;
}

interface LayoutSpec {
    key: string;
    dir?: string;
    bin: string;
    tables: [number, number];
    layout: number;
    suffix?: string;
    replace?: boolean;
    idCheck: [string, number][];
    add: LayoutAdd[];
    move?: { x: number; y: number; eid: number; params: number; slot: number }[];
}

// mod data: the stages' layout picks and the added entities (the script's
// STAGES, in its order)
const DROP = "E_PERSISTENT_ITEM_DROP";
const ORB_MOVE = [{ x: 0x110, y: 0xc0, eid: 0x000b, params: 5, slot: 0x18 }];
const LAYOUTS: LayoutSpec[] = [
    { key: "top", bin: "ST/TOP/TOP.BIN", tables: [0x80180740, 0x80180814], layout: 1,
      idCheck: [["E_RELIC_ORB", 0x0b]], add: [{ x: 640, y: 960, id: "E_RELIC_ORB", slot: 11, params: 30 }] },
    { key: "no4", bin: "ST/NO4/NO4.BIN", tables: [0x80180864, 0x80180938], layout: 25,
      idCheck: [[DROP, 0x0c]], add: [{ x: 216, y: 152, id: DROP, slot: 17, params: 40 }] },
    { key: "bo3", dir: "src/boss/bo3", bin: "BOSS/BO3/BO3.BIN", tables: [0x80180280, 0x80180354], layout: 25,
      idCheck: [[DROP, 0x0c]], add: [{ x: 216, y: 152, id: DROP, slot: 17, params: 40 }] },
    { key: "no1", bin: "ST/NO1/NO1.BIN", tables: [0x80180618, 0x801806ec], layout: 14,
      idCheck: [["E_ID_51", 0x51]], add: [
          { x: 512, y: 776, id: "E_ID_51", slot: 0x824, params: 0x100 | 0x80 | 2 | 4 },
          { x: 480, y: 2072, id: "E_ID_51", slot: 0x926, params: 0x90 | 1 | 2 },
          { x: 480, y: 1600, id: "E_ID_51", slot: 0xa28, params: 0x80 | 1 | 4 },
      ] },
    { key: "cat", bin: "ST/CAT/CAT.BIN", tables: [0x80180e24, 0x80180ef8], layout: 6,
      idCheck: [[DROP, 0x0c]], add: [{ x: 64, y: 185, id: DROP, slot: 0x10, params: 0x15 }] },
    { key: "nz1", bin: "ST/NZ1/NZ1.BIN", tables: [0x80180820, 0x801808f4], layout: 5,
      idCheck: [[DROP, 0x0c]], add: [{ x: 640, y: 297, id: DROP, slot: 3, params: 12 }] },
    { key: "wrp", bin: "ST/WRP/WRP.BIN", tables: [0x8018023c, 0x80180310], layout: 6, replace: true,
      idCheck: [["E_WARP", 0x16], ["E_RED_DOOR", 0x05]], add: [
          { x: 128, y: 144, id: "E_WARP", slot: 0x10, params: 5 },
          { x: 248, y: 97, id: "E_RED_DOOR", slot: 0x90, params: 0x100 },
      ] },
    { key: "no4#2", bin: "ST/NO4/NO4.BIN", tables: [0x80180864, 0x80180938], layout: 6, suffix: "2",
      idCheck: [["E_RED_DOOR", 0x05]], add: [{ x: 8, y: 353, id: "E_RED_DOOR", slot: 0x90, params: 0 }] },
    { key: "no0", bin: "ST/NO0/NO0.BIN", tables: [0x80180778, 0x8018084c], layout: 1,
      idCheck: [["E_RED_DOOR", 0x05]], add: [{ x: 1528, y: 609, id: "E_RED_DOOR", slot: 0x93, params: 0x100 }] },
    { key: "no3", bin: "ST/NO3/NO3.BIN", tables: [0x8018077c, 0x80180850], layout: 1,
      idCheck: [], add: [], move: ORB_MOVE },
    { key: "np3#2", bin: "ST/NP3/NP3.BIN", tables: [0x80180728, 0x801807fc], layout: 1, suffix: "2",
      idCheck: [], add: [], move: ORB_MOVE },
    // the first visit's Entrance: the same shaft list as np3's (filled in below)
    { key: "no3#2", bin: "ST/NO3/NO3.BIN", tables: [0x8018077c, 0x80180850], layout: 2, suffix: "2", replace: true,
      idCheck: [], add: [] },
    { key: "np3", bin: "ST/NP3/NP3.BIN", tables: [0x80180728, 0x801807fc], layout: 2, replace: true,
      idCheck: [["E_BREAKABLE", 0x01], ["E_RED_DOOR", 0x05]], add: [
          { x: 70, y: 480, id: "E_BREAKABLE", flags: 0xa000, slot: 0x101, params: 0x2000 },
          { x: 204, y: 687, id: "E_BREAKABLE", flags: 0xa000, slot: 0x202, params: 0x200e },
          { x: 248, y: 865, id: "E_RED_DOOR", slot: 0x90, params: 0x100 },
          { x: 70, y: 991, id: "E_BREAKABLE", flags: 0xa000, slot: 0x303, params: 0x2000 },
      ] },
    { key: "rno3", bin: "ST/RNO3/RNO3.BIN", tables: [0x801805ec, 0x801806c0], layout: 2, replace: true,
      idCheck: [["E_BREAKABLE", 0x01], ["E_RED_DOOR", 0x05]], add: [
          { x: 8, y: 97, id: "E_RED_DOOR", slot: 0x90, params: 0 },
          { x: 52, y: 337, id: "E_BREAKABLE", flags: 0xa000, slot: 0x202, params: 0x200e },
          { x: 186, y: 544, id: "E_BREAKABLE", flags: 0xa000, slot: 0x101, params: 0x2000 },
          { x: 186, y: 33, id: "E_BREAKABLE", flags: 0xa000, slot: 0x303, params: 0x2000 },
      ] },
    { key: "rwrp", bin: "ST/RWRP/RWRP.BIN", tables: [0x8018023c, 0x80180310], layout: 6, replace: true,
      idCheck: [["E_WARP", 0x16], ["E_RED_DOOR", 0x05]], add: [
          { x: 8, y: 97, id: "E_RED_DOOR", slot: 0x90, params: 0 },
          { x: 128, y: 144, id: "E_WARP", slot: 0x10, params: 5 },
      ] },
    { key: "rno4", bin: "ST/RNO4/RNO4.BIN", tables: [0x801807b8, 0x8018088c], layout: 6,
      idCheck: [["E_RED_DOOR", 0x05]], add: [{ x: 248, y: 97, id: "E_RED_DOOR", slot: 0x90, params: 0x100 }] },
    { key: "rno0", bin: "ST/RNO0/RNO0.BIN", tables: [0x80180754, 0x80180828], layout: 1,
      idCheck: [["E_RED_DOOR", 0x05]], add: [{ x: 8, y: 97, id: "E_RED_DOOR", slot: 0x90, params: 0 }] },
];
{
    const no3 = LAYOUTS.find((l) => l.key === "no3#2")!;
    const np3 = LAYOUTS.find((l) => l.key === "np3")!;
    no3.idCheck = np3.idCheck;
    no3.add = np3.add;
}

// (x, y, entity id, flags, slot, params); the compiled record is x, y,
// id | flags, slot, params
type LayoutRec = [number, number, number, number, number, number];

// src/st/<stage>/mod_layout[2].h: g_ModLayout[2]X / Y (LayoutEntity, kind
// "struct", 5 u16 per record). PS1 data (the retail lists) + the mod's
// entities; reads the PS1 stage overlays only.
export async function modLayouts(ps1: DiscRead): Promise<ModTables> {
    const out: ModTables = new Map();
    const BASE = 0x80180000;
    for (const spec of LAYOUTS) {
        const stage = spec.key.split("#")[0];
        const data = await ps1(spec.bin);
        const read = (table: number): LayoutRec[] => {
            let p = le32(data, table + 4 * spec.layout - BASE);
            const recs: LayoutRec[] = [];
            for (;;) {
                const o = p - BASE;
                const r: LayoutRec = [le16(data, o), le16(data, o + 2), le16(data, o + 4), 0,
                    le16(data, o + 6), le16(data, o + 8)];
                recs.push(r);
                p += 10;
                if (r[0] === 0xffff) {
                    return recs;
                }
            }
        };
        let xs = read(spec.tables[0]);
        let ys = read(spec.tables[1]);
        if (spec.replace) {
            xs = xs.filter((r) => r[0] === 0xfffe || r[0] === 0xffff);
            ys = ys.filter((r) => r[1] === 0xfffe || r[1] === 0xffff);
        }
        for (const mv of spec.move ?? []) {
            for (const lst of [xs, ys]) {
                const hits = lst.map((r, i) => [r, i] as [LayoutRec, number])
                    .filter(([r]) => r[0] === mv.x && r[1] === mv.y && r[2] === mv.eid && r[5] === mv.params);
                check(hits.length === 1, stage + ": move matches " + hits.length);
                const [r, i] = hits[0];
                check(r[4] !== mv.slot, stage + ": move to the same slot");
                lst[i] = [r[0], r[1], r[2], r[3], mv.slot, r[5]];
            }
        }
        const ids = new Map(spec.idCheck);
        for (const n of spec.add) {
            const eid = ids.get(n.id);
            check(eid !== undefined, stage + ": " + n.id + " not checked");
            check(!xs.some((r) => r[0] === n.x && r[1] === n.y && r[5] === n.params), stage + " already has the entity");
            const rec: LayoutRec = [n.x, n.y, eid!, n.flags ?? 0, n.slot, n.params];
            let i = xs.findIndex((r) => r[0] !== 0xfffe && (r[0] === 0xffff || r[0] > rec[0]));
            xs.splice(i, 0, rec);
            i = ys.findIndex((r) => r[1] !== 0xfffe && (r[1] === 0xffff || r[1] > rec[1]));
            ys.splice(i, 0, rec);
        }
        const sfx = spec.suffix ?? "";
        const header = (spec.dir ?? "src/st/" + stage) + "/mod_layout" + sfx + ".h";
        const flat = (l: LayoutRec[]): number[] => l.flatMap((r) => [r[0], r[1], (r[2] | r[3]) & 0xffff, r[4], r[5]]);
        put(out, header, "g_ModLayout" + sfx + "X", { size: 2, values: flat(xs), kind: "struct" });
        put(out, header, "g_ModLayout" + sfx + "Y", { size: 2, values: flat(ys), kind: "struct" });
    }
    return out;
}

// --- sprite frames (gen_np3_sconce.py / gen_no3_horse.py) ---------------------

// a frame as s16s: the part count, each part's 11 fields, 0, padded to even
function frameValues(parts: PsxPart[]): number[] {
    const vals = [parts.length];
    for (const p of parts) {
        vals.push(p.flags, p.x, p.y, p.width, p.height, p.clut, p.tileset, p.left, p.top, p.right, p.bottom);
    }
    vals.push(0);
    if (vals.length & 1) {
        vals.push(0);
    }
    return vals;
}

async function saturnStage(saturn: DiscRead, prg: string): Promise<SaturnStage> {
    return new SaturnStage(await saturn(prg + ".PRG"), await saturn(prg + ".CHR"), await saturn("0.BIN"));
}

interface SconceSpec {
    header: string;
    prg: string;
    bank16: [number, number];
    glow: number;
    sclutBase: number;
    glowFlip: number;
}

const SCONCES: SconceSpec[] = [
    { header: "src/st/np3/mod_sconce.h", prg: "STAGE_01", bank16: [0x060f610c, 0x060f60bc], glow: 0x060f8262,
      sclutBase: 0x2f4, glowFlip: 0 },
    // the first visit's Entrance (NO3): NP3's candelabra, same slots
    { header: "src/st/no3/mod_sconce.h", prg: "STAGE_01", bank16: [0x060f610c, 0x060f60bc], glow: 0x060f8262,
      sclutBase: 0x2f4, glowFlip: 0 },
    { header: "src/st/rno3/mod_sconce.h", prg: "RSTAGE01", bank16: [0x060eed94, 0x060eed44], glow: 0x060f0b52,
      sclutBase: 0x2f0, glowFlip: 3 },
];
const SCONCE_FRAMES = [1, 2, 3, 4, 5];
const GLOW_COLOURS = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x8044, 0x8067, 0x808a, 0x80ce, 0x8112, 0];
const SCONCE_QUADRANT: [number, number] = [18, 0];

// src/st/{np3,no3,rno3}/mod_sconce.h: g_ModSconceArt (u8), g_ModSconceFrame1-5
// (s16), g_ModSconcePals (u16), g_ModCandleGlow (s16 [4][8]). Reads the
// Saturn's STAGE_01 / RSTAGE01 .PRG / .CHR and 0.BIN. Also returns the
// numbers inside the non-parsed tables: the art's rows (GFX_ENTRY) and the
// palette count (PAL_BULK_ size = 16 * count).
export async function sconces(saturn: DiscRead): Promise<{ tables: ModTables; rows: Map<string, number>; pals: Map<string, number> }> {
    const tables: ModTables = new Map();
    const rowsOut = new Map<string, number>();
    const palsOut = new Map<string, number>();
    const state = new SpriteState(); // the Python's module globals, one process for both
    for (const s of SCONCES) {
        const st = await saturnStage(saturn, s.prg);
        const glowRecs: [number, number, number][] = [];
        for (let f = 0; f < 4; f++) {
            const o = s.glow - st.base + f * 10;
            glowRecs.push([bes16(st.prg, o), bes16(st.prg, o + 2), bes16(st.prg, o + 4)]);
        }
        const fill = (sheet: PsxSpriteSheet): [PsxPart[][], PsxPart[]] => [
            SCONCE_FRAMES.map((f) => sheet.frame(st, s.bank16[0], s.bank16[1], f)),
            glowRecs.map(([dx, dy, img]) => sheet.imagePart(st, s.bank16[0], img, dx, dy, GLOW_COLOURS, s.glowFlip)),
        ];
        const probe = new PsxSpriteSheet([[0, 0], [0, 0], [0, 0], [0, 0]], s.sclutBase, null, state);
        fill(probe);
        const sheet = new PsxSpriteSheet([SCONCE_QUADRANT], s.sclutBase, null, state);
        sheet.prepack(st, probe.requests);
        const [frames, glow] = fill(sheet);
        check(sheet.usedQuadrants() === 1, "sconce: more than one quadrant");
        check(s.sclutBase + sheet.cluts.length <= 0x300, "sconce: palettes past the stage's range");
        let rows = 0;
        for (const [, , y, , h] of sheet.placed.values()) {
            rows = Math.max(rows, y + h);
        }
        rows = (rows + 1) & ~1;
        const art = sheet.quadrantPixels(0).subarray(0, rows * 64);
        put(tables, s.header, "g_ModSconceArt", { size: 1, values: [...art], kind: "array" });
        frames.forEach((parts, n) => {
            put(tables, s.header, "g_ModSconceFrame" + (n + 1), { size: 2, values: frameValues(parts), kind: "array" });
        });
        put(tables, s.header, "g_ModSconcePals", { size: 2, values: sheet.cluts.flat(), kind: "array" });
        put(tables, s.header, "g_ModCandleGlow", {
            size: 2, kind: "array", values: glow.flatMap((p) => {
                const q = p.tileset & 3;
                return [p.x, p.y, p.width, p.height, p.left + (q & 1) * 128, p.top + (q >> 1) * 128, p.clut, p.tileset >> 2];
            }),
        });
        rowsOut.set(s.header, rows);
        palsOut.set(s.header, sheet.cluts.length);
    }
    return { tables, rows: rowsOut, pals: palsOut };
}

const HORSE_HEADER = "src/st/no3/mod_horse.h";
const HORSE_BANK = 0x060f3dd4;
const HORSE_FRAMES = 0x060f3ccc;
const HORSE_NFRAMES = 30;
const HORSE_PAL = 0x2d9;
const STRIP_ROWS = 32;

// src/st/no3/mod_horse.h: g_ModHorseLz (u8), g_ModHorseStrips (struct, as
// 6 u16 per strip), g_ModHorseFrame1-30 (s16), g_ModHorsePals (u16). Reads
// the Saturn's STAGE_0X .PRG / .CHR and 0.BIN. MOD_HORSE_NSTRIPS = strips
// / 6 (12), MOD_HORSE_NPALS 5.
export async function no3Horse(saturn: DiscRead): Promise<ModTables> {
    const out: ModTables = new Map();
    const st = await saturnStage(saturn, "STAGE_0X");
    const state = new SpriteState();
    state.trimBanks.set(HORSE_BANK, new Set());
    const quads: [number, number][] = [];
    for (const t of [18, 19]) {
        for (let q = 0; q < 4; q++) {
            quads.push([t, q]);
        }
    }
    const fill = (sheet: PsxSpriteSheet): PsxPart[][] => {
        sheet.paletteBlock(st, HORSE_BANK);
        const fr: PsxPart[][] = [];
        for (let f = 1; f <= HORSE_NFRAMES; f++) {
            fr.push(sheet.frame(st, HORSE_BANK, HORSE_FRAMES, f));
        }
        return fr;
    };
    const probe = new PsxSpriteSheet([...quads, ...quads], HORSE_PAL, null, state);
    fill(probe);
    const sheet = new PsxSpriteSheet(quads, HORSE_PAL, null, state);
    sheet.prepack(st, probe.requests);
    const frames = fill(sheet);
    const nq = sheet.usedQuadrants();
    check(sheet.cluts.length === 5 && HORSE_PAL + 5 <= 0x2e0, "horse: palettes");
    const blob: number[] = [];
    const strips: number[] = [];
    for (let q = 0; q < nq; q++) {
        let rows = -Infinity;
        for (const [qq, , y, , h] of sheet.placed.values()) {
            if (qq === q) {
                rows = Math.max(rows, y + h);
            }
        }
        const px = sheet.quadrantPixels(q);
        const [tpage, quad] = sheet.quadrants[q];
        const vx = (tpage & 15) * 64 + (quad & 1) * 32;
        const vy = (tpage >> 4) * 256 + (quad >> 1) * 128;
        for (let r0 = 0; r0 < rows; r0 += STRIP_ROWS) {
            const n = Math.min(STRIP_ROWS, rows - r0);
            const packed = mbLzss(px.subarray(r0 * 64, (r0 + n) * 64));
            strips.push(blob.length & 0xffff, blob.length >>> 16, vx, vy + r0, n, 0);
            blob.push(...packed);
        }
    }
    put(out, HORSE_HEADER, "g_ModHorseLz", { size: 1, values: blob, kind: "array" });
    put(out, HORSE_HEADER, "g_ModHorseStrips", { size: 2, values: strips, kind: "struct" });
    frames.forEach((parts, n) => {
        put(out, HORSE_HEADER, "g_ModHorseFrame" + (n + 1), { size: 2, values: frameValues(parts), kind: "array" });
    });
    put(out, HORSE_HEADER, "g_ModHorsePals", { size: 2, values: sheet.cluts.flat(), kind: "array" });
    return out;
}

// --- gen_saturn_items.py (equipment_mod.h) -----------------------------------

const EQUIP_HEADER = "src/dra/gen/us/equipment_mod.h";
const PS1_EQUIP = 0x4b04; // DRA.BIN: g_EquipDefs, 0x34-byte records
const PS1_ACCESSORIES = 0x7718; // DRA.BIN: g_AccessoryDefs, 0x20-byte records
const NUM_BODY = 90;
const SAT_EQUIP = 0x16250; // GAME.PRG: 0x34-byte records, big endian
const SAT_ACCESSORIES = 0x190a0; // GAME.PRG: 32-byte records
const NUM_HAND = 169;
const NUM_SPECIAL = 48;
const SHIFT = 7;

// Equipment's numeric fields after the two pointers: [name, offset, bytes, signed]
const EQ_FIELDS: [string, number, number, boolean][] = [
    ["attack", 0x08, 2, true], ["defense", 0x0a, 2, true], ["element", 0x0c, 2, false],
    ["itemCategory", 0x0e, 1, false], ["weaponId", 0x0f, 1, false], ["palette", 0x10, 1, false],
    ["unk11", 0x11, 1, false], ["playerAnim", 0x12, 1, false], ["unk13", 0x13, 1, false],
    ["unk14", 0x14, 1, false], ["lockDuration", 0x15, 1, false], ["chainLimit", 0x16, 1, false],
    ["unk17", 0x17, 1, false], ["specialMove", 0x18, 1, false], ["isConsumable", 0x19, 1, false],
    ["invFrames", 0x1a, 1, false], ["unk1B", 0x1b, 1, false], ["comboSub", 0x1c, 4, false],
    ["comboMain", 0x20, 4, false], ["mpUsage", 0x24, 2, false], ["stunFrames", 0x26, 2, false],
    ["hitType", 0x28, 2, false], ["hitEffect", 0x2a, 2, false], ["icon", 0x2c, 2, false],
    ["iconPalette", 0x2e, 2, false], ["criticalRate", 0x30, 2, false],
];
type Rec = Record<string, number>;

function readEquip(d: Uint8Array, o: number, big: boolean): Rec {
    const r: Rec = {};
    for (const [k, off, n, signed] of EQ_FIELDS) {
        const p = o + off;
        r[k] = n === 1 ? d[p] : n === 2 ? (big ? (signed ? bes16(d, p) : be16(d, p)) : (signed ? les16(d, p) : le16(d, p)))
            : (big ? be32(d, p) : le32(d, p));
    }
    return r;
}

// a record's bytes 8-0x33 as compiled (0x32-0x33: the struct's padding)
function equipBytes(r: Rec): number[] {
    const b = new Array<number>(0x34 - 8).fill(0);
    for (const [k, off, n] of EQ_FIELDS) {
        // the header writes isConsumable as true / false
        const v = k === "isConsumable" ? (r[k] ? 1 : 0) : r[k];
        for (let i = 0; i < n; i++) {
            b[off - 8 + i] = (v >>> (8 * i)) & 0xff;
        }
    }
    return b;
}

const saturnId = (ps1: number): number => (ps1 < 70 ? ps1 : ps1 < 167 ? ps1 + 2 : ps1 < NUM_HAND ? ps1 + 3 : ps1 + SHIFT);

// equipment_mod.h -> g_EquipDefs (228 records), as "g_EquipDefs+0xOFF" runs
// of bytes 8-0x33 of each record (kind "run"). Retail records from the PS1
// DRA.BIN, the Saturn's new ones from GAME.PRG (the mod's typed-in icons /
// palettes / food sprites / player animations applied as the script does).
export async function saturnEquipment(saturn: DiscRead, ps1: DiscRead): Promise<ModTables> {
    const dra = await ps1("DRA.BIN");
    const game = await saturn("GAME.PRG");
    const retail: Rec[] = [];
    for (let i = 0; i < NUM_HAND + NUM_SPECIAL; i++) {
        retail.push(readEquip(dra, PS1_EQUIP + i * 0x34, false));
    }
    const sat = (i: number): Rec => readEquip(game, SAT_EQUIP + i * 0x34, true);
    check(sat(4).comboMain === 0x7ffe, "Saturn Shield rod comboMain");
    // Saturn -> PSX translation of the renumbered fields, per category, from
    // the records both tables share (Counter.most_common: the first counted
    // of the most frequent)
    const TRANSLATED = ["weaponId", "unk13", "playerAnim"];
    const votes = new Map<string, Map<number, number>>();
    retail.forEach((r, i) => {
        const s = sat(saturnId(i));
        for (const k of TRANSLATED) {
            const key = r.itemCategory + "," + k + "," + s[k];
            let c = votes.get(key);
            if (c === undefined) {
                c = new Map();
                votes.set(key, c);
            }
            c.set(r[k], (c.get(r[k]) ?? 0) + 1);
        }
    });
    const translate = (cat: number, k: string, v: number): number => {
        const c = votes.get(cat + "," + k + "," + v);
        if (c === undefined) {
            return v;
        }
        let best = v;
        let n = 0;
        for (const [val, cnt] of c) {
            if (cnt > n) {
                best = val;
                n = cnt;
            }
        }
        return best;
    };
    const fromSaturn = (i: number, iconPal: [number, number] | null, keep: string[], set: Rec): Rec => {
        const s = sat(i);
        const r: Rec = { ...s, ...set };
        for (const k of TRANSLATED) {
            if (!keep.includes(k)) {
                r[k] = translate(s.itemCategory, k, s[k]);
            }
        }
        if (iconPal) {
            [r.icon, r.iconPalette] = iconPal;
        }
        return r;
    };
    const W = ["weaponId"];
    // mod data: the new items' icon / palette slots and food sprites
    const newHand = [
        fromSaturn(70, [2, 2], [], { unk14: 42 }),
        fromSaturn(71, [3, 3], [], { unk14: 43 }),
        fromSaturn(169, [156, 141], [], {}),
        fromSaturn(172, [210, 138], W, {}),
        fromSaturn(173, [233, 211], W, {}),
        fromSaturn(174, [212, 140], W, {}),
        fromSaturn(175, [214, 136], W, {}),
    ];
    const newSpecial = [224, 225, 226, 227].map((i) => fromSaturn(i, null, W, {}));
    // mod data: the player animations the script sets by hand
    check(newSpecial[3].playerAnim === 190 && newSpecial[1].playerAnim === 96 && newSpecial[0].playerAnim === 224,
        "Saturn special records' player animations");
    newSpecial[3].playerAnim = 186;
    newSpecial[1].playerAnim = 104;
    newSpecial[0].playerAnim = 247;
    const special = (v: number): number => (NUM_HAND <= v && v < NUM_HAND + NUM_SPECIAL ? v + SHIFT : v);
    const shifted = (r: Rec): Rec => ({ ...r, unk17: special(r.unk17), specialMove: special(r.specialMove) });
    const table = [...retail.slice(0, NUM_HAND).map(shifted), ...newHand,
        ...retail.slice(NUM_HAND).map(shifted), ...newSpecial];
    check(table.length === 228, "equipment count");
    const out: ModTables = new Map();
    table.forEach((r, i) => {
        put(out, EQUIP_HEADER, "g_EquipDefs+0x" + (i * 0x34 + 8).toString(16).toUpperCase().padStart(4, "0"),
            { size: 1, values: equipBytes(r), kind: "run" });
    });
    return out;
}

// --- gen_saturn_enemies.py ----------------------------------------------------

const ENEMY_HEADER = "src/dra/gen/us/enemydefs_mod.h";
const SAT_ENEMY_DEFS = 0x06080288 - 0x06066000;
const SAT_NUM_HAND = 176;
const MOD_DROP_HAND_BASE = NUM_HAND + 93;

// enemydefs_mod.h -> g_EnemyDefs records 400-425, as "g_EnemyDefs+0xOFF"
// runs of bytes 4-0x27 (kind "run"; the name pointer stays the mod's). Reads
// GAME.PRG and the PS1 DRA.BIN (accessory icons, to renumber the drops).
export async function saturnEnemies(saturn: DiscRead, ps1: DiscRead): Promise<ModTables> {
    const game = await saturn("GAME.PRG");
    const dra = await ps1("DRA.BIN");
    const ps1Acc: [number, number, number][] = [];
    for (let i = 0; i < NUM_BODY; i++) {
        const o = PS1_ACCESSORIES + i * 0x20 + 0x18;
        ps1Acc.push([le16(dra, o), le16(dra, o + 2), le16(dra, o + 4)]);
    }
    const bodyItem = (b: number): number => {
        const o = SAT_ACCESSORIES + b * 32 + 0x18;
        const [icon, pal, kind] = [be16(game, o), be16(game, o + 2), be16(game, o + 4)];
        const hits: number[] = [];
        ps1Acc.forEach(([ic, p, k], i) => {
            if (ic === icon && p === pal && k === kind) {
                hits.push(i);
            }
        });
        check(hits.length === 1, "Saturn body item " + b);
        return hits[0];
    };
    const NEW: Record<number, number> = { 70: 0, 71: 1, 169: 2, 172: 3, 173: 4, 174: 5, 175: 6 };
    const handItem = (h: number): number => (h in NEW ? MOD_DROP_HAND_BASE + NEW[h] : h < 70 ? h : h < 169 ? h - 2 : h - 3);
    const drop = (v: number): number => {
        if (v < 0x80) {
            return v;
        }
        const c = v - 0x80;
        return c < SAT_NUM_HAND ? 0x80 + handItem(c) : 0x80 + NUM_HAND + bodyItem(c - SAT_NUM_HAND);
    };
    const out: ModTables = new Map();
    for (let i = 400; i < 426; i++) {
        const o = SAT_ENEMY_DEFS + i * 0x28;
        const hw: number[] = [];
        for (let k = 0; k < 15; k++) {
            hw.push(be16(game, o + 4 + 2 * k));
        }
        hw[11] = drop(hw[11]); // rareItemId
        hw[12] = drop(hw[12]); // uncommonItemId
        const b: number[] = [];
        for (const v of hw) {
            b.push(v & 0xff, (v >> 8) & 0xff);
        }
        b.push(game[o + 0x22], game[o + 0x23]);
        const flags = be32(game, o + 0x24);
        b.push(flags & 0xff, (flags >>> 8) & 0xff, (flags >>> 16) & 0xff, flags >>> 24);
        put(out, ENEMY_HEADER, "g_EnemyDefs+0x" + (i * 0x28 + 4).toString(16).toUpperCase().padStart(4, "0"),
            { size: 1, values: b, kind: "run" });
    }
    return out;
}

// --- gen_saturn_bestiary.py -----------------------------------------------------

const BESTIARY_HEADER = "src/st/lib/mod_bestiary.h";
const LIB_LIST = 0x1825c;
const LIB_COUNT = 164;
const SAT_LOOKUP = 0x060850f4 - 0x06066000;
const PS1_LOOKUP = 0x1ca8; // ST/LIB/LIB.BIN: g_testCollEnemyLookup (US), 400 u16
const RETAIL_ENEMIES = 146;
const GHOST_ID = 0x0b1;

// src/st/lib/mod_bestiary.h: g_ModLibBit (u8) and g_ModLibEnemy (u16), from
// the Saturn's LIBRARY.PRG list (and its GAME.PRG lookup, checked) and the
// PS1 enemy lookup (LIB.BIN). ghost: with the MOD_OUTER_WALL_GHOST tail (off
// in the RC build, but parseCArrays drops #ifdef lines and so reads it).
export async function saturnBestiary(saturn: DiscRead, ps1: DiscRead, ghost: boolean): Promise<ModTables> {
    const lib = await saturn("LIBRARY.PRG");
    const game = await saturn("GAME.PRG");
    const lt = await ps1("ST/LIB/LIB.BIN");
    const sat: number[] = [];
    for (let k = 0; k < LIB_COUNT; k++) {
        sat.push(be16(lib, LIB_LIST + 2 * k));
    }
    check(be16(lib, LIB_LIST - 2) === 0 && lib[LIB_LIST + LIB_COUNT * 2] === 6, "LIBRARY.PRG list bounds");
    const us: number[] = [];
    for (let k = 0; k < 400; k++) {
        us.push(le16(lt, PS1_LOOKUP + 2 * k));
    }
    check(Math.max(...us) === RETAIL_ENEMIES, "PS1 enemy lookup");
    const newIds = sat.filter((x) => x >= 400).sort((a, b) => a - b);
    check(newIds.length === LIB_COUNT - RETAIL_ENEMIES, "Saturn new enemies");
    const bits: number[] = [];
    const enemy: (number | null)[] = new Array(LIB_COUNT).fill(null);
    sat.forEach((x, k) => {
        const b = x < 400 ? us[x] - 1 : RETAIL_ENEMIES + newIds.indexOf(x);
        check(b >= 0, "enemy 0x" + x.toString(16));
        check(be16(game, SAT_LOOKUP + 2 * x) === k + 1, "Saturn lookup 0x" + x.toString(16));
        check(enemy[b] === null, "bit " + b + " twice");
        bits.push(b);
        enemy[b] = x;
    });
    check(!enemy.includes(null), "bits unused");
    const en = enemy as number[];
    if (ghost) {
        bits.push(LIB_COUNT, LIB_COUNT + 1);
        en.push(GHOST_ID, 0xffff);
    }
    const out: ModTables = new Map();
    put(out, BESTIARY_HEADER, "g_ModLibBit", { size: 1, values: bits, kind: "array" });
    put(out, BESTIARY_HEADER, "g_ModLibEnemy", { size: 2, values: en, kind: "array" });
    return out;
}

// --- gen_mar2_items.py ----------------------------------------------------------

// The PSP DRA item sprite bank D_800D0F68's pointer table and dra.bin's load
// address: the EU UMD's (the script's) and the US UMD's, whose frames
// 0xA1-0xC0 are the same bytes
const MAR2_DRA: Record<string, [number, number]> = { us: [0x6a628, 0x090ca700] };

// mods/sotn-ultimate/src/maria_psp/mar2_items.h: g_Mar2ItemFrames (s16 [32][24]) from the US
// UMD's PSPBIN/DRA.BIN (region "eu": the EU UMD's dra.bin, as the script)
export async function mar2Items(psp: DiscRead, region: "us" = "us"): Promise<ModTables> {
    const d = await psp("PSPBIN/DRA.BIN");
    const [table, vram] = MAR2_DRA[region];
    check(le32(d, table) === 0, "PSP item sprite table");
    for (let n = 1; n < 193; n++) {
        check(le32(d, table + 4 * n) !== 0, "PSP item sprite table");
    }
    const vals: number[] = [];
    for (let n = 0xa1; n < 0xc1; n++) {
        const o = le32(d, table + 4 * n) - vram;
        check(les16(d, o) === 2, "PSP item frame 0x" + n.toString(16));
        for (let k = 0; k < 23; k++) {
            vals.push(les16(d, o + 2 * k));
        }
        vals.push(0);
    }
    const out: ModTables = new Map();
    put(out, "mods/sotn-ultimate/src/maria_psp/mar2_items.h", "g_Mar2ItemFrames", { size: 2, values: vals, kind: "array" });
    return out;
}

// --- everything -----------------------------------------------------------------

export interface ModReaders {
    saturn: DiscRead; // Saturn disc paths ("GAME.PRG")
    ps1: DiscRead; // PS1 disc paths ("ST/NP3/NP3.BIN", "DRA.BIN")
    psp: DiscRead; // US UMD paths under PSP_GAME/USRDIR/res/ps/ ("PSPBIN/DRA.BIN")
}

export async function modTables(rd: ModReaders, ghost: boolean = false): Promise<ModTables> {
    return mergeTables(
        await modLayouts(rd.ps1),
        (await sconces(rd.saturn)).tables,
        await no3Horse(rd.saturn),
        await saturnEquipment(rd.saturn, rd.ps1),
        await saturnEnemies(rd.saturn, rd.ps1),
        await saturnBestiary(rd.saturn, rd.ps1, ghost),
        await mar2Items(rd.psp),
    );
}

// the compiled bytes of a ModArray (little endian)
export function modArrayBytes(a: ModArray): Uint8Array {
    const out = new Uint8Array(a.values.length * a.size);
    const v = new DataView(out.buffer);
    a.values.forEach((x, i) => {
        if (a.size === 1) {
            v.setUint8(i, x & 0xff);
        } else if (a.size === 2) {
            v.setUint16(i * 2, x & 0xffff, true);
        } else {
            v.setUint32(i * 4, x >>> 0, true);
        }
    });
    return out;
}

