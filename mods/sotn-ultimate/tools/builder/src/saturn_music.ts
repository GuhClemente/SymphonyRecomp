// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn-only music (mods/sotn-ultimate/tools/gen_saturn_music.py TRACKS): each Saturn
// SD/SDxx.PCM through satxa enc (mods/sotn-ultimate/tools/satxa.c compiled to WebAssembly:
// IMA decode, 44.1 -> 37.8 kHz windowed sinc, gain, the loop repeated to at
// least minSeconds, the seek back moved to the loop's quietest stretch with a
// t.fadeMs fade-out, stereo XA ADPCM) -> one stream of raw 2352-byte sectors.
import type { IsoFs } from "./disc.ts";

// the Emscripten module with satxa.c (src/audio.ts AudioModule: FS + callMain)
export interface SatxaModule {
    FS: {
        writeFile(path: string, data: Uint8Array): void;
        readFile(path: string): Uint8Array;
        unlink(path: string): void;
    };
    callMain(args: string[]): number;
}

export interface MusicTrack {
    name: string; // MU_MOD_*
    pcm: string; // Saturn SD/ file
    gain: number;
    // the seek back at the loop's quietest stretch, after this fade-out
    // (gen_saturn_music.py jump_fade_ms; 0: at the loop end)
    fadeMs: number;
}

export async function encodeSaturnTrack(saturn: IsoFs, satxa: SatxaModule, t: MusicTrack,
                                        minSeconds: number): Promise<Uint8Array> {
    satxa.FS.writeFile("/in.pcm", await saturn.readFile("SD/" + t.pcm));
    // file 100 / channel 0: the disc keeps its own subheaders, only the audio is used
    const rc = satxa.callMain(["enc", "/in.pcm", "/out.xa", "100", "0", String(t.gain), String(minSeconds), String(t.fadeMs)]);
    if (rc) {
        throw new Error("satxa enc " + t.pcm + " failed (" + rc + ")");
    }
    const out = satxa.FS.readFile("/out.xa").slice();
    satxa.FS.unlink("/in.pcm");
    satxa.FS.unlink("/out.xa");
    return out;
}
