// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn Maria's own voice and sounds (mods/sotn-ultimate/tools/gen_mar_sounds.py,
// ported; its docstring has the whole story): her two PS1 banks, laid out as
// SD_RIH.VH/VB + SD_MAR.VH/VB, from the Saturn's SD_MARI.CRT SEQs (rendered
// by src/saturn_sfx.ts) and SDE7.PCM voice clips; the PSP dub's version swaps
// her voice lines for the PSP Maria's English ones (sd_mar2.spk). Seiryuu's
// loop and the dragon's roar are the user's cuts of the 0x72C render, as
// recipes. The build runs this (mods/sotn-ultimate/tools/builder/node/mar_sounds.ts).
import { makeWav, satxa, type AudioModule } from "./audio.ts";
import type { IsoFs } from "./disc.ts";

type Files = Pick<IsoFs, "readFile">; // a disc, or the extracted files
import { npLinspace, roundHalfEven, type Dsp } from "./dsp.ts";
import { spkEntries } from "./grunts.ts";
import { render, SfxContext, toPcm16Mono } from "./saturn_sfx.ts";
import { centre, decodeVag, resample, trim, Vab, type Tone } from "./vab.ts";

const VB_LEN: Record<number, number> = { 1: 0xE190, 2: 0xFBF0 };
const VH_LEN: Record<number, number> = { 1: 0x1020, 2: 0xE20 };
const TEMPLATE: Record<number, string> = { 1: "VAB/SD_RIH", 2: "VAB/SD_MAR" };
const LAYOUT_SECTORS = [3, 29, 2];
const SECTOR = 2048;
// her voice clips: SDE7.PCM through 0.BIN's clip table for group 0x17
const ZERO_BASE = 0x06004080;
const VOX_TABLE = 0x0603A8B4 + 8;
const VOX_FIRST = 0x5CA;
const VOX_TRIM = 3;

