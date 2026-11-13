// SPDX-License-Identifier: AGPL-3.0-or-later
// The PSP Maria's voice (mods/sotn-ultimate/tools/gen_mar2_sounds.py and the upstream
// tools/maria-sfx `build`, ported): her 16 MARY2* clips of sd_mar2.spk
// (entries 10..25), silence trimmed, resampled to 8 kHz and encoded to PS1
// ADPCM by maria-sfx's own (integer) encoder, in the vab 1 slots Richter's
// 16 voice ids name; vab 2 = the retail SD_MAR padded to vab2Len. Same
// layout as MARIA_SD.BIN (SD_RIH.VH 3 sectors, VB 29, SD_MAR.VH 2, VB).
import { spkEntries } from "./grunts.ts";
import type { SfxCfg } from "./mar_sounds.ts";
import { centre, Vab, type Tone } from "./vab.ts";
import type { Dsp } from "./dsp.ts";

const VB_LEN: Record<number, number> = { 1: 0xE190, 2: 0xFBF0 };
const VH_LEN: Record<number, number> = { 1: 0x1020, 2: 0xE20 };
const LAYOUT_SECTORS = [3, 29, 2];
const SECTOR = 2048;
const RATE = 8000; // maria-sfx's TARGET_RATE
const VOICE_FIRST = 10;
const VOICE_COUNT = 16;
// her ids 0x8E6 + i -> the PS1 id whose slot holds MARY2 clip i
export const MAR2_SLOTS = [0x6F9, 0x6FB, 0x6FA, 0x6FC, 0x6FD, 0x6FE, 0x700, 0x701,
    0x705, 0x706, 0x707, 0x708, 0x702, 0x703, 0x704, 0x6FF];
const ADSR = [0x80FF, 0x5FC0];
const FILTERS: [number, number][] = [[0, 0], [60, 0], [115, -52], [98, -55], [122, -60]];

const clamp16 = (x: number): number => Math.min(32767, Math.max(-32768, x));

// maria-sfx decode_adpcm (to the first end flag, that block included)
function decodeAdpcm(data: Uint8Array): number[] {
    let h1 = 0;
    let h2 = 0;
    const pcm: number[] = [];
    for (let k = 0; k + 16 <= data.length; k += 16) {
        const filter = data[k] >> 4;
        const shift = data[k] & 15;
        if (filter >= FILTERS.length || shift > 12) {
            throw new Error("maria-sfx: invalid ADPCM block header");
        }
        const scale = 1 << (12 - shift);
        const [f0, f1] = FILTERS[filter];
        for (let i = 0; i < 28; i++) {
            const b = data[k + 2 + (i >> 1)];
            const n = i & 1 ? b >> 4 : b & 15;
            const q = n & 8 ? n - 16 : n;
            const pred = ((h1 * f0) >> 6) + ((h2 * f1) >> 6);
            const s = clamp16(q * scale + pred);
            pcm.push(s);
            h2 = h1;
            h1 = s;
        }
        if (data[k + 1] & 1) {
            break;
        }
    }
    return pcm;
}

// i16::abs in a Rust release build (wraps: |-32768| stays -32768)
const abs16 = (x: number): number => (x === -32768 ? -32768 : Math.abs(x));

// maria-sfx trim_silence: 20 ms kept around the samples above 64
function trimSilence(x: number[], rate: number): number[] {
    const margin = Math.floor(rate / 50);
    let first = x.findIndex((v) => abs16(v) > 64);
    if (first < 0) {
        first = 0;
    }
    let last = x.length;
    for (let i = x.length - 1; i >= 0; i--) {
        if (abs16(x[i]) > 64) {
            last = i + 1;
            break;
        }
    }
    const start = Math.max(0, first - margin);
    const end = Math.min(last + margin, x.length);
    return x.slice(start, Math.max(end, start));
}

// maria-sfx resample_linear (integer, rounded)
function resampleLinear(x: number[], src: number, dst: number): number[] {
    if (!x.length || src === dst) {
        return x.slice();
    }
    const len = Math.floor((x.length * dst + Math.floor(src / 2)) / src);
    const out: number[] = [];
    for (let i = 0; i < len; i++) {
        const pos = i * src;
        const a = Math.floor(pos / dst);
        const frac = pos % dst;
        const p = x[Math.min(a, x.length - 1)];
        const q = x[Math.min(a + 1, x.length - 1)];
        out.push(clamp16(p + Math.trunc(((q - p) * frac + Math.floor(dst / 2)) / dst)));
    }
    return out;
}

function divRound(n: number, d: number): number {
    return n >= 0 ? Math.trunc((n + Math.trunc(d / 2)) / d) : Math.trunc((n - Math.trunc(d / 2)) / d);
}

