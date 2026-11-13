// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/build_disc.ts PATCHDIR PS1.bin SATURN.bin PSP.iso OUT.bin
// The builder page's whole flow, from Node (src/build.ts): for testing.
import * as fs from "node:fs";
import * as path from "node:path";
import { IsoFs } from "../src/disc.ts";
import { buildDisc, type Manifest } from "../src/build.ts";
import { fileSource } from "./util.ts";
import { audioModule } from "./audio_cli.ts";
import { satconvModule } from "./satconv.ts";

const [dir, ps1, sat, psp, out] = process.argv.slice(2);
const m: Manifest = JSON.parse(fs.readFileSync(path.join(dir, "manifest.json"), "utf8"));
const patch = new Uint8Array(fs.readFileSync(path.join(dir, m.name + ".builder.xdelta")));
const src = fileSource(ps1);
const discs = {
    ps1: src,
    ps1Fs: await IsoFs.open(src),
    saturn: await IsoFs.open(fileSource(sat)),
    psp: await IsoFs.open(fileSource(psp)),
};
const t0 = Date.now();
let last = "";
const img = await buildDisc(discs, patch, m, { audio: audioModule, satconv: satconvModule }, (step) => {
    if (step !== last) {
        console.log("%s s  %s", ((Date.now() - t0) / 1000).toFixed(1), step);
        last = step;
    }
});
fs.writeFileSync(out, img);
console.log("wrote %s: %d bytes, md5 %s (verified)", out, img.length, m.target.md5);
