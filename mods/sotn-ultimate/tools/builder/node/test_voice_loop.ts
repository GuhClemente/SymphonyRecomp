// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_voice_loop.ts AT3 [N]
// the same voice line encoded N times in one audio module: does anything
// build up (memory, file descriptors) until it fails?
import * as fs from "node:fs";
import { encodeVoice } from "../src/audio.ts";
import { audioModule } from "./audio_cli.ts";

const src = new Uint8Array(fs.readFileSync(process.argv[2]));
const n = Number(process.argv[3] ?? 600);
const a = await audioModule() as unknown as { HEAP8: Int8Array } & Parameters<typeof encodeVoice>[0];
for (let i = 0; i < n; i++) {
    try {
        encodeVoice(a, src, 0.95);
    } catch (e) {
        console.log("failed at call %d: %s (heap %d MB)", i, e, (a.HEAP8?.length ?? 0) >> 20);
        process.exit(1);
    }
    if (i % 100 === 0) {
        console.log("call %d ok, heap %d MB", i, (a.HEAP8?.length ?? 0) >> 20);
    }
}
console.log("all %d ok", n);
