// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_cenp.ts
// src/cenp_sounds.ts against the build's gen_cenp_sounds.py output
// (build/us/CENP_SD.bin, CENP_SD_JP.bin, CENP_SD_PS1.bin), same inputs from disks/
import * as fs from "node:fs";
import * as path from "node:path";
import { buildCenp, cenpCfgIds } from "../src/cenp_sounds.ts";
import { Dsp } from "../src/dsp.ts";
import { md5 } from "../src/md5.ts";
import { audioModule } from "./audio_cli.ts";
import { ROOT, sfxConfig } from "./util.ts";

const rd = (d: string) => async (p: string): Promise<Uint8Array> => new Uint8Array(fs.readFileSync(path.join(ROOT, d, p)));
const res = rd("disks/psp");
const t0 = performance.now();
const r = await buildCenp(audioModule, new Dsp((await audioModule()) as never), { readFile: rd("disks/saturn") },
                          rd("disks/us"), await res("snd_e/SE/sd_mar2.spk"), await res("sound/SE/sd_mar2.spk"),
                          sfxConfig([...cenpCfgIds()]));
console.log("ps1sat rate cap %d, %s s", r.satCap, ((performance.now() - t0) / 1000).toFixed(1));
for (const [name, mine] of [["CENP_SD.bin", r.psp], ["CENP_SD_JP.bin", r.jp], ["CENP_SD_PS1.bin", r.ps1]] as const) {
    const ref = await rd("build/us")(name);
    console.log("%s: ts %s, python %s%s", name, md5(mine), md5(ref), md5(mine) === md5(ref) ? " IDENTICAL" : " DIFFER");
}
