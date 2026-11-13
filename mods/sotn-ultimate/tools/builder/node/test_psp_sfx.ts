// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_psp_sfx.ts FULL.bin
// src/psp_sfx.ts against the build: gen_psp_sfx.py (build/us/SFX_PSP.BIN)
// and gen_psp_sfx.py --set jp (build/us/SFX_JP.BIN), same inputs from
// disks/ (FULL: the build disc, for the mod tools banks)
import * as fs from "node:fs";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import { IsoFs } from "../src/disc.ts";
import { Dsp } from "../src/dsp.ts";
import { md5 } from "../src/md5.ts";
import { buildSfxJp, buildSfxPsp, type SfxManifest } from "../src/psp_sfx.ts";
import { audioModule } from "./audio_cli.ts";
import { ROOT, fileSource } from "./util.ts";

const rd = (p: string): Promise<Uint8Array> => Promise.resolve(new Uint8Array(fs.readFileSync(path.join(ROOT, p))));
const m: SfxManifest = JSON.parse(execFileSync(path.join(ROOT, ".venv/bin/python"),
    [path.join(ROOT, "mods/sotn-ultimate/tools/builder/node/sfx_manifest.py")], { cwd: ROOT, encoding: "utf8" }));
const full = await IsoFs.open(fileSource(process.argv[2]));
const readers = {
    ps1: (p: string) => rd("disks/us/" + p),
    out: (p: string) => full.readFile(p),
    psp: (p: string) => rd("disks/psp/" + p),
};
const dsp = (a: never) => new Dsp(a);
const quiet = (): void => {};
const t0 = performance.now();
const psp = await buildSfxPsp(audioModule, dsp, m, readers, quiet);
const jp = await buildSfxJp(audioModule, dsp, m, readers, psp, quiet);
for (const [name, mine] of [["SFX_PSP.BIN", psp.data], ["SFX_JP.BIN", jp.data]] as const) {
    const ref = new Uint8Array(fs.readFileSync(path.join(ROOT, "build/us", name)));
    console.log("%s: ts %s (%d bytes), python %s (%d bytes)%s", name, md5(mine), mine.length, md5(ref), ref.length,
                md5(mine) === md5(ref) ? " IDENTICAL" : " DIFFER");
}
console.log("PSP banks: %s", [...psp.banks].map(([k, v]) => k + "=" + v).join(" "));
console.log("%s s", ((performance.now() - t0) / 1000).toFixed(1));
