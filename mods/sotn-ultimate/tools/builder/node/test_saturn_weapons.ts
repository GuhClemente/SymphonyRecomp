// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_saturn_weapons.ts
// src/saturn_weapons.ts (the ports of port_saturn_weapon.py and
// port_saturn_food.py) against what the build left: the headers
// mods/sotn-ultimate/src/items/weapon/mod/w_060..063_data.h (whole text, and every array parseCArrays
// finds in them), the art pages build/us/mod/weapons/f_0NN.bin, the ranges
// in build/us/WEAPON2.BIN, and the food boxes in build/us/WEAPON0.BIN /
// WEAPON1.BIN; the borrowed front-frame sets against assets/weapon/*.json.
// Inputs straight from disks/saturn.
import * as fs from "node:fs";
import * as path from "node:path";
import { buildSaturnWeapons, WEAPONS } from "../src/saturn_weapons.ts";
import { parseCArrays } from "./c_arrays.ts";
import { ROOT } from "./util.ts";

const saturn = async (p: string): Promise<Uint8Array> => new Uint8Array(fs.readFileSync(path.join(ROOT, "disks/saturn", p)));
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
    report(what, diff === 0, diff ? diff + " bytes differ, first at 0x" + first.toString(16) +
        " (sizes " + mine.length + " " + ref.length + ")" : "(" + mine.length + " bytes)");
}

function cmpText(what: string, mine: string, ref: string): void {
    if (mine === ref) {
        report(what, true, "(" + mine.length + " chars)");
        return;
    }
    const a = mine.split("\n");
    const b = ref.split("\n");
    const i = a.findIndex((l, k) => l !== b[k]);
    report(what, false, "first difference at line " + (i + 1) + ":\n  mine: " + a[i] + "\n  ref:  " + b[i]);
}

for (const [wid, cfg] of WEAPONS) {
    const ref = JSON.parse(fs.readFileSync(path.join(ROOT, "assets/weapon/" + cfg.frontRef + ".animset.json"), "utf8"));
    const front: number[] = [];
    ref.forEach((f: { flags: number }[] | null, i: number) => {
        if (f && f.some((p) => p.flags & 0x100)) {
            front.push(i);
        }
    });
    report("w_0" + wid + " front frames == " + cfg.frontRef, JSON.stringify(front) === JSON.stringify(cfg.front));
}

const t0 = Date.now();
const r = await buildSaturnWeapons(saturn);
console.log("built in " + (Date.now() - t0) + " ms");

for (const [hdr, text] of r.headers) {
    cmpText(hdr + " (text)", text, fs.readFileSync(path.join(ROOT, hdr), "utf8"));
    const want = parseCArrays(path.join(ROOT, hdr));
    const mine = r.arrays.get(hdr)!;
    let diff = 0;
    const msgs: string[] = [];
    for (const a of want) {
        const m = mine.get(a.name);
        if (!m || m.size !== a.size || JSON.stringify(m.values) !== JSON.stringify(a.values)) {
            diff++;
            msgs.push(a.name);
        }
    }
    const extra = [...mine.keys()].filter((n) => !want.some((a) => a.name === n));
    report(hdr + " arrays (" + want.length + " in the header, " + mine.size + " made, " +
        want.reduce((s, a) => s + a.values.length * a.size, 0) + " bytes)", diff === 0 && extra.length === 0,
        diff || extra.length ? "differ: " + msgs.slice(0, 5).join(" ") + " extra: " + extra.slice(0, 5).join(" ") : "");
    // the non-numeric definitions
    const defs = text.split("\n").filter((l) => l.startsWith("#define"));
    if (defs.length) {
        console.log("     defines: " + defs.map((l) => l.slice(8)).join("; "));
    }
}

for (const [wid, art] of r.art) {
    cmpBytes("build/us/mod/weapons/f_0" + wid + ".bin", art, new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us/mod/weapons/f_0" + wid + ".bin"))));
}

for (const [file, ranges] of r.ranges) {
    const ref = new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us", path.basename(file))));
    let diff = 0;
    let first = "";
    for (const g of ranges) {
        for (let i = 0; i < g.bytes.length; i++) {
            if (g.bytes[i] !== ref[g.offset + i]) {
                diff++;
                if (!first) {
                    first = " first at 0x" + (g.offset + i).toString(16);
                }
            }
        }
    }
    report("build/us/" + path.basename(file) + " ranges (" + ranges.length + " x " + ranges[0].bytes.length + " bytes at " +
        ranges.slice(0, 3).map((g) => "0x" + g.offset.toString(16)).join(" ") + (ranges.length > 3 ? " ..." : "") + ")",
        diff === 0, diff ? diff + " bytes differ" + first : "");
}

// the food boxes are the only Saturn bytes the mod puts in slot 21's art:
// the rest of build/us/WEAPON0.BIN's page is retail
{
    const built = new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us/WEAPON0.BIN")));
    const retail = new Uint8Array(fs.readFileSync(path.join(ROOT, "disks/us/BIN/WEAPON0.BIN")));
    const inBox = new Set<number>();
    for (const g of r.ranges.get("BIN/WEAPON0.BIN")!) {
        for (let i = 0; i < g.bytes.length; i++) {
            inBox.add(g.offset + i);
        }
    }
    let other = 0;
    for (let i = 21 * 0x7000; i < 21 * 0x7000 + 0x4000; i++) {
        if (built[i] !== retail[i] && !inBox.has(i)) {
            other++;
        }
    }
    report("WEAPON0.BIN slot 21 art differs from retail only in the food boxes", other === 0, other ? other + " other bytes" : "");
}

console.log(bad ? bad + " FAILED" : "all ok");
process.exit(bad ? 1 : 0);
