// SPDX-License-Identifier: AGPL-3.0-or-later
// Generated C data on the finished disc: the arrays a generator wrote (the
// asset tool's gen/*.h, the mod's generated mod_*.h) are compiled into an
// overlay as their numbers back to back at the symbol's address. The release
// finds each array's bytes in the overlay through the link map (an object's
// section address) and the object's symbol table (the symbol's offset in that
// section, static symbols included), checks they are the array's numbers, and
// blanks them; the builder writes the numbers it regenerates there.
import * as fs from "node:fs";
import * as path from "node:path";
import { execFileSync } from "node:child_process";

export interface CArray {
    name: string;
    size: number; // bytes per element
    values: number[];
}

const ELEM: Record<string, number> = {
    u8: 1, s8: 1, char: 1, "unsigned char": 1, "signed char": 1, u16: 2, s16: 2, short: 2,
    "unsigned short": 2, u32: 4, s32: 4, int: 4, u_long: 4, "unsigned int": 4,
};

// the arrays of plain numbers defined in a C file (#include "x" inside a
// definition is read from the including file's folder or extra include dirs);
// arrays holding anything else (pointers, macros, structs) are left out
export function parseCArrays(file: string, includeDirs: string[] = []): CArray[] {
    const resolve = (inc: string, from: string): string => {
        for (const d of [path.dirname(from), ...includeDirs]) {
            const p = path.join(d, inc);
            if (fs.existsSync(p)) {
                return p;
            }
        }
        throw new Error(from + ": include " + inc + " not found");
    };
    const expand = (f: string): string => fs.readFileSync(f, "utf8").replace(
        /^\s*#include\s+"([^"]+)"\s*$/gm, (_m, inc: string) => expand(resolve(inc, f)));
    const src = expand(file).replace(/\/\*[\s\S]*?\*\//g, "").replace(/\/\/[^\n]*/g, "");
    const out: CArray[] = [];
    // TYPE NAME[]([N])... [__attribute__((...)) | MACRO]... = { ... }; a 2D
    // array's rows are compiled back to back: its numbers are read flat
    const re = /(?:static\s+)?(?:const\s+)?([A-Za-z_][\w ]*?)\s+(\w+)\s*\[[^\]]*\](?:\s*\[[^\]]*\])*\s*(?:(?:__attribute__\s*\(\((?:[^()]|\([^()]*\))*\)\)|[A-Z_][A-Z0-9_]*)\s*)*=\s*\{((?:[^{}]|\{[^{}]*\})*)\}\s*;/g;
    for (const m of src.matchAll(re)) {
        const size = ELEM[m[1].trim()];
        if (!size) {
            continue;
        }
        // #if lines inside the numbers are taken as true (the build's
        // defines): if they aren't, the array won't match the overlay's
        // bytes and the release leaves it in the patch
        const body = m[3].replace(/^\s*#\s*(if|ifdef|ifndef|endif)\b[^\n]*$/gm, "");
        const items = body.replace(/[{}]/g, ",").split(",").map((s) => s.trim()).filter((s) => s);
        const values: number[] = [];
        let ok = true;
        for (const it of items) {
            const v = /^-?(0x[0-9a-fA-F]+|\d+)$/.test(it) ? Number(it.startsWith("-") ? -Number(it.slice(1)) : Number(it)) : NaN;
            if (Number.isNaN(v)) {
                ok = false;
                break;
            }
            values.push(v);
        }
        if (ok && values.length) {
            out.push({ name: m[2], size, values });
        }
    }
    return out;
}

// the bytes of an array as compiled (little endian)
export function arrayBytes(a: CArray): Uint8Array {
    const out = new Uint8Array(a.values.length * a.size);
    const v = new DataView(out.buffer);
    a.values.forEach((x, i) => {
        if (a.size === 1) {
            v.setUint8(i, x & 0xFF);
        } else if (a.size === 2) {
            v.setUint16(i * 2, x & 0xFFFF, true);
        } else {
            v.setUint32(i * 4, x >>> 0, true);
        }
    });
    return out;
}

// symbol -> its addresses (a static name may be in several objects), from
// the link map's section lines and each object's symbol table (mipsel nm);
// data / rodata / bss symbols, static ones too
export function symbolAddresses(mapFile: string, root: string): Map<string, number[]> {
    const map = fs.readFileSync(mapFile, "utf8");
    const bases = new Map<string, number>(); // "obj|section" -> address
    for (const m of map.matchAll(/^ (\.(?:data|rodata|sdata|bss|sbss))\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+\.o)\s*$/gm)) {
        if (parseInt(m[3], 16)) {
            bases.set(m[4] + "|" + m[1], parseInt(m[2], 16));
        }
    }
    const objs = new Set([...bases.keys()].map((k) => k.split("|")[0]));
    const out = new Map<string, number[]>();
    for (const o of objs) {
        // objdump -t names each symbol's section (nm's letter goes by the
        // section's flags: a section(".rodata") array shows as data)
        const t = execFileSync("mipsel-linux-gnu-objdump", ["-t", path.join(root, o)], { encoding: "utf8" });
        for (const line of t.split("\n")) {
            const m = /^([0-9a-f]{8}) .{7} (\.\S+)\t[0-9a-f]+ (\S+)$/.exec(line);
            if (!m || m[3] === m[2]) {
                continue;
            }
            const base = bases.get(o + "|" + m[2]);
            if (base !== undefined) {
                const l = out.get(m[3]) ?? [];
                l.push(base + parseInt(m[1], 16));
                out.set(m[3], l);
            }
        }
    }
    return out;
}
