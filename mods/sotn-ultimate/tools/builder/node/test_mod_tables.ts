// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_mod_tables.ts US_PSP
// src/mod_tables.ts against the current generated headers (parseCArrays, for
// the plain arrays) and the current build outputs (every array's bytes at its
// symbol in build/us/*.map, or, for a static symbol / an overlay without a
// map, found in the binary). US_PSP: the US UMD (the .iso the builder page
// takes) or an extracted copy of its PSP_GAME/USRDIR/res/ps folder
// (disks/psp).
import * as fs from "node:fs";
import * as path from "node:path";
import { IsoFs } from "../src/disc.ts";
import { parseCArrays, arrayBytes } from "./c_arrays.ts";
import { fileSource, ROOT } from "./util.ts";
import { mar2Items, modArrayBytes, modTables } from "../src/mod_tables.ts";
import type { ModArray } from "../src/mod_tables.ts";

const US_PSP = process.argv[2];
if (!US_PSP) {
    console.error("usage: test_mod_tables.ts US_PSP (the US UMD .iso, or its extracted res/ps folder)");
    process.exit(2);
}

// the UMD's res/ps files, as the builder page reads them
async function pspReader(src: string): Promise<(p: string) => Promise<Uint8Array>> {
    if (fs.statSync(src).isDirectory()) {
        return reader(src);
    }
    const iso = await IsoFs.open(fileSource(src));
    return (p: string) => iso.readFile("PSP_GAME/USRDIR/res/ps/" + p);
}

function reader(dir: string): (p: string) => Promise<Uint8Array> {
    return async (p: string): Promise<Uint8Array> => {
        for (const c of [p, p.toLowerCase(), path.dirname(p) + "/" + path.basename(p).toLowerCase()]) {
            const f = path.join(dir, c);
            if (fs.existsSync(f)) {
                return new Uint8Array(fs.readFileSync(f));
            }
        }
        throw new Error("no " + path.join(dir, p));
    };
}

// header -> [binary, map or null, load address]
function output(header: string): [string, string | null, number] {
    let m = /^src\/st\/(\w+)\//.exec(header);
    if (m) {
        return ["build/us/" + m[1].toUpperCase() + ".BIN", "build/us/st" + m[1] + ".map", 0x80180000];
    }
    m = /^src\/boss\/(\w+)\//.exec(header);
    if (m) {
        return ["build/us/" + m[1].toUpperCase() + ".BIN", "build/us/bo" + m[1] + ".map", 0x80180000];
    }
    if (header.startsWith("src/dra/")) {
        return ["build/us/DRA.BIN", "build/us/dra.map", 0x800a0000];
    }
    return ["build/us/MARIA2.BIN", null, 0];
}

const mapCache = new Map<string, string>();
function symbol(map: string | null, name: string): number | null {
    if (map === null) {
        return null;
    }
    let text = mapCache.get(map);
    if (text === undefined) {
        text = fs.readFileSync(path.join(ROOT, map), "utf8");
        mapCache.set(map, text);
    }
    const m = new RegExp("^\\s+0x([0-9a-f]+)\\s+" + name + "$", "m").exec(text);
    return m ? parseInt(m[1], 16) : null;
}

// a mod_layout.h's LayoutEntity arrays as their u16s (the entity enum names
// resolved by the header's own ModEntityIdCheck lines)
function layoutArrays(file: string): Map<string, number[]> {
    const out = new Map<string, number[]>();
    if (!/mod_layout\d?\.h$/.test(file)) {
        return out;
    }
    const src = fs.readFileSync(file, "utf8");
    const ids = new Map([...src.matchAll(/\[(\w+) == (0x[0-9A-F]+) \? 1 : -1\]/g)].map((m) => [m[1], parseInt(m[2], 16)]));
    for (const m of src.matchAll(/const LayoutEntity (\w+)\[\] = \{([\s\S]*?)\n\};/g)) {
        const vals: number[] = [];
        for (const r of m[2].matchAll(/\{([^}]*)\}/g)) {
            for (const f of r[1].split(",")) {
                vals.push(f.split("|").map((t) => t.trim()).reduce((acc, t) => acc | (ids.get(t) ?? Number(t)), 0));
            }
        }
        out.set(m[1], vals);
    }
    return out;
}

