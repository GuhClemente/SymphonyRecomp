// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_cen.ts
// src/cen_sounds.ts against the build's gen_cen_sounds.py output
// (build/us/SD_ZKCEN.VH/.VB, CEN_SD_PS1.bin), same inputs from disks/
import * as fs from "node:fs";
import * as path from "node:path";
import { buildCen, cenCfgIds } from "../src/cen_sounds.ts";
import { Dsp } from "../src/dsp.ts";
import { md5 } from "../src/md5.ts";
import { audioModule } from "./audio_cli.ts";
import { ROOT, sfxConfig } from "./util.ts";

const rd = (d: string) => async (p: string): Promise<Uint8Array> => new Uint8Array(fs.readFileSync(path.join(ROOT, d, p)));
const t0 = performance.now();
const r = await buildCen(audioModule, new Dsp((await audioModule()) as never), { readFile: rd("disks/saturn") },
                         rd("disks/us"), sfxConfig(cenCfgIds()));
console.log("rate cap %d, %s s", r.cap, ((performance.now() - t0) / 1000).toFixed(1));
for (const [name, mine] of [["SD_ZKCEN.VH", r.vh], ["SD_ZKCEN.VB", r.vb], ["CEN_SD_PS1.bin", r.ps1]] as const) {
    const ref = await rd("build/us")(name);
    console.log("%s: ts %s, python %s%s", name, md5(mine), md5(ref), md5(mine) === md5(ref) ? " IDENTICAL" : " DIFFER");
}
