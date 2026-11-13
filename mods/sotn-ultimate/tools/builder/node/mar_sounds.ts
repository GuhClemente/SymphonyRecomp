// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/mar_sounds.ts [--psp] OUT.BIN [HEADER.h]
// The release build's Saturn Maria banks (src/mar_sounds.ts, in place of
// mods/sotn-ultimate/tools/gen_mar_sounds.py): build/us/MARIA_SD.BIN and mar_sfx.h, or
// with --psp build/us/MARIA_SD_PSP.BIN, from the extracted discs in disks/
import * as fs from "node:fs";
import * as path from "node:path";
import { Dsp } from "../src/dsp.ts";
import { buildMariaSd, marCfgIds, marSfxHeader } from "../src/mar_sounds.ts";
import { audioModule } from "./audio_cli.ts";
import { ROOT, sfxConfig } from "./util.ts";

const args = process.argv.slice(2);
const psp = args[0] === "--psp";
const [out, header] = psp ? args.slice(1) : args;
const dir = (d: string) => async (p: string) => new Uint8Array(fs.readFileSync(path.join(ROOT, d, p)));
const t0 = performance.now();
const r = await buildMariaSd(audioModule, new Dsp((await audioModule()) as never), {
    saturn: { readFile: dir("disks/saturn") },
    ps1: dir("disks/us"),
    pspMar2: await dir("disks/psp")("snd_e/SE/sd_mar2.spk"),
    cfg: sfxConfig(marCfgIds()),
}, psp);
fs.writeFileSync(out, r.data);
if (header) {
    const h = marSfxHeader(r.caps);
    if (!fs.existsSync(header) || fs.readFileSync(header, "utf8") !== h) {
        fs.mkdirSync(path.dirname(header), { recursive: true });
        fs.writeFileSync(header, h);
    }
}
r.caps.forEach((c, i) => console.log("maria sounds: vab %d, caps %s", i + 1, c));
console.log("maria sounds: %s %d bytes, %s s", path.relative(ROOT, out), r.data.length,
            ((performance.now() - t0) / 1000).toFixed(1));