// maria-sfx encode_adpcm: per 28-sample block the filter / shift with the
// least squared error; the last block carries the end flag
function encodeAdpcm(pcm: number[]): Uint8Array {
    const blocks = Math.max(1, Math.ceil(pcm.length / 28));
    const out = new Uint8Array(blocks * 16);
    let hist1 = 0;
    let hist2 = 0;
    const src = new Int32Array(28);
    const qs = new Int32Array(28);
    const bestQs = new Int32Array(28);
    for (let bi = 0; bi < blocks; bi++) {
        src.fill(0);
        for (let j = 0; j < 28 && bi * 28 + j < pcm.length; j++) {
            src[j] = pcm[bi * 28 + j];
        }
        let bestErr = Infinity;
        let bestF = 0;
        let bestShift = 0;
        let bestH1 = hist1;
        let bestH2 = hist2;
        for (let fi = 0; fi < FILTERS.length; fi++) {
            const [f0, f1] = FILTERS[fi];
            for (let shift = 0; shift <= 12; shift++) {
                const scale = 1 << (12 - shift);
                let h1 = hist1;
                let h2 = hist2;
                let err = 0;
                for (let j = 0; j < 28; j++) {
                    const pred = ((h1 * f0) >> 6) + ((h2 * f1) >> 6);
                    const q = Math.min(7, Math.max(-8, divRound(src[j] - pred, scale)));
                    const rec = clamp16(pred + q * scale);
                    const delta = src[j] - rec;
                    err += delta * delta;
                    qs[j] = q;
                    h2 = h1;
                    h1 = rec;
                }
                if (err < bestErr) {
                    bestErr = err;
                    bestF = fi;
                    bestShift = shift;
                    bestQs.set(qs);
                    bestH1 = h1;
                    bestH2 = h2;
                }
            }
        }
        const o = bi * 16;
        out[o] = (bestF << 4) | bestShift;
        out[o + 1] = bi + 1 === blocks ? 1 : 0;
        for (let j = 0; j < 14; j++) {
            out[o + 2 + j] = (bestQs[j * 2] & 15) | ((bestQs[j * 2 + 1] & 15) << 4);
        }
        hist1 = bestH1;
        hist2 = bestH2;
    }
    return out;
}

// maria-sfx build: the 16 voice clips of sd_mar2.spk as PS1 ADPCM bodies
export function mar2Clips(spk: Uint8Array): Uint8Array[] {
    const e = spkEntries(spk);
    if (e.length !== 26) {
        throw new Error("sd_mar2 should have 26 descriptors, found " + e.length);
    }
    const clips = e.slice(VOICE_FIRST, VOICE_FIRST + VOICE_COUNT).map((s) =>
        encodeAdpcm(resampleLinear(trimSilence(decodeAdpcm(s.body), s.rate), s.rate, RATE)));
    if (clips.reduce((n, c) => n + c.length, 0) > VB_LEN[1]) {
        throw new Error("the PSP Maria's clips do not fit vab 1");
    }
    return clips;
}

function finish(vab: Vab, bank: number): [Uint8Array, Uint8Array] {
    let [vh, vb] = vab.write();
    if (vb.length > VB_LEN[bank]) {
        throw new Error("PSP Maria vab " + bank + ": over its size");
    }
    if (vb.length < VB_LEN[bank]) {
        vab.vags.push(new Uint8Array(VB_LEN[bank] - vb.length));
        [vh, vb] = vab.write();
    }
    if (vh.length !== VH_LEN[bank] || vb.length !== VB_LEN[bank]) {
        throw new Error("PSP Maria vab " + bank + ": bank size");
    }
    return [vh, vb];
}

// gen_mar2_sounds.build: MARIA2_SD.BIN (MARIA2_SD_JP.BIN from the Japanese pack)
export async function buildMaria2Sd(dsp: Dsp, spk: Uint8Array, ps1: (p: string) => Promise<Uint8Array>,
                                    cfg: Record<string, SfxCfg>): Promise<Uint8Array> {
    const clips = mar2Clips(spk);
    // vab 1: Richter's bank emptied, her clips in his voice ids' slots
    const vab = new Vab(await ps1("VAB/SD_RIH.VH"), await ps1("VAB/SD_RIH.VB"));
    const template: Tone = { ...vab.tones.get(0)![0] };
    let blank: Tone | undefined;
    for (const tl of vab.tones.values()) {
        blank ??= tl.find((t) => t.vol === 0);
    }
    for (const p of vab.tones.keys()) {
        vab.tones.set(p, Array.from({ length: 16 }, () => ({ ...blank!, prog: p, vag: 1 })));
    }
    const silent = new Uint8Array(48);
    silent[17] = 1;
    silent[33] = 7;
    silent.fill(0x77, 34);
    vab.vags = [silent];
    clips.forEach((body, i) => {
        const e = cfg[MAR2_SLOTS[i].toString(16).toUpperCase()];
        const tl = vab.tones.get(e.prog)!;
        if (e.vabid !== 1 || tl[e.tone].vag !== 1) {
            throw new Error("PSP Maria clip " + i + ": slot");
        }
        vab.vags.push(body);
        const [c, f] = centre(dsp, e.note, RATE);
        tl[e.tone] = { ...template, prog: e.prog, vag: vab.vags.length, vol: 127, pan: 64,
                       center: c, shift: f, min: 0, max: 127, adsr1: ADSR[0], adsr2: ADSR[1] };
    });
    const [vh1, vb1] = finish(vab, 1);
    // vab 2: the retail prologue Maria bank, padded to vab2Len
    const [vh2, vb2] = finish(new Vab(await ps1("VAB/SD_MAR.VH"), await ps1("VAB/SD_MAR.VB")), 2);
    const parts = [vh1, vb1, vh2, vb2];
    const sizes = parts.map((p, i) => (i < 3 ? LAYOUT_SECTORS[i] : Math.ceil(p.length / SECTOR)) * SECTOR);
    const data = new Uint8Array(sizes.reduce((s, n) => s + n, 0));
    let o = 0;
    parts.forEach((p, i) => {
        data.set(p, o);
        o += sizes[i];
    });
    return data;
}
