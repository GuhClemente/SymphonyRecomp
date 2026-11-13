// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn sound effects renderer (mods/sotn-ultimate/tools/saturn_sfx.py, ported): a
// Saturn sound id's SEQ (0.BIN's sound config -> a .CRT bank's SEQ table)
// played on the bank's TON voices by a small SCSP-style software synth.
// See saturn_sfx.py for the formats and the synth's approximations; this
// port keeps numpy's order of operations so it renders the same doubles.
import { npInterp, roundHalfEven, type Dsp } from "./dsp.ts";
import type { IsoFs } from "./disc.ts";

type Files = Pick<IsoFs, "readFile">; // a disc, or the extracted files

const ZERO_BASE = 0x06004080; // load address of 0.BIN
const CFG_ADDR = 0x06041655;
const CFG_FIRST = 0x600;
const CFG_COUNT = 791;
export const SCSP_RATE = 44100;
const MIX_GAIN = 0.5;
const AR_TIMES = [1e9, 1e9, 8100.0, 6900.0, 6000.0, 4800.0, 4000.0, 3400.0, 3000.0,
    2400.0, 2000.0, 1700.0, 1500.0, 1200.0, 1000.0, 860.0, 760.0,
    600.0, 500.0, 430.0, 380.0, 300.0, 250.0, 220.0, 190.0, 150.0,
    130.0, 110.0, 95.0, 76.0, 63.0, 55.0, 47.0, 38.0, 31.0, 27.0,
    24.0, 19.0, 15.0, 13.0, 12.0, 9.4, 7.9, 6.8, 6.0, 4.7, 3.8, 3.4,
    3.0, 2.4, 2.0, 1.8, 1.6, 1.3, 1.1, 0.93, 0.85, 0.65, 0.53, 0.44,
    0.40, 0.35, 0.0, 0.0];
const DR_TIMES = [1e9, 1e9, 118200.0, 101300.0, 88600.0, 70900.0, 59100.0,
    50700.0, 44300.0, 35500.0, 29600.0, 25300.0, 22200.0, 17700.0,
    14800.0, 12700.0, 11100.0, 8900.0, 7400.0, 6300.0, 5500.0, 4400.0,
    3700.0, 3200.0, 2800.0, 2200.0, 1800.0, 1600.0, 1400.0, 1100.0,
    920.0, 790.0, 690.0, 550.0, 460.0, 390.0, 340.0, 270.0, 230.0,
    200.0, 170.0, 140.0, 110.0, 98.0, 85.0, 68.0, 57.0, 49.0, 43.0,
    34.0, 28.0, 25.0, 22.0, 18.0, 14.0, 12.0, 11.0, 8.5, 7.1, 6.1,
    5.4, 4.3, 3.6, 3.1];
const EG_RANGE_DB = 96.0;

const u16 = (d: Uint8Array, o: number): number => (d[o] << 8) | d[o + 1];
const u32 = (d: Uint8Array, o: number): number => ((d[o] << 24) >>> 0) + ((d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]);

// (table type, index) of a Saturn sound id (0.BIN g_SaturnSoundConfigs)
function soundConfig(zero: Uint8Array, sid: number): [number, number] {
    if (sid < CFG_FIRST || sid >= CFG_FIRST + CFG_COUNT) {
        throw new Error("Saturn sound id out of range: " + sid.toString(16));
    }
    const o = CFG_ADDR - ZERO_BASE + 5 * (sid - CFG_FIRST);
    return [zero[o], zero[o + 3]];
}

class Layer {
    lo: number; hi: number; loop: number; pcm8: number; sa: number; lsa: number; lea: number;
    ar: number; d1r: number; d2r: number; rr: number; dl: number; krs: number; tl: number;
    disdl: number; dipan: number; base: number; fine: number; vlIndex: number;
    samples: Float64Array;

