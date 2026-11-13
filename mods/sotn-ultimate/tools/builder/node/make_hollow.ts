// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/make_hollow.ts FULL.bin US_TRACK1.bin SATURN_TRACK1.bin PSP.iso OUTDIR
// The release side of the builder: blanks every Saturn/PSP span of the full
// image, writes the xdelta3 patch US Track 1 -> hollow image (-S none: the
// page's decoder reads no secondary compression) and manifest.json.
// - file spans: the file's sectors lose their user data + EDC/ECC (the
//   sector headers and subheaders stay)
// - XA spans: each stream is made here exactly as the builder will make it
//   (the same code, from the disc images), checked against the build's own
//   encode where there is one, located in SD/XA_STR1. (xa_map.ts), and
//   those sectors lose their audio
// Run it on the build that made FULL (build/us holds that build's dub.json).
import * as fs from "node:fs";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import { IsoFs } from "../src/disc.ts";
import { md5 } from "../src/md5.ts";
import { encodeSong, encodeVoice, makeWav, saturnClip, SATURN_VOICE_RATE } from "../src/audio.ts";
import { CONVERTERS, VOICES_PER_MODULE, type DubStream, type Manifest, type SongStream } from "../src/build.ts";
import { encodeSaturnTrack, type MusicTrack } from "../src/saturn_music.ts";
import { audioModule } from "./audio_cli.ts";
import { ROOT, bestiaryBits, fileSource, sfxConfig } from "./util.ts";
import { cenCfgIds } from "../src/cen_sounds.ts";
import { cenpCfgIds } from "../src/cenp_sounds.ts";
import { marCfgIds } from "../src/mar_sounds.ts";
import { STAGE_BANKS, stageCfgIds } from "../src/stage_sounds.ts";
import { XaIndex, toRuns } from "./xa_map.ts";
import { PspTextFinder } from "./psp_text_scan.ts";
import { locateArrays, type ArraySource } from "./array_spans.ts";
import { findPspCopies } from "./psp_copy_scan.ts";
import { findMonoRuns } from "./xa_mono_scan.ts";
import { MONO_PAYLOAD } from "../src/xa_mono.ts";
import { symbolAddresses } from "./c_arrays.ts";
import type { Ric2Syms } from "../src/ric2.ts";
import { satconvModule } from "./satconv.ts";
import { buildPrsLinks } from "../src/prs_links.ts";
import { DRA_GFX_EQUIP_ICON, DRA_PAL_EQUIP_ICON, ICON_BYTES, ICONS, PAL_BYTES, PALETTES } from "../src/saturn_icons.ts";
import { pspTextBytes, runSectors } from "../src/psp_text.ts";

const FILE_SPANS: [string, string][] = [
    // NO3's bank first: psp_sfx builds the PSP dub's NO3 bank on top of it
    ["ST/NO3/SD_ZKNO3.VH", "no3_sounds"],
    ["ST/NO3/SD_ZKNO3.VB", "no3_sounds"],
    // the Saturn stages' banks too: their PSP / JP versions follow (psp_sfx)
    ...STAGE_BANKS.flatMap((b): [string, string][] => [[b.out + ".VH", "stage_sounds"], [b.out + ".VB", "stage_sounds"]]),
    ["BIN/MONSTER2.BIN", "monster2"],
    ["BIN/MONSTER.BIN", "monster2"],
    ["BIN/ALK_PSP.BIN", "psp_grunts"],
    ["BIN/RIH_PSP.BIN", "psp_grunts"],
    ["BIN/SFX_PSP.BIN", "psp_sfx"],
    ["BIN/MARIA_SD.BIN", "mar_sounds"],
    ["BIN/MARIA_SD_PSP.BIN", "mar_sounds"],
    ["BIN/MARIA2_SD.BIN", "mar2_sounds"],
    ["ST/CEN/SD_ZKCEN.VH", "cen_sounds"],
    ["ST/CEN/SD_ZKCEN.VB", "cen_sounds"],
    ["BIN/F_GAME2.BIN", "mar_hud"],
    ["F_MAP.BIN", "fmap"],
    ["ST/TE1/F_TE1.BIN", "stage_gdn"],
    ["ST/TE2/F_TE2.BIN", "stage_prs"],
    ["ST/TE3/F_TE3.BIN", "stage_rgdn"],
    ["ST/TE4/F_TE4.BIN", "stage_rprs"],
    ["ST/NP3/F_NP3.BIN", "mod_rooms"],
    ["ST/NO3/F_NO3.BIN", "mod_rooms"],
    ["ST/RNO3/F_RNO3.BIN", "mod_rooms"],
    ["ST/WRP/F_WRP.BIN", "mod_rooms"],
    ["ST/RWRP/F_RWRP.BIN", "mod_rooms"],
];
// parts of files made here: [file, part, gen]; where each part is: partsOf()
// the self-describing converters (src/build.ts) and the link maps of the
// files their symbols are in: [map, address of the file's first byte]
const SELF_GENS = ["mar_assets", "mod_tables", "ric2", "saturn_weapons"];
const WEAPON_FILE = (w: number, hand: number): [string, [string, number]] =>
    ["BIN/WEAPON2.BIN#" + w + "." + hand, ["build/us/mod/weapons/w" + hand + "_0" + w + ".map",
     0x8017A000 + hand * 0x3000 - (((w - 60) * 2 + hand) * 0x7000 + 0x4000)]];
