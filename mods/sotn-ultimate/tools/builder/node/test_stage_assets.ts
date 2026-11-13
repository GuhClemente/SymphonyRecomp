// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_stage_assets.ts [stage...] [--work DIR]
// src/stage_assets.ts against the current build's outputs of
// mods/sotn-ultimate/tools/gdn_assets.py / prs_assets.py / rgdn_assets.py / rprs_assets.py
// (assets/st/<st>, src/st/<st>, build/us/F_TE*.BIN).
// Input: the converter's output after the tile swap, per stage in
// <work>/<st>/swapped (--work DIR, kept when there; else a temp folder, made
// by stage_work.ts prepareSwapped), the Saturn files from disks/saturn and
// the PS1 assets from assets/st.
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { inflateZlib } from "../src/inflate.ts";
import { buildStage, WRP_COPY, modStageDir } from "../src/stage_assets.ts";
import { ROOT } from "./util.ts";
import { prepareSwapped } from "./stage_work.ts";

const args = process.argv.slice(2);
let workRoot = "";
const wi = args.indexOf("--work");
if (wi >= 0) {
    workRoot = args[wi + 1];
    args.splice(wi, 2);
}
if (!workRoot) {
    workRoot = fs.mkdtempSync(path.join(os.tmpdir(), "sotn_stage_work_"));
}
prepareSwapped(workRoot);
const stages = args.length ? args : ["gdn", "prs", "rgdn", "rprs"];
const rd = (dir: string) => (p: string): Uint8Array => new Uint8Array(fs.readFileSync(path.join(dir, p)));

// files sprites() rewrites after this part (compared with assets/st/wrp)
const SPRITES_REWRITE = new Set(["us/sprite_banks.json", "graphics_banks.json", "palette_def.json"]);

function firstDiff(a: Uint8Array, b: Uint8Array): string {
    const n = Math.min(a.length, b.length);
    for (let i = 0; i < n; i++) {
        if (a[i] !== b[i]) {
            return `first difference at byte 0x${i.toString(16)} (ts ${a[i]}, oracle ${b[i]})`;
        }
    }
    return `sizes ${a.length} (ts) / ${b.length} (oracle), equal up to the shorter`;
}

function walk(dir: string, rel = ""): string[] {
    const out: string[] = [];
    for (const e of fs.readdirSync(path.join(dir, rel), { withFileTypes: true })) {
        const r = rel ? rel + "/" + e.name : e.name;
        if (e.isDirectory()) {
            out.push(...walk(dir, r));
        } else {
            out.push(r);
        }
    }
    return out;
}

let bad = 0;
// the WRP files copied: the asset dir minus copytree's ignore patterns
const wrpExpect = walk(path.join(ROOT, "assets/st/wrp")).filter((f) =>
    !/(^|\/)wrp_tile[^/]*$/.test(f) && !["layers.json", "rooms.json", "entity_layouts.json"].includes(f)).sort();
if (wrpExpect.join() !== WRP_COPY.slice().sort().join()) {
    console.log("WRP_COPY differs from assets/st/wrp:", wrpExpect);
    bad++;
}

for (const st of stages) {
    const t0 = performance.now();
    const r = await buildStage(st, {
        work: rd(path.join(workRoot, st, "swapped")),
        saturn: rd(path.join(ROOT, "disks/saturn")),
        ps1Asset: rd(ROOT),
    });
    console.log(`=== ${st}: ${r.files.size} files, ${((performance.now() - t0) / 1000).toFixed(1)} s`);
    let same = 0;
    for (const [p, mine] of [...r.files].sort((a, b) => (a[0] < b[0] ? -1 : 1))) {
        const sub = p.startsWith(`assets/st/${st}/`) ? p.slice(`assets/st/${st}/`.length) : null;
        let ref = p;
        let note = "";
        if (sub !== null && SPRITES_REWRITE.has(sub)) {
            ref = "assets/st/wrp/" + sub;
            note = " (as copied, before sprites(); compared with assets/st/wrp)";
        }
        const oracle = path.join(ROOT, ref);
        if (!fs.existsSync(oracle)) {
            console.log(`  ${p}: NO ORACLE FILE`);
            bad++;
            continue;
        }
        const want = new Uint8Array(fs.readFileSync(oracle));
        if (want.length === mine.length && want.every((v, i) => v === mine[i])) {
            same++;
            console.log(`  ${p}: identical${note}`);
        } else {
            bad++;
            console.log(`  ${p}: DIFFERS, ${firstDiff(mine, want)}${note}`);
        }
    }
    // the oracle's files this part doesn't make (sprites(): its PNGs, the
    // sprite headers, the build config)
    const others = walk(path.join(ROOT, "assets/st", st)).map((f) => `assets/st/${st}/${f}`)
        .filter((f) => !r.files.has(f));
    console.log(`  ${same}/${r.files.size} identical; not made here (sprites()): ${others.length} files in assets/st/${st}` +
                (others.length ? ` (${others.filter((f) => !f.endsWith(".png")).join(", ") || "PNGs only"}; ` +
                 `${others.filter((f) => f.endsWith(".png")).length} PNGs)` : ""));
    // the sprites() handoff: PAGE_BLOCKS against the page PNGs sprites()
    // wrote (8-bit indexed, filter 0), SHIMMER_PALETTE against the last
    // sprite palette (rgdn)
    for (const [, , name, data] of r.handoff.pageBlocks) {
        const png = new Uint8Array(fs.readFileSync(path.join(ROOT, "assets/st", st, name + ".png")));
        let p = 8;
        const idat: number[] = [];
        while (p < png.length) {
            const n = (png[p] << 24 | png[p + 1] << 16 | png[p + 2] << 8 | png[p + 3]) >>> 0;
            if (String.fromCharCode(...png.subarray(p + 4, p + 8)) === "IDAT") {
                idat.push(...png.subarray(p + 8, p + 8 + n));
            }
            p += 12 + n;
        }
        const raw = inflateZlib(new Uint8Array(idat));
        let ok = true;
        for (let y = 0; y < 128 && ok; y++) {
            ok = raw[y * 129] === 0;
            for (let x = 0; x < 128 && ok; x += 2) {
                ok = data[y * 64 + x / 2] === (raw[y * 129 + 1 + x] | raw[y * 129 + 2 + x] << 4);
            }
        }
        console.log(`  handoff page block ${name}: ${ok ? "identical" : "DIFFERS"} to ${name}.png`);
        bad += ok ? 0 : 1;
    }
    if (r.handoff.shimmerPalette.length) {
        const pals = fs.readFileSync(path.join(ROOT, modStageDir(st), "mod_sprite_pals.h"), "latin1").trim().split("\n");
        const last = pals[pals.length - 3].trim();
        const mine = r.handoff.shimmerPalette.map((c) => "0x" + c.toString(16).toUpperCase().padStart(4, "0")).join(", ") + ",";
        console.log(`  handoff shimmer palette: ${last === mine ? "identical" : "DIFFERS"} to mod_sprite_pals.h's last`);
        bad += last === mine ? 0 : 1;
    }
    const srcOthers = fs.readdirSync(path.join(ROOT, modStageDir(st))).filter((f) => f.startsWith("mod_") && f.endsWith(".h"))
        .filter((f) => !r.files.has(`${modStageDir(st)}${f}`));
    console.log(`  generated headers not made here: ${srcOthers.join(", ")}`);
}
console.log(bad ? `${bad} problems` : "all identical");
process.exit(bad ? 1 : 0);
