// SPDX-License-Identifier: AGPL-3.0-or-later
// The builder: the player's PS1 Track 1 + the patch -> the hollow disc; the
// converters fill each span from the player's Saturn and PSP discs; every
// span and then the whole disc are checked against the manifest's md5s.
import { IsoFs, type ByteSource } from "./disc.ts";
import { md5, Md5 } from "./md5.ts";
import { buildMonster2 } from "./monster2.ts";
import { encodeSong, encodeVoice, makeWav, saturnClip, SATURN_VOICE_RATE, type AudioModule } from "./audio.ts";
import { Dsp } from "./dsp.ts";
import { buildGrunts } from "./grunts.ts";
import { buildCen } from "./cen_sounds.ts";
import { buildCenp } from "./cenp_sounds.ts";
import { cenMaria } from "./cen_maria.ts";
import { MONO_PAYLOAD, monoPayloads, type MonoRun } from "./xa_mono.ts";
import { marAssetsAll } from "./mar_assets.ts";
import { modArrayBytes, no3Horse, saturnBestiary, saturnEnemies, saturnEquipment, sconces, type ModTables } from "./mod_tables.ts";
import { buildRic2, type Ric2Syms } from "./ric2.ts";
import { buildSaturnWeapons } from "./saturn_weapons.ts";
import { buildStageAll, STAGE_DISC, type SatConvFactory } from "./stage_build.ts";
import { buildModRooms, type GenResult } from "./mod_rooms.ts";
import { buildPrsLinks } from "./prs_links.ts";
import { buildFmap } from "./fmap.ts";
import { buildMarHud } from "./mar_hud.ts";
import { buildSaturnIcons } from "./saturn_icons.ts";
import { buildTitlePortraits } from "./title_portraits.ts";
import { buildMariaSd, type SfxCfg } from "./mar_sounds.ts";
import { buildMaria2Sd } from "./mar2_sounds.ts";
import { buildStageBank, STAGE_BANKS } from "./stage_sounds.ts";
import { buildNo3Bank, rideBank, rideInputs } from "./no3_sounds.ts";
import { pspTextBytes, writeText, type TextRun } from "./psp_text.ts";
import { buildSfxBank, buildSfxJp, buildSfxPsp, type SfxManifest } from "./psp_sfx.ts";
import { encodeSaturnTrack, type MusicTrack } from "./saturn_music.ts";
import { writeFile } from "./sector.ts";
import { vcdiffDecode } from "./vcdiff.ts";

// a whole file on the disc (Form 1 sectors), or a part of one: whole sectors
// of a file several converters fill (BIN/JP_SD.BIN)
export interface FileSpan {
    kind: "file";
    path: string; // e.g. "BIN/MONSTER2.BIN"
    part?: string; // e.g. "SFX": the converter makes "BIN/JP_SD.BIN#SFX"
    lba: number; // of the file (of the part)
    size: number; // bytes of the file (of the part: its sectors)
    md5: string;
    gen: string; // the converter that makes it
}

// an XA stream (raw 2352-byte sectors) scattered over SD/XA_STR1.: only each
// sector's audio (bytes 24..2348) is written; the disc keeps its subheaders
export interface XaSpan {
    kind: "xa";
    stream: string; // the converter's name for it, e.g. "SD25.PCM"
    size: number; // bytes of the encoded stream
    md5: string;
    gen: string;
    runs: [number, number, number, number][]; // [XA_STR1 sector, stride, count, stream sector]
}

// the PSP script text in a disc file (src/psp_text.ts): only those bytes
// (and their sectors' EDC/ECC) are blank in the patch
export interface TextSpan {
    kind: "text";
    path: string;
    key?: string; // the converter's key when not the path (a second span of the file)
    lba: number; // of the file
    size: number; // bytes of text
    md5: string; // of the text bytes, run after run
    gen: string;
    runs: TextRun[];
}

// a generated data array compiled into a disc file (an overlay): its bytes
// (and their sectors' EDC/ECC) are blank in the patch; the converter makes
// "<path>@<name>"
export interface ArraySpan {
    kind: "array";
    path: string;
    name: string; // the array's C symbol
    key?: string; // the converter's key when not "<path>@<name>" (a weapon slot's)
    lba: number; // of the file
    offset: number; // of the array in the file
    size: number;
    md5: string;
    gen: string;
}

// the PS1's dual-mono voices repack_xa.py made mono (src/xa_mono.ts): each
// sector from two retail sectors of the player's disc; the converter makes
// "xa_mono", the sectors' payloads back to back
export interface XaMonoSpan {
    kind: "xamono";
    size: number;
    md5: string;
    gen: string;
    runs: MonoRun[];
}