const SELF_MAPS: Record<string, [string, number]> = {
    "BIN/MARIA.BIN": ["build/us/mod/mar/mar.map", 0x8013C000],
    "DRA.BIN": ["build/us/dra.map", 0x800A0000],
    "ST/NP3/NP3.BIN": ["build/us/stnp3.map", 0x80180000],
    "ST/RNO3/RNO3.BIN": ["build/us/strno3.map", 0x80180000],
    "ST/NO3/NO3.BIN": ["build/us/stno3.map", 0x80180000],
    "ST/LIB/LIB.BIN": ["build/us/stlib.map", 0x80180000],
    ...Object.fromEntries([60, 61, 62, 63].flatMap((w) => [WEAPON_FILE(w, 0), WEAPON_FILE(w, 1)])),
};
// generated C data compiled into disc files (node/array_spans.ts); a source
// is used once its gen has a converter in src/build.ts
const ARRAY_SOURCES: ArraySource[] = [
    { gen: "mod_rooms", disc: "ST/NP3/NP3.BIN", files: ["src/st/np3/mod_rooms.h"], map: "build/us/stnp3.map", base: 0x80180000 },
    { gen: "mod_rooms", disc: "ST/NO3/NO3.BIN", files: ["src/st/no3/mod_rooms.h"], map: "build/us/stno3.map", base: 0x80180000 },
    { gen: "mod_rooms", disc: "ST/RNO3/RNO3.BIN", files: ["src/st/rno3/mod_rooms.h"], map: "build/us/strno3.map", base: 0x80180000 },
    { gen: "mod_rooms", disc: "ST/WRP/WRP.BIN", files: ["src/st/wrp/mod_rooms.h"], map: "build/us/stwrp.map", base: 0x80180000 },
    { gen: "mod_rooms", disc: "ST/RWRP/RWRP.BIN", files: ["src/st/rwrp/mod_rooms.h"], map: "build/us/strwrp.map", base: 0x80180000 },
    { gen: "prs_links", disc: "ST/NO4/NO4.BIN", files: ["src/st/no4/mod_rooms.h"], map: "build/us/stno4.map", base: 0x80180000 },
    { gen: "prs_links", disc: "ST/RNO4/RNO4.BIN", files: ["src/st/rno4/mod_rooms.h"], map: "build/us/strno4.map", base: 0x80180000 },
    { gen: "prs_links", disc: "ST/NO0/NO0.BIN", files: ["src/st/no0/mod_rooms.h"], map: "build/us/stno0.map", base: 0x80180000 },
    { gen: "prs_links", disc: "ST/RNO0/RNO0.BIN", files: ["src/st/rno0/mod_rooms.h"], map: "build/us/strno0.map", base: 0x80180000 },
    { gen: "title_portraits", disc: "ST/SEL/SEL.BIN", files: ["src/st/sel/mod_charsel_gfx.h"], map: "build/us/stsel.map", base: 0x80180000 },
    { gen: "title_portraits", disc: "BIN/SELRIC2.BIN", files: ["src/st/sel/mod_charsel_gfx.h"], map: "build/us/stsel.map", base: 0x80180000 },
    { gen: "cen_maria", disc: "ST/CEN/CEN.BIN", files: ["mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h"], map: "build/us/stcen.map", base: 0x80180000 },
    { gen: "stage_gdn", disc: "ST/TE1/TE1.BIN", files: ["mods/sotn-ultimate/src/garden/gdn"], map: "build/us/mod/gdn/stgdn.map", base: 0x80180000 },
    { gen: "stage_prs", disc: "ST/TE2/TE2.BIN", files: ["mods/sotn-ultimate/src/prison/prs"], map: "build/us/mod/prs/stprs.map", base: 0x80180000 },
    { gen: "stage_rgdn", disc: "ST/TE3/TE3.BIN", files: ["mods/sotn-ultimate/src/garden/rgdn"], map: "build/us/mod/rgdn/strgdn.map", base: 0x80180000 },
    { gen: "stage_rprs", disc: "ST/TE4/TE4.BIN", files: ["mods/sotn-ultimate/src/prison/rprs"], map: "build/us/mod/rprs/strprs.map", base: 0x80180000 },
];
const PART_SPANS: [string, string, string][] = [
    ["BIN/JP_SD.BIN", "ALK", "psp_grunts"], ["BIN/JP_SD.BIN", "RIH", "psp_grunts"],
    ["BIN/JP_SD.BIN", "MAR2", "mar2_sounds"], ["BIN/JP_SD.BIN", "CENP", "cenp_sounds"],
    ["BIN/JP_SD.BIN", "SFX", "psp_sfx"],
    ["BIN/MARIA_SD_PS1.BIN", "PSP", "mar_sounds"], ["BIN/MARIA_SD_PS1.BIN", "MAR2", "mar2_sounds"],
    ["BIN/CENP.BIN", "PSP", "cenp_sounds"], ["BIN/CENP.BIN", "CEN_PS1", "cen_sounds"],
    ["BIN/CENP.BIN", "PSP_PS1", "cenp_sounds"],
    ["BIN/NO3RIDE.BIN", "PS1", "no3_sounds"], ["BIN/NO3RIDE.BIN", "PSP", "psp_sfx"],
];

