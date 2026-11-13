// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn-only stages' sound banks (mods/sotn-ultimate/tools/gen_gdn_sounds.py's build,
// with gen_prs / gen_rgdn / gen_rprs_sounds.py's tables, ported; their
// docstrings have the story): the Saturn stage bank's sounds, rendered by
// src/saturn_sfx.ts, in a PS1 VAB of their own (the unused TE1-TE4 stages'
// bank slots), plus PS1 tones copied where the Saturn's sound is the PS1's
// recording; sample rates capped lower until the bank fits VB_LIMIT. The
// release build runs this (mods/sotn-ultimate/tools/builder/node/stage_sounds.ts).
import { makeWav, satxa, type AudioModule } from "./audio.ts";
import type { IsoFs } from "./disc.ts";
import { roundHalfEven, type Dsp } from "./dsp.ts";
import { renderToPcm } from "./cen_sounds.ts";
import type { SfxCfg } from "./mar_sounds.ts";
import { SfxContext } from "./saturn_sfx.ts";
import { centre, decodeVag, resample, Vab, type Tone } from "./vab.ts";

type Files = Pick<IsoFs, "readFile">;

const VB_LIMIT = 0x1A610; // the largest retail stage bank (DRE)
const RATE_CAPS = [44100, 32000, 22050, ...Array.from({ length: 19 }, (_, i) => 20000 - 500 * i)];
const TEMPLATE = "ST/NP3/SD_ZKNP3"; // the layout template

export interface StageBank {
    key: string; // gdn, prs, rgdn, rprs
    out: string; // the bank on the disc (no extension)
    header: string; // src/st/<key>/mod_<key>_sfx.h
    description: string;
    stage: string; // the Saturn stage bank
    uncapped: string[]; // kept at the Saturn's rate
    sounds: [string, number, number][]; // (name, Saturn id, PS1 id whose slot it fills)
    ps1Tones: [string, number, number, number][]; // (VAB, program, tone, rate divisor) into the same slot
    ps1Pairs: boolean; // also the tone's pair voice
}

