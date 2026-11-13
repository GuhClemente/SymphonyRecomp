// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_saturn_sfx.ts SATURN_TRACK1.bin DUMPDIR
// src/saturn_sfx.ts against saturn_sfx.py renders dumped to DUMPDIR
// (py_<sid>_<cap>.f64: the channels back to back; .pcm: the mono int16)
import * as fs from "node:fs";
import { IsoFs } from "../src/disc.ts";
import { Dsp } from "../src/dsp.ts";
import { render, SfxContext, toPcm16Mono } from "../src/saturn_sfx.ts";
import { audioModule } from "./audio_cli.ts";
import { fileSource } from "./util.ts";

const [sat, dir] = process.argv.slice(2);
const dsp = new Dsp((await audioModule()) as never);
const ctx = await SfxContext.open(await IsoFs.open(fileSource(sat)), "SD_TEI.CRT", "SD_MARI.CRT");
let bad = 0;
let n = 0;
for (let sid = 0x71C; sid < 0x730; sid++) {
    for (const cap of [44100, 11025, 4912]) {
        const f = dir + "/py_" + sid.toString(16).toUpperCase() + "_" + cap;
        const r = render(dsp, ctx, sid, cap);
        if (!fs.existsSync(f + ".f64")) {
            if (!r.empty) {
                console.log("%s: python empty, ts not", f);
                bad++;
            }
            continue;
        }
        n++;
        const ref = new Float64Array(new Uint8Array(fs.readFileSync(f + ".f64")).buffer);
        const mine = new Float64Array(r.ch.reduce((s, c) => s + c.length, 0));
        let o = 0;
        for (const c of r.ch) {
            mine.set(c, o);
            o += c.length;
        }
        let diff = 0;
        let maxd = 0;
        for (let i = 0; i < Math.max(ref.length, mine.length); i++) {
            if (ref[i] !== mine[i]) {
                diff++;
                maxd = Math.max(maxd, Math.abs((ref[i] ?? 0) - (mine[i] ?? 0)));
            }
        }
        const pcm = toPcm16Mono(dsp, r);
        const refPcm = new Int16Array(new Uint8Array(fs.readFileSync(f + ".pcm")).buffer);
        const pcmSame = pcm.length === refPcm.length && pcm.every((v, i) => v === refPcm[i]);
        if (diff || !pcmSame) {
            bad++;
            console.log("%s: rate %d, %d of %d floats differ (max %s), pcm %s", f.split("/").pop(), r.rate, diff,
                        ref.length, maxd.toExponential(2), pcmSame ? "same" : "DIFFERS");
        }
    }
}
console.log("%d renders, %d differ", n, bad);
