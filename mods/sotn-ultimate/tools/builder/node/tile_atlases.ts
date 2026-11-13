// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/tile_atlases.ts OUTDIR
// The tile swap atlases (src/gdn_tile_swap_recipe.ts) rebuilt from the
// player's own discs, for mods/sotn-ultimate/tools/gdn_tile_swap.py: OUTDIR/<data
// folder>/atlas<name>.rgba, each atlas tile 16x16 RGBA (1024 bytes) after
// the other. gdn_tile_swap.py runs this when they are missing or older than
// the recipe.
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { convertStage, rebuildAtlases, STAGE_MAPS } from "./stage_work.ts";
import type { WorkFiles } from "../src/gdn_tile_swap.ts";

// recipe set -> [gdn_tile_swap.py data folder, atlas name]
export const ATLAS_FILES: Record<string, [string, string]> = {
    gdn: ["gdn_ps1_tiles", ""], rgdn: ["gdn_ps1_tiles", "_rgdn"], prs: ["prs_ps1_tiles", ""],
    rprs: ["prs_ps1_tiles", "_rprs"],
};

const out = process.argv[2];
if (!out) {
    console.error("usage: tile_atlases.ts OUTDIR");
    process.exit(2);
}
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "sotn_atlases_"));
const conv: Record<string, WorkFiles> = {};
for (const st of Object.keys(STAGE_MAPS)) {
    conv[st] = convertStage(st, path.join(tmp, st));
}
const atlases = rebuildAtlases(conv);
fs.rmSync(tmp, { recursive: true });
for (const [set, tiles] of Object.entries(atlases)) {
    const [dir, name] = ATLAS_FILES[set];
    const raw = new Uint8Array(tiles.length * 1024);
    tiles.forEach((t, i) => raw.set(t, i * 1024));
    fs.mkdirSync(path.join(out, dir), { recursive: true });
    fs.writeFileSync(path.join(out, dir, `atlas${name}.rgba`), raw);
    console.log("tile atlas %s: %d tiles", set, tiles.length);
}
