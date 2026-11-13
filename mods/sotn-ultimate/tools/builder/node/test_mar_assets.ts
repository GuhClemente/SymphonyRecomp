// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_mar_assets.ts [--locate]
// src/mar_assets.ts against gen_mar_assets.py's headers in build/us/mod/mar
// (Saturn files from disks/saturn, RIC.BIN from disks/us): marAssets per
// header against parseCArrays exactly, and marAssetsExtra against the other
// game-data arrays read here (AnimationFrame / FrameProperty tables,
// spritebank0_N with its `0xJ | 0x8000`, the 3D mar_sp_group_pal).
// --locate: every array's bytes at its symbol in build/us/MARIA.BIN
// (link map build/us/mod/mar/mar.map, loaded at 0x8013C000).
import * as fs from "node:fs";
import * as path from "node:path";
import { marAssetsAll } from "../src/mar_assets.ts";
import type { MarArray, MarHeaders } from "../src/mar_assets.ts";
import { arrayBytes, parseCArrays, symbolAddresses } from "./c_arrays.ts";
import type { CArray } from "./c_arrays.ts";
import { ROOT } from "./util.ts";

const HEADERS = ["mar_sprites.h", "mar_spritebank0.h", "mar_anims.h", "mar_fx.h", "mar_sp.h", "mar_pal.h"]
    .map((h) => "build/us/mod/mar/" + h);
const EXTRA_SIZE: Record<string, number> = { AnimationFrame: 2, FrameProperty: 1, s16: 2, u16: 2 };

// the arrays parseCArrays leaves out that carry numbers (see the top)
function extraArrays(file: string, numeric: Set<string>): CArray[] {
    const src = fs.readFileSync(file, "utf8");
    const out: CArray[] = [];
    const re = /^(?:static )?(AnimationFrame|FrameProperty|s16|u16) (\w+)\[[^=]*= \{(.*?)\};$/gms;
    for (const m of src.matchAll(re)) {
        if (numeric.has(m[2])) {
            continue;
        }
        const values = m[3].replace(/[{}]/g, ",").split(",").map((s) => s.trim()).filter((s) => s)
            .map((t) => t.split("|").reduce((a, x) => a | Number(x.trim()), 0));
        if (values.some((v) => Number.isNaN(v))) {
            throw new Error(m[2] + ": not numbers");
        }
        out.push({ name: m[2], size: EXTRA_SIZE[m[1]], values });
    }
    return out;
}

function compare(label: string, ref: CArray[], mine: Map<string, MarArray>): [number, number] {
    let same = 0;
    let first = "";
    const bad: string[] = [];
    for (const r of ref) {
        const m = mine.get(r.name);
        let why = "";
        if (m === undefined) {
            why = "missing";
        } else if (m.size !== r.size) {
            why = "size " + m.size + " vs " + r.size;
        } else if (m.values.length !== r.values.length) {
            why = "length " + m.values.length + " vs " + r.values.length;
        } else {
            const i = m.values.findIndex((v, k) => v !== r.values[k]);
            if (i >= 0) {
                why = "value " + i + ": " + m.values[i] + " vs " + r.values[i];
            }
        }
        if (why) {
            bad.push(r.name);
            first ||= r.name + ": " + why;
        } else {
            same++;
        }
    }
    const names = new Set(ref.map((r) => r.name));
    const extra = [...mine.keys()].filter((n) => !names.has(n));
    console.log("  %s: %d / %d identical, %d differ%s%s", label, same, ref.length, bad.length,
                bad.length ? " (" + bad.slice(0, 8).join(" ") + (bad.length > 8 ? " ..." : "") + "); first: " + first : "",
                extra.length ? "; not in the header: " + extra.slice(0, 8).join(" ") : "");
    return [same, bad.length + extra.length];
}

const rdSat = async (p: string): Promise<Uint8Array> => new Uint8Array(fs.readFileSync(path.join(ROOT, "disks/saturn", p)));
const rdPs1 = async (p: string): Promise<Uint8Array> => new Uint8Array(fs.readFileSync(path.join(ROOT, "disks/us", p)));
const t0 = performance.now();
const [mine, mineExtra]: [MarHeaders, MarHeaders] = await marAssetsAll(rdSat, rdPs1);
console.log("marAssets: %s s", ((performance.now() - t0) / 1000).toFixed(1));
let fails = 0;
const all: CArray[] = [];
for (const h of HEADERS) {
    const file = path.join(ROOT, h);
    const ref = parseCArrays(file);
    const refExtra = extraArrays(file, new Set(ref.map((r) => r.name)));
    console.log("%s: %d arrays (parseCArrays), %d others", h, ref.length, refExtra.length);
    fails += compare("numeric", ref, mine.get(h) ?? new Map())[1];
    fails += compare("extra", refExtra, mineExtra.get(h) ?? new Map())[1];
    all.push(...ref, ...refExtra);
}
const unknown = [...mine.keys(), ...mineExtra.keys()].filter((h) => !HEADERS.includes(h));
if (unknown.length) {
    console.log("headers not compared: " + unknown.join(" "));
    fails++;
}

if (process.argv.includes("--locate")) {
    const BASE = 0x8013c000;
    const bin = new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us/MARIA.BIN")));
    const syms = symbolAddresses(path.join(ROOT, "build/us/mod/mar/mar.map"), ROOT);
    const missing: string[] = [];
    let bytes = 0;
    for (const a of all) {
        const want = arrayBytes(a);
        const at = (syms.get(a.name) ?? []).map((x) => x - BASE).find((o) =>
            o >= 0 && o + want.length <= bin.length && want.every((b, i) => bin[o + i] === b));
        if (at === undefined) {
            missing.push(a.name + " (" + want.length + " bytes)");
        } else {
            bytes += want.length;
        }
    }
    console.log("locate in build/us/MARIA.BIN: %d / %d arrays found (%d bytes)%s", all.length - missing.length,
                all.length, bytes, missing.length ? "; not: " + missing.slice(0, 20).join(" ") : "");
    fails += missing.length;
}
process.exitCode = fails ? 1 : 0;
