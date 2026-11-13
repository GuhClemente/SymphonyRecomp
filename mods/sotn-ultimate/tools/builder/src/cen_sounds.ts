// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn Maria boss fight's sounds in the Castle Center's bank
// (mods/sotn-ultimate/tools/gen_cen_sounds.py, ported; its docstring has the story): the
// retail ST/CEN/SD_ZKCEN with programs 3, 5, 6 blanked and her Saturn sounds
// (SD_TMARI.CRT, rendered by src/saturn_sfx.ts) in their slots, at the
// highest rate cap that fits the retail sizes. Also CEN_SD_PS1.bin (the same
// bank as VH 4 sectors + VB, CENP.BIN's part at 0x7C800). The release build
// runs this (mods/sotn-ultimate/tools/builder/node/cen_sounds.ts).
import { makeWav, satxa, type AudioModule } from "./audio.ts";
import type { IsoFs } from "./disc.ts";
import { roundHalfEven, type Dsp } from "./dsp.ts";
import type { SfxCfg } from "./mar_sounds.ts";
import { render, SfxContext } from "./saturn_sfx.ts";
import { centre, Vab, type Tone } from "./vab.ts";

type Files = Pick<IsoFs, "readFile">;

const FREE_PROGS = [3, 5, 6];
const STAGE = "SD_TMARI.CRT";
const RATE_CAPS = [32000, 22050, ...Array.from({ length: 13 }, (_, i) => 20000 - 1000 * i)];
const VH_SECTORS = 4;
const SECTOR = 2048;
const VOICE_PEAK = 29200;
export const CEN_VOICES = new Set(["MB_ATK1", "MB_ATK2", "MB_JUMP", "MB_DASH", "MB_HOLY", "MB_SUMMON",
    "MB_HURT1", "MB_HALF", "MB_HURT2", "MB_DEATH", "MB_THROW"]);
// (name, Saturn id, PS1 id whose slot plays it)
export const CEN_SOUNDS: [string, number, number][] = [
    ["MB_ATK1", 0x8B7, 0x725],
    ["MB_ATK2", 0x8B8, 0x728],
    ["MB_JUMP", 0x8B9, 0x723],
    ["MB_DASH", 0x8BD, 0x72D],
    ["MB_HOLY", 0x8BE, 0x72E],
    ["MB_SUMMON", 0x8BF, 0x72F],
    ["MB_HURT1", 0x8C5, 0x751],
    ["MB_HALF", 0x8C6, 0x752],
    ["MB_HURT2", 0x8C7, 0x753],
    ["MB_DEATH", 0x8CA, 0x743],
    ["MB_THROW", 0x8CC, 0x744],
    ["MB_FLAME1", 0x8D4, 0x745],
    ["MB_FLAME2", 0x8D5, 0x763],
];

// saturn_sfx.render_to_wav: the 16-bit WAV's samples (a stereo render
// interleaved, as the WAV holds it), its channels and rate
export interface Rendered {
    pcm: Int16Array;
    ch: number;
    rate: number;
}

export function renderToPcm(dsp: Dsp, ctx: SfxContext, sid: number, cap: number): Rendered {
    const r = render(dsp, ctx, sid, cap);
    if (r.empty) {
        throw new Error("Saturn sound " + sid.toString(16) + ": empty in " + STAGE);
    }
    // saturn_sfx.trim + to_pcm16, both channels
    const thr = dsp.pow(10, -60.0 / 20.0);
    let last = -1;
    let peak = 0;
    for (let k = 0; k < r.ch[0].length; k++) {
        let a = 0;
        for (const c of r.ch) {
            a = Math.max(a, Math.abs(c[k]));
        }
        if (a > thr) {
            last = k;
        }
    }
    for (const c of r.ch) {
        for (let k = 0; k <= last; k++) {
            peak = Math.max(peak, Math.abs(c[k]));
        }
    }
    const scale = peak > 32767 / 32768 ? (32767 / 32768) / peak : 1.0;
    const n = r.ch.length;
    const pcm = new Int16Array((last + 1) * n);
    for (let k = 0; k <= last; k++) {
        for (let j = 0; j < n; j++) {
            pcm[k * n + j] = Math.min(32767, Math.max(-32768, roundHalfEven(r.ch[j][k] * scale * 32768)));
        }
    }
    return { pcm, ch: n, rate: r.rate };
}

export interface CenBank {
    vh: Uint8Array; // build/us/SD_ZKCEN.VH (retail size)
    vb: Uint8Array;
    ps1: Uint8Array; // CEN_SD_PS1.bin: VH (4 sectors) + VB, whole sectors
    cap: number;
}

const SILENT = (): Uint8Array => {
    const s = new Uint8Array(48);
    s[17] = 1;
    s[33] = 7;
    s.fill(0x77, 34);
    return s;
};