export type Span = FileSpan | XaSpan | TextSpan | ArraySpan | XaMonoSpan;

export interface Manifest {
    name: string; // "SOTN Ultimate (PS1 Version) v1.1"
    source: { size: number; md5: string }; // the player's PS1 Track 1
    hollow: { md5: string }; // after the patch, before the converters
    target: { size: number; md5: string };
    xaLba: number; // SD/XA_STR1.
    spans: Span[];
    bestiaryBits: number[];
    music: { tracks: MusicTrack[]; minSeconds: number };
    dub: DubStream[]; // the PSP / JP dub lines (mods/sotn-ultimate/tools/psp_dub.py)
    songs: SongStream[]; // gen_saturn_music.py ONESHOTS
    gruntNotes: Record<string, number[]>; // sfx_config.yaml vab 1: "prog,tone" -> notes
    sfx: SfxManifest; // the PSP stage / familiar banks (mods/sotn-ultimate/tools/builder/node/sfx_manifest.py)
    pspText: { files: string[] }; // the files text runs copy from (by index): PSPBIN names, "sat:" Saturn root files
    ric2Syms: Ric2Syms; // link map addresses build_ric2.py & co. use (src/ric2.ts)
    sfxCfg: Record<string, SfxCfg>; // sfx_config.yaml entries the Maria / CEN banks use, by hex PS1 id
}

// one dub line: an .at3 on the PSP disc, or a Saturn JP take (SD/SDF0.PCM)
export interface DubStream {
    stream: string; // "en/011a", "jp/sat_3BA"...
    src: { psp: string } | { saturn: number }; // a PSP disc path, or a Saturn voice id
    gain: string; // as the build passed it (repr of a double)
}

export interface SongStream {
    stream: string;
    psp: string; // its path on the PSP disc
    gain: string;
    silence: number;
}

export interface Discs {
    ps1: ByteSource; // the PS1 Track 1 image itself (the patch's source)
    ps1Fs: IsoFs;
    saturn: IsoFs;
    psp: IsoFs;
    out?: IsoFs; // the disc being built (buildDisc sets it): a mod tool's file a converter starts from
}

export interface Tools {
    audio: () => Promise<AudioModule>; // a fresh audio module (one per converter run)
    satconv?: SatConvFactory; // a fresh Saturn map converter (the stages)
}

// the Saturn map converter's outputs of a build, shared by the stage converters
// (the Hell Garden's atlas needs the Garden's)
const stageWorks = new WeakMap<Discs, Map<string, Map<string, Uint8Array>>>();

function stageConverter(st: string): Converter {
    return async (d, m, t) => {
        if (!stageWorks.has(d)) {
            stageWorks.set(d, new Map());
        }
        const r = await buildStageAll(st, t.satconv!, (p) => d.saturn.readFile(p), (p) => d.ps1Fs.readFile(p),
                                      stageWorks.get(d));
        const te = STAGE_DISC[st];
        const out = new Map<string, Uint8Array>([["ST/" + te + "/F_" + te + ".BIN", r.fte]]);
        for (const s of m.spans ?? []) {
            const a = s.kind === "array" && s.gen === "stage_" + st ? r.arrays.get(s.name) : undefined;
            if (s.kind === "array" && a) {
                out.set(s.path + "@" + s.name, a);
            }
        }
        return out;
    };
}

export type Log = (step: string, done?: number, total?: number) => void;

// a fresh audio module every this many voice lines: one module slows down
// call after call and fails after a few hundred decodes (something in the
// FFmpeg + satxa module builds up), a fresh one costs a few ms
export const VOICES_PER_MODULE = 32;

// converter name -> the files / streams it makes (name -> bytes)
type Converter = (d: Discs, m: Manifest, t: Tools, log: Log) => Promise<Map<string, Uint8Array>>;

const SECTOR = 2048;

function padSectors(b: Uint8Array, sectors?: number): Uint8Array {
    const out = new Uint8Array((sectors ?? Math.ceil(b.length / SECTOR)) * SECTOR);
    out.set(b);
    return out;
}

function concat(...bs: Uint8Array[]): Uint8Array {
    const out = new Uint8Array(bs.reduce((s, b) => s + b.length, 0));
    let o = 0;
    for (const b of bs) {
        out.set(b, o);
        o += b.length;
    }
    return out;
}

