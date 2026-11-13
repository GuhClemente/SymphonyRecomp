// SPDX-License-Identifier: AGPL-3.0-or-later
// The PSP dub's player grunts (mods/sotn-ultimate/tools/gen_psp_grunts.py, ported): each
// retail voiced tone pair of Alucard's (VAB/SD_ALK) and Richter's
// (VAB/SD_RIH) bank gets the PSP clip of its slot (res/ps/snd_e/SE/sd_alk.spk,
// sd_rih.spk; the JP dub: res/ps/sound/SE), 8 kHz, silence trimmed, one
// gain to the retail loudness. Same programs / tones / volumes; only the
// sample and the centre note change, and the VB keeps its retail length.
import { satxa, makeWav, type AudioModule } from "./audio.ts";
import { clamp16, npMedian, roundHalfEven, type Dsp } from "./dsp.ts";
import { activeRms, centre, decodeVag, resample, trim, Vab, type Tone } from "./vab.ts";

export const GRUNT_RATE = 8000; // the PSP clips' rate
const GAIN_MAX = 1.0;

// (prog, tone) of the retail voiced tone pair -> [spk entry, PSP name]
export const GRUNT_BANKS: Record<string, { vab: string; spk: string; table: [number, number, number, string][] }> = {
    ALK: {
        vab: "VAB/SD_ALK", spk: "sd_alk.spk", table: [
            [0, 0, 0, "ai_dm1"], [0, 2, 2, "ai_dm2"], [0, 4, 4, "ai_dm3"], [0, 6, 6, "ai_dm4"],
            [0, 8, 8, "ai_dm5"], [0, 12, 12, "ai_dm7"], [0, 14, 14, "al_fun"], [1, 0, 16, "al_ha"],
            [1, 2, 18, "al_tei"], [1, 4, 20, "al_tou"], [1, 6, 22, "ALKO1V"], [1, 8, 24, "ALKD1I"],
            [1, 10, 26, "ALKD1J"], [1, 14, 30, "alk2a"],
        ],
    },
    RIH: {
        vab: "VAB/SD_RIH", spk: "sd_rih.spk", table: [
            [0, 0, 0, "LIHK1a"], [0, 2, 2, "LIHK1"], [0, 4, 4, "LIHK1C"], [0, 6, 6, "LIHK1D"],
            [0, 12, 10, "LIHK1M"], [0, 14, 12, "LIHK1H"], [1, 0, 14, "LIHK1I"], [1, 2, 16, "LIHK1J"],
            [1, 4, 18, "LIHK1K"], [1, 6, 20, "LIHK1L"],
        ],
    },
};

export interface SpkEntry {
    name: string;
    rate: number;
    body: Uint8Array;
}

// a SEPK pack: [name, rate, VAG body] per entry (SEPK1.20; sd_togi: 1.10)
export function spkEntries(d: Uint8Array): SpkEntry[] {
    const tag = String.fromCharCode(...d.subarray(0, 6));
    if (tag !== "SEPK1.") {
        throw new Error("not a SEPK pack");
    }
    const v = new DataView(d.buffer, d.byteOffset, d.byteLength);
    const out: SpkEntry[] = [];
    for (let i = 0; i < v.getUint32(8, true); i++) {
        const off = v.getUint32(0x20 + i * 0x20, true);
        if (String.fromCharCode(...d.subarray(off, off + 4)) !== "VAGp") {
            throw new Error("SEPK entry " + i + ": not a VAGp");
        }
        const size = v.getUint32(off + 12, false);
        const rate = v.getUint32(off + 16, false);
        const raw = d.subarray(off + 0x20, off + 0x30);
        const z = raw.indexOf(0);
        const name = String.fromCharCode(...(z < 0 ? raw : raw.subarray(0, z)));
        out.push({ name, rate, body: d.subarray(off + 0x30, off + 0x30 + size) });
    }
    return out;
}

// the tone's effective centre in 1/128 semitones (the shift raises the pitch)
export function pitch(t: Tone): number {
    return t.center * 128 - t.shift;
}

// gen_psp_grunts.jp_pitch: the JP PS1 tone's pitch for a JP take (the PSP's
// JP takes are the JP PS1 samples, labelled 8000 Hz but played by the JP PS1
// at ~7354 Hz, ~7569 for SD_ALK program 0: centre 6 above the US tones,
// SD_ALK program 0 shift 70)
function jpPitch(key: string, p: number, tone: Tone): number {
    if (tone.shift !== 0) {
        throw new Error(key + " p" + p + ": unexpected retail shift");
    }
    return pitch(tone) + 6 * 128 - (key === "ALK" && p === 0 ? 70 : 0);
}

function setPitch(t: Tone, total: number): void {
    t.center = Math.ceil(Math.trunc(total) / 128);
    t.shift = t.center * 128 - Math.trunc(total);
    if (t.center < 0 || t.center > 127) {
        throw new Error("centre note out of range");
    }
}

// gen_psp_grunts.encode: float samples -> the satxa VAG body
export function encodeVag(a: AudioModule, x: ArrayLike<number>, rate: number): Uint8Array {
    const pcm = Int16Array.from({ length: x.length }, (_, i) => clamp16(roundHalfEven(x[i])));
    return satxa(a, "vag", makeWav(pcm, rate), []);
}

