// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn-only stages' map converter output (tools/saturn/assets, the
// native build mods/sotn-ultimate/tools/gdn_assets.py makes), for the node scripts: as it
// comes out (convertStage), and after the PS1 tile swap with the atlases
// rebuilt from the discs (src/gdn_tile_swap.ts, what gdn_tile_swap.py does
// with them): <root>/<stage>/swapped (prepareSwapped)
import * as fs from "node:fs";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import { applyTileSwap, atlasInputs, rebuildAtlas, STAGE_PLANS, type Art, type WorkFiles } from "../src/gdn_tile_swap.ts";
import { TILE_SWAP_SETS } from "../src/gdn_tile_swap_recipe.ts";
import { MOD_WORK, ROOT } from "./util.ts";

export const SATURN_ASSETS = process.env.SATURN_ASSETS ||
    path.join(MOD_WORK, "saturn-assets-target/release/saturn-assets");
// stage -> its Saturn map files (disks/saturn/<name>.PRG / .MAP)
export const STAGE_MAPS: Record<string, string> = {
    gdn: "STAGE_15", rgdn: "RSTAGE15", prs: "STAGE_16", rprs: "RSTAGE16",
};

export function readDir(d: string): WorkFiles {
    const m: WorkFiles = new Map();
    for (const f of fs.readdirSync(d)) {
        const p = path.join(d, f);
        if (fs.statSync(p).isFile()) {
            m.set(f, new Uint8Array(fs.readFileSync(p)));
        }
    }
    return m;
}

// the converter's output for a stage, written to out
export function convertStage(stage: string, out: string): WorkFiles {
    if (!fs.existsSync(SATURN_ASSETS)) {
        throw new Error("no Saturn map converter at " + SATURN_ASSETS +
                        " (mods/sotn-ultimate/tools/gdn_assets.py builds it; or set SATURN_ASSETS)");
    }
    const f = STAGE_MAPS[stage];
    fs.mkdirSync(out, { recursive: true });
    execFileSync(SATURN_ASSETS, ["map", "convert-psx", path.join(ROOT, "disks/saturn", f + ".PRG"),
                                 path.join(ROOT, "disks/saturn", f + ".MAP"), out],
                 { env: { ...process.env, SOTN_MOD_UNCAPPED: "1" }, stdio: ["ignore", "ignore", "pipe"] });
    return readDir(out);
}

// every tile swap atlas, rebuilt from the converter outputs (conv: stage ->
// its pristine output) and the PS1 stages' F_*.BIN in disks/us
export function rebuildAtlases(conv: Record<string, WorkFiles>): Record<string, Art[]> {
    const atlases: Record<string, Art[]> = {};
    for (const set of Object.keys(TILE_SWAP_SETS)) {
        const { areas, stages } = atlasInputs(set);
        const fbins: Record<string, Uint8Array> = {};
        for (const s of stages) {
            fbins[s] = new Uint8Array(fs.readFileSync(path.join(ROOT, "disks/us/ST", s, `F_${s}.BIN`)));
        }
        const works: Record<string, WorkFiles> = {};
        for (const a of areas) {
            works[a] = conv[a];
        }
        atlases[set] = rebuildAtlas(set, works, fbins);
    }
    return atlases;
}

// <root>/<stage>/conv (the converter's output) and <root>/<stage>/swapped
// (the same after the tile swap) for all four stages; kept when there
export function prepareSwapped(root: string): void {
    const stages = Object.keys(STAGE_MAPS);
    if (stages.every((st) => fs.existsSync(path.join(root, st, "swapped")))) {
        return;
    }
    const conv: Record<string, WorkFiles> = {};
    for (const st of stages) {
        conv[st] = convertStage(st, path.join(root, st, "conv"));
    }
    const atlases = rebuildAtlases(conv);
    for (const st of stages) {
        const work = readDir(path.join(root, st, "conv"));
        if (st in STAGE_PLANS) {
            applyTileSwap(st, work, atlases);
        }
        const out = path.join(root, st, "swapped");
        fs.mkdirSync(out, { recursive: true });
        for (const [n, b] of work) {
            fs.writeFileSync(path.join(out, n), b);
        }
    }
}