const NO4 = "ST/NO4/SD_ZKNO4";
const RNO3 = "ST/RNO3/SD_ZRNO3";
const RNZ1 = "ST/RNZ1/SD_ZRNZ1";
const RNO2 = "ST/RNO2/SD_ZRNO2";
export const STAGE_BANKS: StageBank[] = [
    {
        key: "gdn", out: "ST/TE1/SD_ZKTE1", header: "mods/sotn-ultimate/src/garden/gdn/mod_gdn_sfx.h",
        description: "the Garden's own sounds (its stage bank, build/us/SD_ZKTE1)", stage: "SD_TEI.CRT",
        uncapped: ["GARDENER_SHEARS"],
        sounds: [
            ["BOSS_BOMB", 0x8F7, 0x70B], ["BOSS_DEATH", 0x8F8, 0x72D], ["TREE_FACE_FALL", 0x8FA, 0x709],
            ["TREE_FACE_BURST", 0x8FB, 0x70A], ["TREE_HURT", 0x8FC, 0x713], ["TREE_DEATH", 0x8FD, 0x731],
            ["GARGOYLE_CLAW", 0x8FE, 0x725], ["GARGOYLE_DEBRIS", 0x8FF, 0x728], ["MANEATER_BURST", 0x90B, 0x734],
            ["MANEATER_OPEN", 0x90C, 0x726], ["MANEATER_SNAP", 0x90D, 0x733], ["GARDENER_DEATH", 0x90E, 0x735],
            ["GARDENER_SHEARS", 0x90F, 0x73B], ["LEAPER_DEATH", 0x911, 0x751], ["LEAPER_LEAP", 0x912, 0x74B],
            ["LEAPER_FALL", 0x913, 0x74C], ["WATER_SPLASH", 0x777, 0x7C2], ["WATER_SURFACE", 0x778, 0x7C3],
        ],
        ps1Tones: [["ST/WRP/SD_ZKWRP", 1, 14, 2]], ps1Pairs: false,
    },
    {
        key: "prs", out: "ST/TE2/SD_ZKTE2", header: "mods/sotn-ultimate/src/prison/prs/mod_prs_sfx.h",
        description: "the Prison's own sounds (its stage bank, build/us/SD_ZKTE2)", stage: "SD_ROUG.CRT",
        uncapped: [],
        sounds: [
            ["GARGOYLE_CLAW", 0x8FE, 0x725], ["GARGOYLE_DEBRIS", 0x8FF, 0x728], ["SPECTER_A", 0x900, 0x709],
            ["SPECTER_B", 0x901, 0x70A], ["WISP", 0x903, 0x70B],
        ],
        ps1Tones: [[NO4, 6, 10, 1], [NO4, 6, 12, 1], [NO4, 6, 14, 1]], ps1Pairs: true,
    },
    {
        key: "rgdn", out: "ST/TE3/SD_ZKTE3", header: "mods/sotn-ultimate/src/garden/rgdn/mod_rgdn_sfx.h",
        description: "the Hell Garden's own sounds (its stage bank, build/us/SD_ZKTE3)", stage: "SD_TEIX.CRT",
        uncapped: [],
        sounds: [
            ["COPPER_ARMOR_DEATH", 0x8F0, 0x72D], ["IFRIT_DEATH", 0x8F2, 0x731], ["IFRIT_FIRE_BREATH", 0x8F3, 0x733],
            ["IFRIT_FIRE_RING", 0x8F4, 0x734], ["IFRIT_FIRE_PILLAR", 0x8F5, 0x735], ["IFRIT_CHARGE", 0x8F6, 0x73B],
        ],
        ps1Tones: [[RNO3, 2, 4, 1], [RNO3, 2, 6, 1], [RNZ1, 5, 0, 1], [RNZ1, 5, 4, 1]], ps1Pairs: true,
    },
    {
        key: "rprs", out: "ST/TE4/SD_ZKTE4", header: "mods/sotn-ultimate/src/prison/rprs/mod_rprs_sfx.h",
        description: "the Soul Prison's own sounds (its stage bank, build/us/SD_ZKTE4)", stage: "SD_ROUGX.CRT",
        uncapped: [],
        sounds: [
            ["GARGOYLE_CLAW", 0x8FE, 0x725], ["GARGOYLE_DEBRIS", 0x8FF, 0x728], ["WRAITH_GRAB", 0x904, 0x72D],
            ["WRAITH_DRAIN", 0x905, 0x731], ["WRAITH_DEATH", 0x906, 0x733], ["WRAITH_VANISH", 0x907, 0x734],
            ["WIGHT_SLASH", 0x908, 0x735], ["WIGHT_DEATH", 0x909, 0x73B],
        ],
        ps1Tones: [[RNO2, 2, 8, 1], [RNO2, 2, 10, 1], [RNO2, 2, 12, 1]], ps1Pairs: true,
    },
];

export interface StageResult {
    vh: Uint8Array;
    vb: Uint8Array;
    cap: number;
}

const SILENT = (): Uint8Array => {
    const s = new Uint8Array(48);
    s[17] = 1;
    s[33] = 7;
    s.fill(0x77, 34);
    return s;
};

