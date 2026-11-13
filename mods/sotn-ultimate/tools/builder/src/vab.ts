// SPDX-License-Identifier: AGPL-3.0-or-later
// PS1 VAB sound banks (VH header + VB sample bodies) and their 4-bit ADPCM
// samples: mods/sotn-ultimate/tools/vab.py and the helpers of gen_gdn_sounds.py /
// gen_psp_grunts.py, ported for the builder.
//
// VH layout (little endian): a 32-byte header ("pBAV", version, id, size,
// reserved, ps programs, ts tones, vs vags, master volume/pan, attributes),
// 128 program records of 16 bytes, then 16 tone records of 32 bytes for each
// of the ps programs (in program order, programs with tones only), then 256
// u16 VAG sizes (bytes >> 3; entry 0 unused). VB = the VAG bodies back to back.
import { clamp16, npInterp, npConvolveSame, npMean, npSum, roundHalfEven, type Dsp } from "./dsp.ts";

export interface Tone {
    prior: number; mode: number; vol: number; pan: number; center: number; shift: number;
    min: number; max: number; vibW: number; vibT: number; porW: number; porT: number;
    pbmin: number; pbmax: number; r1: number; r2: number; adsr1: number; adsr2: number;
    prog: number; vag: number; r3: number; r4: number; r5: number; r6: number;
}

const TONE_FIELDS: [keyof Tone, number, boolean][] = [ // name, bytes, signed
    ["prior", 1, false], ["mode", 1, false], ["vol", 1, false], ["pan", 1, false],
    ["center", 1, false], ["shift", 1, false], ["min", 1, false], ["max", 1, false],
    ["vibW", 1, false], ["vibT", 1, false], ["porW", 1, false], ["porT", 1, false],
    ["pbmin", 1, false], ["pbmax", 1, false], ["r1", 1, false], ["r2", 1, false],
    ["adsr1", 2, false], ["adsr2", 2, false], ["prog", 2, true], ["vag", 2, true],
    ["r3", 2, true], ["r4", 2, true], ["r5", 2, true], ["r6", 2, true],
];

export class Vab {
    header: Uint8Array;
    programs: Uint8Array[]; // 128 raw 16-byte records
    tones: Map<number, Tone[]>; // program -> its 16 tones
    vags: Uint8Array[];

    constructor(vh: Uint8Array, vb: Uint8Array) {
        const v = new DataView(vh.buffer, vh.byteOffset, vh.byteLength);
        if (v.getUint32(0, true) !== 0x56414270) {
            throw new Error("not a VAB header");
        }
        const ps = v.getUint16(18, true);
        const vs = v.getUint16(22, true);
        this.header = vh.slice(0, 32);
        this.programs = [];
        for (let i = 0; i < 128; i++) {
            this.programs.push(vh.slice(32 + i * 16, 48 + i * 16));
        }
        this.tones = new Map();
        let off = 32 + 128 * 16;
        for (let p = 0; p < 128; p++) {
            if (this.programs[p][0] === 0) {
                continue;
            }
            const tl: Tone[] = [];
            for (let t = 0; t < 16; t++) {
                const rec = {} as Tone;
                let o = off + t * 32;
                for (const [name, size, signed] of TONE_FIELDS) {
                    rec[name] = size === 1 ? v.getUint8(o) : signed ? v.getInt16(o, true) : v.getUint16(o, true);
                    o += size;
                }
                tl.push(rec);
            }
            this.tones.set(p, tl);
            off += 16 * 32;
        }
        if (this.tones.size !== ps) {
            throw new Error("VAB: program count");
        }
        this.vags = [];
        let pos = 0;
        for (let i = 1; i <= vs; i++) {
            const n = v.getUint16(off + 2 * i, true) << 3;
            this.vags.push(vb.slice(pos, pos + n));
            pos += n;
        }
        if (vh.length !== off + 512) {
            throw new Error("VAB: header size");
        }
    }

    write(): [Uint8Array, Uint8Array] {
        const progs = [...this.tones.keys()].sort((a, b) => a - b);
        const ps = progs.length;
        let ts = 0;
        for (const p of progs) {
            ts += this.programs[p][0];
        }
        const vbLen = this.vags.reduce((s, x) => s + x.length, 0);
        const vb = new Uint8Array(vbLen);
        let pos = 0;
        for (const x of this.vags) {
            vb.set(x, pos);
            pos += x.length;
        }
        const vh = new Uint8Array(32 + 128 * 16 + ps * 512 + 512);
        const v = new DataView(vh.buffer);
        vh.set(this.header, 0);
        v.setUint32(12, 32 + 128 * 16 + ps * 512 + 512 + vbLen, true);
        v.setUint16(18, ps, true);
        v.setUint16(20, ts, true);
        v.setUint16(22, this.vags.length, true);
        for (let p = 0; p < 128; p++) {
            vh.set(this.programs[p], 32 + p * 16);
        }
        let off = 32 + 128 * 16;
        for (const p of progs) {
            for (const tn of this.tones.get(p)!) {
                let o = off;
                for (const [name, size, signed] of TONE_FIELDS) {
                    if (size === 1) {
                        v.setUint8(o, tn[name]);
                    } else if (signed) {
                        v.setInt16(o, tn[name], true);
                    } else {
                        v.setUint16(o, tn[name], true);
                    }
                    o += size;
                }
                off += 32;
            }
        }
        for (let i = 0; i < this.vags.length; i++) {
            v.setUint16(off + 2 * (i + 1), this.vags[i].length >> 3, true);
        }
        return [vh, vb];
    }
}