type Kind = "seq" | "vox" | "ps1" | "loop" | "roar" | "psp";
// (name, bank, PS1 id whose slot it fills, kind, source, class)
type Sound = [string, number, number, Kind, number | string, string];
export const SOUNDS: Sound[] = [
    ["ATK_A", 1, 0x6F9, "seq", 0x71C, "voice"],
    ["ATK_B", 1, 0x6FA, "seq", 0x71D, "voice"],
    ["ATK_C", 1, 0x6FB, "seq", 0x71E, "voice"],
    ["KICK", 1, 0x6FC, "seq", 0x71F, "voice"],
    ["SHOT", 1, 0x6FD, "seq", 0x728, "sfx"],
    ["FIRE", 1, 0x6FE, "seq", 0x722, "sfx"],
    ["DEATH", 1, 0x6FF, "seq", 0x721, "voice"],
    ["BALL", 1, 0x700, "seq", 0x723, "sfx"],
    ["HURT_A", 1, 0x702, "vox", 0x5D4, "voice"],
    ["HURT_B", 1, 0x703, "vox", 0x5D6, "voice"],
    ["HURT_C", 1, 0x704, "vox", 0x5D7, "voice"],
    ["HIT", 1, 0x705, "ps1", 0x62E, "sfx"], // SD_J010's (the common bank)
    ["LUNGE", 1, 0x6F4, "seq", 0x725, "buzz"],
    ["BEASTS_END", 1, 0x6F5, "seq", 0x727, "sfx"],
    ["GRAB_C", 1, 0x6F6, "vox", 0x5D8, "voice"],
    ["FLAP", 1, 0x6F7, "seq", 0x72E, "sfx"],
    ["SKID", 1, 0x707, "seq", 0x726, "sfx"],
    ["DIVE", 2, 0x87D, "vox", 0x5CA, "voice"],
    ["FOUR", 2, 0x87E, "vox", 0x5CE, "spell"],
    ["CUE", 2, 0x87F, "vox", 0x5CF, "voice"],
    ["SPELL", 2, 0x8A1, "vox", 0x5CC, "spell"],
    ["SPELL_1", 2, 0x8A2, "vox", 0x5D3, "spell"],
    ["SPELL_2", 2, 0x8A3, "vox", 0x5D2, "spell"],
    ["SPELL_3", 2, 0x8A4, "vox", 0x5D1, "spell"],
    ["SPELL_4", 2, 0x8A5, "vox", 0x5D0, "spell"],
    ["GRAB_B", 2, 0x880, "vox", 0x5D5, "voice"],
    ["GENBU", 2, 0x887, "seq", 0x72F, "long"],
    ["BYAKKO_FLAME", 2, 0x888, "seq", 0x72B, "sfx"],
    ["SEIRYU", 2, 0x889, "loop", 0x72C, "seiryu"],
    ["ROAR", 2, 0x88A, "roar", 0x72C, "roar"],
];
// Seiryuu's loop and the dragon's roar: cuts of the 0x72C render at SEIRYU_RATE
const SEIRYU_RATE = 4912;
const LOOP_CUT = [0, 15756];
const LOOP_START = 9156;
const LOOP_XFADE = 0.15;
const ROAR_CUT = [15122, 15122 + 10111];
const ROAR_FADE = [0.010, 0.080];
// the PSP dub's English line for each of her voice sounds
const PSP_VOICE: Record<string, string> = {
    ATK_A: "MARY2A", ATK_B: "MARY2B", ATK_C: "MARY2C",
    KICK: "MARY2D", DEATH: "MARY2W", DIVE: "MARY2E", CUE: "MARY2M",
    HURT_A: "MARY2R", HURT_B: "MARY2T", HURT_C: "MARY2V",
    GRAB_B: "MARY2T", GRAB_C: "MARY2V",
    SPELL_1: "MARY2Q", SPELL_2: "MARY2P", SPELL_3: "MARY2O",
    SPELL_4: "MARY2N",
};
// rate caps tried in order until each bank fits (gen_mar_sounds.STEPS)
type Caps = Record<string, number>;
const FULL: Caps = { sfx: 44100, buzz: 44100, long: 44100, spell: 11025, seiryu: 4912, roar: 3500 };
const STEPS: Caps[] = [
    ...[3500, 3200, 3000, 2756, 2500].map((r) => ({ ...FULL, roar: r })),
    ...[10000, 9000, 8000].map((n) => ({ ...FULL, roar: 2500, spell: n })),
    ...[22050, 16000, 11025].map((f) => ({ ...FULL, roar: 2500, spell: 8000, sfx: f, long: f })),
    ...[22050, 14000].map((b) => ({ ...FULL, roar: 2500, spell: 8000, sfx: 11025, long: 11025, buzz: b })),
    ...([[9000, 4000, 11025], [8000, 3300, 11025], [7000, 3300, 11025], [6000, 3300, 11025],
         [6000, 2756, 10000], [6000, 2756, 9000], [5500, 2756, 8000]] as const).map(([f, r, n]) =>
        ({ spell: n, seiryu: r, sfx: f, long: Math.min(f, 5500), buzz: 14000, roar: 2205 })),
];
const VOICE_PEAK = 29200;
// her attacks' sounds (class "sfx") as the voice: the synth's half-scale
// mix (saturn_sfx.MIX_GAIN) left them 6 dB under the Saturn's balance with
// her voice (the user's pick, 2026-11-07), as far as the headroom allows
const SFX_GAIN = 2.0;
const ADSR = [0x80FF, 0x5FC0];

// sfx_config.yaml entries the banks use (PS1 sound id -> entry)
export interface SfxCfg {
    vabid: number; prog: number; tone: number; note: number; volume: number;
}

export interface MarInputs {
    saturn: Files;
    ps1: (path: string) => Promise<Uint8Array>;
    pspMar2: Uint8Array; // res/ps/snd_e/SE/sd_mar2.spk
    cfg: Record<string, SfxCfg>; // hex PS1 id -> entry
}

