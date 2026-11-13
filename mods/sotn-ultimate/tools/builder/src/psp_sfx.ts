// SPDX-License-Identifier: AGPL-3.0-or-later
// The PSP dub's stage and familiar sound banks (mods/sotn-ultimate/tools/gen_psp_sfx.py,
// ported). Each PS1 bank's tones whose sample is another recording than the
// PSP's (res/ps/snd_e/SE/sd_<pack>.spk) get the PSP clip of their slot; the
// slot -> spk entry lists come from the US UMD's PS.ELF (gen_psp_sfx.py), shipped
// in the manifest. Same programs / tones / ADSR / volumes; only the sample and
// the centre note change, and the VB keeps its retail length.
import { type AudioModule } from "./audio.ts";
import { npInterp, npLinspace, npMedian, npSum, roundHalfEven, type Dsp } from "./dsp.ts";
import { encodeVag, pitch, spkEntries } from "./grunts.ts";
import { md5 } from "./md5.ts";
import { activeRms, centre, decodeVag, resample, trim, Vab, type Tone } from "./vab.ts";
import { JP_SFX_PITCH } from "./jp_sfx_pitch.ts";

const SAME = 0.3; // correlation from which a PS1 sample is the PSP's recording

// one bank to build: [ovl token or "srv<i>", its bank path on the disc (no
// extension), modded (the build starts from a mod tool's bank: the finished
// disc's file at that path, else the retail one), PSP pack, sfx_config vabid]
export type SfxJob = [string, string, boolean, string, number];

export interface SfxInputs {
    vh0: Uint8Array; vb0: Uint8Array; // the retail bank
    vhb: Uint8Array; vbb: Uint8Array; // the bank the build starts from
    table: [number, number][]; // PS.ELF: spk entry -> (prog, tone)
    en: Uint8Array; // snd_e/SE/sd_<pack>.spk
    jp: Uint8Array; // sound/SE/sd_<pack>.spk
    notes: Record<string, number[]>; // "vabid,prog,tone" -> notes the game keys
}

export function spkVols(d: Uint8Array): number[] {
    const v = new DataView(d.buffer, d.byteOffset, d.byteLength);
    return Array.from({ length: v.getUint32(8, true) }, (_, i) => v.getUint16(0x20 + i * 0x20 + 12, true));
}

function corr(a: Float64Array, b: Float64Array): number {
    const n = Math.min(a.length, b.length);
    const aa = new Float64Array(n);
    const bb = new Float64Array(n);
    const ab = new Float64Array(n);
    for (let i = 0; i < n; i++) {
        aa[i] = a[i] * a[i];
        bb[i] = b[i] * b[i];
        ab[i] = a[i] * b[i];
    }
    const d = Math.sqrt(npSum(aa) * npSum(bb));
    return d ? npSum(ab) / d : 0;
}

function sameRecording(x: Float64Array, y: Float64Array): boolean {
    if (x.length < 64 || y.length < 64) {
        return false;
    }
    let s = corr(x, y);
    const q = y.length / x.length;
    if (0.25 < q && q < 4) {
        const idx = Float64Array.from({ length: y.length }, (_, i) => i);
        s = Math.max(s, corr(x, npInterp(npLinspace(0, y.length - 1, x.length), idx, y)));
    }
    return s >= SAME;
}

// an audible loop: the first end flag repeats (3) back to a loop start (4)
// over blocks with sound (one-shots end (1), or repeat silence)
function loops(b: Uint8Array): boolean {
    let start = 0;
    const n = b.length >= 16 ? Math.floor((b.length - 16) / 16) + 1 : 0;
    for (let i = 0; i < n; i++) {
        const f = b[i * 16 + 1];
        if (f & 4) {
            start = i;
        }
        if (f & 1) {
            if (!(f & 2)) {
                return false;
            }
            for (let k = start; k <= i; k++) {
                for (let j = 2; j < 16; j++) {
                    if (b[k * 16 + j]) {
                        return true;
                    }
                }
            }
            return false;
        }
    }
    return false;
}

function setPitch(t: Tone, total: number): void {
    t.center = Math.ceil(total / 128);
    t.shift = t.center * 128 - total;
    if (t.center < 0 || t.center > 127) {
        throw new Error("centre note out of range");
    }
}

