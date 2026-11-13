// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_stage_build.ts [stage...]
// src/stage_build.ts end to end (the WASM converter, the tile swap, the
// stage's tiles and sprites) against the build: every array the release
// locates in build/us/TE*.BIN (node/array_spans.ts) and F_TE*.BIN.
import { modStageDir } from "../src/stage_assets.ts";
import * as fs from "node:fs";
import * as path from "node:path";
import { createRequire } from "node:module";
import { md5 } from "../src/md5.ts";
import { buildStageAll, STAGE_DISC, type SatConvFactory, type SatConvModule } from "../src/stage_build.ts";
import { locateArrays } from "./array_spans.ts";
import { MOD_WORK, ROOT } from "./util.ts";

export const SATCONV_JS = path.join(MOD_WORK, "saturn-assets-wasm/wasm32-unknown-emscripten/release/saturn-assets.js");
const factory: SatConvFactory = () => createRequire(import.meta.url)(SATCONV_JS)({
    print: () => {}, printErr: () => {},
    preRun: [(mod: { ENV: Record<string, string> }) => { mod.ENV.SOTN_MOD_UNCAPPED = "1"; }],
}) as Promise<SatConvModule>;
const rd = (d: string) => async (p: string): Promise<Uint8Array> => new Uint8Array(fs.readFileSync(path.join(ROOT, d, p)));
const MAP: Record<string, string> = { gdn: "gdn/stgdn.map", prs: "prs/stprs.map", rgdn: "rgdn/strgdn.map", rprs: "rprs/strprs.map" };
const stages = process.argv.slice(2).length ? process.argv.slice(2) : ["gdn", "prs", "rgdn", "rprs"];
const works = new Map();
let bad = 0;
for (const st of stages) {
    const t0 = performance.now();
    const r = await buildStageAll(st, factory, rd("disks/saturn"), rd("disks/us"), works);
    const te = STAGE_DISC[st];
    const bin = new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us", te + ".BIN")));
    const located = locateArrays({ gen: "stage_" + st, disc: "ST/" + te + "/" + te + ".BIN", files: [modStageDir(st)],
                                   map: "build/us/mod/" + MAP[st], base: 0x80180000 }, bin);
    let same = 0;
    const diff: string[] = [];
    for (const a of located) {
        const mine = r.arrays.get(a.name);
        if (mine && md5(mine) === md5(a.bytes)) {
            same++;
        } else {
            diff.push(a.name + (mine ? " differs" : " missing"));
        }
    }
    const fte = md5(r.fte) === md5(new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us/F_" + te + ".BIN"))));
    bad += diff.length + (fte ? 0 : 1);
    console.log("%s: %d / %d arrays identical, F_%s %s (%s s)%s", st, same, located.length, te, fte ? "identical" : "DIFFERS",
                ((performance.now() - t0) / 1000).toFixed(1), diff.length ? "\n  " + diff.slice(0, 20).join("\n  ") : "");
}
console.log(bad ? "PROBLEMS: " + bad : "all identical");
