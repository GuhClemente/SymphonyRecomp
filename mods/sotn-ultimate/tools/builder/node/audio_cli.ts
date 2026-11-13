// SPDX-License-Identifier: AGPL-3.0-or-later
// The release build's door to the builder's audio code (src/audio.ts), so
// the build and the player's builder page make the same bytes:
//   node audio_cli.ts voice SRC OUT.xa GAIN       psp_dub.py's encode(); prints
//       {"sectors", "samples", "seconds", "unk228"}
//   node audio_cli.ts decode SRC OUT.wav CHAIN    a plain decode (s16 WAV)
//   node audio_cli.ts song SRC OUT.xa GAIN SILENCE FILE CHANNEL
//       gen_saturn_music.py's one-shots; prints {"sectors", "last_sample"}
import * as fs from "node:fs";
import { createRequire } from "node:module";
import * as path from "node:path";
import { decode, encodeSong, encodeVoice, makeWav, type AudioModule } from "../src/audio.ts";
import { MOD_WORK } from "./util.ts";

export const AUDIO_JS = path.join(MOD_WORK, "builder/audio/audio_core.js");

export async function audioModule(): Promise<AudioModule> {
    const factory = createRequire(import.meta.url)(AUDIO_JS);
    return factory({ print: () => {}, printErr: () => {} });
}

async function main(): Promise<void> {
    const [cmd, src, out, ...rest] = process.argv.slice(2);
    const a = await audioModule();
    const input = new Uint8Array(fs.readFileSync(src));
    if (cmd === "voice") {
        const r = encodeVoice(a, input, Number(rest[0]));
        fs.writeFileSync(out, r.xa);
        console.log(JSON.stringify({ sectors: r.xa.length / 2352, samples: r.samples,
                                     seconds: r.seconds, unk228: r.unk228 }));
    } else if (cmd === "decode") {
        const pcm = decode(a, input, rest[0]);
        const ch = /channel_layouts=stereo/.test(rest[0]) ? 2 : 1;
        const rate = Number(/aresample=(\d+)/.exec(rest[0])?.[1]);
        fs.writeFileSync(out, makeWav(pcm, rate, ch));
    } else if (cmd === "song") {
        const r = encodeSong(a, input, Number(rest[0]), Number(rest[1]), Number(rest[2]), Number(rest[3]));
        fs.writeFileSync(out, r.xa);
        console.log(JSON.stringify({ sectors: r.xa.length / 2352, last_sample: r.lastSample }));
    } else {
        throw new Error("usage: audio_cli.ts voice|decode|song ...");
    }
}

if (import.meta.main) {
    await main();
}
