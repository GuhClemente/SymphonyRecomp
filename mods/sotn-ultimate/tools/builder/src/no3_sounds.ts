// SPDX-License-Identifier: AGPL-3.0-or-later
// mods/sotn-ultimate/tools/gen_no3_sounds.py: NO3's bank with the Saturn's hoofbeat for
// Richter's / Maria's horse ride (SDD2.PCM clip 1, signed 8-bit 22,050 Hz,
// at 11,025 Hz) in SFX_SCIFI_BLAST's slot (0x7D1), with the envelope and
// loudness of the PS1's horse, the Valhalla Knight's gallop (0x719, NO2);
// and Richter's / Maria's NO3 bank (BIN/NO3RIDE.BIN, rideBank): the neigh at
// the gate (clip 2, 22,050 Hz) over Death's two Alucard-only samples
import { type AudioModule } from "./audio.ts";
import { roundHalfEven, type Dsp } from "./dsp.ts";
import { encodeVag } from "./grunts.ts";
import type { SfxCfg } from "./mar_sounds.ts";
import { centre, resample, Vab, type Tone } from "./vab.ts";

const NO3 = "ST/NO3/SD_ZKNO3";
const NO2 = "ST/NO2/SD_ZKNO2";
const CLIP = [0, 0x1AB0];
const RATE = 11025;
const VB_LIMIT = 0x1A610; // the largest retail stage bank (gen_gdn_sounds.py)
const NEIGH = [0x1AC0, 0x7930];
const NEIGH_RATE = 22050;
const NEIGH_ID = "79E";
const NEIGH_OVER = ["7A0", "7A1"];
const SECTOR = 2048;

// what rideBank needs besides the bank: the Saturn's clips, the hoofbeat's
// tone (the Valhalla Knight's) and the sfx config
export interface RideInputs {
    sdd2: Uint8Array;
    gallop: Tone;
    cfg: Record<string, SfxCfg>;
}

export async function rideInputs(ps1: (path: string) => Promise<Uint8Array>, sdd2: Uint8Array,
                                 cfg: Record<string, SfxCfg>): Promise<RideInputs> {
    const sv = new Vab(await ps1(NO2 + ".VH"), await ps1(NO2 + ".VB"));
    const o = cfg["719"];
    return { sdd2, gallop: sv.tones.get(o.prog)![o.tone], cfg };
}

// -> [SD_ZKNO3.VH, SD_ZKNO3.VB, BIN/NO3RIDE.BIN's first part]
export async function buildNo3Bank(a: AudioModule, dsp: Dsp, ps1: (path: string) => Promise<Uint8Array>,
                                   sdd2: Uint8Array, cfg: Record<string, SfxCfg>): Promise<[Uint8Array, Uint8Array, Uint8Array]> {
    const vab = new Vab(await ps1(NO3 + ".VH"), await ps1(NO3 + ".VB"));
    const r = await rideInputs(ps1, sdd2, cfg);
    const o = cfg["719"];
    const e = cfg["7D1"];
    if (!o || !e || o.vabid !== 3 || e.vabid !== 3) {
        throw new Error("no3 sounds: sfx config");
    }
    const slot = vab.tones.get(e.prog)!;
    if (slot[e.tone].vol !== 0 || slot[e.tone + 1].vol !== 0) {
        throw new Error("no3 sounds: the slot is not empty");
    }
    const x = Array.from(sdd2.subarray(CLIP[0], CLIP[1]), (b) => (b >= 128 ? b - 256 : b) * 256);
    vab.vags.push(encodeVag(a, resample(dsp, x, 22050, RATE)[0], RATE));
    const st = r.gallop;
    const [c, f] = centre(dsp, e.note, RATE);
    slot[e.tone] = { ...st, prog: e.prog, vag: vab.vags.length, center: c, shift: f,
                     vol: Math.min(127, roundHalfEven(st.vol * o.volume / e.volume)) };
    const [vh, vb] = vab.write();
    if (vb.length > VB_LIMIT) {
        throw new Error("no3 sounds: NO3's bank too big");
    }
    return [vh, vb, rideBank(a, dsp, vh, vb, r)];
}

// gen_no3_sounds.ride_bank: a part of BIN/NO3RIDE.BIN, NO3's bank vh + vb
// (ours, or the PSP dub's) with the neigh over the two Alucard-only samples,
// then a silent filler as long as the rest; their tones silenced, the neigh's
// tone on it. Its VH in whole sectors, then its VB in whole sectors
export function rideBank(a: AudioModule, dsp: Dsp, vh: Uint8Array, vb: Uint8Array, r: RideInputs): Uint8Array {
    const ride = new Vab(vh, vb);
    const o = r.cfg["719"];
    const vags = NEIGH_OVER.map((i) => ride.tones.get(r.cfg[i].prog)![r.cfg[i].tone].vag);
    const room = ride.vags[vags[0] - 1].length + ride.vags[vags[1] - 1].length;
    const nx = Array.from(r.sdd2.subarray(NEIGH[0], NEIGH[1]), (b) => (b >= 128 ? b - 256 : b) * 256);
    const body = encodeVag(a, nx, NEIGH_RATE);
    if (vags[1] !== vags[0] + 1 || body.length >= room) {
        throw new Error("no3 sounds: the neigh doesn't fit");
    }
    ride.vags[vags[0] - 1] = body;
    ride.vags[vags[1] - 1] = new Uint8Array(room - body.length);
    for (const tones of ride.tones.values()) {
        for (const tn of tones) {
            if (vags.includes(tn.vag)) {
                tn.vol = 0;
            }
        }
    }
    const n = r.cfg[NEIGH_ID];
    const ns = ride.tones.get(n.prog)!;
    if (n.vabid !== 3 || ns[n.tone].vol !== 0 || ns[n.tone + 1].vol !== 0) {
        throw new Error("no3 sounds: the neigh's slot is not empty");
    }
    const st = r.gallop;
    const [cn, fn] = centre(dsp, n.note, NEIGH_RATE);
    ns[n.tone] = { ...st, prog: n.prog, vag: vags[0], center: cn, shift: fn,
                   vol: Math.min(127, roundHalfEven(st.vol * o.volume / n.volume)) };
    const [rvh, rvb] = ride.write();
    if (rvh.length !== vh.length || rvb.length !== vb.length) {
        throw new Error("no3 sounds: the ride's bank changed size");
    }
    const vhLen = Math.ceil(rvh.length / SECTOR) * SECTOR;
    const out = new Uint8Array(vhLen + Math.ceil(rvb.length / SECTOR) * SECTOR);
    out.set(rvh);
    out.set(rvb, vhLen);
    return out;
}