interface Pcm {
    pcm: Int16Array | number[];
    rate: number;
}

class Sources {
    private cache = new Map<string, Pcm>();
    private seiryu: number[] | null = null;

    readonly dsp: Dsp;
    readonly ctx: SfxContext;
    readonly zero: Uint8Array;
    readonly sde7: Uint8Array;
    readonly psp: Map<string, { rate: number; body: Uint8Array }>;

    constructor(dsp: Dsp, ctx: SfxContext, zero: Uint8Array, sde7: Uint8Array,
                psp: Map<string, { rate: number; body: Uint8Array }>) {
        this.dsp = dsp;
        this.ctx = ctx;
        this.zero = zero;
        this.sde7 = sde7;
        this.psp = psp;
    }

    // a SDE7.PCM clip, its quiet ends cut
    vox(sid: number): Pcm {
        const o = VOX_TABLE - ZERO_BASE + 12 * (sid - VOX_FIRST);
        const v = new DataView(this.zero.buffer, this.zero.byteOffset);
        const off = v.getUint32(o);
        const ln = v.getUint32(o + 4);
        const rate = v.getUint16(o + 8);
        if (v.getUint16(o + 10) !== 1 || rate !== 11025) {
            throw new Error("Saturn voice clip " + sid.toString(16) + ": unexpected format");
        }
        const s = Array.from(this.sde7.subarray(off, off + ln), (b) => (b << 24) >> 24);
        let first = -1;
        let last = -1;
        s.forEach((x, i) => {
            if (Math.abs(x) > VOX_TRIM) {
                if (first < 0) {
                    first = i;
                }
                last = i;
            }
        });
        return { pcm: s.slice(first, last + 1).map((x) => x * 128), rate };
    }

    pspClip(name: string): Pcm {
        const c = this.psp.get(name)!;
        const x = trim(this.dsp, decodeVag(c.body), c.rate);
        return { pcm: Array.from(x, (v) => roundHalfEven(v)), rate: c.rate };
    }

    seq(sid: number, cap: number): Pcm {
        const r = render(this.dsp, this.ctx, sid, cap);
        if (r.empty) {
            throw new Error("Saturn sound " + sid.toString(16) + ": empty");
        }
        return { pcm: toPcm16Mono(this.dsp, r), rate: r.rate };
    }

    seiryuRender(): number[] {
        if (!this.seiryu) {
            const r = render(this.dsp, this.ctx, 0x72C, SEIRYU_RATE);
            if (r.empty || r.rate !== SEIRYU_RATE) {
                throw new Error("Seiryuu: unexpected render");
            }
            this.seiryu = Array.from(toPcm16Mono(this.dsp, r));
        }
        return this.seiryu;
    }

    // the dragon's roar: ROAR_CUT of the 0x72C render, faded in and out
    roar(): Pcm {
        const x = this.seiryuRender().slice(ROAR_CUT[0], ROAR_CUT[1]);
        if (x.length !== ROAR_CUT[1] - ROAR_CUT[0]) {
            throw new Error("Seiryuu: the render is too short for the roar");
        }
        const fin = Math.trunc(ROAR_FADE[0] * SEIRYU_RATE);
        const fout = Math.trunc(ROAR_FADE[1] * SEIRYU_RATE);
        for (let k = 0; k < fin; k++) {
            x[k] *= 0.5 - 0.5 * this.dsp.cos(Math.PI * k / fin);
        }
        for (let k = 0; k < fout; k++) {
            x[x.length - fout + k] *= 0.5 + 0.5 * this.dsp.cos(Math.PI * (k + 1) / fout);
        }
        return { pcm: x.map((v) => roundHalfEven(v)), rate: SEIRYU_RATE };
    }