// the note the game keys this tone with: of the sound ids on the slot, the
// one at which the retail sample plays at a sane rate (60 first); only notes
// in the tone's key range play it (gen_psp_sfx.key_note)
function keyNote(dsp: Dsp, notes: number[], tone: Tone): number {
    const inRange = notes.filter((n) => tone.min <= n && n <= tone.max);
    const ok = (inRange.length ? inRange : notes).filter((n) => {
        const r = 44100 * dsp.pow(2, (n * 128 - pitch(tone)) / 1536);
        return 3000 <= r && r <= 48000;
    });
    if (ok.includes(60) || !ok.length) {
        return 60;
    }
    return ok.reduce((m, n) => (Math.abs(n - 60) < Math.abs(m - 60) ? n : m));
}

const same = (a: Uint8Array, b: Uint8Array): boolean => a.length === b.length && a.every((v, i) => v === b[i]);

interface Group {
    tones: [number, number][];
    name: string;
    rate: number;
    pcm: Float64Array;
    ps1: Float64Array;
    en: boolean;
    note: number;
    body: Uint8Array;
    r: number;
    vag: number;
}

// gen_psp_sfx.build_bank: -> [vh, vb, report] or null when nothing changes.
// jp: the clips (and their PSP volumes) come from the Japanese pack;
// prefix: only the clips whose name starts with it
export function buildSfxBank(a: AudioModule, dsp: Dsp, key: string, vabid: number, inp: SfxInputs,
                             jp = false, prefix = ""): [Uint8Array, Uint8Array, string[]] | null {
    const retail = new Vab(inp.vh0, inp.vb0);
    const vab = new Vab(inp.vhb, inp.vbb);
    const tab = inp.table;
    const jpE = spkEntries(inp.jp);
    const en = jp ? jpE : spkEntries(inp.en);
    const vols = spkVols(jp ? inp.jp : inp.en);
    const groups = new Map<string, Group>();
    const silent: [number, number][] = [];
    const report: string[] = [];
    tab.forEach(([p, t], k) => {
        if (k >= en.length || !vab.tones.has(p) || t > 15) {
            return;
        }
        const tone = vab.tones.get(p)![t];
        if (!tone.vag || !tone.vol) {
            return; // the PS1 is silent here: stays so
        }
        const rt = retail.tones.has(p) ? retail.tones.get(p)![t] : null;
        if (!rt || rt.vag !== tone.vag || !same(retail.vags[rt.vag - 1], vab.vags[tone.vag - 1])) {
            return; // a mod tool's sound
        }
        const body = vab.vags[tone.vag - 1];
        const x = Float64Array.from(decodeVag(body));
        const { name, rate, body: clip } = en[k];
        if (!name.startsWith(prefix)) {
            return;
        }
        if (vols[k] === 0) {
            silent.push([p, t]);
            report.push("p" + p + "t" + t + " silent (PSP " + name + " volume 0)");
            return;
        }
        const y = Float64Array.from(decodeVag(clip));
        if (sameRecording(x, y)) {
            return;
        }
        if (loops(body) || loops(clip)) {
            report.push("p" + p + "t" + t + " kept: looping sample");
            return;
        }
        const gk = tone.vag + "/" + md5(clip);
        if (!groups.has(gk)) {
            groups.set(gk, { tones: [], name, rate, pcm: y, ps1: x, en: !same(clip, jpE[k].body),
                             note: 0, body: new Uint8Array(0), r: 0, vag: 0 });
        }
        groups.get(gk)!.tones.push([p, t]);
    });
    // a played tone the PSP's list leaves out (Succubus p3t7: the list has
    // p3t9 there) follows its pair, so no retail recording is left playing
    const inTab = (p: number, t: number): boolean => tab.some(([a, b]) => a === p && b === t);
    const has = (g: Group, p: number, t: number): boolean => g.tones.some(([a, b]) => a === p && b === t);
    for (const [p, tl] of vab.tones) {
        for (let t = 0; t < 16; t++) {
            const tn = tl[t];
            if (inTab(p, t) || !tn.vag || !tn.vol) {
                continue;
            }
            const pals = [...groups.values()].filter((g) => vab.tones.get(g.tones[0][0])![g.tones[0][1]].vag === tn.vag);
            if (!pals.length) {
                continue;
            }
            const g = pals.find((g) => has(g, p, t - 1)) ?? pals[0];
            g.tones.push([p, t]);
            report.push("p" + p + "t" + t + " follows " + g.name + " (not in the PSP list)");
        }
    }
    if (!groups.size && !silent.length) {
        return null;
    }
    for (const [p, t] of silent) {
        vab.tones.get(p)![t].vol = 0;
    }
    // the retail samples no tone plays any more are dropped
    const moved = new Set<string>();
    for (const g of groups.values()) {
        for (const [p, t] of g.tones) {
            moved.add(p + "," + t);
        }
    }
    const usedSet = new Set<number>();
    for (const [p, tl] of vab.tones) {
        for (let t = 0; t < 16; t++) {
            if (tl[t].vag && !moved.has(p + "," + t)) {
                usedSet.add(tl[t].vag);
            }
        }
    }
    const used = [...usedSet].sort((x, y) => x - y);
    const budget = inp.vbb.length - used.reduce((s, i) => s + vab.vags[i - 1].length, 0);
    const ratios: number[] = [];
    for (const g of groups.values()) {
        const [p, t] = g.tones[0];
        const tone = vab.tones.get(p)![t];
        g.note = keyNote(dsp, inp.notes[vabid + "," + p + "," + t] ?? [], tone);
        const rPs1 = 44100 * dsp.pow(2, (g.note * 128 - pitch(tone)) / 1536);
        g.pcm = trim(dsp, g.pcm, g.rate);
        const ra = activeRms(dsp, g.ps1, Math.trunc(rPs1));
        const rb = activeRms(dsp, g.pcm, g.rate);
        if (ra && rb) {
            ratios.push(ra / rb);
        }
    }
    const gain = ratios.length ? Math.min(1.0, npMedian(ratios)) : 1.0;
    let scale = 1.0;
    let usedBytes = 0;
    for (;;) {
        usedBytes = 0;
        for (const g of groups.values()) {
            const r = scale === 1.0 ? g.rate : Math.max(4000, Math.trunc(g.rate * scale));
            let x = Float64Array.from(g.pcm, (v) => v * gain);
            let peak = 0;
            for (const v of x) {
                peak = Math.max(peak, Math.abs(v));
            }
            if (peak > 32000) {
                const k = 32000 / peak;
                x = x.map((v) => v * k);
            }
            g.body = encodeVag(a, r !== g.rate ? resample(dsp, x, g.rate, r)[0] : x, r);
            g.r = r;
            usedBytes += g.body.length;
        }
        if (usedBytes <= budget) {
            break;
        }
        scale -= 0.05;
        if (scale < 0.5) {
            return null; // the clips do not fit: the retail bank stays
        }
    }
    // rebuild the sample list: kept retail samples, then the PSP clips
    const remap = new Map(used.map((old, n) => [old, n + 1]));
    const vags = used.map((i) => vab.vags[i - 1]);
    for (const g of groups.values()) {
        vags.push(g.body);
        g.vag = vags.length;
    }
    for (const [p, tl] of vab.tones) {
        for (let t = 0; t < 16; t++) {
            if (tl[t].vag && !moved.has(p + "," + t)) {
                tl[t].vag = remap.get(tl[t].vag)!;
            }
        }
    }
    // the JP PS1's pitch for the slots its samples play elsewhere (jp only)
    const jpt: Record<string, [number, number]> = jp ? JP_SFX_PITCH[key.startsWith("jp_") ? key.slice(3) : key.slice(0, -3)] ?? {} : {};
    for (const g of groups.values()) {
        const [p0, t0] = g.tones[0];
        const ref = pitch(vab.tones.get(p0)![t0]);
        // a JP PS1 pitch is for its sample at the PSP header's rate: lowered
        // as the clip was resampled
        const adj = roundHalfEven(1536 * dsp.log2(g.rate / g.r));
        let delta: number;
        const j0 = jpt[p0 + "," + t0];
        if (j0) {
            delta = j0[0] * 128 - j0[1] + adj - ref;
        } else {
            const [c, f] = centre(dsp, g.note, g.r);
            delta = c * 128 - f - ref;
        }
        for (const [p, t] of g.tones) {
            const tn = vab.tones.get(p)![t];
            tn.vag = g.vag;
            const j = jpt[p + "," + t];
            setPitch(tn, j ? j[0] * 128 - j[1] + adj : pitch(tn) + delta);
        }
        report.push(g.tones.map(([p, t]) => "p" + p + "t" + t).join(" ") + " <- " + g.name +
                    (g.en ? " EN" : " (PSP JP take)") + " (" + g.r + " Hz)");
    }
    vab.vags = vags;
    let [vh, vb] = vab.write();
    if (vb.length < inp.vbb.length) {
        vab.vags.push(new Uint8Array(inp.vbb.length - vb.length));
        [vh, vb] = vab.write();
    }
    if (vh.length !== inp.vhb.length || vb.length !== inp.vbb.length) {
        throw new Error(key + ": bank size changed");
    }
    report.unshift(groups.size + " clips, " + silent.length + " silenced, gain " + gain.toFixed(2) +
                   ", rate x" + scale.toFixed(2) + ", " + usedBytes + "/" + budget + " bytes");
    return [vh, vb, report];
}

