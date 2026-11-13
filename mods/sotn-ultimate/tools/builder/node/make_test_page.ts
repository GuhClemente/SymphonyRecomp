// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/make_test_page.ts OUT.html
// web/monster2_test.html + the bundled modules + the manifest (bestiary bits
// and the reference md5s of build/us's MONSTER2.BIN / MONSTER.BIN).
// The page holds no game data.
import * as fs from "node:fs";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import { bestiaryBits } from "./util.ts";
import { md5 } from "../src/md5.ts";

const B = path.resolve(import.meta.dirname, "..");
const ROOT = path.resolve(B, "../../../..");
const out = process.argv[2];
const bundle = path.join(B, "dist/builder_core.js");
fs.mkdirSync(path.dirname(bundle), { recursive: true });
execFileSync(process.execPath, [path.join(B, "node/bundle.ts"), bundle,
    ...["md5", "disc", "identify", "saturn_lzss", "monster2"].map((m) => path.join(B, "src", m + ".ts"))],
    { stdio: "inherit" });
const manifest = {
    bestiaryBits: bestiaryBits(),
    expect: Object.fromEntries(["MONSTER2.BIN", "MONSTER.BIN"].map((f) =>
        [f, md5(fs.readFileSync(path.join(ROOT, "build/us", f)))])),
};
let html = fs.readFileSync(path.join(B, "web/monster2_test.html"), "utf8");
for (const [tag, val] of [["/*__BUNDLE__*/", fs.readFileSync(bundle, "utf8").replaceAll("</script", "<\\/script")],
                          ["/*__MANIFEST__*/", JSON.stringify(manifest)]]) {
    if (html.split(tag).length !== 2) {
        throw new Error("template tag " + tag);
    }
    html = html.replace(tag, () => val);
}
fs.writeFileSync(out, html);
console.log("page: %s (%d bytes), expect %j", out, html.length, manifest.expect);