// src/ric2.ts's link map addresses, as build_ric2.py (ric.map: a name's last
// definition) and build_bo6ric2.py & co. (their maps: the first) read them
function ric2SymsOf(): Ric2Syms {
    const read = (map: string, names: string[], last: boolean): Record<string, number> => {
        const out: Record<string, number> = {};
        for (const line of fs.readFileSync(path.join(ROOT, map), "utf8").split("\n")) {
            const m = /^\s+0x([0-9a-f]+)\s+(\w+)\s*$/.exec(line);
            if (m && names.includes(m[2]) && (last || !(m[2] in out))) {
                out[m[2]] = parseInt(m[1], 16);
            }
        }
        for (const n of names) {
            if (!(n in out)) {
                throw new Error(map + ": no " + n);
            }
        }
        return out;
    };
    return {
        ric: read("build/us/ric.map", ["richter_sprites", "spritebank0", "spritebank1"], true),
        bo6: read("build/us/bobo6.map", ["richter_sprites", "D_us_801A39B0", "D_us_801A43A4", "D_us_8019E9A0"], false),
        top: read("build/us/sttop.map", ["richter_sprites", "g_ModTopRic2"], false),
        sel: read("build/us/stsel.map", ["D_8018C718", "D_8018C92C", "D_8018CB4C", "D_8018D5D4"], false),
    } as unknown as Ric2Syms;
}

// file -> part -> [sector, sectors or null: to the end]
function partsOf(): Record<string, Record<string, [number, number | null]>> {
    return {
        // mods/sotn-ultimate/tools/gen_jp_sd.py PARTS
        "BIN/JP_SD.BIN": JSON.parse(python(
            "import gen_jp_sd as J; print(json.dumps({n: [J.offset(n), s] for n, _, s in J.PARTS}))")),
        // mods/sotn-ultimate/tools/build_disc.sh: her PSP bank, then the PSP Maria's (0x42
        // sectors each, src/dra/cd.c MOD_MAR_SD_SECTORS)
        "BIN/MARIA_SD_PS1.BIN": { PSP: [0, 0x42], MAR2: [0x42, null] },
        // mods/sotn-ultimate/tools/build_cenp.sh: CEN banks of 0x1C800 bytes after the code
        // (0x60000): the PSP dub's, the Saturn boss's PS1-dub one, the PSP boss's
        "BIN/CENP.BIN": { PSP: [0x60000 / 2048, 57], CEN_PS1: [0x7C800 / 2048, 57], PSP_PS1: [0x99000 / 2048, 57] },
        // mods/sotn-ultimate/tools/gen_no3_sounds.py: Richter's / Maria's NO3 bank, then the
        // PSP dub's (src/dra/cd.c MOD_NO3RIDE_SECTORS)
        "BIN/NO3RIDE.BIN": { PS1: [0, 57], PSP: [57, 57] },
    };
}
const PSP_RES = "PSP_GAME/USRDIR/res/ps/";

// sfx_config.yaml (vab 1, the player banks): "prog,tone" -> the notes the game keys
function gruntNotes(): Record<string, number[]> {
    return JSON.parse(python(
        "import yaml; c = yaml.safe_load(open('assets/dra/us/sfx_config.yaml')); " +
        "n = {}; [n.setdefault('%d,%d' % (e['prog'], e['tone']), set()).add(e['note']) " +
        "for e in c if e['vabid'] == 1]; " +
        "print(json.dumps({k: sorted(v) for k, v in n.items()}))"));
}

function python(code: string): string {
    return execFileSync(path.join(ROOT, ".venv/bin/python"), ["-c",
        "import sys, json; sys.path.insert(0, 'mods/sotn-ultimate/tools'); " + code], { cwd: ROOT, encoding: "utf8" }).trim();
}