    // Seiryuu's stored part: the intro and whole blocks of loop, the loop's
    // last LOOP_XFADE s faded into what comes just before its start
    loop(): Pcm {
        const pcm = this.seiryuRender().slice(LOOP_CUT[0], LOOP_CUT[1]);
        const s = LOOP_START;
        const b = 28;
        const end = s + Math.floor((pcm.length - s) / b) * b;
        const x = pcm.slice(0, end);
        const c = Math.floor(Math.trunc(LOOP_XFADE * SEIRYU_RATE) / b) * b;
        const fade = npLinspace(0, 1, c);
        const head = x.slice(end - c, end);
        const tail = x.slice(s - c, s);
        for (let k = 0; k < c; k++) {
            x[end - c + k] = head[k] * (1 - fade[k]) + tail[k] * fade[k];
        }
        return { pcm: x.map((v) => roundHalfEven(v)), rate: SEIRYU_RATE };
    }

    // gen_mar_sounds.source_wav: the sound as mono 16-bit at <= cap Hz
    get(kind: Kind, src: number | string, cap: number): Pcm {
        const key = kind + "/" + src + "/" + cap;
        let r = this.cache.get(key);
        if (!r) {
            if (kind === "loop") {
                r = this.loop();
            } else if (kind === "seq") {
                r = this.seq(src as number, cap);
            } else {
                r = kind === "psp" ? this.pspClip(src as string) : kind === "roar" ? this.roar() : this.vox(src as number);
                if (r.rate > cap) {
                    const [y] = resample(this.dsp, r.pcm, r.rate, cap);
                    r = { pcm: y, rate: cap };
                }
            }
            this.cache.set(key, r);
        }
        return r;
    }
}

// loop flags on a satxa VAG (a zero block, the data blocks, an end block):
// block flags 6 at the loop start, 2 inside, 3 at the last
function setLoop(vag: Uint8Array, start: number): Uint8Array {
    const v = vag.slice();
    const n = v.length / 16 - 2;
    const first = 1 + start / 28;
    for (let k = 1; k <= n; k++) {
        v[k * 16 + 1] = k < first ? 0 : k === first ? 6 : 2;
    }
    v[n * 16 + 1] = 3;
    return v;
}

const hex = (n: number): string => n.toString(16).toUpperCase();

