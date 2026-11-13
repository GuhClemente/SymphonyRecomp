// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_mod_rooms.ts
// src/mod_rooms.ts and src/prs_links.ts (the ports of gen_mod_rooms.py and
// gen_prs_links.py) from disks/saturn and disks/us, against what the scripts
// wrote: each header's plain-number arrays (parseCArrays of the current
// src/st/<s>/mod_rooms.h), the textures (build/us/F_<S>.BIN) and the edited
// tilemaps (assets/st/<s>/*.bin) byte for byte. Also checks the overlay
// tables parseStage() reads against the extracted assets they stand for.
import * as fs from "node:fs";
import * as path from "node:path";
import { parseCArrays } from "./c_arrays.ts";
import { buildModRooms, parseStage, type GenResult } from "../src/mod_rooms.ts";
import { buildPrsLinks, tilemapNumber } from "../src/prs_links.ts";
import { MOD_WORK, ROOT } from "./util.ts";

const reader = (dir: string) => async (p: string): Promise<Uint8Array> =>
    new Uint8Array(fs.readFileSync(path.join(ROOT, dir, p)));
const sat = reader("disks/saturn");
const psx = reader("disks/us");

let bad = 0;

// the overlay tables vs the assets folder: layers.json order and names
for (const stage of ["np3", "no3", "rno3", "wrp", "rwrp", "no0", "no4", "rno0", "rno4"]) {
    const up = stage.toUpperCase();
    const st = parseStage(await psx(`ST/${up}/${up}.BIN`));
    let adir = path.join(ROOT, "assets/st", stage);
    if (fs.existsSync(path.join(adir, "us", "layers.json"))) {
        adir = path.join(adir, "us");
    }
    const lj = JSON.parse(fs.readFileSync(path.join(adir, "layers.json"), "utf8")) as Record<string, { data: string }>[];
    const want: string[] = [];
    for (const L of lj) {
        for (const part of Object.values(L)) {
            want.push(part.data);
        }
    }
    const got: string[] = [];
    for (const [f, b] of st.roomdefs) {
        for (const i of [b, f]) {
            if (i >= 0) {
                got.push(`${stage}_tilemap_${tilemapNumber(st, i)}.bin`);
            }
        }
    }
    const ok = want.join() === got.join();
    // every tilemap as extracted (the edited ones aside)
    let same = 0;
    let differ = 0;
    for (let i = 0; i < st.layers.length; i++) {
        const f = path.join(adir, `${stage}_tilemap_${tilemapNumber(st, i)}.bin`);
        const d = new Uint8Array(fs.readFileSync(f));
        if (Buffer.compare(d, st.layers[i].tilemapBytes) === 0) {
            same++;
        } else {
            differ++;
        }
    }
    console.log(`${stage}: layers.json order ${ok ? "ok" : "DIFFERS"}, ${st.layers.length} layers, ` +
        `${st.roomdefs.length} roomdefs, ${st.rooms.length} rooms; tilemaps = assets: ${same}, differ: ${differ}`);
    if (!ok) {
        bad++;
    }
}

function compare(res: GenResult, label: string): void {
    for (const [hdr, arrays] of res.headers) {
        const want = parseCArrays(path.join(ROOT, hdr));
        const names = new Set(want.map((a) => a.name));
        for (const a of want) {
            const g = arrays.get(a.name);
            if (g === undefined) {
                console.log(`${hdr} ${a.name}: MISSING from the port`);
                bad++;
                continue;
            }
            let first = -1;
            const n = Math.max(a.values.length, g.values.length);
            for (let i = 0; i < n; i++) {
                if (a.values[i] !== g.values[i]) {
                    first = i;
                    break;
                }
            }
            const ok = first < 0 && a.size === g.size;
            if (!ok) {
                bad++;
            }
            console.log(`${hdr} ${a.name}: u${a.size * 8} x${a.values.length} ` + (ok ? "exact" :
                `DIFFERS (port u${g.size * 8} x${g.values.length}; first at ${first}: ${a.values[first]} vs ${g.values[first]})`));
        }
        for (const k of arrays.keys()) {
            if (!names.has(k)) {
                console.log(`${hdr} ${k}: not in the header`);
                bad++;
            }
        }
    }
    for (const [p, got] of res.binaries) {
        const want = new Uint8Array(fs.readFileSync(path.join(ROOT, p)));
        let first = -1;
        let diffs = 0;
        for (let i = 0; i < Math.max(want.length, got.length); i++) {
            if (want[i] !== got[i]) {
                diffs++;
                if (first < 0) {
                    first = i;
                }
            }
        }
        if (diffs) {
            bad++;
        }
        console.log(`${p}: ${got.length} bytes ` + (diffs ? `DIFFERS in ${diffs} bytes (first at 0x${first.toString(16)})` : "exact"));
    }
    void label;
}

let t = Date.now();
const rooms = await buildModRooms(sat, psx);
console.log(`gen_mod_rooms port: ${Date.now() - t} ms`);
compare(rooms, "rooms");
t = Date.now();
const prs = await buildPrsLinks(sat, psx);
console.log(`gen_prs_links port: ${Date.now() - t} ms, graft passes ${JSON.stringify([...prs.passes])}`);
compare(prs, "prs");
for (const [k, e] of prs.tilemapEdits) {
    console.log(`${k}: ${e.overlay} +0x${e.offset.toString(16)}`);
    // the pristine copy the script edits is the retail tilemap
    const pristine = path.join(MOD_WORK, "retail-assets", k.split("/")[2], e.name + ".bin");
    if (fs.existsSync(pristine)) {
        const ov = await psx(e.overlay);
        const d = new Uint8Array(fs.readFileSync(pristine));
        const same = Buffer.compare(d, ov.subarray(e.offset, e.offset + d.length)) === 0;
        console.log(`  pristine copy = retail overlay: ${same}`);
        if (!same) {
            bad++;
        }
    }
}
console.log(bad ? `FAIL: ${bad} differences` : "all exact");
process.exit(bad ? 1 : 0);
