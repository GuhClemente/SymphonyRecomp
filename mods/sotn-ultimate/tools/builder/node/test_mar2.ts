// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_mar2.ts
// src/mar2_sounds.ts against the build's maria-sfx + gen_mar2_sounds.py
// output (build/us/MARIA2_SD.BIN, MARIA2_SD_JP.BIN), same inputs from disks/
import * as fs from "node:fs";
import * as path from "node:path";
import { Dsp } from "../src/dsp.ts";
import { buildMaria2Sd, MAR2_SLOTS } from "../src/mar2_sounds.ts";
import { md5 } from "../src/md5.ts";
import { audioModule } from "./audio_cli.ts";
import { ROOT, sfxConfig } from "./util.ts";

const rd = async (p: string): Promise<Uint8Array> => new Uint8Array(fs.readFileSync(path.join(ROOT, p)));
const dsp = new Dsp((await audioModule()) as never);
const cfg = sfxConfig(MAR2_SLOTS);
const RES = "disks/psp/";
for (const [name, spk] of [["MARIA2_SD.BIN", "snd_e/SE/sd_mar2.spk"], ["MARIA2_SD_JP.BIN", "sound/SE/sd_mar2.spk"]]) {
    const t0 = performance.now();
    const mine = await buildMaria2Sd(dsp, await rd(RES + spk), (p) => rd("disks/us/" + p), cfg);
    const ref = await rd("build/us/" + name);
    console.log("%s: ts %s, python %s%s (%s s)", name, md5(mine), md5(ref), md5(mine) === md5(ref) ? " IDENTICAL" : " DIFFER",
                ((performance.now() - t0) / 1000).toFixed(1));
}