// a room generator's mod_rooms.h arrays for its array spans ("ST/NP3/NP3.BIN"
// -> src/st/np3/mod_rooms.h)
function roomArrays(r: GenResult, m: Manifest, gen: string): Map<string, Uint8Array> {
    const out = new Map<string, Uint8Array>();
    for (const s of m.spans ?? []) {
        if (s.kind !== "array" || s.gen !== gen) {
            continue;
        }
        const header = "src/st/" + s.path.split("/")[1].toLowerCase() + "/mod_rooms.h";
        const a = r.headers.get(header)?.get(s.name);
        if (a) {
            out.set(s.path + "@" + s.name, cArrayBytes(a.size, a.values));
        }
    }
    return out;
}

// src/mod_tables.ts headers -> the disc file they are compiled into
const TABLE_DISC: Record<string, string> = {
    "src/st/np3/mod_sconce.h": "ST/NP3/NP3.BIN", "src/st/no3/mod_sconce.h": "ST/NO3/NO3.BIN",
    "src/st/rno3/mod_sconce.h": "ST/RNO3/RNO3.BIN",
    "src/st/no3/mod_horse.h": "ST/NO3/NO3.BIN", "src/dra/gen/us/equipment_mod.h": "DRA.BIN",
    "src/dra/gen/us/enemydefs_mod.h": "DRA.BIN", "src/st/lib/mod_bestiary.h": "ST/LIB/LIB.BIN",
};

// byte ranges as self-describing keys ("<disc>@r_<hex offset>")
function rangeKeys(ranges: Map<string, { offset: number; bytes: Uint8Array }[]>): Map<string, Uint8Array> {
    const out = new Map<string, Uint8Array>();
    for (const [disc, rs] of ranges) {
        for (const r of rs) {
            out.set(disc + "@r_" + r.offset.toString(16), r.bytes);
        }
    }
    return out;
}

// a C array of numbers as compiled (little endian, size bytes each)
function cArrayBytes(size: number, values: number[]): Uint8Array {
    const out = new Uint8Array(values.length * size);
    const v = new DataView(out.buffer);
    values.forEach((x, i) => {
        if (size === 1) {
            v.setUint8(i, x & 0xFF);
        } else if (size === 2) {
            v.setUint16(i * 2, x & 0xFFFF, true);
        } else {
            v.setUint32(i * 4, x >>> 0, true);
        }
    });
    return out;
}

