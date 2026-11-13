// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/stage_sounds.ts [--check]
// The release build's Saturn-only stage banks (src/stage_sounds.ts, in place
// of mods/sotn-ultimate/tools/gen_gdn / gen_prs / gen_rgdn / gen_rprs_sounds.py), from the
// extracted discs in disks/: build/us/SD_ZKTE1..4.VH/.VB and their
// src/st/<stage>/mod_<stage>_sfx.h. --check: compare with the files in
// build/us instead of writing them.
import * as fs from "node:fs";
import * as path from "node:path";
import { Dsp } from "../src/dsp.ts";
import { md5 } from "../src/md5.ts";
import { buildStageBank, STAGE_BANKS, stageCfgIds, stageHeader } from "../src/stage_sounds.ts";
import { audioModule } from "./audio_cli.ts";
import { ROOT, sfxConfig } from "./util.ts";

const check = process.argv[2] === "--check";
const rd = (d: string) => async (p: string): Promise<Uint8Array> => new Uint8Array(fs.readFileSync(path.join(ROOT, d, p)));
const dsp = new Dsp((await audioModule()) as never);
const cfg = sfxConfig(stageCfgIds());
for (const b of STAGE_BANKS) {
    const t0 = performance.now();
    const r = await buildStageBank(audioModule, dsp, { readFile: rd("disks/saturn") }, rd("disks/us"), cfg, b);
    const base = path.join(ROOT, "build/us", path.basename(b.out));
    const secs = ((performance.now() - t0) / 1000).toFixed(1);
    if (check) {
        const ok = [[".VH", r.vh], [".VB", r.vb]].every(([ext, d]) =>
            md5(d as Uint8Array) === md5(new Uint8Array(fs.readFileSync(base + ext))));
        const h = fs.readFileSync(path.join(ROOT, b.header), "utf8") === stageHeader(b);
        console.log("%s: rate cap %d, %d bytes, %s s: bank %s, header %s", b.key, r.cap, r.vb.length, secs,
                    ok ? "IDENTICAL" : "DIFFERS", h ? "same" : "DIFFERS");
        continue;
    }
    fs.writeFileSync(base + ".VH", r.vh);
    fs.writeFileSync(base + ".VB", r.vb);
    const hp = path.join(ROOT, b.header);
    if (!fs.existsSync(hp) || fs.readFileSync(hp, "utf8") !== stageHeader(b)) {
        fs.writeFileSync(hp, stageHeader(b));
    }
    console.log("%s sounds: %d sounds, rate cap %d, bank %d bytes", b.key, b.sounds.length, r.cap, r.vb.length);
}
