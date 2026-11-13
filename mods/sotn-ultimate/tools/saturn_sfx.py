#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: render Saturn Castlevania SOTN sound effects to WAV.

The Saturn plays every sound effect as a Sega sound-driver SEQ (MIDI-like
sequence) that triggers TON (tone bank) voices on the SCSP. This tool reads
those from the Saturn disc files and renders a sound id to a 16-bit WAV with
a small SCSP-style software synth, so the result can be packed into a PS1 VAB
(mods/sotn-ultimate/tools/gen_gdn_sounds.py uses render_to_wav()).

Where the data is
-----------------
  0.BIN (loaded at 0x06004080):
    0x0604128C  17-byte records: sound bank file name + flag (0x10 = common
                bank SD_JO10.CRT, 0x11 = player bank SD_ALK/LIH/MARI.CRT,
                0x12 = stage bank); sound id 0xB00 + n loads record n.
    0x06041655  5 bytes per sound id from 0x600 (src/saturn/zero/snddata.c
                g_SaturnSoundConfigs): [0] which SEQ table (0 common,
                1 player, 2 stage "Saturn-only" table, 3 stage table),
                [3] index in that table, [4] priority.
  *.CRT = an image of one area of SCSP sound RAM, big-endian:
    stage bank (163032 bytes): SEQ table A at 0 (type 2), SEQ table B at
      0x800 (type 3, offsets relative to 0x800), TON tone bank at 0x1A60.
    SD_JO10.CRT (common): SEQ table at 0, tone bank at 0x57BC (60 voices).
    SD_ALK.CRT (player):  SEQ table at 0, tone bank at 0x878 (13 voices).
    A SEQ table is u16 count + count u32 offsets. Sample start addresses in
    a tone bank (SA, 20 bits) are relative to the start of that tone bank
    (the driver adds the bank's sound-RAM base), so no RAM offset is needed.
  SEQ bank select (CC32, with CC0 = 0) picks the tone bank: 0 = common
  (SD_JO10), 1 = player (SD_ALK), 2 = the loaded stage bank.

SEQ format (Sega): u16 resolution, u16 tempo event count, u16 data offset,
u16 tempo loop offset, tempo events (u32 steps, u32 microseconds per quarter),
then events. Every event's step byte is the delay (ticks) BEFORE that event
(checked: the first tempo event's step count = last note-on + its gate):
  0x00-0x7F  note: (bits 0-3 channel, 0x40 gate+0x100, 0x20 step+0x100)
             key, velocity, gate, step
  0x81       reference: u16 offset (from data start), u8 event count
  0x83       end
  0x88-0x8B  gate extension +0x200/+0x800/+0x1000/+0x2000 (for next note)
  0x8C-0x8F  step extension +0x100/+0x200/+0x800/+0x1000 (a rest)
  0xA0|ch    poly pressure: key, value, step (ignored)
  0xB0|ch    control change: number, value, step
  0xC0|ch    program change: program, step
  0xD0|ch    channel pressure: value, step (ignored)
  0xE0|ch    pitch bend: value (0x40 centre), step

TON layer (32 bytes): key lo/hi, SCSP slot regs 0x00 (loop mode, PCM8B,
SA hi), SA lo, LSA, LEA, 0x08 (D2R D1R EGHOLD AR), 0x0A (LPSLNK KRS DL RR),
0x0C (SDIR STWINH TL), 0x0E, 0x10, 0x12 (LFO), 0x14 (ISEL IMXL),
[0x18] DISDL<<5|DIPAN, [0x19] base note, [0x1A] fine tune, ...
At the base note the sample plays at 44100 Hz.

Synth approximations (documented in the report):
  - pitch: 44100 * 2^((key - base + fine/128 + bend) / 12), linear
    interpolation between samples (as the SCSP does);
  - bend: (value - 0x40) / 0x40 * range, range = voice header byte 0 & 0xF;
  - EG: MAME scsp.cpp attack/decay time tables (ms for the full 96 dB
    range); attack linear in amplitude, decays/release linear in dB;
    rate = 2*R + key rate scaling (KRS != 0xF);
  - level: velocity through the bank's velocity curve (piecewise linear
    through the table's (point, level) pairs); velocity level, CC7 and CC11
    each attenuate 0.375 dB (one TL step) per step below 127 (VOLUME_MODEL
    "tl"; "linear" = MIDI-style amplitude); TL 0.375 dB/step, DISDL 6 dB/step;
  - pan: CC10 / DIPAN; stereo only when some note is off centre.
"""
import argparse
import math
import os
import struct
import sys
import wave

import numpy as np
from mod_work import work_dir  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
SAT = os.path.join(ROOT, "disks/saturn")
SD = os.path.join(SAT, "SD")
ZERO_BASE = 0x06004080  # load address of 0.BIN
NAMES_ADDR = 0x0604128C
CFG_ADDR = 0x06041655
CFG_FIRST, CFG_COUNT = 0x600, 791
OUT_DIR = work_dir("sfx")
DEFAULT_STAGE = "SD_TEI.CRT"  # the Underground Garden (bank id 0xB35)
SCSP_RATE = 44100
# one slot at full level and centre pan comes out at half scale (MAME scsp.cpp
# shifts the panned slot output by SHIFT+1); keeps the doubled voices in range
MIX_GAIN = 0.5
# how velocity level / CC7 / CC11 become gain: "tl" = attenuation of one SCSP
# TL step (0.375 dB) per step below 127 (the SCSP attenuates in dB);
# "linear" = MIDI-style linear amplitude. Not verified against hardware.
VOLUME_MODEL = "tl"

# MAME scsp.cpp: time (ms) for the attack / for a decay over the full range
AR_TIMES = [1e9, 1e9, 8100.0, 6900.0, 6000.0, 4800.0, 4000.0, 3400.0, 3000.0,
            2400.0, 2000.0, 1700.0, 1500.0, 1200.0, 1000.0, 860.0, 760.0,
            600.0, 500.0, 430.0, 380.0, 300.0, 250.0, 220.0, 190.0, 150.0,
            130.0, 110.0, 95.0, 76.0, 63.0, 55.0, 47.0, 38.0, 31.0, 27.0,
            24.0, 19.0, 15.0, 13.0, 12.0, 9.4, 7.9, 6.8, 6.0, 4.7, 3.8, 3.4,
            3.0, 2.4, 2.0, 1.8, 1.6, 1.3, 1.1, 0.93, 0.85, 0.65, 0.53, 0.44,
            0.40, 0.35, 0.0, 0.0]
DR_TIMES = [1e9, 1e9, 118200.0, 101300.0, 88600.0, 70900.0, 59100.0,
            50700.0, 44300.0, 35500.0, 29600.0, 25300.0, 22200.0, 17700.0,
            14800.0, 12700.0, 11100.0, 8900.0, 7400.0, 6300.0, 5500.0, 4400.0,
            3700.0, 3200.0, 2800.0, 2200.0, 1800.0, 1600.0, 1400.0, 1100.0,
            920.0, 790.0, 690.0, 550.0, 460.0, 390.0, 340.0, 270.0, 230.0,
            200.0, 170.0, 140.0, 110.0, 98.0, 85.0, 68.0, 57.0, 49.0, 43.0,
            34.0, 28.0, 25.0, 22.0, 18.0, 14.0, 12.0, 11.0, 8.5, 7.1, 6.1,
            5.4, 4.3, 3.6, 3.1]
EG_RANGE_DB = 96.0


def u16(d, o):
    return struct.unpack_from(">H", d, o)[0]


def u32(d, o):
    return struct.unpack_from(">I", d, o)[0]


# ---------------------------------------------------------------- 0.BIN
_zero = None


def zero_bin():
    global _zero
    if _zero is None:
        _zero = open(os.path.join(SAT, "0.BIN"), "rb").read()
    return _zero


def sound_config(sid):
    """(table type, index, byte1, byte2, priority) of Saturn sound id"""
    assert CFG_FIRST <= sid < CFG_FIRST + CFG_COUNT, hex(sid)
    o = CFG_ADDR - ZERO_BASE + 5 * (sid - CFG_FIRST)
    e = zero_bin()[o:o + 5]
    return e[0], e[3], e[1], e[2], e[4]


def bank_files():
    """the sound bank file records of 0.BIN: [(name, flag)], index = id-0xB00"""
    d = zero_bin()
    o = NAMES_ADDR - ZERO_BASE
    out = []
    while True:
        rec = d[o:o + 17]
        name = rec[:16].split(b"\0")[0]
        if not name.startswith(b"SD_"):
            break
        out.append((name.decode(), rec[16]))
        o += 17
    return out


# ---------------------------------------------------------------- tones
class Layer:
    def __init__(self, bank, o):
        d = bank.data
        self.lo, self.hi = d[o], d[o + 1]
        r0 = u16(d, o + 2)
        self.loop = (r0 >> 5) & 3
        self.pcm8 = (r0 >> 4) & 1
        self.sa = ((r0 & 0xF) << 16) | u16(d, o + 4)
        self.lsa, self.lea = u16(d, o + 6), u16(d, o + 8)
        a1, a2 = u16(d, o + 10), u16(d, o + 12)
        self.ar, self.eghold = a1 & 0x1F, (a1 >> 5) & 1
        self.d1r, self.d2r = (a1 >> 6) & 0x1F, (a1 >> 11) & 0x1F
        self.rr, self.dl = a2 & 0x1F, (a2 >> 5) & 0x1F
        self.krs = (a2 >> 10) & 0xF
        self.tl = d[o + 15]
        self.disdl, self.dipan = d[o + 24] >> 5, d[o + 24] & 0x1F
        self.base = d[o + 25]
        self.fine = struct.unpack_from("b", d, o + 26)[0]
        self.vl_index = d[o + 29]
        self.raw = d[o:o + 32]
        # sample data, relative to the tone bank start
        n = self.lea + 1
        so = bank.start + self.sa
        if self.pcm8:
            s = np.frombuffer(d[so:so + n], dtype=np.int8).astype(np.float32) / 128.0
        else:
            s = np.frombuffer(d[so:so + 2 * n], dtype=">i2").astype(np.float32) / 32768.0
        self.samples = s

    def describe(self):
        return ("SA=%05X len=%d %s loop=%d(LSA=%X) base=%d fine=%d AR=%d D1R=%d "
                "D2R=%d DL=%d RR=%d KRS=%d TL=%d DISDL=%d DIPAN=%d" % (
                    self.sa, self.lea + 1, "8-bit" if self.pcm8 else "16-bit",
                    self.loop, self.lsa, self.base, self.fine, self.ar, self.d1r,
                    self.d2r, self.dl, self.rr, self.krs, self.tl, self.disdl,
                    self.dipan))


class ToneBank:
    def __init__(self, data, start, name):
        self.data, self.start, self.name = data, start, name
        d = data
        mix, vl, peg, plfo = (u16(d, start + 2 * i) for i in range(4))
        nv = (mix - 8) // 2
        self.vl_tables = [d[start + o:start + o + 10] for o in range(vl, peg, 10)]
        self.voices = []
        for v in range(nv):
            vo = start + u16(d, start + 8 + 2 * v)
            hdr = d[vo:vo + 4]
            layers = [Layer(self, vo + 4 + 32 * i) for i in range(hdr[2] + 1)]
            self.voices.append((hdr, layers))

    def vel_level(self, idx, vel):
        """velocity -> level 0..127 through velocity table idx (approximation:
        the table read as slope0, (point, level) x3 interleaved with slopes;
        the curve is drawn through (0, level0)...(point2, level2), (127, 127))"""
        t = self.vl_tables[idx] if idx < len(self.vl_tables) else None
        if t is None:
            return vel
        pts = [(0, t[2]), (t[4], t[5]), (t[7], t[8]), (127, 127)]
        pts = [(p, l) for p, l in pts]
        xs = [p for p, _ in pts]
        ys = [l for _, l in pts]
        return float(np.interp(vel, xs, ys))


def find_tone_bank(d):
    for tb in range(0, len(d) - 64, 2):
        mix, vl, peg, plfo = (u16(d, tb + 2 * i) for i in range(4))
        if not (8 < mix < vl <= peg <= plfo < 0x2000) or mix % 2:
            continue
        nv = (mix - 8) // 2
        offs = [u16(d, tb + 8 + 2 * i) for i in range(nv)]
        if nv < 1 or offs[0] < plfo:
            continue
        ok = True
        for i in range(nv - 1):
            if offs[i + 1] - offs[i] != 4 + 32 * (d[tb + offs[i] + 2] + 1):
                ok = False
                break
        if ok:
            return tb
    raise ValueError("no tone bank")


class Crt:
    """a .CRT sound bank file: SEQ tables + one tone bank"""

    def __init__(self, name):
        self.name = name
        self.data = d = open(os.path.join(SD, name), "rb").read()
        self.tables = {}
        self.tables["A"] = (0, [u32(d, 2 + 4 * i) for i in range(u16(d, 0))])
        tb = find_tone_bank(d)
        if tb > 0x800 and u16(d, 0x800) and u32(d, 0x802) == 2 + 4 * u16(d, 0x800):
            self.tables["B"] = (0x800, [u32(d, 0x802 + 4 * i) for i in range(u16(d, 0x800))])
        self.tones = ToneBank(d, tb, name)

    def seq(self, table, idx):
        base, offs = self.tables[table]
        if idx >= len(offs):
            return None
        return base + offs[idx]


_crts = {}


def crt(name):
    if name not in _crts:
        _crts[name] = Crt(name)
    return _crts[name]


class Context:
    """the banks loaded while a stage plays"""

    def __init__(self, stage=DEFAULT_STAGE, player="SD_ALK.CRT", common="SD_JO10.CRT"):
        self.common, self.player, self.stage = crt(common), crt(player), crt(stage)
        self.tone_banks = {0: self.common.tones, 1: self.player.tones, 2: self.stage.tones}

    def locate(self, sid):
        """(crt, file offset of the SEQ) of a sound id, or (crt, None)"""
        typ, idx = sound_config(sid)[:2]
        c, table = {0: (self.common, "A"), 1: (self.player, "A"),
                    2: (self.stage, "A"), 3: (self.stage, "B")}[typ]
        if table not in c.tables:
            return c, None
        return c, c.seq(table, idx)


# ---------------------------------------------------------------- SEQ
GATE_EXT = {0x88: 0x200, 0x89: 0x800, 0x8A: 0x1000, 0x8B: 0x2000}
STEP_EXT = {0x8C: 0x100, 0x8D: 0x200, 0x8E: 0x800, 0x8F: 0x1000}


class Seq:
    def __init__(self, d, s):
        self.start = s
        self.resolution = u16(d, s)
        nt = u16(d, s + 2)
        self.data_off = u16(d, s + 4)
        self.tempos = [(u32(d, s + 8 + 8 * i), u32(d, s + 12 + 8 * i)) for i in range(nt)]
        self.events = []  # (tick, kind, channel, args...)
        self.warnings = []
        self.step_ext = {}
        self._parse(d, s + self.data_off)

    def _parse(self, d, base):
        t = 0
        gate_ext = 0
        p = base
        ret = None  # (return position, events left) while in a reference
        while True:
            if ret is not None and ret[1] == 0:
                p, ret = ret[0], None
            b = d[p]
            if ret is not None:
                ret = (ret[0], ret[1] - 1)
            if b < 0x80:
                key, vel, gate, step = d[p + 1:p + 5]
                gate += gate_ext + (0x100 if b & 0x40 else 0)
                step += 0x100 if b & 0x20 else 0
                gate_ext = 0
                t += step
                self.events.append((t, "note", b & 0xF, key, vel, gate))
                p += 5
            elif b == 0x81:
                if ret is not None:
                    self.warnings.append("nested reference at %X" % p)
                ref, cnt = u16(d, p + 1), d[p + 3]
                ret = (p + 4, cnt)
                p = base + ref
            elif b == 0x83:
                break
            elif b in GATE_EXT:
                gate_ext += GATE_EXT[b]
                p += 1
            elif b in STEP_EXT:
                t += STEP_EXT[b]
                self.step_ext[b] = self.step_ext.get(b, 0) + 1
                p += 1
            elif 0xA0 <= b < 0xB0:
                t += d[p + 3]
                p += 4
            elif 0xB0 <= b < 0xC0:
                t += d[p + 3]
                self.events.append((t, "cc", b & 0xF, d[p + 1], d[p + 2]))
                p += 4
            elif 0xC0 <= b < 0xD0:
                t += d[p + 2]
                self.events.append((t, "prog", b & 0xF, d[p + 1]))
                p += 3
            elif 0xD0 <= b < 0xE0:
                t += d[p + 2]
                p += 3
            elif 0xE0 <= b < 0xF0:
                t += d[p + 2]
                self.events.append((t, "bend", b & 0xF, d[p + 1]))
                p += 3
            else:
                self.warnings.append("unknown event %02X at %X, stopped" % (b, p))
                break
        self.length = t
        ends = [e[0] + e[5] for e in self.events if e[1] == "note"]
        self.end_tick = max([t] + ends)

    def seconds(self, tick):
        """tick -> seconds through the tempo events"""
        sec, t0 = 0.0, 0
        tempos = self.tempos or [(0, 500000)]
        for i, (steps, usec) in enumerate(tempos):
            last = i == len(tempos) - 1
            span = tick - t0 if last else min(steps, tick - t0)
            sec += span * usec / 1e6 / self.resolution
            t0 += span
            if t0 >= tick:
                break
        return sec

    def dump(self):
        lines = ["res=%d tempos=%s length=%d ticks" % (self.resolution, self.tempos, self.length)]
        for e in self.events:
            lines.append("  %5d %-4s ch%d %s" % (e[0], e[1], e[2], " ".join("%X" % x for x in e[3:])))
        return "\n".join(lines + ["  ! " + w for w in self.warnings])


# ---------------------------------------------------------------- synth
def eg_rate(r, oct_, fns_hi, krs):
    if r == 0:
        return 0
    rate = 2 * r
    if krs != 0xF:
        rate += oct_ + 2 * krs + fns_hi
    return max(0, min(63, rate))


def envelope(layer, n_on, n_total, sr, oct_, fns_hi):
    """amplitude envelope: key on at 0, key off at n_on, n_total samples"""
    env = np.zeros(n_total, dtype=np.float64)
    ar = eg_rate(layer.ar, oct_, fns_hi, layer.krs)
    d1 = eg_rate(layer.d1r, oct_, fns_hi, layer.krs)
    d2 = eg_rate(layer.d2r, oct_, fns_hi, layer.krs)
    rr = eg_rate(layer.rr, oct_, fns_hi, layer.krs)
    db = 0.0
    att = 0.0  # attack amplitude 0..1
    a_step = 1.0 / max(1.0, AR_TIMES[ar] * sr / 1000.0) if AR_TIMES[ar] < 1e8 else 0.0
    if ar >= 62 or AR_TIMES[ar] == 0.0:
        att = 1.0

    def dstep(rate):
        tm = DR_TIMES[rate]
        return 0.0 if tm >= 1e8 else EG_RANGE_DB / (tm * sr / 1000.0)
    s1, s2, sr_ = dstep(d1), dstep(d2), dstep(rr)
    dl_db = layer.dl * EG_RANGE_DB / 32.0
    i = 0
    # vectorised by phases
    # attack
    if att < 1.0 and a_step > 0:
        na = min(n_on, int(math.ceil((1.0 - att) / a_step)))
        env[:na] = att + a_step * np.arange(na)
        att = min(1.0, att + a_step * na)
        i = na
    elif att < 1.0:
        return env  # never attacks
    # decay 1 to DL, then decay 2
    if i < n_on:
        n = n_on - i
        k = np.arange(n, dtype=np.float64)
        if s1 > 0:
            n1 = int(math.ceil(dl_db / s1)) if dl_db > 0 else 0
        else:
            n1 = n
        n1 = min(n1, n)
        curve = np.empty(n)
        curve[:n1] = s1 * k[:n1]
        lvl = s1 * n1 if s1 > 0 else 0.0
        curve[n1:] = lvl + s2 * (k[n1:] - n1)
        curve = np.minimum(curve, EG_RANGE_DB)
        env[i:n_on] = att * 10 ** (-curve / 20.0)
        db = curve[-1] if n else 0.0
        i = n_on
    # release from the level reached
    if i < n_total:
        start = env[i - 1] if i > 0 else att
        start_db = -20 * math.log10(max(start, 1e-9))
        n = n_total - i
        if sr_ > 0:
            curve = np.minimum(start_db + sr_ * np.arange(1, n + 1), EG_RANGE_DB)
            env[i:] = 10 ** (-curve / 20.0)
            env[i:][curve >= EG_RANGE_DB] = 0.0
        else:
            env[i:] = start
    return env


def render_note(layer, sr, t_on, t_off, pitch_fn, tail):
    """sample-domain voice: returns (start sample, float array)"""
    i0 = int(round(t_on * sr))
    n_on = max(1, int(round((t_off - t_on) * sr)))
    # phase increments (per output sample) may change with pitch bends
    n_max = n_on + int(tail * sr)
    times = t_on + np.arange(n_max) / sr
    semis = pitch_fn(times)
    inc = SCSP_RATE * np.power(2.0, semis / 12.0) / sr
    pos = np.concatenate(([0.0], np.cumsum(inc)[:-1]))
    s = layer.samples
    n = len(s)
    if layer.loop == 0:
        valid = pos < n - 1 + 1e-9
        stop = int(np.argmin(valid)) if not valid.all() else len(pos)
        pos = pos[:stop]
    else:
        ls, le = min(layer.lsa, n - 1), n - 1
        span = max(1, le - ls)
        if layer.loop in (1, 2):
            over = pos > ls
            pos = np.where(over, ls + np.mod(pos - ls, span), pos)
        else:  # alternating
            over = pos > ls
            q = np.mod(pos - ls, 2 * span)
            pos = np.where(over, ls + np.where(q < span, q, 2 * span - q), pos)
    if len(pos) == 0:
        return i0, np.zeros(0)
    ip = np.floor(pos).astype(np.int64)
    fr = pos - ip
    a = s[np.clip(ip, 0, n - 1)]
    b = s[np.clip(ip + 1, 0, n - 1)]
    out = a + (b - a) * fr
    # key scaling inputs from the start pitch: ratio = 2^oct * (1 + fns/1024)
    ratio = SCSP_RATE * 2 ** (semis[0] / 12.0) / SCSP_RATE
    oct_ = int(math.floor(math.log2(ratio)))
    fns = int((ratio / 2 ** oct_ - 1) * 1024)
    env = envelope(layer, min(n_on, len(out)), len(out), sr, oct_, (fns >> 9) & 1)
    return i0, out * env


def piecewise(points, default):
    """[(time, value)] -> vectorised step function of time"""
    pts = sorted(points)

    def f(t):
        t = np.asarray(t, dtype=np.float64)
        out = np.full(t.shape, float(default))
        for tm, v in pts:
            out[t >= tm - 1e-12] = v
        return out
    return f


def plan(ctx, sid):
    """parse a sound: (seq, notes, info). notes = list of dicts"""
    c, off = ctx.locate(sid)
    if off is None:
        return None, [], {"crt": c.name, "missing": True}
    seq = Seq(c.data, off)
    chan = {ch: {"bank": 0, "prog": 0, "vol": [(0.0, 127)], "expr": [(0.0, 127)],
                 "pan": [(0.0, 64)], "bend": [(0.0, 0x40)], "cc0": 0} for ch in range(16)}
    notes = []
    for e in seq.events:
        t, kind, ch = e[0], e[1], e[2]
        st = chan[ch]
        ts = seq.seconds(t)
        if kind == "cc":
            num, val = e[3], e[4]
            if num == 0:
                st["cc0"] = val
            elif num == 32:
                st["bank"] = val
            elif num == 7:
                st["vol"].append((ts, val))
            elif num == 11:
                st["expr"].append((ts, val))
            elif num == 10:
                st["pan"].append((ts, val))
            else:
                seq.warnings.append("unhandled CC%d=%X ch%d" % (num, val, ch))
        elif kind == "prog":
            st["prog"] = e[3]
        elif kind == "bend":
            st["bend"].append((ts, e[3]))
        elif kind == "note":
            key, vel, gate = e[3], e[4], e[5]
            notes.append(dict(t=ts, off=seq.seconds(t + gate), ch=ch, key=key, vel=vel,
                              gate=gate, tick=t, bank=st["bank"], prog=st["prog"],
                              vol=list(st["vol"]), expr=list(st["expr"]),
                              pan=list(st["pan"]), bend=list(st["bend"])))
    # later controller changes also apply to notes already playing
    for n in notes:
        st = chan[n["ch"]]
        n["vol"], n["expr"], n["pan"], n["bend"] = st["vol"], st["expr"], st["pan"], st["bend"]
    return seq, notes, {"crt": c.name, "seq_offset": off}


def resolve(ctx, n):
    tb = ctx.tone_banks.get(n["bank"])
    if tb is None or n["prog"] >= len(tb.voices):
        return None, None, None
    hdr, layers = tb.voices[n["prog"]]
    for ly in layers:
        if ly.lo <= n["key"] <= ly.hi:
            return tb, hdr, ly
    return tb, hdr, None


def render(sid, ctx=None, rate=None, tail=2.0, max_rate=SCSP_RATE):
    """render Saturn sound id -> (float array [n] or [n, 2], rate, info).
    rate None: the highest note playback rate (rounded up), at most max_rate"""
    ctx = ctx or Context()
    seq, notes, info = plan(ctx, sid)
    info.update(id=sid, config=sound_config(sid), notes=[], tones=[])
    if seq is None or not notes:
        info["empty"] = True
        return np.zeros(0), rate or SCSP_RATE, info
    info["seq"] = seq
    voices = []
    for n in notes:
        tb, hdr, ly = resolve(ctx, n)
        if ly is None:
            info["notes"].append("ch%d key %d: no tone for bank %d prog %d" % (
                n["ch"], n["key"], n["bank"], n["prog"]))
            continue
        bend_range = hdr[0] & 0xF
        base = ly.base - ly.fine / 128.0
        bendf = piecewise(n["bend"], 0x40)

        def pitch(t, n=n, base=base, bendf=bendf, rng=bend_range):
            return n["key"] - base + (bendf(t) - 0x40) / 64.0 * rng
        eff = SCSP_RATE * 2 ** (pitch(np.array([n["t"]]))[0] / 12.0)
        voices.append((n, tb, hdr, ly, pitch, eff))
        tag = "%s v%02X" % (tb.name, n["prog"])
        if tag not in [x[0] for x in info["tones"]]:
            info["tones"].append((tag, ly.describe()))
        info["notes"].append("t=%.3fs ch%d bank%d prog%02X key %d vel %d gate %d (%.3fs) -> %.0f Hz" % (
            n["t"], n["ch"], n["bank"], n["prog"], n["key"], n["vel"], n["gate"],
            n["off"] - n["t"], eff))
    if not voices:
        info["empty"] = True
        return np.zeros(0), rate or SCSP_RATE, info
    if rate is None:
        rate = int(math.ceil(min(max_rate, SCSP_RATE, max(v[5] for v in voices))))
    stereo = False
    rendered = []
    for n, tb, hdr, ly, pitch, eff in voices:
        i0, wav = render_note(ly, rate, n["t"], n["off"], pitch, tail)
        if len(wav) == 0:
            continue
        times = n["t"] + np.arange(len(wav)) / rate
        vl = tb.vel_level(ly.vl_index, n["vel"])
        vol = piecewise(n["vol"], 127)(times)
        expr = piecewise(n["expr"], 127)(times)
        if VOLUME_MODEL == "linear":
            gain = vl / 127.0 * vol / 127.0 * expr / 127.0
        else:  # "tl": each level step below 127 is one TL step (0.375 dB)
            gain = 10 ** (-0.375 * ((127 - vl) + (127 - vol) + (127 - expr)) / 20.0)
            gain = np.where(vol <= 0, 0.0, gain)
        gain = gain * MIX_GAIN * 10 ** (-ly.tl * 0.375 / 20.0)
        gain = gain * (0.0 if ly.disdl == 0 else 2.0 ** (ly.disdl - 7))
        # pan: CC10 (0 left .. 127 right) and DIPAN (bit 4 side, 3 dB steps)
        pan = piecewise(n["pan"], 64)(times)
        lg = np.clip((127 - pan) / 63.0, 0, 1)
        rg = np.clip(pan / 64.0, 0, 1)
        if ly.dipan & 0xF:
            att = 10 ** (-3 * (ly.dipan & 0xF) / 20.0) if (ly.dipan & 0xF) != 0xF else 0.0
            if ly.dipan & 0x10:
                rg = rg * att
            else:
                lg = lg * att
        if np.any(np.abs(lg - rg) > 1e-6):
            stereo = True
        rendered.append((i0, wav * gain, lg, rg))
    total = max(i0 + len(w) for i0, w, _, _ in rendered)
    out = np.zeros((total, 2))
    for i0, w, lg, rg in rendered:
        out[i0:i0 + len(w), 0] += w * lg
        out[i0:i0 + len(w), 1] += w * rg
    if not stereo:
        out = out[:, 0]
    info["stereo"] = stereo
    return out, rate, info


def trim(x, threshold_db=-60.0):
    """drop the trailing silence (below threshold_db dBFS)"""
    a = np.abs(x) if x.ndim == 1 else np.abs(x).max(axis=1)
    nz = np.nonzero(a > 10 ** (threshold_db / 20.0))[0]
    return x[:nz[-1] + 1] if len(nz) else x[:0]


def to_pcm16(x):
    """float -> int16; a sound over full scale is scaled down to fit"""
    peak = float(np.abs(x).max()) if len(x) else 0.0
    scale = 1.0
    if peak > 32767 / 32768:
        scale = (32767 / 32768) / peak
    y = np.clip(np.round(x * scale * 32768), -32768, 32767).astype("<i2")
    return y, peak, scale


def write_wav(path, pcm, rate):
    ch = 1 if pcm.ndim == 1 else pcm.shape[1]
    with wave.open(path, "wb") as w:
        w.setnchannels(ch)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm.tobytes())


def adpcm_size(nsamples):
    """PS1 VAG body as mods/sotn-ultimate/tools/satxa.c writes it (zero block, data, end)"""
    return ((nsamples + 27) // 28 + 2) * 16


def stages_with(sid):
    """the stage banks that have a SEQ with notes for a type 2/3 id"""
    typ, idx = sound_config(sid)[:2]
    table = {2: "A", 3: "B"}.get(typ)
    out = []
    if table is None:
        return out
    for name, flag in bank_files():
        if flag != 0x12 or not os.path.exists(os.path.join(SD, name)):
            continue
        c = crt(name)
        if table in c.tables and idx < len(c.tables[table][1]):
            sq = Seq(c.data, c.seq(table, idx))
            if any(e[1] == "note" for e in sq.events):
                out.append(name)
    return out


# The Garden plays these ids but SD_TEI.CRT has no SEQ for them (its type-3
# table has 0x49 entries): render them from a stage bank that has one.
# 0x7C4 is the PS1's SFX_WATER_BUBBLE: CHI and LIB have the same SEQ+tone
# (CAT the same tone, ROUX another); 0x7CA is the PS1's Olrox sound
# (SFX_BO0_UNK_7CA): OLRO (Olrox's Quarters), GOLD has the same 16-bit tone.
FALLBACK_STAGE = {0x7C4: "SD_CHI.CRT", 0x7CA: "SD_OLRO.CRT"}


def render_to_wav(sid, path=None, rate=None, stage=DEFAULT_STAGE, stats=None,
                  max_rate=SCSP_RATE, fallback=True):
    """render Saturn sound id to a 16-bit WAV (mono unless it pans; rate:
    the source playback rate unless given); returns its path"""
    ctx = Context(stage=stage)
    x, sr, info = render(sid, ctx, rate, max_rate=max_rate)
    if info.get("empty") and fallback and sound_config(sid)[0] in (2, 3):
        have = stages_with(sid)
        alt = FALLBACK_STAGE.get(sid) if FALLBACK_STAGE.get(sid) in have else (have[0] if have else None)
        if alt:
            ctx = Context(stage=alt)
            x, sr, info = render(sid, ctx, rate, max_rate=max_rate)
            info["fallback"] = (stage, alt, have)
    x = trim(x)
    pcm, peak, scale = to_pcm16(x)
    if path is None:
        os.makedirs(OUT_DIR, exist_ok=True)
        path = os.path.join(OUT_DIR, "sat_%03X.wav" % sid)
    write_wav(path, pcm, sr)
    if stats is not None:
        n = len(pcm)
        stats.update(info)
        stats.update(path=path, rate=sr, samples=n, seconds=n / sr,
                     peak_db=20 * math.log10(peak) if peak > 0 else -math.inf,
                     scale_db=20 * math.log10(scale), adpcm=adpcm_size(n))
    return path


# the Garden's sounds (Saturn ids)
GARDEN = [
    (0x8F7, "boss bomb blast"), (0x8F8, "boss death"),
    (0x8FA, "tree face falls"), (0x8FB, "face bursts"),
    (0x8FC, "tree hurt (laugh)"), (0x8FD, "tree death"),
    (0x8FE, "gargoyle claw swing"), (0x8FF, "gargoyle debris lands"),
    (0x90B, "Man Eater bursts"), (0x90C, "Man Eater opens"),
    (0x90D, "Man Eater jaws snap"), (0x90E, "Gardener death"),
    (0x90F, "Gardener shears click"), (0x911, "Leaper death"),
    (0x912, "Leaper jumps out"), (0x913, "Leaper falls in"),
    (0x766, "boss reward jingle"), (0x777, "water splash"),
    (0x778, "water surfacing"), (0x7C4, "Corpseweed spit"),
    (0x7CA, "poison drip"),
]


def describe(sid, st):
    cfg = st["config"]
    if st.get("empty"):
        return "%03X type %d idx %02X (%s): EMPTY" % (sid, cfg[0], cfg[1], st.get("crt"))
    fb = st.get("fallback")
    return "%03X type %d idx %02X (%s%s): %.3fs @%d Hz %s peak %.1f dBFS%s, ADPCM %d bytes -> %s" % (
        sid, cfg[0], cfg[1], st.get("crt"),
        "" if not fb else ", not in %s" % fb[0],
        st["seconds"], st["rate"], "stereo" if st.get("stereo") else "mono",
        st["peak_db"], "" if st["scale_db"] == 0 else " (scaled %.1f dB)" % st["scale_db"],
        st["adpcm"], st["path"])


def main():
    global VOLUME_MODEL
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("ids", nargs="*", help="Saturn sound ids (hex); none: the Garden's")
    ap.add_argument("--stage", default=DEFAULT_STAGE, help="stage bank file")
    ap.add_argument("--rate", type=int, help="output rate (default: the highest "
                    "note's playback rate, at most --max-rate)")
    ap.add_argument("--max-rate", type=int, default=SCSP_RATE)
    ap.add_argument("--out", help="output WAV (one id) or directory")
    ap.add_argument("--dump", action="store_true", help="print the SEQ events")
    ap.add_argument("--volume", choices=("tl", "linear"), default=VOLUME_MODEL,
                    help="velocity/CC7 to gain model (default %(default)s)")
    ap.add_argument("--no-fallback", action="store_true",
                    help="do not take ids missing from --stage from another stage bank")
    a = ap.parse_args()
    VOLUME_MODEL = a.volume
    ids = [int(s, 16) for s in a.ids] or [g[0] for g in GARDEN]
    total = 0
    for sid in ids:
        path = None
        if a.out:
            path = a.out if len(ids) == 1 and a.out.endswith(".wav") else \
                os.path.join(a.out, "sat_%03X.wav" % sid)
            os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        st = {}
        render_to_wav(sid, path, a.rate, a.stage, st, a.max_rate, not a.no_fallback)
        print(describe(sid, st))
        total += st.get("adpcm", 0) if not st.get("empty") else 0
        if a.dump and st.get("seq") is not None:
            print(st["seq"].dump())
        if st.get("fallback"):
            print("    taken from %s (has it: %s)" % (st["fallback"][1], ", ".join(st["fallback"][2])))
        for line in st.get("notes", []):
            print("   ", line)
        for tag, desc in st.get("tones", []):
            print("    tone %s: %s" % (tag, desc))
    if len(ids) > 1:
        print("total PS1 ADPCM: %d bytes" % total)


if __name__ == "__main__":
    main()