    constructor(bank: ToneBank, o: number) {
        const d = bank.data;
        this.lo = d[o];
        this.hi = d[o + 1];
        const r0 = u16(d, o + 2);
        this.loop = (r0 >> 5) & 3;
        this.pcm8 = (r0 >> 4) & 1;
        this.sa = ((r0 & 0xF) << 16) | u16(d, o + 4);
        this.lsa = u16(d, o + 6);
        this.lea = u16(d, o + 8);
        const a1 = u16(d, o + 10);
        const a2 = u16(d, o + 12);
        this.ar = a1 & 0x1F;
        this.d1r = (a1 >> 6) & 0x1F;
        this.d2r = (a1 >> 11) & 0x1F;
        this.rr = a2 & 0x1F;
        this.dl = (a2 >> 5) & 0x1F;
        this.krs = (a2 >> 10) & 0xF;
        this.tl = d[o + 15];
        this.disdl = d[o + 24] >> 5;
        this.dipan = d[o + 24] & 0x1F;
        this.base = d[o + 25];
        this.fine = (d[o + 26] << 24) >> 24;
        this.vlIndex = d[o + 29];
        // sample data, relative to the tone bank start
        const n = this.lea + 1;
        const so = bank.start + this.sa;
        if (this.pcm8) {
            const raw = d.subarray(so, Math.min(d.length, so + n));
            this.samples = Float64Array.from(raw, (b) => ((b << 24) >> 24) / 128.0);
        } else {
            const end = Math.min(d.length, so + 2 * n);
            const cnt = Math.max(0, Math.floor((end - so) / 2));
            this.samples = Float64Array.from({ length: cnt }, (_, i) => ((u16(d, so + 2 * i) << 16) >> 16) / 32768.0);
        }
    }
}

type Voice = [Uint8Array, Layer[]]; // 4-byte voice header, layers

class ToneBank {
    data: Uint8Array;
    start: number;
    name: string;
    vlTables: Uint8Array[] = [];
    voices: Voice[] = [];

    constructor(data: Uint8Array, start: number, name: string) {
        this.data = data;
        this.start = start;
        this.name = name;
        const d = data;
        const [mix, vl, peg] = [0, 1, 2].map((i) => u16(d, start + 2 * i));
        const nv = Math.floor((mix - 8) / 2);
        for (let o = vl; o < peg; o += 10) {
            this.vlTables.push(d.subarray(start + o, start + o + 10));
        }
        for (let v = 0; v < nv; v++) {
            const vo = start + u16(d, start + 8 + 2 * v);
            const hdr = d.subarray(vo, vo + 4);
            const layers: Layer[] = [];
            for (let i = 0; i < hdr[2] + 1; i++) {
                layers.push(new Layer(this, vo + 4 + 32 * i));
            }
            this.voices.push([hdr, layers]);
        }
    }

    velLevel(idx: number, vel: number): number {
        const t = idx < this.vlTables.length ? this.vlTables[idx] : null;
        if (!t) {
            return vel;
        }
        return npInterp([vel], [0, t[4], t[7], 127], [t[2], t[5], t[8], 127])[0];
    }
}

