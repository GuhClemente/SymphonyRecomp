// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_sel_gfx.ts
// The builder's ports of gen_title_portraits.py, gen_sel_soundmode.py,
// gen_mar_hud.py, port_saturn_icons.py and paint_fmap.py against what the
// generators left in the repo: the headers (whole text, and every array
// parseCArrays finds in them), build/us/F_GAME2.BIN, build/us/F_MAP.BIN and
// the icon slots in build/us/DRA.BIN, byte for byte. Inputs straight from
// disks/saturn and disks/us.
import * as fs from "node:fs";
import * as path from "node:path";
import { parseCArrays } from "./c_arrays.ts";
import type { CArray } from "./c_arrays.ts";
import { ROOT } from "./util.ts";
import { buildTitlePortraits } from "../src/title_portraits.ts";
import { buildSelSoundMode } from "../src/sel_soundmode.ts";
import { buildMarHud } from "../src/mar_hud.ts";
import { buildSaturnIcons, DRA_GFX_EQUIP_ICON, DRA_PAL_EQUIP_ICON, ICON_BYTES, PAL_BYTES } from "../src/saturn_icons.ts";
import { buildFmap } from "../src/fmap.ts";

const rd = (d: string) => async (p: string): Promise<Uint8Array> => new Uint8Array(fs.readFileSync(path.join(ROOT, d, p)));
const saturn = rd("disks/saturn");
const ps1 = rd("disks/us");
let bad = 0;

function report(what: string, ok: boolean, detail = ""): void {
    console.log((ok ? "ok   " : "FAIL ") + what + (detail ? " " + detail : ""));
    if (!ok) {
        bad++;
    }
}

function cmpBytes(what: string, mine: Uint8Array, ref: Uint8Array): void {
    let diff = 0;
    let first = -1;
    for (let i = 0; i < Math.max(mine.length, ref.length); i++) {
        if (mine[i] !== ref[i]) {
            diff++;
            if (first < 0) {
                first = i;
            }
        }
    }
    report(what, diff === 0 && mine.length === ref.length,
        mine.length + " bytes" + (diff ? ", " + diff + " differ, first at 0x" + first.toString(16) : ""));
}

function cmpText(what: string, mine: string, ref: string): void {
    const a = mine.split("\n");
    const b = ref.split("\n");
    const i = a.findIndex((l, k) => l !== b[k]);
    const ok = mine === ref;
    report(what + " text", ok, ok ? "" : "first different line " + (i < 0 ? a.length : i + 1));
}

// the arrays parseCArrays finds in a header, against mine by name
function cmpArrays(what: string, file: string, mine: { name: string; size: number; values: number[] }[]): void {
    const ref: CArray[] = parseCArrays(file);
    const byName = new Map(mine.map((a) => [a.name, a]));
    for (const r of ref) {
        const m = byName.get(r.name);
        if (!m) {
            report(what + " " + r.name, false, "not generated");
            continue;
        }
        const i = r.values.findIndex((v, k) => v !== m.values[k]);
        const ok = m.size === r.size && m.values.length === r.values.length && i < 0;
        report(what + " " + r.name, ok, r.values.length + " x u" + r.size * 8 + (ok ? "" : ", mine " + m.values.length + " x u" + m.size * 8 + ", first difference at " + i));
    }
    const parsed = new Set(ref.map((r) => r.name));
    const missing = mine.filter((m) => !parsed.has(m.name)).map((m) => m.name);
    if (missing.length) {
        console.log("     (not found by parseCArrays: " + missing.join(", ") + ")");
    }
}

const charselH = path.join(ROOT, "src/st/sel/mod_charsel_gfx.h");
const cs = await buildTitlePortraits(saturn);
cmpText("mod_charsel_gfx.h", cs.header, fs.readFileSync(charselH, "utf8"));
cmpArrays("charsel", charselH, cs.arrays);

const soundH = path.join(ROOT, "src/st/sel/mod_sel_soundmode_gfx.h");
const sm = await buildSelSoundMode(ps1);
cmpText("mod_sel_soundmode_gfx.h", sm.header, fs.readFileSync(soundH, "utf8"));
cmpArrays("soundmode", soundH, [sm]);

cmpBytes("F_GAME2.BIN", await buildMarHud(saturn, ps1), new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us/F_GAME2.BIN"))));
cmpBytes("F_MAP.BIN", await buildFmap(saturn, ps1), new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us/F_MAP.BIN"))));

const dra = new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us/DRA.BIN")));
const icons = await buildSaturnIcons(saturn);
for (const { slot, bytes } of icons.icons) {
    const o = DRA_GFX_EQUIP_ICON + slot * ICON_BYTES;
    cmpBytes("DRA icon " + slot, bytes, dra.subarray(o, o + ICON_BYTES));
}
for (const { slot, bytes } of icons.palettes) {
    const o = DRA_PAL_EQUIP_ICON + slot * PAL_BYTES;
    cmpBytes("DRA icon palette " + slot, bytes, dra.subarray(o, o + PAL_BYTES));
}
console.log(bad ? bad + " FAILED" : "all exact");
process.exit(bad ? 1 : 0);