export const CONVERTERS: Record<string, [string, Converter]> = {
    monster2: ["Saturn bestiary pictures", async (d, m) => {
        const r = buildMonster2({
            oyaji: await d.saturn.readFile("OYAJI.CHR"),
            library: await d.saturn.readFile("LIBRARY.PRG"),
            monster: await d.ps1Fs.readFile("BIN/MONSTER.BIN"),
            pspMaria: await d.psp.readFile("PSP_GAME/USRDIR/res/ps/hdbin/mo_139.bin"),
            bits: m.bestiaryBits,
        });
        return new Map([["BIN/MONSTER2.BIN", r.monster2], ["BIN/MONSTER.BIN", r.monster]]);
    }],
    // gen_psp_grunts.py: BIN/ALK_PSP.BIN = SD_ALK VH (3 sectors) + VB;
    // BIN/RIH_PSP.BIN = SD_RIH VH (3) + VB (29) + SD_MAR VH (2) + VB (the
    // prologue Maria with her English PSP clips).
    // --set jp: the same from the Japanese pack, in BIN/JP_SD.BIN (ALK, RIH;
    // RIH's SD_MAR with the prologue Maria's Japanese clips)
    psp_grunts: ["PSP player voices", async (d, m, t) => {
        const a = await t.audio();
        const dsp = new Dsp(a as never);
        const RES = "PSP_GAME/USRDIR/res/ps/";
        const out = new Map<string, Uint8Array>();
        for (const jp of [false, true]) {
            const se = RES + (jp ? "sound/SE/" : "snd_e/SE/");
            const bank = async (key: string, vab: string, spk: string) => buildGrunts(a, dsp, key, {
                vh: await d.ps1Fs.readFile(vab + ".VH"), vb: await d.ps1Fs.readFile(vab + ".VB"),
                spk: await d.psp.readFile(se + spk), notes: m.gruntNotes,
            }, jp);
            const [avh, avb] = await bank("ALK", "VAB/SD_ALK", "sd_alk.spk");
            const [rvh, rvb] = await bank("RIH", "VAB/SD_RIH", "sd_rih.spk");
            const vh0 = await d.ps1Fs.readFile("VAB/SD_MAR.VH");
            const vb0 = await d.ps1Fs.readFile("VAB/SD_MAR.VB");
            const r = buildSfxBank(a, dsp, jp ? "SD_MAR_jp" : "SD_MAR_psp", 2, {
                vh0, vb0, vhb: vh0, vbb: vb0, table: m.sfx.tables.mar,
                en: await d.psp.readFile(RES + "snd_e/SE/sd_mar.spk"),
                jp: await d.psp.readFile(RES + "sound/SE/sd_mar.spk"), notes: m.sfx.notes,
            }, jp, "MARY");
            if (!r) {
                throw new Error("SD_MAR: no PSP clip");
            }
            const [mvh, mvb] = r;
            const alk = concat(padSectors(avh, 3), padSectors(avb));
            const rih = concat(padSectors(rvh, 3), padSectors(rvb, 29), padSectors(mvh, 2), padSectors(mvb));
            out.set(jp ? "BIN/JP_SD.BIN#ALK" : "BIN/ALK_PSP.BIN", alk);
            out.set(jp ? "BIN/JP_SD.BIN#RIH" : "BIN/RIH_PSP.BIN", rih);
        }
        return out;
    }],
    // gen_psp_sfx.py: the stage and familiar banks with the PSP's voices
    psp_sfx: ["PSP stage and familiar sounds", async (d, m, t, log) => {
        const RES = "PSP_GAME/USRDIR/res/ps/";
        const rd = {
            ps1: (p: string) => d.ps1Fs.readFile(p),
            out: (p: string) => d.out!.readFile(p),
            psp: (p: string) => d.psp.readFile(RES + p),
        };
        const dsp = (a: AudioModule) => new Dsp(a as never);
        const n = m.sfx.jobs.length;
        const psp = await buildSfxPsp(t.audio, dsp, m.sfx, rd,
                                      (i) => log("Converting the PSP stage and familiar sounds", i, 2 * n));
        // --set jp: SFX_JP.BIN, BIN/JP_SD.BIN's last part
        const jp = await buildSfxJp(t.audio, dsp, m.sfx, rd, psp,
                                    (i, k) => log("Converting the PSP stage and familiar sounds", 2 * n - k + i, 2 * n));
        // BIN/NO3RIDE.BIN's second part: Richter's / Maria's NO3 bank from
        // the PSP dub's (gen_psp_sfx.main; from ours when it has none)
        let vh = await rd.out("ST/NO3/SD_ZKNO3.VH");
        let vb = await rd.out("ST/NO3/SD_ZKNO3.VB");
        const no3 = psp.banks.get("NO3");
        if (no3) {
            const at = (no3 - 1) * 2048;
            const vhs = Math.ceil(vh.length / 2048) * 2048;
            vh = psp.data.slice(at, at + vh.length);
            vb = psp.data.slice(at + vhs, at + vhs + vb.length);
        }
        const a = await t.audio();
        const ride = rideBank(a, dsp(a), vh, vb, await rideInputs(rd.ps1, await d.saturn.readFile("SD/SDD2.PCM"),
                                                                     m.sfxCfg));
        return new Map([["BIN/SFX_PSP.BIN", psp.data], ["BIN/JP_SD.BIN#SFX", jp.data],
                        ["BIN/NO3RIDE.BIN#PSP", ride]]);
    }],
    // the Saturn Maria's banks (src/mar_sounds.ts): MARIA_SD.BIN, and with
    // the PSP Maria's English lines MARIA_SD_PSP.BIN, also MARIA_SD_PS1.BIN's
    // first part (the normal build's PS1 dub row: a copy of it)
    mar_sounds: ["Saturn Maria's voice and sounds", async (d, m, t) => {
        const inp = {
            saturn: d.saturn,
            ps1: (p: string) => d.ps1Fs.readFile(p),
            pspMar2: await d.psp.readFile("PSP_GAME/USRDIR/res/ps/snd_e/SE/sd_mar2.spk"),
            cfg: m.sfxCfg,
        };
        const dsp = new Dsp((await t.audio()) as never);
        const sat = await buildMariaSd(t.audio, dsp, inp, false);
        const psp = await buildMariaSd(t.audio, dsp, inp, true);
        return new Map([["BIN/MARIA_SD.BIN", sat.data], ["BIN/MARIA_SD_PSP.BIN", psp.data],
                        ["BIN/MARIA_SD_PS1.BIN#PSP", psp.data]]);
    }],
    // the PSP Maria's voice (src/mar2_sounds.ts): MARIA2_SD.BIN, also
    // MARIA_SD_PS1.BIN's second part (the normal build: a copy of it); the
    // Japanese clips' version: BIN/JP_SD.BIN's MAR2 part
    mar2_sounds: ["PSP Maria's voice", async (d, m, t) => {
        const SE = "PSP_GAME/USRDIR/res/ps/";
        const dsp = new Dsp((await t.audio()) as never);
        const ps1 = (p: string) => d.ps1Fs.readFile(p);
        const en = await buildMaria2Sd(dsp, await d.psp.readFile(SE + "snd_e/SE/sd_mar2.spk"), ps1, m.sfxCfg);
        const jp = await buildMaria2Sd(dsp, await d.psp.readFile(SE + "sound/SE/sd_mar2.spk"), ps1, m.sfxCfg);
        return new Map([["BIN/MARIA2_SD.BIN", en], ["BIN/MARIA_SD_PS1.BIN#MAR2", en], ["BIN/JP_SD.BIN#MAR2", jp]]);
    }],
    // the Saturn Maria boss's sounds in CEN's bank (src/cen_sounds.ts):
    // ST/CEN/SD_ZKCEN.VH/.VB, and as one file CENP.BIN's part at 0x7C800
    cen_sounds: ["Saturn Maria boss sounds", async (d, m, t) => {
        const dsp = new Dsp((await t.audio()) as never);
        const r = await buildCen(t.audio, dsp, d.saturn, (p) => d.ps1Fs.readFile(p), m.sfxCfg);
        return new Map([["ST/CEN/SD_ZKCEN.VH", r.vh], ["ST/CEN/SD_ZKCEN.VB", r.vb], ["BIN/CENP.BIN#CEN_PS1", r.ps1]]);
    }],
    // the PSP Maria boss's CEN banks (src/cenp_sounds.ts): CENP.BIN's parts
    // at 0x60000 (PSP dub) and 0x99000 (PS1 dub), BIN/JP_SD.BIN's CENP
    // the Saturn Maria boss fight's art, tables and scripts (src/cen_maria.ts,
    // mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h), compiled into CEN.BIN and CENP.BIN
    cen_maria: ["Saturn Maria boss data", async (d, m) => {
        const arrays = await cenMaria((p) => d.saturn.readFile(p));
        const out = new Map<string, Uint8Array>();
        for (const s of m.spans) {
            const a = s.kind === "array" && s.gen === "cen_maria" ? arrays.get(s.name) : undefined;
            if (s.kind === "array" && a) {
                out.set(s.path + "@" + s.name, cArrayBytes(a.size, a.values));
            }
        }
        return out;
    }],
    // the Saturn title portraits on the select screen (src/title_portraits.ts,
    // src/st/sel/mod_charsel_gfx.h), in SEL.BIN and SELRIC2.BIN
    title_portraits: ["Saturn title portraits", async (d, m) => {
        const r = await buildTitlePortraits((p) => d.saturn.readFile(p));
        const arrays = new Map(r.arrays.map((a) => [a.name, a]));
        const out = new Map<string, Uint8Array>();
        for (const s of m.spans) {
            const a = s.kind === "array" && s.gen === "title_portraits" ? arrays.get(s.name) : undefined;
            if (s.kind === "array" && a) {
                out.set(s.path + "@" + s.name, cArrayBytes(a.size, a.values));
            }
        }
        return out;
    }],
    // the Saturn equipment icons and palettes in DRA.BIN (src/saturn_icons.ts)
    saturn_icons: ["Saturn equipment icons", async (d) => {
        const r = await buildSaturnIcons((p) => d.saturn.readFile(p));
        return new Map([...r.icons.map((i): [string, Uint8Array] => ["DRA.BIN@icon_" + i.slot, i.bytes]),
                        ...r.palettes.map((p): [string, Uint8Array] => ["DRA.BIN@pal_" + p.slot, p.bytes])]);
    }],
    // the Saturn Maria's HUD art in F_GAME2 (src/mar_hud.ts), the castle map
    // with the Saturn's Garden cells (src/fmap.ts)
    mar_hud: ["Saturn Maria HUD", async (d) => new Map([
        ["BIN/F_GAME2.BIN", await buildMarHud((p) => d.saturn.readFile(p), (p) => d.ps1Fs.readFile(p))]])],
    fmap: ["castle map", async (d) => new Map([
        ["F_MAP.BIN", await buildFmap((p) => d.saturn.readFile(p), (p) => d.ps1Fs.readFile(p))]])],
    // the Saturn rooms grafted into PS1 stages (src/mod_rooms.ts: NP3, NO3,
    // RNO3, WRP, RWRP's mod_rooms.h and F_ files; src/prs_links.ts: NO0, NO4,
    // RNO0, RNO4's mod_rooms.h and tilemaps edited in place)
    mod_rooms: ["Saturn rooms in PS1 stages", async (d, m) => {
        const r = await buildModRooms((p) => d.saturn.readFile(p), (p) => d.ps1Fs.readFile(p));
        const out = roomArrays(r, m, "mod_rooms");
        for (const [repo, bytes] of r.binaries) {
            const st = /F_(\w+)\.BIN$/.exec(repo)![1];
            out.set("ST/" + st + "/F_" + st + ".BIN", bytes);
        }
        return out;
    }],
    prs_links: ["Saturn room links in PS1 stages", async (d, m) => {
        const r = await buildPrsLinks((p) => d.saturn.readFile(p), (p) => d.ps1Fs.readFile(p));
        const out = roomArrays(r, m, "prs_links");
        for (const [repo, e] of r.tilemapEdits) {
            out.set(e.overlay + "@" + e.name, r.binaries.get(repo)!);
        }
        return out;
    }],
    // Self-describing converters: each makes everything it can, keyed
    // "<disc file>@<name>" (a C symbol, "symbol+0xOFF" for a part of a struct
    // array, "r_<hex offset>" for a byte range); the release keeps as spans
    // the ones it finds on the disc as they are (node/make_hollow.ts).
    // the Saturn playable Maria's art and tables in MARIA.BIN (src/mar_assets.ts)
    mar_assets: ["Saturn Maria", async (d) => {
        const [main, extra] = await marAssetsAll((p) => d.saturn.readFile(p), (p) => d.ps1Fs.readFile(p));
        const out = new Map<string, Uint8Array>();
        for (const h of [main, extra]) {
            for (const arrays of h.values()) {
                for (const [n, a] of arrays) {
                    out.set("BIN/MARIA.BIN@" + n, cArrayBytes(a.size, a.values));
                }
            }
        }
        return out;
    }],
    // Saturn tables: the NP3 / NO3 / RNO3 sconce, the NO3 horse, the Saturn items,
    // enemies and bestiary list (src/mod_tables.ts)
    mod_tables: ["Saturn items, enemies and props", async (d) => {
        const sat = (p: string) => d.saturn.readFile(p);
        const ps1 = (p: string) => d.ps1Fs.readFile(p);
        const out = new Map<string, Uint8Array>();
        const add = (t: ModTables): void => {
            for (const [header, arrays] of t) {
                const disc = TABLE_DISC[header];
                for (const [n, a] of disc ? arrays : []) {
                    out.set(disc + "@" + n, modArrayBytes(a));
                }
            }
        };
        add((await sconces(sat)).tables);
        add(await no3Horse(sat));
        add(await saturnEquipment(sat, ps1));
        add(await saturnEnemies(sat, ps1));
        add(await saturnBestiary(sat, ps1, false));
        return out;
    }],
    // Richter's Saturn coat (src/ric2.ts): RIC2.BIN, BO6RIC2.BIN, SELRIC2.BIN,
    // TOP's kneeling Richter; read over the disc being built
    ric2: ["Richter's Saturn coat", async (d, m) => {
        const r = await buildRic2((p) => d.saturn.readFile(p), (p) => d.ps1Fs.readFile(p),
                                  (p) => d.out!.readFile(p), m.ric2Syms);
        return rangeKeys(r.ranges);
    }],
    // the Saturn weapons 60-63 (src/saturn_weapons.ts): WEAPON2.BIN's art and
    // each slot's frame data, the food art in WEAPON0/1.BIN
    saturn_weapons: ["Saturn weapons", async (d) => {
        const r = await buildSaturnWeapons((p) => d.saturn.readFile(p));
        const out = rangeKeys(r.ranges);
        for (const [header, arrays] of r.arrays) {
            const w = Number(/w_(\d+)_data\.h$/.exec(header)![1]);
            for (const hand of [0, 1]) {
                for (const [n, a] of arrays) {
                    out.set("BIN/WEAPON2.BIN#" + w + "." + hand + "@" + n, cArrayBytes(a.size, a.values));
                }
            }
        }
        return out;
    }],
    // the PS1's own voices made mono (src/xa_mono.ts), from the player's disc
    xa_mono: ["PS1 voices", async (d, m, t, log) => {
        const rx = await d.ps1Fs.lookup("SD/XA_STR1.");
        const out = new Map<string, Uint8Array>();
        for (const s of m.spans ?? []) {
            if (s.kind !== "xamono") {
                continue;
            }
            out.set("xa_mono", await monoPayloads(s.runs, (x, n) => d.ps1.read((rx.lba + x) * 2352, n * 2352),
                                                  (a, b) => log("Converting the PS1 voices", a, b)));
        }
        return out;
    }],
    // the Saturn-only stages (src/stage_build.ts): TE1-TE4.BIN's generated
    // arrays and F_TE1-4.BIN
    stage_gdn: ["Underground Garden", stageConverter("gdn")],
    stage_prs: ["Cursed Prison", stageConverter("prs")],
    stage_rgdn: ["Hell Garden", stageConverter("rgdn")],
    stage_rprs: ["Soul Prison", stageConverter("rprs")],
    // the Saturn-only stages' banks (src/stage_sounds.ts): ST/TE1..TE4's
    // the PSP English script's text (src/psp_text.ts), per disc file
    psp_text: ["PSP English script", async (d, m) => {
        // the files the runs use: the UMD's PSPBIN, the Saturn disc's root ("sat:"),
        // the UMD's res/ps beside PSPBIN ("res:")
        const used = new Set<number>();
        for (const s of m.spans) {
            if (s.kind === "text") {
                for (const r of s.runs) {
                    used.add(r[1]);
                }
            }
        }
        const corpus: Uint8Array[] = [];
        for (let i = 0; i < m.pspText.files.length; i++) {
            const f = m.pspText.files[i];
            corpus.push(!used.has(i) ? new Uint8Array(0) : f.startsWith("sat:") ? await d.saturn.readFile(f.slice(4))
                : f.startsWith("res:") ? await d.psp.readFile("PSP_GAME/USRDIR/res/ps/" + f.slice(4))
                : await d.psp.readFile("PSP_GAME/USRDIR/res/ps/PSPBIN/" + f));
        }
        const out = new Map<string, Uint8Array>();
        for (const s of m.spans) {
            if (s.kind === "text") {
                out.set(s.key ?? s.path, pspTextBytes(s.runs, corpus));
            }
        }
        return out;
    }],
    stage_sounds: ["Saturn stage sounds", async (d, m, t, log) => {
        const dsp = new Dsp((await t.audio()) as never);
        const out = new Map<string, Uint8Array>();
        for (let i = 0; i < STAGE_BANKS.length; i++) {
            log("Converting the Saturn stage sounds", i, STAGE_BANKS.length);
            const b = STAGE_BANKS[i];
            const r = await buildStageBank(t.audio, dsp, d.saturn, (p) => d.ps1Fs.readFile(p), m.sfxCfg, b);
            out.set(b.out + ".VH", r.vh);
            out.set(b.out + ".VB", r.vb);
        }
        return out;
    }],
    // NO3's bank with the Saturn's hoofbeat, and Richter's / Maria's with
    // its neigh too: BIN/NO3RIDE.BIN's first part (src/no3_sounds.ts)
    no3_sounds: ["the horse ride's sounds", async (d, m, t) => {
        const a = await t.audio();
        const [vh, vb, ride] = await buildNo3Bank(a, new Dsp(a as never), (p) => d.ps1Fs.readFile(p),
                                                  await d.saturn.readFile("SD/SDD2.PCM"), m.sfxCfg);
        return new Map([["ST/NO3/SD_ZKNO3.VH", vh], ["ST/NO3/SD_ZKNO3.VB", vb], ["BIN/NO3RIDE.BIN#PS1", ride]]);
    }],
    cenp_sounds: ["PSP Maria boss sounds", async (d, m, t) => {
        const SE = "PSP_GAME/USRDIR/res/ps/";
        const dsp = new Dsp((await t.audio()) as never);
        const r = await buildCenp(t.audio, dsp, d.saturn, (p) => d.ps1Fs.readFile(p),
                                  await d.psp.readFile(SE + "snd_e/SE/sd_mar2.spk"),
                                  await d.psp.readFile(SE + "sound/SE/sd_mar2.spk"), m.sfxCfg);
        return new Map([["BIN/CENP.BIN#PSP", r.psp], ["BIN/CENP.BIN#PSP_PS1", r.ps1], ["BIN/JP_SD.BIN#CENP", r.jp]]);
    }],
    saturn_music: ["Saturn music", async (d, m, t, log) => {
        const satxa = await t.audio();
        const out = new Map<string, Uint8Array>();
        const tracks = m.music.tracks;
        for (let i = 0; i < tracks.length; i++) {
            log("Converting the Saturn music", i, tracks.length);
            out.set(tracks[i].pcm, await encodeSaturnTrack(d.saturn, satxa, tracks[i], m.music.minSeconds));
            await new Promise((res) => setTimeout(res, 0));
        }
        log("Converting the Saturn music", tracks.length, tracks.length);
        return out;
    }],
    psp_dub: ["PSP and Japanese voices", async (d, m, t, log) => {
        let a = await t.audio();
        const out = new Map<string, Uint8Array>();
        let zero: Uint8Array | null = null;
        let sdf0: Uint8Array | null = null;
        for (let i = 0; i < m.dub.length; i++) {
            if (i > 0 && i % VOICES_PER_MODULE === 0) {
                a = await t.audio();
            }
            const s = m.dub[i];
            let src: Uint8Array;
            if ("psp" in s.src) {
                src = await d.psp.readFile(s.src.psp);
            } else {
                zero ??= await d.saturn.readFile("0.BIN");
                sdf0 ??= await d.saturn.readFile("SD/SDF0.PCM");
                src = makeWav(saturnClip(zero, sdf0, s.src.saturn), SATURN_VOICE_RATE);
            }
            out.set(s.stream, encodeVoice(a, src, Number(s.gain)).xa);
            if (i % 8 === 0) {
                log("Converting the PSP and Japanese voices", i, m.dub.length);
                await new Promise((res) => setTimeout(res, 0));
            }
        }
        return out;
    }],
    psp_song: ["PSP fairy song", async (d, m, t) => {
        const a = await t.audio();
        const out = new Map<string, Uint8Array>();
        for (const s of m.songs) {
            out.set(s.stream, encodeSong(a, await d.psp.readFile(s.psp), Number(s.gain), s.silence).xa);
        }
        return out;
    }],
};