function findToneBank(d: Uint8Array): number {
    for (let tb = 0; tb < d.length - 64; tb += 2) {
        const [mix, vl, peg, plfo] = [0, 1, 2, 3].map((i) => u16(d, tb + 2 * i));
        if (!(8 < mix && mix < vl && vl <= peg && peg <= plfo && plfo < 0x2000) || mix % 2) {
            continue;
        }
        const nv = Math.floor((mix - 8) / 2);
        const offs = Array.from({ length: nv }, (_, i) => u16(d, tb + 8 + 2 * i));
        if (nv < 1 || offs[0] < plfo) {
            continue;
        }
        let ok = true;
        for (let i = 0; i < nv - 1; i++) {
            if (offs[i + 1] - offs[i] !== 4 + 32 * (d[tb + offs[i] + 2] + 1)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return tb;
        }
    }
    throw new Error("no tone bank");
}

// a .CRT sound bank file: SEQ tables + one tone bank
class Crt {
    name: string;
    data: Uint8Array;
    tables = new Map<string, [number, number[]]>();
    tones: ToneBank;

    constructor(name: string, data: Uint8Array) {
        this.name = name;
        this.data = data;
        const d = data;
        this.tables.set("A", [0, Array.from({ length: u16(d, 0) }, (_, i) => u32(d, 2 + 4 * i))]);
        const tb = findToneBank(d);
        if (tb > 0x800 && u16(d, 0x800) && u32(d, 0x802) === 2 + 4 * u16(d, 0x800)) {
            this.tables.set("B", [0x800, Array.from({ length: u16(d, 0x800) }, (_, i) => u32(d, 0x802 + 4 * i))]);
        }
        this.tones = new ToneBank(d, tb, name);
    }

    seq(table: string, idx: number): number | null {
        const [base, offs] = this.tables.get(table)!;
        return idx < offs.length ? base + offs[idx] : null;
    }
}

// the banks loaded while a stage plays (saturn_sfx.Context)
export class SfxContext {
    zero: Uint8Array;
    common: Crt;
    player: Crt;
    stage: Crt;
    toneBanks: Map<number, ToneBank>;

    private constructor(zero: Uint8Array, common: Crt, player: Crt, stage: Crt) {
        this.zero = zero;
        this.common = common;
        this.player = player;
        this.stage = stage;
        this.toneBanks = new Map([[0, common.tones], [1, player.tones], [2, stage.tones]]);
    }

    static async open(saturn: Files, stage = "SD_TEI.CRT", player = "SD_ALK.CRT",
                      common = "SD_JO10.CRT"): Promise<SfxContext> {
        const load = async (n: string) => new Crt(n, await saturn.readFile("SD/" + n));
        return new SfxContext(await saturn.readFile("0.BIN"), await load(common), await load(player), await load(stage));
    }

    locate(sid: number): [Crt, number | null] {
        const [typ, idx] = soundConfig(this.zero, sid);
        const [c, table] = ([[this.common, "A"], [this.player, "A"], [this.stage, "A"], [this.stage, "B"]] as [Crt, string][])[typ];
        if (!c.tables.has(table)) {
            return [c, null];
        }
        return [c, c.seq(table, idx)];
    }
}

// ---------------------------------------------------------------- SEQ
const GATE_EXT: Record<number, number> = { 0x88: 0x200, 0x89: 0x800, 0x8A: 0x1000, 0x8B: 0x2000 };
const STEP_EXT: Record<number, number> = { 0x8C: 0x100, 0x8D: 0x200, 0x8E: 0x800, 0x8F: 0x1000 };

type SeqEvent =
    | { t: number; kind: "note"; ch: number; key: number; vel: number; gate: number }
    | { t: number; kind: "cc"; ch: number; num: number; val: number }
    | { t: number; kind: "prog"; ch: number; prog: number }
    | { t: number; kind: "bend"; ch: number; val: number };

class Seq {
    resolution: number;
    tempos: [number, number][];
    events: SeqEvent[] = [];

    constructor(d: Uint8Array, s: number) {
        this.resolution = u16(d, s);
        const nt = u16(d, s + 2);
        const dataOff = u16(d, s + 4);
        this.tempos = Array.from({ length: nt }, (_, i) => [u32(d, s + 8 + 8 * i), u32(d, s + 12 + 8 * i)]);
        this.parse(d, s + dataOff);
    }

    private parse(d: Uint8Array, base: number): void {
        let t = 0;
        let gateExt = 0;
        let p = base;
        let ret: [number, number] | null = null; // (return position, events left) in a reference
        for (;;) {
            if (ret !== null && ret[1] === 0) {
                p = ret[0];
                ret = null;
            }
            const b = d[p];
            if (ret !== null) {
                ret = [ret[0], ret[1] - 1];
            }
            if (b < 0x80) {
                const gate = d[p + 3] + gateExt + (b & 0x40 ? 0x100 : 0);
                const step = d[p + 4] + (b & 0x20 ? 0x100 : 0);
                gateExt = 0;
                t += step;
                this.events.push({ t, kind: "note", ch: b & 0xF, key: d[p + 1], vel: d[p + 2], gate });
                p += 5;
            } else if (b === 0x81) {
                const ref = u16(d, p + 1);
                ret = [p + 4, d[p + 3]];
                p = base + ref;
            } else if (b === 0x83) {
                break;
            } else if (b in GATE_EXT) {
                gateExt += GATE_EXT[b];
                p += 1;
            } else if (b in STEP_EXT) {
                t += STEP_EXT[b];
                p += 1;
            } else if (b >= 0xA0 && b < 0xB0) {
                t += d[p + 3];
                p += 4;
            } else if (b >= 0xB0 && b < 0xC0) {
                t += d[p + 3];
                this.events.push({ t, kind: "cc", ch: b & 0xF, num: d[p + 1], val: d[p + 2] });
                p += 4;
            } else if (b >= 0xC0 && b < 0xD0) {
                t += d[p + 2];
                this.events.push({ t, kind: "prog", ch: b & 0xF, prog: d[p + 1] });
                p += 3;
            } else if (b >= 0xD0 && b < 0xE0) {
                t += d[p + 2];
                p += 3;
            } else if (b >= 0xE0 && b < 0xF0) {
                t += d[p + 2];
                this.events.push({ t, kind: "bend", ch: b & 0xF, val: d[p + 1] });
                p += 3;
            } else {
                break; // an unknown event: stopped (as saturn_sfx.py)
            }
        }
    }

    // tick -> seconds through the tempo events
    seconds(tick: number): number {
        let sec = 0.0;
        let t0 = 0;
        const tempos = this.tempos.length ? this.tempos : [[0, 500000]];
        for (let i = 0; i < tempos.length; i++) {
            const [steps, usec] = tempos[i];
            const span = i === tempos.length - 1 ? tick - t0 : Math.min(steps, tick - t0);
            sec += span * usec / 1e6 / this.resolution;
            t0 += span;
            if (t0 >= tick) {
                break;
            }
        }
        return sec;
    }
}

// ---------------------------------------------------------------- synth
function egRate(r: number, oct: number, fnsHi: number, krs: number): number {
    if (r === 0) {
        return 0;
    }
    let rate = 2 * r;
    if (krs !== 0xF) {
        rate += oct + 2 * krs + fnsHi;
    }
    return Math.max(0, Math.min(63, rate));
}

// amplitude envelope: key on at 0, key off at nOn, nTotal samples
function envelope(dsp: Dsp, layer: Layer, nOn: number, nTotal: number, sr: number, oct: number, fnsHi: number): Float64Array {
    const env = new Float64Array(nTotal);
    const ar = egRate(layer.ar, oct, fnsHi, layer.krs);
    const d1 = egRate(layer.d1r, oct, fnsHi, layer.krs);
    const d2 = egRate(layer.d2r, oct, fnsHi, layer.krs);
    const rr = egRate(layer.rr, oct, fnsHi, layer.krs);
    let att = 0.0;
    const aStep = AR_TIMES[ar] < 1e8 ? 1.0 / Math.max(1.0, AR_TIMES[ar] * sr / 1000.0) : 0.0;
    if (ar >= 62 || AR_TIMES[ar] === 0.0) {
        att = 1.0;
    }
    const dstep = (rate: number): number => {
        const tm = DR_TIMES[rate];
        return tm >= 1e8 ? 0.0 : EG_RANGE_DB / (tm * sr / 1000.0);
    };
    const s1 = dstep(d1);
    const s2 = dstep(d2);
    const sr_ = dstep(rr);
    const dlDb = layer.dl * EG_RANGE_DB / 32.0;
    let i = 0;
    if (att < 1.0 && aStep > 0) {
        const na = Math.min(nOn, Math.ceil((1.0 - att) / aStep));
        for (let k = 0; k < na; k++) {
            env[k] = att + aStep * k;
        }
        att = Math.min(1.0, att + aStep * na);
        i = na;
    } else if (att < 1.0) {
        return env; // never attacks
    }
    if (i < nOn) {
        const n = nOn - i;
        let n1 = s1 > 0 ? (dlDb > 0 ? Math.ceil(dlDb / s1) : 0) : n;
        n1 = Math.min(n1, n);
        const lvl = s1 > 0 ? s1 * n1 : 0.0;
        for (let k = 0; k < n; k++) {
            let c = k < n1 ? s1 * k : lvl + s2 * (k - n1);
            c = Math.min(c, EG_RANGE_DB);
            env[i + k] = att * dsp.pow(10, -c / 20.0);
        }
        i = nOn;
    }
    if (i < nTotal) {
        const start = i > 0 ? env[i - 1] : att;
        const startDb = -20 * dsp.log10(Math.max(start, 1e-9));
        const n = nTotal - i;
        for (let k = 0; k < n; k++) {
            if (sr_ > 0) {
                const c = Math.min(startDb + sr_ * (k + 1), EG_RANGE_DB);
                env[i + k] = c >= EG_RANGE_DB ? 0.0 : dsp.pow(10, -c / 20.0);
            } else {
                env[i + k] = start;
            }
        }
    }
    return env;
}

type StepFn = (t: number) => number;

// [(time, value)] -> step function of time (saturn_sfx.piecewise)
function piecewise(points: [number, number][], dflt: number): StepFn {
    const pts = [...points].sort((a, b) => a[0] - b[0] || a[1] - b[1]);
    return (t) => {
        let v = dflt;
        for (const [tm, x] of pts) {
            if (t >= tm - 1e-12) {
                v = x;
            }
        }
        return v;
    };
}

// sample-domain voice: [start sample, samples]
function renderNote(dsp: Dsp, layer: Layer, sr: number, tOn: number, tOff: number,
                    pitch: StepFn, tail: number): [number, Float64Array] {
    const i0 = roundHalfEven(tOn * sr);
    const nOn = Math.max(1, roundHalfEven((tOff - tOn) * sr));
    const nMax = nOn + Math.trunc(tail * sr);
    const semis = new Float64Array(nMax);
    const pos = new Float64Array(nMax);
    let acc = 0;
    for (let k = 0; k < nMax; k++) {
        semis[k] = pitch(tOn + k / sr);
        pos[k] = acc;
        acc += SCSP_RATE * dsp.pow(2.0, semis[k] / 12.0) / sr;
    }
    const s = layer.samples;
    const n = s.length;
    let len = nMax;
    if (layer.loop === 0) {
        for (let k = 0; k < nMax; k++) {
            if (!(pos[k] < n - 1 + 1e-9)) {
                len = k;
                break;
            }
        }
    } else {
        const ls = Math.min(layer.lsa, n - 1);
        const span = Math.max(1, (n - 1) - ls);
        for (let k = 0; k < nMax; k++) {
            if (pos[k] > ls) {
                if (layer.loop === 1 || layer.loop === 2) {
                    pos[k] = ls + (pos[k] - ls) % span;
                } else {
                    const q = (pos[k] - ls) % (2 * span);
                    pos[k] = ls + (q < span ? q : 2 * span - q);
                }
            }
        }
    }
    if (len === 0) {
        return [i0, new Float64Array(0)];
    }
    const out = new Float64Array(len);
    const clip = (i: number): number => Math.min(Math.max(i, 0), n - 1);
    for (let k = 0; k < len; k++) {
        const ip = Math.floor(pos[k]);
        const fr = pos[k] - ip;
        const a = s[clip(ip)];
        const b = s[clip(ip + 1)];
        out[k] = a + (b - a) * fr;
    }
    // key scaling inputs from the start pitch: ratio = 2^oct * (1 + fns/1024)
    const ratio = SCSP_RATE * dsp.pow(2, semis[0] / 12.0) / SCSP_RATE;
    const oct = Math.floor(dsp.log2(ratio));
    const fns = Math.trunc((ratio / Math.pow(2, oct) - 1) * 1024);
    const env = envelope(dsp, layer, Math.min(nOn, len), len, sr, oct, (fns >> 9) & 1);
    for (let k = 0; k < len; k++) {
        out[k] *= env[k];
    }
    return [i0, out];
}

interface Note {
    t: number; off: number; ch: number; key: number; vel: number; bank: number; prog: number;
}

interface Chan {
    bank: number; prog: number;
    vol: [number, number][]; expr: [number, number][]; pan: [number, number][]; bend: [number, number][];
}

export interface Render {
    ch: Float64Array[]; // one (mono) or two (left, right) channels
    rate: number;
    empty: boolean;
}

// saturn_sfx.render: Saturn sound id -> samples (floats, full scale 1.0) at
// the highest note playback rate (rounded up), at most maxRate
export function render(dsp: Dsp, ctx: SfxContext, sid: number, maxRate = SCSP_RATE, tail = 2.0): Render {
    const [c, off] = ctx.locate(sid);
    const empty: Render = { ch: [new Float64Array(0)], rate: SCSP_RATE, empty: true };
    if (off === null) {
        return empty;
    }
    const seq = new Seq(c.data, off);
    const chan: Chan[] = Array.from({ length: 16 }, () => ({
        bank: 0, prog: 0, vol: [[0.0, 127]], expr: [[0.0, 127]], pan: [[0.0, 64]], bend: [[0.0, 0x40]],
    }));
    const notes: Note[] = [];
    for (const e of seq.events) {
        const st = chan[e.ch];
        const ts = seq.seconds(e.t);
        if (e.kind === "cc") {
            if (e.num === 32) {
                st.bank = e.val;
            } else if (e.num === 7) {
                st.vol.push([ts, e.val]);
            } else if (e.num === 11) {
                st.expr.push([ts, e.val]);
            } else if (e.num === 10) {
                st.pan.push([ts, e.val]);
            }
        } else if (e.kind === "prog") {
            st.prog = e.prog;
        } else if (e.kind === "bend") {
            st.bend.push([ts, e.val]);
        } else {
            notes.push({ t: ts, off: seq.seconds(e.t + e.gate), ch: e.ch, key: e.key, vel: e.vel,
                         bank: st.bank, prog: st.prog });
        }
    }
    // later controller changes also apply to notes already playing: each
    // note uses its channel's whole lists
    const voices: { n: Note; tb: ToneBank; ly: Layer; pitch: StepFn; eff: number }[] = [];
    for (const n of notes) {
        const tb = ctx.toneBanks.get(n.bank);
        if (!tb || n.prog >= tb.voices.length) {
            continue;
        }
        const [hdr, layers] = tb.voices[n.prog];
        const ly = layers.find((l) => l.lo <= n.key && n.key <= l.hi);
        if (!ly) {
            continue;
        }
        const rng = hdr[0] & 0xF;
        const base = ly.base - ly.fine / 128.0;
        const bendf = piecewise(chan[n.ch].bend, 0x40);
        const pitch: StepFn = (t) => n.key - base + (bendf(t) - 0x40) / 64.0 * rng;
        voices.push({ n, tb, ly, pitch, eff: SCSP_RATE * dsp.pow(2, pitch(n.t) / 12.0) });
    }
    if (!voices.length) {
        return empty;
    }
    const rate = Math.ceil(Math.min(maxRate, SCSP_RATE, Math.max(...voices.map((v) => v.eff))));
    let stereo = false;
    const rendered: [number, Float64Array, Float64Array, Float64Array][] = [];
    for (const { n, tb, ly, pitch } of voices) {
        const [i0, wav] = renderNote(dsp, ly, rate, n.t, n.off, pitch, tail);
        if (!wav.length) {
            continue;
        }
        const st = chan[n.ch];
        const vl = tb.velLevel(ly.vlIndex, n.vel);
        const volf = piecewise(st.vol, 127);
        const exprf = piecewise(st.expr, 127);
        const panf = piecewise(st.pan, 64);
        const tlGain = dsp.pow(10, -ly.tl * 0.375 / 20.0);
        const disdl = ly.disdl === 0 ? 0.0 : Math.pow(2.0, ly.disdl - 7);
        let att = 1;
        if (ly.dipan & 0xF) {
            att = (ly.dipan & 0xF) !== 0xF ? dsp.pow(10, -3 * (ly.dipan & 0xF) / 20.0) : 0.0;
        }
        const w = new Float64Array(wav.length);
        const lg = new Float64Array(wav.length);
        const rg = new Float64Array(wav.length);
        for (let k = 0; k < wav.length; k++) {
            const t = n.t + k / rate;
            const vol = volf(t);
            const expr = exprf(t);
            let g = dsp.pow(10, -0.375 * ((127 - vl) + (127 - vol) + (127 - expr)) / 20.0);
            if (vol <= 0) {
                g = 0.0;
            }
            g = g * MIX_GAIN * tlGain * disdl;
            w[k] = wav[k] * g;
            const pan = panf(t);
            let l = Math.min(Math.max((127 - pan) / 63.0, 0), 1);
            let r = Math.min(Math.max(pan / 64.0, 0), 1);
            if (ly.dipan & 0xF) {
                if (ly.dipan & 0x10) {
                    r = r * att;
                } else {
                    l = l * att;
                }
            }
            lg[k] = l;
            rg[k] = r;
            if (Math.abs(l - r) > 1e-6) {
                stereo = true;
            }
        }
        rendered.push([i0, w, lg, rg]);
    }
    const total = Math.max(...rendered.map(([i0, w]) => i0 + w.length));
    const L = new Float64Array(total);
    const R = new Float64Array(total);
    for (const [i0, w, lg, rg] of rendered) {
        for (let k = 0; k < w.length; k++) {
            L[i0 + k] += w[k] * lg[k];
            R[i0 + k] += w[k] * rg[k];
        }
    }
    return { ch: stereo ? [L, R] : [L], rate, empty: false };
}

// saturn_sfx.trim: drop the trailing silence (below -60 dBFS)
export function trim(dsp: Dsp, x: Float64Array[]): Float64Array[] {
    const thr = dsp.pow(10, -60.0 / 20.0);
    let last = -1;
    for (let k = 0; k < x[0].length; k++) {
        let a = 0;
        for (const c of x) {
            a = Math.max(a, Math.abs(c[k]));
        }
        if (a > thr) {
            last = k;
        }
    }
    return x.map((c) => c.slice(0, last + 1));
}

// the "seq" sounds' path: trimmed, a stereo render averaged to mono, then
// saturn_sfx.to_pcm16 (scaled down if over full scale)
export function toPcm16Mono(dsp: Dsp, r: Render): Int16Array {
    let x = trim(dsp, r.ch);
    if (x.length === 2) {
        x = [Float64Array.from(x[0], (v, k) => (v + x[1][k]) / 2)];
    }
    const m = x[0];
    let peak = 0;
    for (const v of m) {
        peak = Math.max(peak, Math.abs(v));
    }
    let scale = 1.0;
    if (peak > 32767 / 32768) {
        scale = (32767 / 32768) / peak;
    }
    return Int16Array.from(m, (v) => Math.min(32767, Math.max(-32768, roundHalfEven(v * scale * 32768))));
}