async function buildBank(a: AudioModule, src: Sources, inp: MarInputs, bank: number, caps: Caps,
                         psp: boolean): Promise<Vab> {
    const vab = new Vab(await inp.ps1(TEMPLATE[bank] + ".VH"), await inp.ps1(TEMPLATE[bank] + ".VB"));
    const template: Tone = { ...vab.tones.get(0)![0] };
    let blank: Tone | undefined;
    for (const tl of vab.tones.values()) {
        blank ??= tl.find((t) => t.vol === 0);
    }
    for (const p of vab.tones.keys()) {
        vab.tones.set(p, Array.from({ length: 16 }, () => ({ ...blank!, prog: p, vag: 1 })));
    }
    // silence that ends
    const silent = new Uint8Array(48);
    silent[17] = 1;
    silent[33] = 7;
    silent.fill(0x77, 34);
    vab.vags = [silent];
    const take = (prog: number, tone: number): void => {
        if (vab.tones.get(prog)![tone].vag !== 1) {
            throw new Error("Maria vab " + bank + ": slot taken");
        }
    };
    const shared = new Map<string, [number, number]>();
    const loops = new Set<number>(); // the looping samples' vag numbers
    const oneShot = (prog: number, vag: number, c: number, f: number): Tone =>
        ({ ...template, prog, vag, vol: 127, pan: 64, center: c, shift: f, min: 0, max: 127, adsr1: ADSR[0], adsr2: ADSR[1] });
    for (const [name, b, ps1, kind0, src0, cls] of SOUNDS) {
        if (b !== bank) {
            continue;
        }
        let kind = kind0;
        let source = src0;
        if (psp && name in PSP_VOICE) {
            kind = "psp";
            source = PSP_VOICE[name];
        }
        const e = inp.cfg[hex(ps1)];
        if (e.vabid !== bank) {
            throw new Error(name + ": sfx_config vab");
        }
        take(e.prog, e.tone);
        if (kind === "ps1") {
            const sv = new Vab(await inp.ps1("SD_J010.VH"), await inp.ps1("SD_J010.VB"));
            const o = inp.cfg[hex(source as number)];
            const st = sv.tones.get(o.prog)![o.tone];
            vab.vags.push(sv.vags[st.vag - 1]);
            for (const k of [0, 1]) { // the tone and its pair voice
                const t = sv.tones.get(o.prog)![o.tone + k];
                if (k && (t.vag !== st.vag || !t.vol)) {
                    break;
                }
                take(e.prog, e.tone + k);
                vab.tones.get(e.prog)![e.tone + k] = {
                    ...t, prog: e.prog, vag: vab.vags.length, center: t.center + e.note - o.note,
                    vol: Math.min(127, roundHalfEven(t.vol * o.volume / e.volume)),
                };
            }
            continue;
        }
        const cap = caps[cls];
        const key = [kind, source, cls, cap, e.volume].join("/");
        const tones = vab.tones.get(e.prog)!;
        if (kind === "psp" && shared.has(key)) {
            const [vag, rate] = shared.get(key)!;
            const [c, f] = centre(src.dsp, e.note, rate);
            tones[e.tone] = oneShot(e.prog, vag, c, f);
            continue;
        }
        const { pcm, rate } = src.get(kind, source, cap);
        let peak = 1;
        for (const v of pcm) {
            peak = Math.max(peak, Math.abs(v));
        }
        let gain = Math.min(127.0 / e.volume, 32000.0 / peak);
        if (cls === "sfx") {
            gain = Math.min(SFX_GAIN * 127.0 / e.volume, 32000.0 / peak);
        }
        if (cls === "voice" || cls === "spell") {
            gain = Math.min(VOICE_PEAK / peak * 127.0 / e.volume, 32000.0 / peak);
        }
        const out = gain > 1.0 ? Int16Array.from(pcm, (v) => roundHalfEven(v * gain)) : Int16Array.from(pcm);
        let vag = satxa(a, "vag", makeWav(out, rate), []);
        if (kind === "loop") {
            vag = setLoop(vag, LOOP_START);
            loops.add(vab.vags.length + 1);
        }
        vab.vags.push(vag);
        shared.set(key, [vab.vags.length, rate]);
        const [c, f] = centre(src.dsp, e.note, rate);
        if (c < 0 || c > 127) {
            throw new Error(name + ": centre note out of range");
        }
        tones[e.tone] = oneShot(e.prog, vab.vags.length, c, f);
    }
    // the pair voice. The game plays a sound on its tone and the next one
    // (src/dra/sound.c func_80132A04) and frees its channel group only once both
    // voices stop. Left as the blank, whose centre note (the template's) is far
    // above her notes, that voice never stopped: her grunts and hurt sounds held
    // the groups at priority 90 and the stage's sounds stayed silent until the
    // next stage. So the pair voice is the sound itself at volume 0, ending with
    // it (the retail banks pair a tone with its copy). Not the loop's: Seiryuu
    // plays on channels 22/23, which her dragon releases
    for (const tl of vab.tones.values()) {
        const real = tl.map((t) => t.vag > 1 && !loops.has(t.vag));
        for (let t = 0; t < 15; t++) {
            if (real[t] && tl[t + 1].vag === 1) {
                tl[t + 1] = { ...tl[t], vol: 0 };
            }
        }
    }
    return vab;
}

// pad the VB to the size DRA transfers (an unused VAG); null: over
function finish(vab: Vab, bank: number): [Uint8Array, Uint8Array] | null {
    let [vh, vb] = vab.write();
    if (vb.length > VB_LEN[bank]) {
        return null;
    }
    if (vb.length < VB_LEN[bank]) {
        vab.vags.push(new Uint8Array(VB_LEN[bank] - vb.length));
        [vh, vb] = vab.write();
    }
    if (vh.length !== VH_LEN[bank] || vb.length !== VB_LEN[bank]) {
        throw new Error("Maria vab " + bank + ": bank size");
    }
    return [vh, vb];
}

