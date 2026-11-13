// SPDX-License-Identifier: AGPL-3.0-or-later
// The audio module (mods/sotn-ultimate/tools/builder/native/build.sh: FFmpeg's ATRAC3/WAV
// decoding + filters, and mods/sotn-ultimate/tools/satxa.c) and the encoders built on it:
// the PSP dub lines (mods/sotn-ultimate/tools/psp_dub.py), the Saturn JP takes, the PSP
// fairy song (mods/sotn-ultimate/tools/gen_saturn_music.py ONESHOTS). The release build
// runs these through Node (node/audio_cli.ts), the builder page in the
// browser: one implementation, the same bytes.

export interface AudioModule {
    FS: {
        writeFile(path: string, data: Uint8Array): void;
        readFile(path: string): Uint8Array;
        unlink(path: string): void;
    };
    callMain(args: string[]): number; // satxa
    ccall(name: string, ret: string, types: string[], args: unknown[]): number;
}

export const VOICE_RATE = 37800;
const HEADROOM = 0.5; // psp_dub.py: the decode at half scale, x2 after
const SILENCE = 250;
const LEAD_S = 0.03;
const TAIL_S = 0.12;
const TIMER_MARGIN = 3;
const WIN = 378; // 10 ms
const VOICE_CHAIN = "highpass=f=50,volume=" + HEADROOM +
    ",aresample=37800,aformat=sample_fmts=s16:channel_layouts=mono";

// numpy's round: half to even
export function roundHalfEven(x: number): number {
    const r = Math.round(x);
    return Math.abs(x % 1) === 0.5 && r % 2 !== 0 ? r - 1 : r;
}

function pcmOf(wav: Uint8Array): Int16Array {
    // at3dec / makeWav headers are 44 bytes
    return new Int16Array(wav.slice(44).buffer);
}

export function makeWav(pcm: Int16Array, rate: number, ch = 1): Uint8Array {
    const out = new Uint8Array(44 + pcm.length * 2);
    const v = new DataView(out.buffer);
    out.set([0x52, 0x49, 0x46, 0x46], 0);
    v.setUint32(4, 36 + pcm.length * 2, true);
    out.set([0x57, 0x41, 0x56, 0x45, 0x66, 0x6d, 0x74, 0x20], 8);
    v.setUint32(16, 16, true);
    v.setUint16(20, 1, true);
    v.setUint16(22, ch, true);
    v.setUint32(24, rate, true);
    v.setUint32(28, rate * 2 * ch, true);
    v.setUint16(32, 2 * ch, true);
    v.setUint16(34, 16, true);
    out.set([0x64, 0x61, 0x74, 0x61], 36);
    v.setUint32(40, pcm.length * 2, true);
    out.set(new Uint8Array(pcm.buffer, pcm.byteOffset, pcm.length * 2), 44);
    return out;
}

// any file the decoder reads (.at3, .wav) -> s16 PCM through `chain`
export function decode(a: AudioModule, src: Uint8Array, chain: string): Int16Array {
    a.FS.writeFile("/dec.in", src);
    const n = a.ccall("at3_decode", "number", ["string", "string", "string"], ["/dec.in", "/dec.wav", chain]);
    if (n < 0) {
        throw new Error("audio decode failed (" + n + ")");
    }
    const pcm = pcmOf(a.FS.readFile("/dec.wav"));
    a.FS.unlink("/dec.in");
    a.FS.unlink("/dec.wav");
    return pcm;
}

export function satxa(a: AudioModule, cmd: string, wav: Uint8Array, args: string[]): Uint8Array {
    a.FS.writeFile("/sx.wav", wav);
    const rc = a.callMain([cmd, "/sx.wav", "/sx.xa", ...args]);
    if (rc) {
        throw new Error("satxa " + cmd + " failed (" + rc + ")");
    }
    const xa = a.FS.readFile("/sx.xa").slice();
    a.FS.unlink("/sx.wav");
    a.FS.unlink("/sx.xa");
    return xa;
}

export interface VoiceResult {
    xa: Uint8Array; // raw 2352-byte mono XA sectors
    samples: number;
    seconds: number;
    unk228: number; // the config's length (VSyncs)
}