// gen_gdn_sounds.build at each rate cap until the bank fits
export async function buildStageBank(audio: () => Promise<AudioModule>, dsp: Dsp, saturn: Files,
                                     ps1: (p: string) => Promise<Uint8Array>, cfg: Record<string, SfxCfg>,
                                     b: StageBank): Promise<StageResult> {
    const ctx = await SfxContext.open(saturn, b.stage);
    const vabOf = async (p: string): Promise<Vab> => new Vab(await ps1(p + ".VH"), await ps1(p + ".VB"));
    for (const cap of RATE_CAPS) {
        const a = await audio();
        const vab = await vabOf(TEMPLATE);
        const template: Tone = { ...vab.tones.get(0)![4] }; // a used tone (attack, sustain)
        const blank: Tone = { ...vab.tones.get(0)![8] }; // an empty slot
        for (const p of vab.tones.keys()) {
            vab.tones.set(p, Array.from({ length: 16 }, () => ({ ...blank, prog: p, vag: 1 })));
        }
        vab.vags = [SILENT()];
        const take = (prog: number, tone: number): void => {
            if (vab.tones.get(prog)![tone].vag !== 1) {
                throw new Error(b.key + " sounds: slot taken (" + prog + ", " + tone + ")");
            }
        };
        for (const [src, prog, tone, div] of b.ps1Tones) {
            const sv = await vabOf(src);
            const st: Tone = { ...sv.tones.get(prog)![tone] };
            let body = sv.vags[st.vag - 1];
            const shift = div > 1 ? roundHalfEven(12 * dsp.log2(div)) : 0;
            if (div > 1) {
                const [pcm] = resample(dsp, decodeVag(body).slice(28), 44100, Math.floor(44100 / div));
                body = satxa(a, "vag", makeWav(Int16Array.from(pcm), Math.floor(44100 / div)), []);
                st.center += shift;
            }
            vab.vags.push(body);
            take(prog, tone);
            vab.tones.get(prog)![tone] = { ...st, prog, vag: vab.vags.length };
            const pair = tone < 15 ? { ...sv.tones.get(prog)![tone + 1] } : null;
            if (b.ps1Pairs && pair && pair.vag === sv.tones.get(prog)![tone].vag && pair.vol) {
                pair.center += shift;
                take(prog, tone + 1);
                vab.tones.get(prog)![tone + 1] = { ...pair, prog, vag: vab.vags.length };
            }
        }
        for (const [name, sat, id] of b.sounds) {
            const e = cfg[id.toString(16).toUpperCase()];
            if (e.vabid !== 3) {
                throw new Error(name + ": not a stage id");
            }
            const r = renderToPcm(dsp, ctx, sat, b.uncapped.includes(name) ? 44100 : cap);
            let peak = 1;
            for (const v of r.pcm) {
                peak = Math.max(peak, Math.abs(v));
            }
            // the id's table volume scales it: undo that, as far as the headroom allows
            const gain = Math.min(127.0 / e.volume, 32000.0 / peak);
            const wav = gain > 1.0 ? makeWav(Int16Array.from(r.pcm, (v) => roundHalfEven(v * gain)), r.rate)
                : makeWav(r.pcm, r.rate, r.ch);
            vab.vags.push(satxa(a, "vag", wav, []));
            const [c, f] = centre(dsp, e.note, r.rate);
            take(e.prog, e.tone);
            vab.tones.get(e.prog)![e.tone] = { ...template, prog: e.prog, vag: vab.vags.length, vol: 127, pan: 64,
                                               center: c, shift: f, min: 0, max: 127, adsr1: 0x80FF, adsr2: 0x5FC0 };
        }
        const [vh, vb] = vab.write();
        if (vb.length <= VB_LIMIT) {
            return { vh, vb, cap };
        }
    }
    throw new Error(b.key + ": the sounds don't fit the bank at any rate cap");
}

// src/st/<key>/mod_<key>_sfx.h
export function stageHeader(b: StageBank): string {
    return ["// Generated by mods/sotn-ultimate/tools/gen_" + b.key + "_sounds.py, do not edit", "// " + b.description,
        ...b.sounds.map(([n, sat, id]) => "#define MOD_SFX_" + n + " 0x" + id.toString(16).toUpperCase() +
                        " // Saturn 0x" + sat.toString(16).toUpperCase())].join("\n") + "\n";
}

export function stageCfgIds(): number[] {
    return [...new Set(STAGE_BANKS.flatMap((b) => b.sounds.map(([, , id]) => id)))];
}