// the retail CEN bank with the free programs blanked: [vab, blank vag, template tone]
export function blankCen(vh: Uint8Array, vb: Uint8Array): [Vab, number, Tone] {
    const vab = new Vab(vh, vb);
    const keepSet = new Set<number>();
    for (const [p, tl] of vab.tones) {
        if (!FREE_PROGS.includes(p)) {
            for (const t of tl) {
                if (t.vag > 0) {
                    keepSet.add(t.vag);
                }
            }
        }
    }
    const keep = [...keepSet].sort((a, b) => a - b);
    const remap = new Map(keep.map((v, k) => [v, k + 1]));
    vab.vags = keep.map((v) => vab.vags[v - 1]);
    vab.vags.push(SILENT());
    const blank = vab.vags.length;
    let template: Tone | null = null;
    for (const [p, tl] of vab.tones) {
        for (const t of tl) {
            if (FREE_PROGS.includes(p)) {
                if (template === null && t.vag > 0) {
                    template = { ...t };
                }
                t.vag = blank;
                t.vol = 0;
            } else if (t.vag > 0) {
                t.vag = remap.get(t.vag)!;
            }
        }
    }
    return [vab, blank, template!];
}

// pad the bank to the retail VB size (the blank sample grows by silent
// blocks, so the VH's sample sizes add up to the VB) and lay it out as VH
// (4 sectors) + VB; null when over
export function finishCen(vab: Vab, blank: number, vhRetail: Uint8Array, vbRetail: Uint8Array):
        [Uint8Array, Uint8Array, Uint8Array] | null {
    let [vh, vb] = vab.write();
    if (vh.length !== vhRetail.length) {
        throw new Error("CEN bank: VH size");
    }
    if (vb.length > vbRetail.length) {
        return null;
    }
    const pad = vbRetail.length - vb.length;
    const grown = new Uint8Array(pad + vab.vags[blank - 1].length);
    grown.set(vab.vags[blank - 1], pad);
    vab.vags[blank - 1] = grown;
    [vh, vb] = vab.write();
    new DataView(vh.buffer).setUint32(12, vh.length + vb.length, true); // VAB file size field
    const n = VH_SECTORS * SECTOR + vb.length;
    const data = new Uint8Array(Math.ceil(n / SECTOR) * SECTOR);
    data.set(vh);
    data.set(vb, VH_SECTORS * SECTOR);
    return [vh, vb, data];
}

// gen_cen_sounds.build at each rate cap until the bank fits
export async function buildCen(audio: () => Promise<AudioModule>, dsp: Dsp, saturn: Files,
                               ps1: (p: string) => Promise<Uint8Array>, cfg: Record<string, SfxCfg>): Promise<CenBank> {
    const vhRetail = await ps1("ST/CEN/SD_ZKCEN.VH");
    const vbRetail = await ps1("ST/CEN/SD_ZKCEN.VB");
    const ctx = await SfxContext.open(saturn, STAGE);
    for (const cap of RATE_CAPS) {
        const a = await audio();
        const [vab, blank, template] = blankCen(vhRetail, vbRetail);
        for (const [name, sat, id] of CEN_SOUNDS) {
            const e = cfg[id.toString(16).toUpperCase()];
            if (e.vabid !== 3 || !FREE_PROGS.includes(e.prog)) {
                throw new Error(name + ": not a free CEN slot");
            }
            const r = renderToPcm(dsp, ctx, sat, cap);
            let peak = 1;
            for (const v of r.pcm) {
                peak = Math.max(peak, Math.abs(v));
            }
            let gain = Math.min(127.0 / e.volume, 32000.0 / peak);
            if (CEN_VOICES.has(name)) {
                gain = Math.min(VOICE_PEAK / peak * 127.0 / e.volume, 32000.0 / peak);
            }
            // a louder copy is written as a mono WAV (a stereo render's
            // frames then read as mono samples, as gen_cen_sounds.py does)
            const wav = gain > 1.0 ? makeWav(Int16Array.from(r.pcm, (v) => roundHalfEven(v * gain)), r.rate)
                : makeWav(r.pcm, r.rate, r.ch);
            vab.vags.push(satxa(a, "vag", wav, []));
            const [c, f] = centre(dsp, e.note, r.rate);
            const tl = vab.tones.get(e.prog)!;
            if (tl[e.tone].vag !== blank) {
                throw new Error(name + ": slot taken");
            }
            tl[e.tone] = { ...template, prog: e.prog, vag: vab.vags.length, vol: 127, pan: 64, center: c,
                           shift: f, min: 0, max: 127, adsr1: 0x80FF, adsr2: 0x5FC0 };
        }
        const done = finishCen(vab, blank, vhRetail, vbRetail);
        if (done) {
            return { vh: done[0], vb: done[1], ps1: done[2], cap };
        }
    }
    throw new Error("Maria's sounds don't fit CEN's bank at any rate cap");
}

export function cenCfgIds(): number[] {
    return CEN_SOUNDS.map(([, , id]) => id);
}