// a Saturn-only stage's bank and the PS1 tones it copies (gen_psp_sfx.py
// SATURN_COPIES): [ovl token, its bank, [[source token, its PS1 bank,
// program, tones (each with tone + 1)]]], disc paths
export type SaturnCopies = [string, string, [string, string, number, number[]][]];

export interface SfxManifest {
    tables: Record<string, [number, number][]>; // PSP pack -> its PS.ELF list
    jobs: SfxJob[]; // gen_psp_sfx.main order
    notes: Record<string, number[]>;
    saturn: SaturnCopies[]; // in token order
}

export interface SfxReaders {
    ps1: (path: string) => Promise<Uint8Array>; // the player's PS1 disc
    out: (path: string) => Promise<Uint8Array>; // the disc being built (a mod tool's bank)
    psp: (path: string) => Promise<Uint8Array>; // res/ps/ on the PSP disc
}

const SECTOR = 2048;

export interface SfxSet {
    data: Uint8Array; // SFX_PSP.BIN / SFX_JP.BIN
    banks: Map<string, number>; // job token -> its bank's sector in data + 1
    dubs: Map<string, [Uint8Array, Uint8Array]>; // job token -> its bank (vh, vb)
    retail: Set<string>; // the jobs that changed nothing
}

const sameTone = (a: Tone, b: Tone) =>
    (Object.keys(a) as (keyof Tone)[]).every((k) => k === "vag" || a[k] === b[k]);