async function md5Chunked(b: Uint8Array, log: Log, step: string): Promise<string> {
    const h = new Md5();
    const CH = 32 << 20;
    for (let i = 0; i < b.length; i += CH) {
        h.update(b.subarray(i, Math.min(b.length, i + CH)));
        log(step, Math.min(b.length, i + CH), b.length);
        await new Promise((res) => setTimeout(res, 0));
    }
    return h.hex();
}

function writeXa(img: Uint8Array, xaLba: number, s: XaSpan, data: Uint8Array): void {
    for (const [at, stride, count, first] of s.runs) {
        for (let k = 0; k < count; k++) {
            const src = (first + k) * 2352;
            img.set(data.subarray(src + 24, src + 2348), (xaLba + at + k * stride) * 2352 + 24);
        }
    }
}

export async function buildDisc(d: Discs, patch: Uint8Array, m: Manifest, t: Tools, log: Log): Promise<Uint8Array> {
    if (d.ps1.size !== m.source.size) {
        throw new Error("the PS1 Track 1 has the wrong size (" + d.ps1.size + " bytes, expected " +
                        m.source.size + "): use the Redump dump's Track 1 .bin");
    }
    const img = new Uint8Array(m.target.size);
    const step = "Applying the patch to your PS1 disc";
    log(step);
    const n = await vcdiffDecode(patch, d.ps1, img, (a, b) => log(step, a, b));
    if (n !== m.target.size) {
        throw new Error("the patch made " + n + " bytes, expected " + m.target.size);
    }
    if (await md5Chunked(img, log, "Checking the patched disc") !== m.hollow.md5) {
        throw new Error("the patched disc is not as expected: is this the right PS1 disc and patch?");
    }
    d = { ...d, out: await IsoFs.open({ size: img.length, read: async (o, n) => img.slice(o, o + n) }) };
    for (const g of [...new Set(m.spans.map((s) => s.gen))]) {
        const [label, conv] = CONVERTERS[g];
        log("Converting the " + label);
        const made = await conv(d, m, t, log);
        for (const s of m.spans.filter((x) => x.gen === g)) {
            const name = s.kind === "xa" ? s.stream : s.kind === "xamono" ? "xa_mono"
                : s.kind === "array" ? (s.key ?? s.path + "@" + s.name)
                : s.kind === "text" ? (s.key ?? s.path)
                : s.kind === "file" && s.part ? s.path + "#" + s.part : s.path;
            let data = made.get(name);
            if (data && s.kind === "file" && s.part && data.length < s.size) {
                data = padSectors(data, s.size / SECTOR); // a part runs to its next one
            }
            if (!data || data.length !== s.size || md5(data) !== s.md5) {
                throw new Error(name + " came out different from the release (" + label +
                                "): please report this, with your disc versions");
            }
            if (s.kind === "file") {
                writeFile(img, s.lba, data);
            } else if (s.kind === "text") {
                writeText(img, s.lba, s.runs, data);
            } else if (s.kind === "array") {
                writeText(img, s.lba, [[s.offset, 0, 0, s.size]], data);
            } else if (s.kind === "xamono") {
                s.runs.forEach(([at], i) => {
                    img.set(data.subarray(i * MONO_PAYLOAD, (i + 1) * MONO_PAYLOAD), (m.xaLba + at) * 2352 + 24);
                });
            } else {
                writeXa(img, m.xaLba, s, data);
            }
        }
    }
    if (await md5Chunked(img, log, "Checking the finished disc") !== m.target.md5) {
        throw new Error("the finished disc's md5 differs from the release");
    }
    log("Done");
    return img;
}