// psp_dub.py encode(): one voice line (an .at3, or a WAV such as a Saturn
// take) -> trimmed, scaled 37.8 kHz mono XA
export function encodeVoice(a: AudioModule, src: Uint8Array, gain: number): VoiceResult {
    const raw = decode(a, src, VOICE_CHAIN);
    const x = new Float64Array(raw.length);
    for (let i = 0; i < raw.length; i++) {
        x[i] = raw[i] / HEADROOM; // back to full scale, peaks over it kept
    }
    const nwin = Math.floor(x.length / WIN);
    let first = -1;
    let last = -1;
    for (let w = 0; w < nwin; w++) {
        let s = 0;
        for (let i = w * WIN; i < (w + 1) * WIN; i++) {
            s += x[i] * x[i];
        }
        if (Math.sqrt(s / WIN) > SILENCE) {
            if (first < 0) {
                first = w;
            }
            last = w;
        }
    }
    let a0 = 0;
    let b0 = x.length;
    if (first >= 0) {
        a0 = Math.max(0, first * WIN - Math.trunc(LEAD_S * VOICE_RATE));
        b0 = Math.min(x.length, (last + 1) * WIN + Math.trunc(TAIL_S * VOICE_RATE));
    }
    const y = new Int16Array(b0 - a0);
    for (let i = 0; i < y.length; i++) {
        y[i] = Math.max(-32768, Math.min(32767, roundHalfEven(x[a0 + i] * gain)));
    }
    const xa = satxa(a, "encmono", makeWav(y, VOICE_RATE), ["1", "0"]);
    return {
        xa,
        samples: y.length,
        seconds: Math.round(y.length / VOICE_RATE * 1000) / 1000,
        unk228: Math.ceil(y.length / VOICE_RATE * 60) + TIMER_MARGIN,
    };
}

export interface SongResult {
    xa: Uint8Array; // raw 2352-byte stereo XA sectors
    lastSample: number; // after the last sample over the silence level
}

// gen_saturn_music.py ONESHOTS: a PSP .at3 -> 37.8 kHz stereo XA, gain in
// the decode (its float stage: no clipped peaks). file / channel: the
// sectors' subheader (repack_xa.py finds the stream by them; the builder
// keeps the disc's own subheaders)
export function encodeSong(a: AudioModule, src: Uint8Array, gain: number, silence: number,
                           file = 1, channel = 0): SongResult {
    const pcm = decode(a, src, "volume=" + gain + ",aresample=37800,aformat=sample_fmts=s16:channel_layouts=stereo");
    let lastSample = 0;
    for (let i = pcm.length - 1; i >= 0; i--) {
        if (Math.abs(pcm[i]) > silence) {
            lastSample = (i >> 1) + 1;
            break;
        }
    }
    return { xa: satxa(a, "encstereo", makeWav(pcm, VOICE_RATE, 2), [String(file), String(channel), "1"]),
             lastSample };
}

const IMA_STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767];
const IMA_IDX = [-1, -1, -1, -1, 2, 4, 6, 8];
export const SATURN_VOICE_RATE = 22050;

// psp_dub.py saturn_clip(): Saturn voice id -> its 22,050 Hz PCM (SD/SDF0.PCM,
// 4-bit IMA, high nibble first; index in 0.BIN at 0x06039414)
export function saturnClip(zeroBin: Uint8Array, sdf0: Uint8Array, sid: number): Int16Array {
    const base = 0x06004080;
    const hdr = 0x06039414 - base;
    const want = [0, 0, 0x7f, 0xff, 0, 0, 3, 0xff, 0, 0, 0, 0];
    if (!want.every((b, i) => zeroBin[hdr + i] === b)) {
        throw new Error("Saturn 0.BIN: unexpected voice index");
    }
    const v = new DataView(zeroBin.buffer, zeroBin.byteOffset, zeroBin.byteLength);
    const o = 0x0603941c - base + (sid - 0x33d) * 8;
    const off = v.getUint32(o, false);
    const size = v.getUint32(o + 4, false);
    const out = new Int16Array(size * 2);
    let p = 0;
    let idx = 0;
    let k = 0;
    for (let j = off; j < off + size; j++) {
        for (const n of [sdf0[j] >> 4, sdf0[j] & 15]) {
            const s = IMA_STEP[idx];
            let d = s >> 3;
            if (n & 4) {
                d += s;
            }
            if (n & 2) {
                d += s >> 1;
            }
            if (n & 1) {
                d += s >> 2;
            }
            p = Math.max(-32768, Math.min(32767, n & 8 ? p - d : p + d));
            idx = Math.max(0, Math.min(88, idx + IMA_IDX[n & 7]));
            out[k++] = p;
        }
    }
    return out;
}