// gen_saturn_music.py TRACKS / MIN_SECONDS / ONESHOTS, read from the script
function musicConfig(): { tracks: MusicTrack[]; minSeconds: number; songs: SongStream[] } {
    const [t, ms, fade, os_, sil, bgm] = python(
        "import gen_saturn_music as g; print(json.dumps(g.TRACKS)); print(g.MIN_SECONDS); " +
        "print(json.dumps([g.jump_fade_ms(p) for _, p, _ in g.TRACKS])); " +
        "print(json.dumps(g.ONESHOTS)); print(g.ONESHOT_SILENCE); print(g.PSP_BGM)").split("\n");
    const bgmPsp = PSP_RES + path.relative(path.join(ROOT, "disks/psp"), bgm);
    return {
        tracks: JSON.parse(t).map(([name, pcm, gain]: [string, string, number], i: number) =>
            ({ name, pcm, gain, fadeMs: JSON.parse(fade)[i] })),
        minSeconds: Number(ms),
        songs: JSON.parse(os_).map(([, at3, gain]: [string, string, number]) => ({
            stream: at3, psp: bgmPsp + "/" + at3, gain: python("print(repr(" + gain + "))"), silence: Number(sil),
        })),
    };
}

// the dub lines the build used (build/us/{psp,jp}_dub/dub.json and each
// line's meta json: its source and gain), once per encoded stream
function dubStreams(): { streams: DubStream[]; built: Map<string, string> } {
    const streams: DubStream[] = [];
    const built = new Map<string, string>(); // stream -> the build's .xa
    for (const [set, dir] of [["en", "build/us/psp_dub"], ["jp", "build/us/jp_dub"]]) {
        const dub = JSON.parse(fs.readFileSync(path.join(ROOT, dir, "dub.json"), "utf8"));
        const seen = new Set<string>();
        for (const e of [...Object.values(dub.replace), ...dub.extra] as { xa: string }[]) {
            if (seen.has(e.xa)) {
                continue;
            }
            seen.add(e.xa);
            const meta = JSON.parse(fs.readFileSync(e.xa.replace(/\.xa$/, ".json"), "utf8"));
            let src: DubStream["src"];
            const sat = /sat_([0-9A-F]+)\.src\.wav$/.exec(meta.src);
            if (sat) {
                src = { saturn: parseInt(sat[1], 16) };
            } else if (meta.src.startsWith("disks/psp/")) {
                src = { psp: PSP_RES + meta.src.slice("disks/psp/".length) };
            } else {
                throw new Error("dub source not on a disc: " + meta.src);
            }
            const stream = set + "/" + meta.name;
            streams.push({ stream, src, gain: meta.gain });
            built.set(stream, e.xa);
        }
    }
    return { streams, built };
}