export interface GruntInputs {
    vh: Uint8Array; // the retail VAB/SD_<key>.VH
    vb: Uint8Array;
    spk: Uint8Array; // the PSP pack (snd_e/SE or sound/SE)
    notes: Record<string, number[]>; // "prog,tone" -> notes the game keys (sfx_config.yaml, vab 1)
}

// gen_psp_grunts.build: -> [vh, vb] and a report line
export function buildGrunts(a: AudioModule, dsp: Dsp, key: string, inp: GruntInputs, jp: boolean): [Uint8Array, Uint8Array, string] {
    const bank = GRUNT_BANKS[key];
    const vab = new Vab(inp.vh, inp.vb);
    const entries = spkEntries(inp.spk);
    const clips = new Map<string, { name: string; psp: Float64Array; note: number; jpp: number | null }>();
    const ratios: number[] = [];
    const sorted = [...bank.table].sort((x, y) => x[0] - y[0] || x[1] - y[1]);
    for (const [p, t, i, name] of sorted) {
        const e0 = entries[i];
        const e1 = entries[i + 1];
        const same = e1.name === e0.name && e1.body.length === e0.body.length && e1.body.every((b, k) => b === e0.body[k]);
        if (!((jp || e0.name.startsWith(name)) && same) || e0.rate !== GRUNT_RATE) {
            throw new Error(key + " p" + p + "t" + t + ": unexpected PSP entry " + e0.name);
        }
        const tl = vab.tones.get(p)!;
        const tone = tl[t];
        const notes = inp.notes[p + "," + t];
        if (!notes || notes.length !== 1) {
            throw new Error(key + " p" + p + "t" + t + ": key note");
        }
        const psp = trim(dsp, decodeVag(e0.body).slice(28), GRUNT_RATE);
        const note = notes[0];
        const rPs1 = 44100 * dsp.pow(2, (note * 128 - pitch(tone)) / 128 / 12);
        const ps1 = decodeVag(vab.vags[tone.vag - 1]).slice(28);
        const ra = activeRms(dsp, ps1, Math.trunc(rPs1));
        const rb = activeRms(dsp, psp, GRUNT_RATE);
        if (ra && rb) {
            ratios.push(ra / rb);
        }
        clips.set(p + "," + t, { name: e0.name, psp, note, jpp: jp ? jpPitch(key, p, tone) : null });
    }
    const gain = Math.min(GAIN_MAX, npMedian(ratios));
    const replaced = new Set([...clips.keys()].map((k) => {
        const [p, t] = k.split(",").map(Number);
        return vab.tones.get(p)![t].vag;
    }));
    let kept = 0;
    vab.vags.forEach((v, i) => {
        if (!replaced.has(i + 1)) {
            kept += v.length;
        }
    });
    const budget = inp.vb.length - kept;
    let rate = GRUNT_RATE;
    let bodies = new Map<string, Uint8Array>();
    let used = 0;
    for (;;) {
        bodies = new Map();
        for (const [k, c] of clips) {
            let x = Float64Array.from(c.psp, (v) => v * gain);
            let peak = 0;
            for (const v of x) {
                peak = Math.max(peak, Math.abs(v));
            }
            if (peak > 32000) {
                x = x.map((v) => v * (32000 / peak));
            }
            if (rate !== GRUNT_RATE) {
                x = Float64Array.from(resample(dsp, x, GRUNT_RATE, rate)[0]);
            }
            bodies.set(k, encodeVag(a, x, rate));
        }
        used = [...bodies.values()].reduce((s, b) => s + b.length, 0);
        if (used <= budget) {
            break;
        }
        rate -= 250;
        if (rate < 6000) {
            throw new Error(key + ": the clips do not fit");
        }
    }
    for (const [k, body] of bodies) {
        const [p, t] = k.split(",").map(Number);
        const tl = vab.tones.get(p)!;
        const tone = tl[t];
        const pair = tl[t + 1];
        vab.vags[tone.vag - 1] = body;
        const { note, jpp } = clips.get(k)!;
        let total: number;
        if (jpp === null) {
            // the PSP clip at its own rate
            const [c, f] = centre(dsp, note, rate);
            total = c * 128 - f;
        } else {
            // a JP take with the JP PS1 tone's pitch, lowered as the clip was
            // resampled from 8000 Hz (gen_psp_grunts: int(round(...)))
            total = jpp + roundHalfEven(1536 * dsp.log2(GRUNT_RATE / rate));
        }
        let d = pitch(pair) - pitch(tone);
        if (jpp !== null && key === "ALK" && p === 1 && t === 14) {
            d -= 2 * 128; // the JP PS1's death line doubles 2 semitones up
        }
        setPitch(tone, total);
        setPitch(pair, total + d);
    }
    let [vh, vb] = vab.write();
    if (vb.length < inp.vb.length) {
        vab.vags.push(new Uint8Array(inp.vb.length - vb.length));
        [vh, vb] = vab.write();
    }
    if (vh.length !== inp.vh.length || vb.length !== inp.vb.length) {
        throw new Error(key + ": bank size changed");
    }
    const report = (jp ? "jp" : "psp") + " grunts " + key + ": " + clips.size + " clips, " + rate +
        " Hz, gain " + gain.toFixed(2) + ", " + used + "/" + budget + " bytes";
    return [vh, vb, report];
}