function firstDiff(a: Uint8Array, b: Uint8Array): number {
    const n = Math.min(a.length, b.length);
    for (let i = 0; i < n; i++) {
        if (a[i] !== b[i]) {
            return i;
        }
    }
    return a.length === b.length ? -1 : n;
}

async function main(): Promise<void> {
    const rd = {
        saturn: reader(path.join(ROOT, "disks/saturn")),
        ps1: reader(path.join(ROOT, "disks/us")),
        psp: await pspReader(US_PSP),
    };
    let bad = 0;
    // headers: with the ghost tail (parseCArrays reads the #ifdef lines)
    const ghostOn = await modTables(rd, true);
    const ghostOff = await modTables(rd, false);
    for (const [header, arrays] of ghostOn) {
        const parsed = new Map(parseCArrays(path.join(ROOT, header)).map((a) => [a.name, a]));
        const [binName, map, base] = output(header);
        const bin = new Uint8Array(fs.readFileSync(path.join(ROOT, binName)));
        let nHeader = 0;
        let nBin = 0;
        let notBuilt = 0;
        const errs: string[] = [];
        const layouts = layoutArrays(path.join(ROOT, header));
        for (const [name, a] of arrays) {
            const lay = layouts.get(name);
            if (lay) {
                const d = firstDiff(modArrayBytes(a), modArrayBytes({ size: 2, values: lay, kind: "struct" }));
                if (d < 0) {
                    nHeader++;
                } else {
                    errs.push(name + ": header differs at byte " + d);
                }
            }
            if (a.kind === "array") {
                const p = parsed.get(name);
                if (!p) {
                    errs.push(name + ": not in the header");
                } else {
                    const d = firstDiff(modArrayBytes(a), arrayBytes(p));
                    if (d < 0 && a.values.length === p.values.length) {
                        nHeader++;
                    } else {
                        errs.push(name + ": header differs at byte " + d + " (" + a.values.length + " vs " + p.values.length + " values)");
                    }
                }
            }
            // the build: the RC's arrays (no ghost tail)
            const built: ModArray = ghostOff.get(header)!.get(name)!;
            const bytes = modArrayBytes(built);
            const [sym, off] = name.includes("+") ? [name.split("+")[0], parseInt(name.split("+")[1], 16)] : [name, 0];
            const addr = symbol(map, sym);
            if (addr !== null) {
                const o = addr - base + off;
                const d = firstDiff(bytes, bin.subarray(o, o + bytes.length));
                if (d < 0) {
                    nBin++;
                } else {
                    errs.push(name + ": " + binName + " differs at byte " + d + " (file 0x" + (o + d).toString(16) + ")");
                }
            } else if (Buffer.from(bin).indexOf(Buffer.from(bytes)) >= 0) {
                nBin++;
            } else if (header === "src/st/no1/mod_layout.h") {
                // compiled only with MOD_OUTER_WALL_GHOST (off in the RC)
                notBuilt++;
            } else {
                errs.push(name + ": not found in " + binName);
            }
        }
        const nArr = [...arrays.values()].filter((a) => a.kind === "array").length + layouts.size;
        const skipped = [...parsed.keys()].filter((n) => !arrays.has(n));
        console.log(header + ": " + arrays.size + " arrays; header " + nHeader + "/" + nArr + ", " + binName + " " +
            nBin + "/" + arrays.size + (notBuilt ? " (" + notBuilt + " not built: MOD_OUTER_WALL_GHOST off)" : "") +
            (skipped.length ? "; parsed, not ported: " + skipped.join(" ") : ""));
        for (const e of errs.slice(0, 5)) {
            console.log("  FAIL " + e);
        }
        bad += errs.length;
    }
    console.log(bad ? "FAIL: " + bad : "OK");
    process.exitCode = bad ? 1 : 0;
}

await main();