async function main(): Promise<void> {
    const [full, us, sat, psp, out] = process.argv.slice(2);
    fs.mkdirSync(out, { recursive: true });
    const img = new Uint8Array(fs.readFileSync(full));
    const iso = await IsoFs.open(fileSource(full));
    const spans = [];
    // the file converters, run here from the disc images as the builder runs
    // them: each file must come out as the build made it
    const usSrc = fileSource(us);
    const discs = { ps1: usSrc, ps1Fs: await IsoFs.open(usSrc), saturn: await IsoFs.open(fileSource(sat)),
                    psp: await IsoFs.open(fileSource(psp)), out: iso };
    const sfx = JSON.parse(execFileSync(path.join(ROOT, ".venv/bin/python"),
        [path.join(ROOT, "mods/sotn-ultimate/tools/builder/node/sfx_manifest.py")], { cwd: ROOT, encoding: "utf8" }));
    const sfxCfg = sfxConfig([...new Set([...marCfgIds(), ...cenCfgIds(), ...cenpCfgIds(), ...stageCfgIds(), 0x719, 0x7D1, 0x79E, 0x7A0, 0x7A1])]);
    const ric2Syms = ric2SymsOf();
    const partial = { bestiaryBits: bestiaryBits(), gruntNotes: gruntNotes(), sfx, sfxCfg, ric2Syms } as unknown as Manifest;
    const made = new Map<string, Uint8Array>();
    for (const gen of new Set([...FILE_SPANS.map(([, g]) => g), ...PART_SPANS.map(([, , g]) => g)])) {
        const [, conv] = CONVERTERS[gen];
        for (const [k, v] of await conv(discs, partial, { audio: audioModule, satconv: satconvModule }, () => {})) {
            made.set(k, v);
        }
    }
    for (const [p, gen] of FILE_SPANS) {
        const e = await iso.lookup(p);
        const data = await iso.readFile(p);
        if (md5(made.get(p) ?? new Uint8Array(0)) !== md5(data)) {
            throw new Error(p + ": the converter " + gen + " makes other bytes than the build");
        }
        for (let k = 0; k < Math.ceil(e.size / 2048); k++) {
            img.fill(0, (e.lba + k) * 2352 + 24, (e.lba + k + 1) * 2352);
        }
        spans.push({ kind: "file", path: p, lba: e.lba, size: e.size, md5: md5(data), gen });
        console.log("hollow: %s lba %d, %d bytes", p, e.lba, e.size);
    }
    const parts = partsOf();
    for (const [p, part, gen] of PART_SPANS) {
        const e = await iso.lookup(p);
        const file = await iso.readFile(p);
        const [at, n] = parts[p][part];
        const size = (n ?? Math.ceil(e.size / 2048) - at) * 2048;
        const data = file.slice(at * 2048, at * 2048 + size);
        const mine = new Uint8Array(size);
        mine.set(made.get(p + "#" + part) ?? new Uint8Array(0));
        if (data.length !== size || md5(mine) !== md5(data)) {
            throw new Error(p + " " + part + ": the converter " + gen + " makes other bytes than the build");
        }
        for (let k = 0; k < size / 2048; k++) {
            img.fill(0, (e.lba + at + k) * 2352 + 24, (e.lba + at + k + 1) * 2352);
        }
        spans.push({ kind: "file", path: p, part, lba: e.lba + at, size, md5: md5(data), gen });
        console.log("hollow: %s %s lba %d, %d bytes", p, part, e.lba + at, size);
    }
    // the PSP English script's text (psp_script.py's TEXT tokens, found in
    // the overlays that hold its copies): those bytes and their sectors'
    // EDC/ECC (from which the text could be worked out again)
    const textPlan: { tokens: string[]; files: Record<string, string> } = JSON.parse(execFileSync(
        path.join(ROOT, ".venv/bin/python"), [path.join(ROOT, "mods/sotn-ultimate/tools/builder/node/psp_text_tokens.py")],
        { cwd: ROOT, encoding: "utf8" }));
    // ... and every other Saturn / PSP byte the build took as it is (the PSP
    // Maria's and her boss fight's data, tables, art the mod reuses): the same
    // kind of runs (node/psp_copy_scan.ts), in every file the mod changed,
    // from every file of the UMD's PSPBIN and of the Saturn disc's root
    // ("sat:" names). Runs the same retail PS1 file has stay in the patch.
    const pspBin = [...(await discs.psp.listDir(PSP_RES + "PSPBIN"))].filter(([, e]) => !e.dir)
        .map(([n]) => n).sort();
    const satRoot = [...(await discs.saturn.listDir(""))].filter(([, e]) => !e.dir).map(([n]) => "sat:" + n).sort();
    // the UMD's res/ps files beside PSPBIN (PS.ELF...): "res:"
    const resRoot = [...(await discs.psp.listDir(PSP_RES.replace(/\/$/, "")))].filter(([, e]) => !e.dir)
        .map(([n]) => "res:" + n).sort();
    const corpus: Uint8Array[] = [];
    for (const n of pspBin) {
        corpus.push(await discs.psp.readFile(PSP_RES + "PSPBIN/" + n));
    }
    for (const n of satRoot) {
        corpus.push(await discs.saturn.readFile(n.slice(4)));
    }
    for (const n of resRoot) {
        corpus.push(await discs.psp.readFile(PSP_RES + n.slice(4)));
    }
    const corpusNames = [...pspBin, ...satRoot, ...resRoot];
    const finder = new PspTextFinder(corpus.slice(0, pspBin.length)); // the script text: the UMD's
    const tokens = new Set(textPlan.tokens);
    const usFs = discs.ps1Fs;
    const changed: string[] = [];
    const walkDisc = async (dir: string): Promise<void> => {
        for (const [n, e] of await iso.listDir(dir)) {
            const p = dir ? dir + "/" + n : n;
            if (e.dir) {
                await walkDisc(p);
            } else if (!p.startsWith("SD/XA_STR1")) {
                let retail: Uint8Array | null = null;
                try {
                    retail = await usFs.readFile(p);
                } catch {
                    // a new file
                }
                if (!retail || md5(retail) !== md5(await iso.readFile(p))) {
                    changed.push(p);
                }
            }
        }
    };
    await walkDisc("");
    let textBytes = 0;
    let copyBytes = 0;
    for (const p of [...new Set([...Object.keys(textPlan.files), ...changed])].sort()) {
        const e = await iso.lookup(p);
        const data = await iso.readFile(p);
        let runs = p in textPlan.files ? finder.find(data, tokens) : [];
        if (changed.includes(p)) {
            const covered = new Uint8Array(data.length);
            for (const [dst, , , n] of runs) {
                covered.fill(1, dst, dst + n);
            }
            let retail: Buffer | null = null;
            try {
                retail = Buffer.from(await usFs.readFile(p));
            } catch {
                // a new file
            }
            const copies = findPspCopies(data, corpus, covered).filter(([dst, , , n]) =>
                !retail || retail.indexOf(data.subarray(dst, dst + n)) < 0);
            copyBytes += copies.reduce((s, r) => s + r[3], 0);
            runs = [...runs, ...copies].sort((a, b) => a[0] - b[0]);
        }
        if (!runs.length) {
            continue;
        }
        const text = pspTextBytes(runs, corpus);
        const sectors = new Set<number>();
        for (const [dst, , , n] of runs) {
            for (let k = 0; k < n; k++) {
                const at = dst + k;
                img[(e.lba + Math.floor(at / 2048)) * 2352 + 24 + (at % 2048)] = 0;
            }
            const [a, b] = runSectors(dst, n);
            for (let s = a; s <= b; s++) {
                sectors.add(s);
            }
        }
        for (const s of sectors) {
            img.fill(0, (e.lba + s) * 2352 + 24 + 2048, (e.lba + s + 1) * 2352);
        }
        spans.push({ kind: "text", path: p, lba: e.lba, size: text.length, md5: md5(text), gen: "psp_text", runs });
        textBytes += text.length;
    }
    console.log("hollow: PSP script text and copied PSP data, %d bytes (%d of them copied data)", textBytes, copyBytes);

    // generated data arrays compiled into disc files (node/array_spans.ts),
    // for the generators the builder has a converter for: each converter
    // must make every array as the build compiled it
    const arraySpans = [];
    // where a self-describing key is in its file: "r_<hex>" an offset,
    // "symbol[+0xOFF]" through the file's link map
    const symCache = new Map<string, Map<string, number[]>>();
    const selfOffset = (file: string, name: string): number | undefined => {
        const r = /^r_([0-9a-f]+)$/.exec(name);
        if (r) {
            return parseInt(r[1], 16);
        }
        const lm = SELF_MAPS[file];
        if (!lm) {
            return undefined;
        }
        if (!symCache.has(lm[0])) {
            symCache.set(lm[0], symbolAddresses(path.join(ROOT, lm[0]), ROOT));
        }
        const [sym, off] = name.split("+");
        const a = symCache.get(lm[0])!.get(sym);
        return a && a.length === 1 ? a[0] - lm[1] + (off ? parseInt(off, 16) : 0) : undefined;
    };
    // ... and fixed ranges: the Saturn equipment icons and palettes in DRA.BIN
    {
        const e = await iso.lookup("DRA.BIN");
        const dra = await iso.readFile("DRA.BIN");
        const ranges = [...ICONS.map((s) => ["icon_" + s, DRA_GFX_EQUIP_ICON + s * ICON_BYTES, ICON_BYTES] as const),
                        ...PALETTES.map(([s]) => ["pal_" + s, DRA_PAL_EQUIP_ICON + s * PAL_BYTES, PAL_BYTES] as const)];
        for (const [name, offset, size] of ranges) {
            const bytes = dra.slice(offset, offset + size);
            arraySpans.push({ kind: "array", path: "DRA.BIN", name, lba: e.lba, offset, size, md5: md5(bytes),
                              gen: "saturn_icons", bytes });
        }
    }
    // ... and the PS1 tilemaps the Saturn room links edit in place (where: the
    // port's own result, checked against the converter below)
    {
        const r = await buildPrsLinks((p) => discs.saturn.readFile(p), (p) => discs.ps1Fs.readFile(p));
        for (const [, ed] of r.tilemapEdits) {
            const e = await iso.lookup(ed.overlay);
            const file = await iso.readFile(ed.overlay);
            const size = r.binaries.get([...r.tilemapEdits].find(([, x]) => x === ed)![0])!.length;
            const bytes = file.slice(ed.offset, ed.offset + size);
            arraySpans.push({ kind: "array", path: ed.overlay, name: ed.name, lba: e.lba, offset: ed.offset, size,
                              md5: md5(bytes), gen: "prs_links", bytes });
        }
    }
    for (const src of ARRAY_SOURCES.filter((s) => s.gen in CONVERTERS)) {
        const e = await iso.lookup(src.disc);
        const arrays = locateArrays(src, await iso.readFile(src.disc));
        for (const a of arrays) {
            arraySpans.push({ kind: "array", path: src.disc, name: a.name, lba: e.lba, offset: a.offset,
                              size: a.bytes.length, md5: md5(a.bytes), gen: src.gen, bytes: a.bytes });
        }
    }
    // ... and the self-describing converters' outputs found on the disc as
    // they are (src/build.ts): at their symbol, struct part or offset
    for (const gen of SELF_GENS) {
        const [, conv] = CONVERTERS[gen];
        const got = await conv(discs, { ...partial, spans: [] } as unknown as Manifest,
                               { audio: audioModule, satconv: satconvModule }, () => {});
        let found = 0;
        let missed = 0;
        for (const [key, bytes] of got) {
            const [file, name] = key.split("@");
            const disc = file.split("#")[0];
            const e = await iso.lookup(disc);
            const data = await iso.readFile(disc);
            const at = selfOffset(file, name);
            if (at === undefined || at + bytes.length > data.length || !bytes.every((v, i) => data[at + i] === v)) {
                missed++;
                continue;
            }
            found++;
            arraySpans.push({ kind: "array", path: disc, name, key, lba: e.lba, offset: at, size: bytes.length,
                              md5: md5(bytes), gen, bytes });
        }
        console.log("hollow: %s: %d of %d outputs found on the disc", gen, found, found + missed);
    }
    const arrayGens = new Set(arraySpans.map((s) => s.gen));
    for (const gen of arrayGens) {
        const spansOf = arraySpans.filter((s) => s.gen === gen);
        const [, conv] = CONVERTERS[gen];
        const got = await conv(discs, { ...partial, spans: spansOf } as unknown as Manifest, { audio: audioModule, satconv: satconvModule }, () => {});
        let bytes = 0;
        for (const s of spansOf) {
            const mine = got.get(s.key ?? s.path + "@" + s.name);
            if (!mine || md5(mine) !== s.md5) {
                throw new Error(s.path + " " + s.name + ": the converter " + gen + " makes other bytes than the build");
            }
            const touched = new Set<number>();
            for (let k = 0; k < s.size; k++) {
                const at = s.offset + k;
                img[(s.lba + Math.floor(at / 2048)) * 2352 + 24 + (at % 2048)] = 0;
                touched.add(Math.floor(at / 2048));
            }
            for (const t of touched) {
                img.fill(0, (s.lba + t) * 2352 + 24 + 2048, (s.lba + t + 1) * 2352);
            }
            bytes += s.size;
            const { bytes: _b, ...span } = s;
            spans.push(span);
        }
        console.log("hollow: %s, %d arrays, %d bytes", gen, spansOf.length, bytes);
    }

    const xa = await iso.lookup("SD/XA_STR1.");
    const fd = fs.openSync(full, "r");
    const index = new XaIndex(fd, xa.lba, Math.ceil(xa.size / 2048));
    const taken = new Set<number>();
    const saturn = await IsoFs.open(fileSource(sat));
    const pspFs = await IsoFs.open(fileSource(psp));
    const a = await audioModule();
    // a stream's place on the disc; placed streams are kept for the second
    // pass (morePlaces), which finds their other copies
    const placed: [string, string, Uint8Array][] = [];
    const placeOnce = (name: string, gen: string, stream: Uint8Array, at: number[]): void => {
        for (const i of at) {
            taken.add(i);
            img.fill(0, (xa.lba + i) * 2352 + 24, (xa.lba + i + 1) * 2352);
        }
        spans.push({ kind: "xa", stream: name, size: stream.length, md5: md5(stream), gen, runs: toRuns(at) });
    };
    const hollowStream = (name: string, gen: string, stream: Uint8Array): number => {
        const at = index.place(stream, taken);
        placeOnce(name, gen, stream, at);
        placed.push([name, gen, stream]);
        return at.length;
    };
    // repack_xa.py writes a line once per voice config that plays it: every
    // other copy of a placed stream is its own span of the same stream (after
    // every stream has its first place: copies must not take another's sectors)
    const morePlaces = (): number => {
        let n = 0;
        for (const [name, gen, stream] of placed) {
            for (let copy = 1; copy < 8; copy++) {
                let at: number[];
                try {
                    at = index.place(stream, new Set(taken));
                } catch {
                    break;
                }
                placeOnce(name, gen, stream, at);
                n++;
            }
        }
        return n;
    };

    const music = musicConfig();
    for (const t of music.tracks) {
        const n = hollowStream(t.pcm, "saturn_music", await encodeSaturnTrack(saturn, a, t, music.minSeconds));
        console.log("hollow: %s %s, %d sectors", t.name, t.pcm, n);
    }
    for (const s of music.songs) {
        const n = hollowStream(s.stream, "psp_song", encodeSong(a, await pspFs.readFile(s.psp), Number(s.gain), s.silence).xa);
        console.log("hollow: song %s, %d sectors", s.stream, n);
    }
    const dub = dubStreams();
    // a fresh module, as the builder does per converter: satxa's enc keeps
    // its buffers (a process each in the original CLI), ~50 MB a track
    let av = await audioModule();
    const zero = await saturn.readFile("0.BIN");
    const sdf0 = await saturn.readFile("SD/SDF0.PCM");
    let lines = 0;
    let sectors = 0;
    for (const s of dub.streams) {
        if (lines > 0 && lines % VOICES_PER_MODULE === 0) {
            av = await audioModule(); // as the builder (src/build.ts)
        }
        const src = "psp" in s.src ? await pspFs.readFile(s.src.psp)
            : makeWav(saturnClip(zero, sdf0, s.src.saturn), SATURN_VOICE_RATE);
        let stream: Uint8Array;
        try {
            stream = encodeVoice(av, src, Number(s.gain)).xa;
        } catch (e) {
            throw new Error(s.stream + " (" + JSON.stringify(s.src) + ", " + src.length + " bytes): " + e);
        }
        // the build's encode (psp_dub.py through node/audio_cli.ts) from disks/
        if (md5(stream) !== md5(new Uint8Array(fs.readFileSync(dub.built.get(s.stream)!)))) {
            throw new Error(s.stream + ": the disc-image encode differs from the build's");
        }
        sectors += hollowStream(s.stream, "psp_dub", stream);
        lines++;
    }
    console.log("hollow: dub, %d lines, %d sectors", lines, sectors);
    console.log("hollow: %d more copies of placed streams", morePlaces());

    // the PS1's voices repack_xa.py made mono: the remaining XA_STR1 sectors
    // that are neither blank nor retail ones, made from two retail sectors
    {
        const usImg = new Uint8Array(fs.readFileSync(us));
        const rx = await discs.ps1Fs.lookup("SD/XA_STR1.");
        const rCount = Math.ceil(rx.size / 2048);
        const retailXa = new Set<string>();
        for (let s = 0; s < rCount; s++) {
            const o = (rx.lba + s) * 2352 + 24;
            retailXa.add(md5(usImg.subarray(o, o + 2304)));
        }
        const other: number[] = [];
        for (let s = 0; s < Math.ceil(xa.size / 2048); s++) {
            const o = (xa.lba + s) * 2352 + 24;
            const a = img.subarray(o, o + 2304);
            if (!a.every((v) => v === 0) && !retailXa.has(md5(a))) {
                other.push(s);
            }
        }
        const runs = findMonoRuns(img, xa.lba, other, usImg, rx.lba, rCount);
        const payload = new Uint8Array(runs.length * MONO_PAYLOAD);
        runs.forEach(([at], i) => {
            const o = (xa.lba + at) * 2352;
            payload.set(img.subarray(o + 24, o + 24 + MONO_PAYLOAD), i * MONO_PAYLOAD);
            img.fill(0, o + 24, o + 2352);
        });
        spans.push({ kind: "xamono", size: payload.length, md5: md5(payload), gen: "xa_mono", runs });
        console.log("hollow: PS1 voices made mono, %d of %d sectors (the rest: %d)", runs.length, other.length,
                    other.length - runs.length);
    }

    // a last sweep: Saturn / PSP runs still on the hollow image as they are
    // (bytes the copies above didn't take: a run cut where an array was
    // blanked, art the build recompressed as the PSP has it...), up to the
    // bytes other spans blanked (written later, they stay theirs)
    {
        const fileBytes = (lba: number, size: number): Uint8Array => {
            const d = new Uint8Array(size);
            for (let o = 0; o < size; o += 2048) {
                const s = (lba + o / 2048) * 2352 + 24;
                d.set(img.subarray(s, s + Math.min(2048, size - o)), o);
            }
            return d;
        };
        let swept = 0;
        for (const p of changed) {
            const e = await iso.lookup(p);
            const fullData = await iso.readFile(p);
            const h = fileBytes(e.lba, e.size);
            const covered = new Uint8Array(h.length);
            for (let i = 0; i < h.length; i++) {
                covered[i] = h[i] === 0 && fullData[i] !== 0 ? 1 : 0;
            }
            let retail: Buffer | null = null;
            try {
                retail = Buffer.from(await usFs.readFile(p));
            } catch {
                // a new file
            }
            const runs = findPspCopies(h, corpus, covered).filter(([dst, , , n]) =>
                !retail || retail.indexOf(h.subarray(dst, dst + n)) < 0);
            if (!runs.length) {
                continue;
            }
            const text = pspTextBytes(runs, corpus);
            const sectors = new Set<number>();
            for (const [dst, , , n] of runs) {
                for (let k = 0; k < n; k++) {
                    const at = dst + k;
                    img[(e.lba + Math.floor(at / 2048)) * 2352 + 24 + (at % 2048)] = 0;
                }
                const [a, b] = runSectors(dst, n);
                for (let s = a; s <= b; s++) {
                    sectors.add(s);
                }
            }
            for (const s of sectors) {
                img.fill(0, (e.lba + s) * 2352 + 24 + 2048, (e.lba + s + 1) * 2352);
            }
            spans.push({ kind: "text", path: p, key: p + "#sweep", lba: e.lba, size: text.length, md5: md5(text),
                         gen: "psp_text", runs });
            swept += text.length;
        }
        console.log("hollow: last sweep, %d more bytes copied from the discs", swept);
    }

    const hollow = path.join(out, "hollow.bin");
    fs.writeFileSync(hollow, img);
    const name = "SOTN Ultimate (PS1 Version) v1.4";
    const patch = path.join(out, name + ".builder.xdelta");
    const usSize = fs.statSync(us).size;
    execFileSync("xdelta3", ["-e", "-f", "-9", "-S", "none", "-B", String(usSize), "-s", us, hollow, patch]);
    const manifest = {
        name,
        source: { size: usSize, md5: md5(new Uint8Array(fs.readFileSync(us))) },
        hollow: { md5: md5(img) },
        target: { size: img.length, md5: md5(new Uint8Array(fs.readFileSync(full))) },
        xaLba: xa.lba,
        spans,
        bestiaryBits: partial.bestiaryBits,
        gruntNotes: partial.gruntNotes,
        sfx,
        sfxCfg,
        ric2Syms,
        pspText: { files: corpusNames },
        music: { tracks: music.tracks, minSeconds: music.minSeconds },
        dub: dub.streams,
        songs: music.songs,
    };
    fs.writeFileSync(path.join(out, "manifest.json"), JSON.stringify(manifest));
    fs.rmSync(hollow);
    console.log("patch: %s (%d bytes)", patch, fs.statSync(patch).size);
    console.log("target md5 %s, hollow md5 %s", manifest.target.md5, manifest.hollow.md5);
}

if (import.meta.main) {
    await main();
}
