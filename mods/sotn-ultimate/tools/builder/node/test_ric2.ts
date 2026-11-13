// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_ric2.ts
// src/ric2.ts (the ports of build_ric2.py, build_bo6ric2.py,
// build_topric2.py and build_selric2.py) against what the build left:
// build/us/RIC2.BIN, BO6RIC2.BIN, SELRIC2.BIN and g_ModTopRic2 in TOP.BIN,
// whole files byte for byte, and each range against the file. Inputs:
// disks/saturn, disks/us, the built RIC/BO6/TOP/SEL (TOP's g_ModTopRic2
// blanked first, as on the patched disc) and the link maps.
import * as fs from "node:fs";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import { buildRic2, RIC2_PATHS } from "../src/ric2.ts";
import type { Ric2Syms } from "../src/ric2.ts";
import { parseCArrays } from "./c_arrays.ts";
import { ROOT } from "./util.ts";

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
    report(what, diff === 0, diff ? diff + " bytes differ, first at 0x" + first.toString(16) +
        " (sizes " + mine.length + " " + ref.length + ")" : "(" + mine.length + " bytes)");
}

// the symbols as the scripts read them (ric_syms: the last definition; B.syms: the first)
const SYMS = `
import json, os, re, sys
sys.path.insert(0, "mods/sotn-ultimate/tools")
import build_bo6ric2 as B
ric = {}
for line in open("build/us/ric.map"):
    f = line.split()
    if len(f) == 2 and f[0].startswith("0x") and f[1].isidentifier():
        try:
            ric[f[1]] = int(f[0], 16)
        except ValueError:
            pass
bo6, top, sel = B.syms("build/us/bobo6.map"), B.syms("build/us/sttop.map"), B.syms("build/us/stsel.map")
pick = lambda d, ks: {k: d[k] for k in ks}
print(json.dumps({
    "ric": pick(ric, ["richter_sprites", "spritebank0", "spritebank1"]),
    "rics": pick(B.syms("build/us/ric.map"), ["richter_sprites", "spritebank0", "spritebank1"]),
    "bo6": pick(bo6, ["richter_sprites", "D_us_801A39B0", "D_us_801A43A4", "D_us_8019E9A0"]),
    "top": pick(top, ["richter_sprites", "g_ModTopRic2"]),
    "sel": pick(sel, ["D_8018C718", "D_8018C92C", "D_8018CB4C", "D_8018D5D4"]),
}))
`;

const all = JSON.parse(execFileSync(path.join(ROOT, ".venv/bin/python"), ["-c", SYMS], { cwd: ROOT, encoding: "utf8" }));
// (the BO6 / TOP / SEL scripts read ric.map with B.syms; same values)
report("ric.map symbols agree between the two readers", JSON.stringify(all.ric) === JSON.stringify(all.rics));
const syms: Ric2Syms = { ric: all.ric, bo6: all.bo6, top: all.top, sel: all.sel };

const BUILD: Record<string, string> = {
    [RIC2_PATHS.ric]: "RIC.BIN", [RIC2_PATHS.bo6]: "BO6.BIN", [RIC2_PATHS.top]: "TOP.BIN", [RIC2_PATHS.sel]: "SEL.BIN",
    [RIC2_PATHS.ric2]: "RIC2.BIN", [RIC2_PATHS.bo6ric2]: "BO6RIC2.BIN", [RIC2_PATHS.selric2]: "SELRIC2.BIN",
};
const topAt = syms.top.g_ModTopRic2 - 0x80180000;
const builtTop = new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us/TOP.BIN")));
const built = async (p: string): Promise<Uint8Array> => {
    const d = new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us", BUILD[p])));
    if (p === RIC2_PATHS.top) {
        d.fill(0, topAt, topAt + 231 * 4); // blank on the patched disc
    }
    return d;
};

const t0 = Date.now();
const r = await buildRic2(saturn, ps1, built, syms);
console.log("built in " + (Date.now() - t0) + " ms");

for (const [p, data] of r.files) {
    const ref = p === RIC2_PATHS.top ? builtTop : new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us", BUILD[p])));
    cmpBytes(BUILD[p] + " (whole file)", data, ref);
    // the ranges alone over the input file give the output
    const base = p === RIC2_PATHS.ric2 ? await built(RIC2_PATHS.ric) : p === RIC2_PATHS.bo6ric2 ? await built(RIC2_PATHS.bo6)
        : p === RIC2_PATHS.selric2 ? await built(RIC2_PATHS.sel) : await built(RIC2_PATHS.top);
    const ranges = r.ranges.get(p)!;
    for (const rg of ranges) {
        base.set(rg.bytes, rg.offset);
    }
    cmpBytes(BUILD[p] + " (input + " + ranges.length + " ranges, " + ranges.reduce((a, g) => a + g.bytes.length, 0) +
        " bytes: " + ranges.map((g) => "0x" + g.offset.toString(16) + "+" + g.bytes.length).join(" ") + ")", base, ref);
}

// g_ModTopRic2 as an array (its header declares the zeros; the words are the range's)
const words = r.arrays.get("mods/sotn-ultimate/src/richter_saturn/mod_ric2.h")!.get("g_ModTopRic2")!;
const ref = parseCArrays(path.join(ROOT, "mods/sotn-ultimate/src/richter_saturn/mod_ric2.h")).find((a) => a.name === "g_ModTopRic2");
report("g_ModTopRic2 in mod_ric2.h (" + (ref ? ref.values.length + " value(s), size " + ref.size : "not found") + ")",
    ref !== undefined && ref.size === words.size);
const tw: number[] = [];
for (let i = 0; i < 231; i++) {
    tw.push((builtTop[topAt + 4 * i] | (builtTop[topAt + 4 * i + 1] << 8) | (builtTop[topAt + 4 * i + 2] << 16) |
        (builtTop[topAt + 4 * i + 3] << 24)) >>> 0);
}
report("g_ModTopRic2 words == build/us/TOP.BIN", JSON.stringify(tw) === JSON.stringify(words.values));

console.log(bad ? bad + " FAILED" : "all ok");
process.exit(bad ? 1 : 0);