// gen_psp_sfx.saturn_bank: a Saturn stage's bank (vh0, vb0) with its copied
// PS1 tones as the source stages' dub banks have them (a source missing in
// dubs keeps the PS1 tones), or null when nothing changes; same VB length
function saturnBank(vh0: Uint8Array, vb0: Uint8Array,
                    srcs: [Vab, Vab | undefined, number, number[]][]): [Uint8Array, Uint8Array] | null {
    const vab = new Vab(vh0, vb0);
    const orig = [...vab.vags];
    let changed = false;
    for (const [ps1, dub, p, tones] of srcs) {
        if (!dub) {
            continue;
        }
        const done = new Map<number, Uint8Array>();
        for (const t of tones.flatMap((u) => [u, u + 1])) {
            const bt = vab.tones.get(p)![t], rt = ps1.tones.get(p)![t], dt = dub.tones.get(p)![t];
            if (bt.vag < 2) {
                continue; // a pair the copy has not (vag 1: the blank)
            }
            if (!same(orig[bt.vag - 1], ps1.vags[rt.vag - 1])) {
                throw new Error("psp sfx: a Saturn stage's PS1 copy is not the PS1's (p" + p + "t" + t + ")");
            }
            const body = dt.vag ? dub.vags[dt.vag - 1] : orig[bt.vag - 1];
            if (sameTone(dt, rt) && same(body, ps1.vags[rt.vag - 1])) {
                continue;
            }
            const had = done.get(bt.vag);
            if (had && !same(had, body)) {
                throw new Error("psp sfx: a pair's samples differ (p" + p + "t" + t + ")");
            }
            done.set(bt.vag, body);
            vab.vags[bt.vag - 1] = body;
            vab.tones.get(p)![t] = { ...dt, vag: bt.vag };
            changed = true;
        }
    }
    if (!changed) {
        return null;
    }
    const grow = vb0.length - vab.vags.reduce((n, x) => n + x.length, 0);
    if (grow < 0 || grow % 16) {
        throw new Error("psp sfx: a Saturn stage's bank grew");
    }
    const last = vab.vags[vab.vags.length - 1];
    const padded = new Uint8Array(last.length + grow); // never played
    padded.set(last);
    vab.vags[vab.vags.length - 1] = padded;
    const [vh, vb] = vab.write();
    if (vh.length !== vh0.length || vb.length !== vb0.length) {
        throw new Error("psp sfx: a Saturn stage's bank changed size");
    }
    return [vh, vb];
}

type SfxLog = (i: number, n: number, line: string) => void;

