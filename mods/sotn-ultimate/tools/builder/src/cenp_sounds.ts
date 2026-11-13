// SPDX-License-Identifier: AGPL-3.0-or-later
// The PSP Maria boss's CEN banks (mods/sotn-ultimate/tools/gen_cenp_sounds.py, ported; its
// docstring has the story): the retail SD_ZKCEN with programs 3, 5, 6
// blanked (src/cen_sounds.ts) and her 16 clips in 16 of their slots (clip 6
// left out): the PSP dub's (CENP_SD.bin: the PSP Maria's English clips as
// MARIA2_SD.BIN holds them), the JP dub's (CENP_SD_JP.bin: her Japanese
// clips) and the PS1 dub's (CENP_SD_PS1.bin, --ps1sat: the Saturn boss's
// sounds, at the highest rate cap that fits). Each = VH (4 sectors) + VB,
// whole sectors.
import { makeWav, satxa, type AudioModule } from "./audio.ts";
import type { IsoFs } from "./disc.ts";
import { roundHalfEven, type Dsp } from "./dsp.ts";
import { blankCen, CEN_SOUNDS, CEN_VOICES, finishCen, renderToPcm } from "./cen_sounds.ts";
import type { SfxCfg } from "./mar_sounds.ts";
import { MAR2_SLOTS, mar2Clips } from "./mar2_sounds.ts";
import { SfxContext } from "./saturn_sfx.ts";
import { centre, resample } from "./vab.ts";

type Files = Pick<IsoFs, "readFile">;

const RATE = 8000; // the PSP clips' (maria-sfx's)
const VOICE_PEAK = 29200;
const NO_CLIP = 6; // 0x8EC: Guardian Knuckle
// her clip i (PSP id 0x8E6 + i) -> the CEN stage id whose slot plays it
export const CENP_SLOTS = [0x7F5, 0x7F6, 0x7F7, 0x7F8, 0x72D, 0x731, 0x733, 0x879,
    0x834, 0x835, 0x836, 0x837, 0x75E, 0x839, 0x83A, 0x83B];
// --ps1sat: her clip i -> the Saturn boss's sound (src/cen_sounds.ts)
const SAT_PS1 = ["MB_ATK1", "MB_ATK2", "MB_ATK1", "MB_THROW", "MB_SUMMON", "MB_HOLY", null,
    "MB_SUMMON", "MB_FLAME1", "MB_HOLY", "MB_SUMMON", "MB_JUMP", "MB_HURT1",
    "MB_HURT2", "MB_HURT1", "MB_DEATH"];
const SAT_RATES = [22050, 16000, 14000, 12000, 11025, 10000, 9000, 8000];

const hex = (n: number): string => n.toString(16).toUpperCase();

type Clip = [Uint8Array | null, number]; // VAG body, the volume its MARIA2 slot gave it

function bank(dsp: Dsp, clips: Clip[], rates: number[], vhRetail: Uint8Array, vbRetail: Uint8Array,
              cfg: Record<string, SfxCfg>): Uint8Array | null {
    const [vab, blank, template] = blankCen(vhRetail, vbRetail);
    clips.forEach(([body, volMar], i) => {
        if (i === NO_CLIP || body === null) {
            return;
        }
        const e = cfg[hex(CENP_SLOTS[i])];
        const tl = vab.tones.get(e.prog)!;
        if (e.vabid !== 3 || tl[e.tone].vag !== blank) {
            throw new Error("PSP Maria boss clip " + i + ": slot");
        }
        vab.vags.push(body);
        const [c, f] = centre(dsp, e.note, rates[i]);
        // the loudness MARIA2's slot gave it (her player id's volume)
        const vol = Math.min(127, roundHalfEven(127 * volMar / e.volume));
        tl[e.tone] = { ...template, prog: e.prog, vag: vab.vags.length, vol, pan: 64, center: c,
                       shift: f, min: 0, max: 127, adsr1: 0x80FF, adsr2: 0x5FC0 };
    });
    const done = finishCen(vab, blank, vhRetail, vbRetail);
    return done ? done[2] : null;
}

export interface CenpBanks {
    psp: Uint8Array; // CENP_SD.bin
    jp: Uint8Array; // CENP_SD_JP.bin
    ps1: Uint8Array; // CENP_SD_PS1.bin (--ps1sat)
    satCap: number;
}

// spkEn / spkJp: the English and Japanese sd_mar2.spk
export async function buildCenp(audio: () => Promise<AudioModule>, dsp: Dsp, saturn: Files,
                                ps1: (p: string) => Promise<Uint8Array>, spkEn: Uint8Array, spkJp: Uint8Array,
                                cfg: Record<string, SfxCfg>): Promise<CenpBanks> {
    const vhRetail = await ps1("ST/CEN/SD_ZKCEN.VH");
    const vbRetail = await ps1("ST/CEN/SD_ZKCEN.VB");
    const vols = MAR2_SLOTS.map((id) => cfg[hex(id)].volume);
    const fixed = (spk: Uint8Array): Uint8Array => {
        const b = bank(dsp, mar2Clips(spk).map((c, i): Clip => [c, vols[i]]), CENP_SLOTS.map(() => RATE),
                       vhRetail, vbRetail, cfg);
        if (!b) {
            throw new Error("the PSP Maria boss's clips do not fit CEN's bank");
        }
        return b;
    };
    const psp = fixed(spkEn);
    const jp = fixed(spkJp);
    // --ps1sat: the Saturn boss's sounds, rendered at 22050 Hz, then capped
    const ctx = await SfxContext.open(saturn, "SD_TMARI.CRT");
    const sid = new Map(CEN_SOUNDS.map(([n, s]) => [n, s]));
    const renders = new Map(CEN_SOUNDS.map(([n, s]) => [n, renderToPcm(dsp, ctx, s, 22050)]));
    for (const cap of SAT_RATES) {
        const a = await audio();
        const clips: Clip[] = [];
        const rates = CENP_SLOTS.map(() => RATE);
        SAT_PS1.forEach((name, i) => {
            if (name === null || !sid.has(name)) {
                clips.push([null, 0]);
                return;
            }
            const r = renders.get(name)!;
            let pcm: ArrayLike<number> = r.pcm;
            let rate = r.rate;
            if (rate > cap) {
                [pcm, rate] = resample(dsp, pcm, rate, cap);
            }
            // up to full scale against this slot's volume, her voice at VOICE_PEAK
            const e = cfg[hex(CENP_SLOTS[i])];
            let peak = 1;
            for (let k = 0; k < pcm.length; k++) {
                peak = Math.max(peak, Math.abs(pcm[k]));
            }
            let gain = Math.min(127.0 / e.volume, 32000.0 / peak);
            if (CEN_VOICES.has(name)) {
                gain = Math.min(VOICE_PEAK / peak * 127.0 / e.volume, 32000.0 / peak);
            }
            const out = Int16Array.from({ length: pcm.length }, (_, k) => roundHalfEven(pcm[k] * gain));
            clips.push([satxa(a, "vag", makeWav(out, rate), []), e.volume]);
            rates[i] = rate;
        });
        const b = bank(dsp, clips, rates, vhRetail, vbRetail, cfg);
        if (b) {
            return { psp, jp, ps1: b, satCap: cap };
        }
    }
    throw new Error("the Saturn boss's sounds don't fit the PSP boss's CEN bank");
}

export function cenpCfgIds(): number[] {
    return [...CENP_SLOTS, ...MAR2_SLOTS];
}