const K0 = [0, 60, 115, 98, 122];
const K1 = [0, 0, -52, -55, -60];

// gen_gdn_sounds.decode_vag: a VAB sample body -> int16 samples (to the end flag)
export function decodeVag(b: Uint8Array): number[] {
    const out: number[] = [];
    let o = 0;
    let oo = 0;
    for (let k = 0; k + 15 < b.length; k += 16) {
        const f = Math.min((b[k] >> 4) & 7, 4);
        const r = b[k] & 15;
        for (let i = 0; i < 28; i++) {
            let t = (b[k + 2 + (i >> 1)] >> ((i & 1) * 4)) & 15;
            t = t >= 8 ? t - 16 : t;
            const s = clamp16(((t << 12) >> r) + ((o * K0[f] + oo * K1[f] + 32) >> 6));
            oo = o;
            o = s;
            out.push(s);
        }
        if (b[k + 1] & 1 && !(b[k + 1] & 2)) {
            break;
        }
    }
    return out;
}

// gen_gdn_sounds.centre: the VAB centre note and shift at which `note` plays
// the sample at its own rate (44100 Hz at the centre; the shift raises the
// pitch, in 1/128 semitones read in 1/16 steps, so the centre rounds up)
export function centre(dsp: Dsp, note: number, rate: number, fine = 0): [number, number] {
    const q = roundHalfEven((note + 12 * dsp.log2(44100 / rate) - fine / 128.0) * 16);
    const c = Math.ceil(q / 16);
    return [c, (c * 16 - q) * 8];
}

// gen_gdn_sounds.resample: windowed-sinc lowpass then linear interpolation
export function resample(dsp: Dsp, samples: ArrayLike<number>, rate: number, newRate: number): [number[], number] {
    const n = Math.trunc(samples.length * newRate / rate);
    const cutoff = 0.45 * newRate / rate;
    const win = dsp.hanning(65);
    const h = new Float64Array(65);
    for (let i = 0; i < 65; i++) {
        const k = i - 32;
        h[i] = 2 * cutoff * dsp.sinc(2 * cutoff * k) * win[i];
    }
    const sum = npSum(h);
    for (let i = 0; i < 65; i++) {
        h[i] /= sum;
    }
    const y = npConvolveSame(samples, h);
    const xs = new Float64Array(n);
    for (let i = 0; i < n; i++) {
        xs[i] = i * rate / newRate;
    }
    const idx = new Float64Array(y.length);
    for (let i = 0; i < y.length; i++) {
        idx[i] = i;
    }
    const out = npInterp(xs, idx, y);
    return [Array.from(out, (v) => clamp16(roundHalfEven(v))), newRate];
}

// gen_psp_grunts.active_rms: RMS over the 10 ms frames within 30 dB of the loudest
export function activeRms(dsp: Dsp, x: ArrayLike<number>, rate: number): number {
    const n = Math.max(1, Math.floor(rate / 100));
    const e: number[] = [];
    for (let i = 0; i + n <= x.length; i += n) {
        const f = new Float64Array(n);
        for (let j = 0; j < n; j++) {
            f[j] = x[i + j] * x[i + j];
        }
        e.push(Math.sqrt(npMean(f)));
    }
    if (!e.length) {
        e.push(0);
    }
    const top = Math.max(...e);
    const thr = top * dsp.pow(10, -30 / 20);
    const act = e.filter((v) => v > thr).map((v) => v * v);
    return act.length ? Math.sqrt(npMean(act)) : 0;
}

// gen_psp_grunts.trim: cut silence (below -45 dB of the peak) at both ends,
// a 10 ms fade at the end
export function trim(dsp: Dsp, x: ArrayLike<number>, rate: number, lead = 0.01, tail = 0.04): Float64Array {
    let peak = 0;
    for (let i = 0; i < x.length; i++) {
        peak = Math.max(peak, Math.abs(x[i]));
    }
    const thr = peak * dsp.pow(10, -45 / 20);
    let first = -1;
    let last = -1;
    for (let i = 0; i < x.length; i++) {
        if (Math.abs(x[i]) > thr) {
            if (first < 0) {
                first = i;
            }
            last = i;
        }
    }
    if (first < 0) {
        return Float64Array.from(x);
    }
    const s = Math.max(0, first - Math.trunc(lead * rate));
    const e = Math.min(x.length, last + Math.trunc(tail * rate));
    const y = Float64Array.from({ length: e - s }, (_, i) => x[s + i]);
    const fade = Math.min(y.length, Math.trunc(0.01 * rate));
    if (fade) {
        // np.linspace(1, 0, fade)
        const step = fade > 1 ? -1 / (fade - 1) : 0;
        for (let i = 0; i < fade; i++) {
            const w = i === fade - 1 && fade > 1 ? 0 : i * step + 1;
            y[y.length - fade + i] *= w;
        }
    }
    return y;
}
