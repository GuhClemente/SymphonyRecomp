// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/make_builder_page.ts PATCHDIR OUT.html
// web/builder.html + the bundled modules + PATCHDIR/manifest.json.
// The page holds no game data (the patch is a separate file).
import * as fs from "node:fs";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import { AUDIO_JS } from "./audio_cli.ts";
import { SATCONV_JS } from "./satconv.ts";

const B = path.resolve(import.meta.dirname, "..");
const [dir, out] = process.argv.slice(2);
const bundle = path.join(B, "dist/builder_core.js");
fs.mkdirSync(path.dirname(bundle), { recursive: true });
// the modules the page uses (src/), and every module they import, each after
// its imports: the bundle runs them in this order (bundle.ts)
const ROOTS = ["md5", "disc", "sector", "identify", "saturn_lzss", "monster2", "audio", "dsp", "vab",
    "grunts", "saturn_sprites", "cen_maria", "title_portraits", "saturn_icons", "mar_hud", "fmap",
    "saturn_tilematch", "mod_rooms", "prs_links", "sotn_cmpgfx", "inflate", "pyjson", "go_png", "ps1_assets",
    "gdn_tile_swap_recipe", "gdn_tile_swap", "stage_assets", "stage_sprites", "stage_build", "xa_mono",
    "mar_assets", "mod_tables", "ric2", "saturn_weapons", "jp_sfx_pitch", "psp_sfx", "saturn_sfx",
    "mar_sounds", "mar2_sounds", "cen_sounds", "cenp_sounds", "stage_sounds", "psp_text", "saturn_music",
    "vcdiff", "build"];
const order: string[] = [];
const visiting = new Set<string>();
const visit = (m: string): void => {
    if (order.includes(m)) {
        return;
    }
    if (visiting.has(m)) {
        throw new Error("src/" + m + ".ts: an import cycle");
    }
    visiting.add(m);
    const src = fs.readFileSync(path.join(B, "src", m + ".ts"), "utf8");
    for (const [, list, dep] of src.matchAll(/^import\s*\{([^}]*)\}\s*from\s*"\.\/(\w+)\.ts"/gm)) {
        if (list.split(",").some((x) => x.trim() && !x.trim().startsWith("type "))) {
            visit(dep);
        }
    }
    visiting.delete(m);
    order.push(m);
};
ROOTS.forEach(visit);
execFileSync(process.execPath, ["--no-warnings", path.join(B, "node/bundle.ts"), bundle,
    ...order.map((m) => path.join(B, "src", m + ".ts"))], { stdio: "inherit" });
const manifest = fs.readFileSync(path.join(dir, "manifest.json"), "utf8");
// the patch itself, embedded (base64): the page takes no other
const patchName = fs.readdirSync(dir).find((n) => n.endsWith(".xdelta"));
if (!patchName) {
    throw new Error(dir + ": no .xdelta");
}
const patchB64 = fs.readFileSync(path.join(dir, patchName)).toString("base64");
let html = fs.readFileSync(path.join(B, "web/builder.html"), "utf8");
for (const [tag, val] of [["/*__BUNDLE__*/", fs.readFileSync(bundle, "utf8").replaceAll("</script", "<\\/script")],
                          ["/*__AUDIO__*/", fs.readFileSync(AUDIO_JS, "utf8").replaceAll("</script", "<\\/script")],
                          ["/*__SATCONV__*/", fs.readFileSync(SATCONV_JS, "utf8").replaceAll("</script", "<\\/script")],
                          ["/*__MANIFEST__*/", manifest],
                          ["/*__PATCH__*/", patchB64]]) {
    if (html.split(tag).length !== 2) {
        throw new Error("template tag " + tag);
    }
    html = html.replace(tag, () => val);
}
fs.writeFileSync(out, html);
console.log("page: %s (%d bytes)", out, html.length);