export interface MarResult {
    data: Uint8Array; // MARIA_SD.BIN / MARIA_SD_PSP.BIN
    caps: string[]; // per bank: the rate caps used ("buzz 44100, sfx 44100")
}

// gen_mar_sounds.main (psp: --psp)
export async function buildMariaSd(audio: () => Promise<AudioModule>, dsp: Dsp, inp: MarInputs,
                                   psp: boolean): Promise<MarResult> {
    const psps = new Map(spkEntries(inp.pspMar2).map((e) => [e.name, e]));
    const src = new Sources(dsp, await SfxContext.open(inp.saturn, "SD_TEI.CRT", "SD_MARI.CRT"),
                            await inp.saturn.readFile("0.BIN"), await inp.saturn.readFile("SD/SDE7.PCM"), psps);
    const banks: [Uint8Array, Uint8Array][] = [];
    const used: string[] = [];
    for (const bank of [1, 2]) {
        const classes = [...new Set(SOUNDS.filter((e) => e[1] === bank).map((e) => e[5]))]
            .filter((c) => c !== "voice").sort();
        const tried = new Set<string>();
        let done: [Uint8Array, Uint8Array] | null = null;
        for (const step of STEPS) {
            const mine = classes.map((c) => step[c]).join(",");
            if (tried.has(mine)) {
                continue; // the same caps for this bank's sounds
            }
            tried.add(mine);
            const vab = await buildBank(await audio(), src, inp, bank, { ...step, voice: 44100 }, psp);
            done = finish(vab, bank);
            if (done) {
                used.push(classes.map((c) => c + " " + step[c]).join(", "));
                break;
            }
        }
        if (!done) {
            throw new Error("Maria's vab " + bank + " doesn't fit at any rate cap");
        }
        banks.push(done);
    }
    const parts = [banks[0][0], banks[0][1], banks[1][0], banks[1][1]];
    const sizes = parts.map((p, i) => (i < 3 ? LAYOUT_SECTORS[i] : Math.ceil(p.length / SECTOR)) * SECTOR);
    const data = new Uint8Array(sizes.reduce((s, n) => s + n, 0));
    let o = 0;
    parts.forEach((p, i) => {
        data.set(p, o);
        o += sizes[i];
    });
    return { data, caps: used };
}

// build/us/mod/mar/mar_sfx.h (the MARIA_SD.BIN run's)
export function marSfxHeader(caps: string[]): string {
    const out = [
        "// Generated by mods/sotn-ultimate/tools/gen_mar_sounds.py, do not edit",
        "// the Saturn Maria's sounds: PS1 ids whose vab 1 / vab 2 slots",
        "// hold them in BIN/MARIA_SD.BIN",
        ...caps.map((c, i) => "// vab " + (i + 1) + " rate caps (Hz): " + c),
    ];
    for (const [name, , ps1, kind, src] of SOUNDS) {
        let what: string;
        if (kind === "roar") {
            what = "Saturn 0x" + hex(src as number) + " " + (ROAR_CUT[0] / SEIRYU_RATE).toFixed(3) + "-" +
                (ROAR_CUT[1] / SEIRYU_RATE).toFixed(3) + " s, faded";
        } else if (kind === "loop") {
            what = "Saturn 0x" + hex(src as number) + ", a loop";
            out.push("#define MAR_SFX_" + name + "_LOOP 1");
        } else {
            what = (kind === "ps1" ? "PS1 0x" : "Saturn 0x") + hex(src as number);
        }
        out.push("#define MAR_SFX_" + name + " 0x" + hex(ps1) + " // " + what);
    }
    return out.join("\n") + "\n";
}

// the PS1 sound ids whose sfx_config.yaml entries the banks use
export function marCfgIds(): number[] {
    return [...new Set(SOUNDS.flatMap(([, , ps1, kind, src]) => (kind === "ps1" ? [ps1, src as number] : [ps1])))];
}
