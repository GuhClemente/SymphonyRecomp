// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_grunts.ts
// src/grunts.ts against the build's gen_psp_grunts.py output
// (build/us/ALK_PSP.BIN, RIH_PSP.BIN), same inputs from disks/
import * as fs from "node:fs";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import { Dsp } from "../src/dsp.ts";
import { buildGrunts } from "../src/grunts.ts";
import { md5 } from "../src/md5.ts";
import { audioModule } from "./audio_cli.ts";
import { ROOT } from "./util.ts";

const SECTOR = 2048;
const rd = (p: string): Uint8Array => new Uint8Array(fs.readFileSync(path.join(ROOT, p)));
const pad = (b: Uint8Array, sectors?: number): Uint8Array => {
    const n = sectors ? sectors * SECTOR : Math.ceil(b.length / SECTOR) * SECTOR;
    const out = new Uint8Array(n);
    out.set(b);
    return out;
};
const cat = (...bs: Uint8Array[]): Uint8Array => {
    const out = new Uint8Array(bs.reduce((s, b) => s + b.length, 0));
    let o = 0;
    for (const b of bs) {
        out.set(b, o);
        o += b.length;
    }
    return out;
};

// sfx_config.yaml (vab 1): "prog,tone" -> notes
const notes: Record<string, number[]> = JSON.parse(execFileSync(path.join(ROOT, ".venv/bin/python"), ["-c",
    "import yaml, json; c = yaml.safe_load(open('assets/dra/us/sfx_config.yaml')); n = {}\n" +
    "for e in c:\n    if e['vabid'] == 1: n.setdefault('%d,%d' % (e['prog'], e['tone']), set()).add(e['note'])\n" +
    "print(json.dumps({k: sorted(v) for k, v in n.items()}))"], { cwd: ROOT, encoding: "utf8" }));

const a = await audioModule();
const dsp = new Dsp(a as never);
const SPK = "disks/psp/snd_e/SE/";
const [avh, avb, ar] = buildGrunts(a, dsp, "ALK", {
    vh: rd("disks/us/VAB/SD_ALK.VH"), vb: rd("disks/us/VAB/SD_ALK.VB"), spk: rd(SPK + "sd_alk.spk"), notes,
}, false);
console.log(ar);
const alk = cat(pad(avh, 3), pad(avb));
const [rvh, rvb, rr] = buildGrunts(a, dsp, "RIH", {
    vh: rd("disks/us/VAB/SD_RIH.VH"), vb: rd("disks/us/VAB/SD_RIH.VB"), spk: rd(SPK + "sd_rih.spk"), notes,
}, false);
console.log(rr);
// RIH_PSP.BIN's grunts only (VH 3 + VB 29 sectors): its SD_MAR tail is the
// prologue Maria's dubbed bank (src/build.ts buildSfxBank, checked by the
// builder rebuild), not made here
const rih = cat(pad(rvh, 3), pad(rvb, 29));
for (const [name, mine] of [["ALK_PSP.BIN", alk], ["RIH_PSP.BIN", rih]] as const) {
    const ref = rd("build/us/" + name).slice(0, mine.length);
    let diff = 0;
    for (let i = 0; i < Math.max(ref.length, mine.length); i++) {
        if (ref[i] !== mine[i]) {
            diff++;
        }
    }
    console.log("%s: ts %s, python %s, %d bytes differ of %d", name, md5(mine), md5(ref), diff, ref.length);
    if (diff) {
        process.exitCode = 1;
    }
}