// gen_psp_sfx.main / main_jp: the changed banks back to back (VH padded to
// whole sectors, then VB), a bank made twice stored once
async function buildSet(audio: () => Promise<AudioModule>, dsp: (a: AudioModule) => Dsp, m: SfxManifest,
                        rd: SfxReaders, jobs: SfxJob[], jp: boolean, log: SfxLog,
                        psp?: SfxSet): Promise<SfxSet> {
    const parts: Uint8Array[] = [];
    const seen = new Map<string, number>();
    const banks = new Map<string, number>();
    const dubs = new Map<string, [Uint8Array, Uint8Array]>();
    const retail = new Set<string>();
    let sectors = 0;
    const add = (tok: string, vh: Uint8Array, vb: Uint8Array) => {
        const vhLen = Math.ceil(vh.length / SECTOR) * SECTOR;
        const blob = new Uint8Array(vhLen + Math.ceil(vb.length / SECTOR) * SECTOR);
        blob.set(vh);
        blob.set(vb, vhLen);
        const h = md5(blob);
        if (!seen.has(h)) {
            seen.set(h, sectors);
            parts.push(blob);
            sectors += blob.length / SECTOR;
        }
        banks.set(tok, seen.get(h)! + 1);
    };
    for (let i = 0; i < jobs.length; i++) {
        const [tok, src, modded, pack, vabid] = jobs[i];
        const a = await audio(); // a fresh module a bank (see VOICES_PER_MODULE)
        const baseRd = modded ? rd.out : rd.ps1;
        const inp: SfxInputs = {
            vh0: await rd.ps1(src + ".VH"), vb0: await rd.ps1(src + ".VB"),
            vhb: await baseRd(src + ".VH"), vbb: await baseRd(src + ".VB"),
            table: m.tables[pack], en: await rd.psp("snd_e/SE/sd_" + pack + ".spk"),
            jp: await rd.psp("sound/SE/sd_" + pack + ".spk"), notes: m.notes,
        };
        const r = buildSfxBank(a, dsp(a), (jp ? "jp_" : "") + tok, vabid, inp, jp);
        log(i, jobs.length, (jp ? "jp" : "psp") + " sfx " + tok + " (sd_" + pack + "): " + (r ? r[2][0] : "no change"));
        if (!r) {
            retail.add(tok);
            continue;
        }
        dubs.set(tok, [r[0], r[1]]);
        add(tok, r[0], r[1]);
    }
    // the Saturn stages' copies of PS1 tones (gen_psp_sfx.main / main_jp):
    // with the JP dub, a source's JP bank, else its PSP one unless the JP
    // dub plays the retail bank there, and only for a stage with a PSP bank
    for (const [tok, base, copies] of m.saturn) {
        if (psp && !psp.banks.has(tok)) {
            continue;
        }
        const srcs: [Vab, Vab | undefined, number, number[]][] = [];
        for (const [stok, src, p, tones] of copies) {
            const d = dubs.get(stok) ?? (psp && !retail.has(stok) ? psp.dubs.get(stok) : undefined);
            srcs.push([new Vab(await rd.ps1(src + ".VH"), await rd.ps1(src + ".VB")),
                       d && new Vab(d[0], d[1]), p, tones]);
        }
        const r = saturnBank(await rd.out(base + ".VH"), await rd.out(base + ".VB"), srcs);
        if (r) {
            add(tok, r[0], r[1]);
        }
    }
    if (!parts.length) {
        parts.push(new Uint8Array(SECTOR));
    }
    const data = new Uint8Array(parts.reduce((s, b) => s + b.length, 0));
    let o = 0;
    for (const b of parts) {
        data.set(b, o);
        o += b.length;
    }
    return { data, banks, dubs, retail };
}

// BIN/SFX_PSP.BIN: the PSP dub's banks
export function buildSfxPsp(audio: () => Promise<AudioModule>, dsp: (a: AudioModule) => Dsp, m: SfxManifest,
                            rd: SfxReaders, log: SfxLog): Promise<SfxSet> {
    return buildSet(audio, dsp, m, rd, m.jobs, false, log);
}

// SFX_JP.BIN (in BIN/JP_SD.BIN): the JP dub's banks, built again from the
// Japanese pack where it differs from the English one, for the banks the
// PSP dub changed (psp: buildSfxPsp's banks)
export async function buildSfxJp(audio: () => Promise<AudioModule>, dsp: (a: AudioModule) => Dsp, m: SfxManifest,
                                 rd: SfxReaders, psp: SfxSet, log: SfxLog): Promise<SfxSet> {
    const jobs: SfxJob[] = [];
    for (const j of m.jobs) {
        const [tok, , , pack] = j;
        const en = await rd.psp("snd_e/SE/sd_" + pack + ".spk");
        const jp = await rd.psp("sound/SE/sd_" + pack + ".spk");
        if ((!same(en, jp) || tok in JP_SFX_PITCH) && psp.banks.get(tok)) {
            jobs.push(j);
        }
    }
    return buildSet(audio, dsp, m, rd, jobs, true, log, psp);
}
