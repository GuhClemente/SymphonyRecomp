#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: repack SD/XA_STR1 (music + voices) to free disc space.

Input: build/us/XA_STR1.unpacked (retail stream + the Saturn tracks appended
by mods/sotn-ultimate/tools/gen_saturn_music.py), the retail configs
(assets/dra/us/xa_music_config.yaml) and the mod ones
(build/us/mod_xa_music.json). Output: build/us/XA_STR1. (taken by `make
disk`), src/dra/gen/us/xa_music_config.h and the table in
src/dra/mod_xa_music.h, rewritten in place (same entry count: g_SfxScripts
sits right after the retail table).

How the player reads it (src/dra/sound.c): CdlSetfilter(file, channel % 16),
CdlSetloc(cd_addr + channel), CdlReadN at double speed, then a VSync timer
(the config's length) ends it or seeks back to the loop entry (config + 1).
So a config's sound is the run of sectors from cd_addr + channel, every 8th
(37.8 kHz stereo) or 16th (mono), that matches its file/channel. An intro
and its loop share one run (the loop config points inside it); a voice that
runs into another one of the same file/channel shares that run too.

The repack:
  - extracts every config's run, merges runs that end on the same sector
    into one stream (configs then point at offsets in it),
  - drops the retail filler sectors (unused interleave slots),
  - unless --keep-all: points the JP-only lines (0x341-0x357, 0x52E-0x530,
    0x34B is the debug stage's BGM) at one short silent stream,
  - unless --no-mono: stores dual-mono streams (left == right: bit exact,
    or within encoder noise, see dual_mono) as 37.8 kHz mono, which the drive plays from every 16th sector: two
    stereo sectors' left sound units make one mono sector, same decode,
  - packs the streams in a 16-slot interleave (stereo = slots L and L + 8,
    mono = one slot), first fit decreasing, each stream with its own
    file/channel (so a timer running past a stream's end plays silence,
    as retail),
  - with --dub (mods/sotn-ultimate/tools/psp_dub.py's dub.json, the PSP English dub):
    the PS1 voices stay as they are (the in-game toggle, include/game.h
    MOD_PSP_DUB_ON); each dubbed voice id gets a second, mono stream, packed
    after all the others (one region, so src/dra/mod_psp_xa.h can store
    each as 15 bits of sector past MOD_PSP_XA_BASE, 10 of length and 6 of
    file: src/dra/sound.c ModXaConfig plays it while the dub is on); new
    configs (lines with no PS1 slot) are appended to the mod table in
    src/dra/mod_xa_music.h,
  - with --dub-jp (psp_dub.py --set jp's dub.json, the JP dub; needs
    --dub): a third version of those voice ids (and of --dub's new configs,
    by define), packed after the PSP region in one of its own; its table
    (src/st/sel/mod_jp_xa.h: 15 bits of sector past MOD_JP_XA_BASE, 11 of
    length (lines up to 34 s; the JP ones run to 25.3 s), 6 of file) is
    SEL rodata that SEL copies into DRA's g_ModDub.jp (include/game.h),
  - then re-extracts every config from the new file and compares it with
    the original run (stereo: whole sectors' data; mono: the left channel's
    sound units; dubbed: the encoded sectors' data), and fails on any
    mismatch.
"""
import argparse
import json
import os
import re
import subprocess
import sys
import wave

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
IN_XA = os.path.join(ROOT, "build/us/XA_STR1.unpacked")
OUT_XA = os.path.join(ROOT, "build/us/XA_STR1.")
YAML = os.path.join(ROOT, "assets/dra/us/xa_music_config.yaml")
MOD_JSON = os.path.join(ROOT, "build/us/mod_xa_music.json")
GEN_H = os.path.join(ROOT, "src/dra/gen/us/xa_music_config.h")
MOD_H = os.path.join(ROOT, "src/dra/mod_xa_music.h")
PSP_H = os.path.join(ROOT, "src/dra/mod_psp_xa.h")
JP_H = os.path.join(ROOT, "src/st/sel/mod_jp_xa.h")
PSP_FIRST, PSP_LAST = 0x358, 0x532  # include/game.h MOD_PSP_XA_FIRST/LAST

S = 2352
DATA = 24  # sync 12, header 4, subheader 8
GROUPS = 18  # 128-byte sound groups per sector
SM_AUDIO, SM_EOF = 0x04, 0x80
CODING_STEREO = 0x01  # 37.8 kHz, 4 bit; bit 0 clear = mono
# JP-only voices (the US disc keeps them, nothing plays them on US) and the
# debug stage MAD's BGM (0x34B)
JP_IDS = list(range(0x341, 0x358)) + list(range(0x52E, 0x531))
SILENT_SECTORS = 2
MONO_DB = -40  # see dual_mono
SATXA = os.path.join(ROOT, "bin/satxa")  # built by gen_saturn_music.py
TMP = os.path.join(ROOT, "build/us/xa_repack/stream")


def read_configs():
    keys = ["cd_addr", "unk228", "filter_file", "filter_channel_id", "volume",
            "unk22f", "unk230"]
    cfg = []
    cur = None
    for line in open(YAML):
        m = re.match(r"\s*(-\s*)?(\w+):\s*(-?\w+)", line)
        if not m:
            continue
        if m.group(1):
            cur = dict.fromkeys(keys, 0)
            cfg.append(cur)
        cur[m.group(2)] = int(m.group(3), 0)
    retail = [[c[k] for k in keys] for c in cfg]
    mod = json.load(open(MOD_JSON))
    return retail, mod


class Xa:
    def __init__(self, path):
        self.buf = open(path, "rb").read()
        self.n = len(self.buf) // S
        assert self.n * S == len(self.buf)
        a = np.frombuffer(self.buf, np.uint8).reshape(self.n, S)
        self.file, self.chan = a[:, 16].copy(), a[:, 17].copy()
        self.sm, self.coding = a[:, 18].copy(), a[:, 19].copy()

    def sector(self, i):
        return self.buf[i * S:(i + 1) * S]

    def run(self, start, f, ch):
        """sectors of the stream starting at sector start (file f, chan ch)"""
        if start >= self.n or not self.sm[start] & SM_AUDIO:
            return []
        step = 8 if self.coding[start] & CODING_STEREO else 16
        out = []
        s = start
        while (s < self.n and self.file[s] == f and self.chan[s] == ch and
               self.sm[s] & SM_AUDIO):
            out.append(s)
            if self.sm[s] & SM_EOF:
                break
            s += step
        return out


def groups_of(xa, secs):
    """the sound groups of sectors secs: (n * 18, 128) uint8"""
    if not secs:
        return np.zeros((0, 128), np.uint8)
    a = np.frombuffer(b"".join(xa.sector(s)[DATA:DATA + GROUPS * 128]
                               for s in secs), np.uint8)
    return a.reshape(-1, 128)


def units_of(g):
    """sound units of groups g: params (n, 8), nibbles (n, 8, 28)"""
    params = g[:, 4:12]
    d = g[:, 16:].reshape(-1, 28, 4)
    nib = np.empty((len(g), 8, 28), np.uint8)
    for k in range(4):
        nib[:, 2 * k] = d[:, :, k] & 15
        nib[:, 2 * k + 1] = d[:, :, k] >> 4
    return params, nib


def dual_mono(xa, secs, g):
    """Left and right the same sound? Bit exact for some voices; most
    others were encoded from one mono source per channel and differ by
    encoder noise (the decoded L - R at -44 to -85 dB under the signal)
    while true stereo ones sit at -35 dB (one line) or above (-25..+3)"""
    if not (g[:, 0:4] == g[:, 4:8]).all() or not (g[:, 8:12] == g[:, 12:16]).all():
        return False
    p, n = units_of(g)
    same = (p[:, 0::2] == p[:, 1::2]) & (n[:, 0::2] == n[:, 1::2]).all(axis=2)
    if same.all():
        return True
    if same.mean() < 0.8:
        return False
    os.makedirs(os.path.dirname(TMP), exist_ok=True)
    with open(TMP + ".xa", "wb") as f:
        f.write(b"".join(xa.sector(s) for s in secs))
    s0 = xa.sector(secs[0])
    subprocess.run([SATXA, "dec", TMP + ".xa", TMP + ".wav", str(s0[16]), str(s0[17])],
                   check=True, capture_output=True)
    with wave.open(TMP + ".wav") as w:
        pcm = np.frombuffer(w.readframes(w.getnframes()), np.int16)
    pcm = pcm.reshape(-1, 2).astype(np.float64)
    rms = np.sqrt((pcm[:, 0] ** 2).mean())
    diff = np.sqrt(((pcm[:, 0] - pcm[:, 1]) ** 2).mean())
    return rms > 0 and 20 * np.log10(max(diff, 1e-9) / rms) <= MONO_DB


def to_mono(g):
    """left units of stereo groups g, 8 per mono group (silent padding to
    a whole sector)"""
    p, n = units_of(g)
    lp, ln = p[:, 0::2], n[:, 0::2]  # (n, 4), (n, 4, 28)
    pad = -len(g) % (2 * GROUPS)  # whole mono sectors
    if pad:
        lp = np.concatenate([lp, np.zeros((pad, 4), np.uint8)])
        ln = np.concatenate([ln, np.zeros((pad, 4, 28), np.uint8)])
    m = len(lp) // 2
    lp, ln = lp.reshape(m, 8), ln.reshape(m, 8, 28)
    out = np.zeros((m, 128), np.uint8)
    out[:, 0:4] = lp[:, 0:4]
    out[:, 4:8] = lp[:, 0:4]
    out[:, 8:12] = lp[:, 4:8]
    out[:, 12:16] = lp[:, 4:8]
    d = np.zeros((m, 28, 4), np.uint8)
    for k in range(4):
        d[:, :, k] = ln[:, 2 * k] | (ln[:, 2 * k + 1] << 4)
    out[:, 16:] = d.reshape(m, 112)
    return out


def sector_bytes(template, f, ch, sm, coding, data):
    """a sector: template's sync/header, the new subheader, data, zero tail"""
    sub = bytes((f, ch, sm, coding))
    s = template[:16] + sub + sub + data
    return s + bytes(S - len(s))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-mono", action="store_true")
    ap.add_argument("--keep-all", action="store_true", help="keep the JP lines")
    ap.add_argument("--dub", help="psp_dub.py's dub.json: replacement voices")
    ap.add_argument("--dub-jp", help="psp_dub.py --set jp's dub.json: the JP versions")
    args = ap.parse_args()
    if args.dub_jp and not args.dub:
        sys.exit("repack_xa: --dub-jp needs --dub")

    retail, mod = read_configs()
    assert len(retail) == 563, len(retail)
    cfg = retail + mod
    # dubbed configs: index -> {xa, sectors, unk228, ...}
    dubbed = {}
    extra = []
    if args.dub:
        dub = json.load(open(args.dub))
        for c, e in dub["replace"].items():
            c = int(c)
            assert c < len(retail) and cfg[c][6] == 2, "0x%X is no voice" % (0x300 + c)
            assert PSP_FIRST <= 0x300 + c <= PSP_LAST, hex(0x300 + c)
            e["psp"] = True  # a second version of a PS1 config
            dubbed[c] = e
        for e in dub["extra"]:
            like = cfg[e["like"]]
            e["index"] = len(cfg)
            cfg.append([0, e["unk228"], 0, 0, like[4], like[5], like[6]])
            dubbed[e["index"]] = e
            extra.append(e)
    # the JP versions: index -> entry (a region after the PSP one)
    jpdub = {}
    if args.dub_jp:
        dj = json.load(open(args.dub_jp))
        for c, e in dj["replace"].items():
            c = int(c)
            assert c < len(retail) and cfg[c][6] == 2, "0x%X is no voice" % (0x300 + c)
            assert PSP_FIRST <= 0x300 + c <= PSP_LAST, hex(0x300 + c)
            jpdub[c] = e
        by_define = {x["define"]: x["index"] for x in extra}
        for e in dj["extra"]:
            e["index"] = by_define[e["define"]]
            jpdub[e["index"]] = e
    xa = Xa(IN_XA)
    filler = next(xa.sector(i) for i in range(xa.n) if xa.sm[i] == 0)

    # 1. every config's run; group runs by their last sector
    runs = {}
    groups = {}  # last sector -> [config indexes]
    for i, (addr, ln, f, ch, vol, _, mode) in enumerate(cfg):
        if addr == 0 and ln == 0 and f == 0 and vol == 0:
            continue  # unused entry
        if i in dubbed and not dubbed[i].get("psp"):
            continue  # a new config: no retail run
        r = xa.run(addr + ch, f, ch)
        assert r, "config 0x%X has no stream" % (0x300 + i)
        assert xa.coding[r[0]] == CODING_STEREO
        runs[i] = r
        groups.setdefault(r[-1], []).append(i)
    streams = []  # {cfgs, secs (original), offs {cfg: offset}}
    owner = {}
    for last, cs in sorted(groups.items()):
        secs = max((runs[c] for c in cs), key=len)
        for s in secs:
            assert s not in owner, "streams overlap at sector %d" % s
            owner[s] = last
        offs = {c: len(secs) - len(runs[c]) for c in cs}
        for c in cs:
            assert secs[offs[c]:] == runs[c]
        streams.append({"cfgs": sorted(cs), "secs": secs, "offs": offs})
    unowned = sum(1 for s in range(xa.n) if xa.sm[s] & SM_AUDIO and s not in owner)
    print("xa repack: %d configs, %d streams, %d audio sectors (%d unplayed), "
          "%d sectors in" % (len(runs), len(streams), len(owner), unowned, xa.n))

    # 2. JP drops: a stream goes only if every config on it is JP
    jp = {i - 0x300 for i in JP_IDS}
    silent = None
    if not args.keep_all:
        mixed = 0
        for st in streams:
            if all(c in jp for c in st["cfgs"]):
                st["dropped"] = True
            elif any(c in jp for c in st["cfgs"]):
                mixed += 1
        dropped = [st for st in streams if st.get("dropped")]
        streams = [st for st in streams if not st.get("dropped")]
        silent = {"cfgs": sorted(c for st in dropped for c in st["cfgs"]),
                  "secs": None, "offs": {}, "silent": True}
        silent["offs"] = {c: 0 for c in silent["cfgs"]}
        streams.append(silent)
        print("  JP lines: %d streams (%d sectors) -> one silent stream%s" % (
            len(dropped), sum(len(st["secs"]) for st in dropped),
            ", %d streams shared with kept lines stay" % mixed if mixed else ""))

    # the dub's streams (already 37.8 kHz mono sectors)
    for c, e in sorted(dubbed.items()):
        streams.append({"cfgs": [c], "secs": None, "offs": {c: 0}, "dub": e,
                        "psp": bool(e.get("psp"))})
    if dubbed:
        print("  dub: %d configs -> new mono streams (%d sectors)" % (
            len(dubbed), sum(e["sectors"] for e in dubbed.values())))
    for c, e in sorted(jpdub.items()):
        streams.append({"cfgs": [c], "secs": None, "offs": {c: 0}, "dub": e,
                        "jp": True})
    if jpdub:
        print("  jp dub: %d configs -> new mono streams (%d sectors)" % (
            len(jpdub), sum(e["sectors"] for e in jpdub.values())))

    # 3. the payload of each stream: stereo sectors, or mono groups
    nmono = saved = 0
    for st in streams:
        if st.get("dub"):
            raw = open(st["dub"]["xa"], "rb").read()
            st["dsecs"] = [raw[k:k + S] for k in range(0, len(raw), S)]
            assert len(st["dsecs"]) == st["dub"]["sectors"]
            assert all(s[18] & SM_AUDIO and not s[19] & CODING_STEREO for s in st["dsecs"])
            st["mono"] = True
            st["n"] = len(st["dsecs"])
            continue
        if st.get("silent"):
            st["mono"] = False
            st["n"] = SILENT_SECTORS
            continue
        st["mono"] = False
        st["n"] = len(st["secs"])
        if args.no_mono or any(o % 2 for o in st["offs"].values()):
            continue
        g = groups_of(xa, st["secs"])
        if dual_mono(xa, st["secs"], g):
            st["mono"] = True
            st["mgroups"] = to_mono(g)
            st["n"] = len(st["mgroups"]) // GROUPS
            nmono += 1
            saved += len(st["secs"]) - st["n"]
    if not args.no_mono:
        print("  mono: %d dual-mono streams, %d sectors saved" % (nmono, saved))

    # 4. pack: 16 slots, rows of 16 sectors; first fit decreasing. The PSP
    # versions go after everything else, in a region of their own, then the
    # JP ones in another
    def region(st):
        return 2 if st.get("jp") else 1 if st.get("psp") else 0
    h = [0] * 16
    order = sorted(range(len(streams)), key=lambda k: (
        region(streams[k]), -streams[k]["n"], streams[k]["cfgs"][0]))
    cur_region = 0
    for k in order:
        if region(streams[k]) != cur_region:
            cur_region = region(streams[k])
            h = [max(h)] * 16
        st = streams[k]
        n = st["n"]
        if st["mono"]:
            slot = min(range(16), key=lambda s: (h[s], s))
            st["slot"], st["row"] = slot, h[slot]
            h[slot] += n
        else:
            lane = min(range(8), key=lambda L: (max(h[L], h[L + 8]), abs(h[L] - h[L + 8]), L))
            row = max(h[lane], h[lane + 8])
            st["slot"], st["row"] = lane, row
            h[lane] = row + (n + 1) // 2
            h[lane + 8] = row + n // 2
    rows = max(h)
    total = rows * 16
    # file/channel: channel = slot, file = a counter per channel (from 1:
    # the filler sectors are file 0 / channel 0)
    # The PSP region (after every other row) counts again from 1 in each
    # channel (mod_psp_xa.h has 6 bits for it): only the stream right before
    # in the same channel must differ (a timer running past a stream's end),
    # so the first PSP stream skips the file of the last one before it
    # (the JP region the same, after the PSP one)
    nfile = [1] * 16
    started = [0] * 16  # the region each channel's counter is in
    for k in sorted(range(len(streams)), key=lambda k: (streams[k]["slot"], streams[k]["row"])):
        st = streams[k]
        st["ch"] = st["slot"]
        if region(st) != started[st["ch"]]:
            started[st["ch"]] = region(st)
            last = nfile[st["ch"]] - 1
            nfile[st["ch"]] = 2 if last == 1 else 1
        st["file"] = nfile[st["ch"]]
        nfile[st["ch"]] += 1
        assert st["file"] < 256

    def pos(st, k):
        """the new sector of the stream's k-th sector"""
        if st["mono"]:
            return 16 * (st["row"] + k) + st["slot"]
        return 16 * st["row"] + st["slot"] + 8 * k

    # 5. write
    out = [None] * total
    for st in streams:
        f, ch = st["file"], st["ch"]
        if st.get("dub"):
            for k, s in enumerate(st["dsecs"]):
                out[pos(st, k)] = sector_bytes(s, f, ch, s[18], s[19],
                                               s[DATA:DATA + GROUPS * 128])
        elif st.get("silent"):
            for k in range(st["n"]):
                out[pos(st, k)] = sector_bytes(filler, f, ch, 0x64, CODING_STEREO,
                                               bytes(GROUPS * 128))
        elif st["mono"]:
            g = st["mgroups"]
            for k in range(st["n"]):
                src = st["secs"][min(2 * k, len(st["secs"]) - 1)]
                sm = int(xa.sm[src]) & ~SM_EOF & 0xFF
                if k == st["n"] - 1:
                    sm |= int(xa.sm[st["secs"][-1]]) & SM_EOF
                out[pos(st, k)] = sector_bytes(
                    xa.sector(src), f, ch, sm, int(xa.coding[src]) & ~CODING_STEREO & 0xFF,
                    g[k * GROUPS:(k + 1) * GROUPS].tobytes())
        else:
            for k, src in enumerate(st["secs"]):
                s = xa.sector(src)
                sub = bytes((f, ch, s[18], s[19]))
                out[pos(st, k)] = s[:16] + sub + sub + s[DATA:]
    for i in range(total):
        assert out[i] is None or len(out[i]) == S
    tmp = OUT_XA + ".tmp"
    with open(tmp, "wb") as fo:
        for s in out:
            fo.write(filler if s is None else s)

    # 6. the new configs (PSP versions: their own table)
    new = [list(c) for c in cfg]
    psp, jp = {}, {}
    for st in streams:
        if st.get("jp"):
            (c,) = st["cfgs"]
            jp[c] = [pos(st, 0) - st["ch"], st["dub"]["unk228"], st["file"], st["ch"]] + cfg[c][4:]
            continue
        if st.get("psp"):
            (c,) = st["cfgs"]
            psp[c] = [pos(st, 0) - st["ch"], st["dub"]["unk228"], st["file"], st["ch"]] + cfg[c][4:]
            continue
        for c, off in st["offs"].items():
            k = off // 2 if st["mono"] else off
            new[c][0] = pos(st, k) - st["ch"]
            new[c][2] = st["file"]
            new[c][3] = st["ch"]
            if st.get("dub"):
                new[c][1] = st["dub"]["unk228"]

    # 7. verify: re-extract every config from the written file
    nx = Xa(tmp)
    assert nx.n == total
    pairs = {}
    for st in streams:
        key = (st["file"], st["ch"], region(st))  # (each region apart)
        assert key not in pairs
        pairs[key] = 1
    bad = 0
    for st in streams:
        for c, off in st["offs"].items():
            addr, ln, f, ch, vol, u, mode = (jp[c] if st.get("jp") else
                                             psp[c] if st.get("psp") else new[c])
            r = nx.run(addr + ch, f, ch)
            o = cfg[c]
            ok = (ln, vol, u, mode) == (o[1], o[4], o[5], o[6])
            if st.get("dub"):
                ok = (ln == st["dub"]["unk228"] and (vol, u, mode) == (o[4], o[5], o[6])
                      and len(r) == st["n"] and all(
                          nx.coding[a] == 0 and
                          nx.sector(a)[DATA:DATA + GROUPS * 128] == b[DATA:DATA + GROUPS * 128]
                          for a, b in zip(r, st["dsecs"])))
            elif st.get("silent"):
                ok = ok and len(r) == SILENT_SECTORS and all(
                    nx.sector(s)[DATA:DATA + GROUPS * 128] == bytes(GROUPS * 128) for s in r)
            elif st["mono"]:
                orig = runs[c]
                exp = (len(orig) + 1) // 2
                ok = ok and len(r) == exp and all(nx.coding[s] == 0 for s in r)
                if ok:
                    p0, n0 = units_of(groups_of(xa, orig))
                    p1, n1 = units_of(groups_of(nx, r))
                    lp, ln_ = p0[:, 0::2].reshape(-1), n0[:, 0::2].reshape(-1, 28)
                    mp, mn = p1.reshape(-1), n1.reshape(-1, 28)
                    m = len(lp)
                    ok = (np.array_equal(lp, mp[:m]) and np.array_equal(ln_, mn[:m]) and
                          not mp[m:].any() and not mn[m:].any())
            else:
                orig = runs[c]
                ok = ok and len(r) == len(orig) and all(
                    nx.sector(a)[18:20] == xa.sector(b)[18:20] and
                    nx.sector(a)[DATA:] == xa.sector(b)[DATA:] for a, b in zip(r, orig))
            if not ok:
                bad += 1
                print("  MISMATCH config 0x%X" % (0x300 + c), file=sys.stderr)
    if bad:
        os.remove(tmp)
        sys.exit("xa repack: %d configs failed verification" % bad)
    os.replace(tmp, OUT_XA)

    # 8. the config tables
    lines = open(GEN_H).read().split("\n")
    k = 0
    for j, line in enumerate(lines):
        if line.startswith("{"):
            c = new[k]
            lines[j] = "{0x%05X, 0x%04X, 0x%02X, %d, %d, %d, %d}," % tuple(c)
            k += 1
    assert k == len(retail)
    open(GEN_H, "w").write("\n".join(lines))
    src = open(MOD_H).read()
    # a rerun on an already repacked header: drop the dub's lines first
    src = re.sub(r"    // PSP dub .*\n    \{.*\},\n", "", src)
    src = re.sub(r"#define \w+ 0x\w+ // PSP dub .*\n", "", src)
    src = re.sub(r"#define MOD_XA_CONFIGS \d+\n", "#define MOD_XA_CONFIGS %d\n" % len(mod), src)
    it = iter(new[len(retail):])
    src, cnt = re.subn(r"\{\d+, \d+, \d+, \d+, \d+, \d+, \d+\},",
                       lambda m: "{%d, %d, %d, %d, %d, %d, %d}," % tuple(next(it)), src)
    assert cnt == len(mod)
    if extra:  # the dub's new lines, after the music (gen_saturn_music.py
        # rewrites the header every build, so this runs on a fresh one)
        rows = "".join("    // PSP dub %s.at3 (mods/sotn-ultimate/tools/psp_dub.py)\n"
                       "    {%d, %d, %d, %d, %d, %d, %d},\n" % ((e["name"],) + tuple(next(it)))
                       for e in extra)
        defs = "".join("#define %s 0x%X // PSP dub %s.at3\n" % (
            e["define"], 0x300 + e["index"], e["name"]) for e in extra)
        src, n1 = re.subn(r"#define MOD_XA_CONFIGS (\d+)\n",
                          lambda m: defs + "#define MOD_XA_CONFIGS %d\n" % (
                              len(mod) + len(extra)), src)
        src, n2 = re.subn(r"\};\n#endif", lambda m: rows + "};\n#endif", src)
        assert n1 == 1 and n2 == 1 and len(mod) + len(extra) == len(new) - len(retail)
    src = re.sub(r"\(file \d+\)\n", "(repacked: mods/sotn-ultimate/tools/repack_xa.py)\n", src, count=1)
    open(MOD_H, "w").write(src)
    write_psp_h(psp)
    write_jp_h(jp, {x["define"]: x["index"] for x in extra})
    print("xa repack: %d streams, %d sectors out (%.1f MB), %d saved; "
          "all %d configs verified" % (len(streams), total, total * S / 1e6,
                                       xa.n - total, sum(len(st["offs"]) for st in streams)))


def write_psp_h(psp):
    """src/dra/mod_psp_xa.h: the PSP versions' table (src/dra/sound.c)"""
    n = PSP_LAST - PSP_FIRST + 1
    bits = [0] * ((n - 1) // 8 + 1)
    base = min((a + ch) & ~15 for a, _, _, ch, *_ in psp.values()) if psp else 0

    def word(t, c):
        a, ln, f, ch = t[c][:4]
        rel = a + ch - base
        assert a % 16 == 0 and ch < 16, hex(0x300 + c)
        assert 0 <= rel < 1 << 15 and 0 < ln < 1 << 10 and 0 < f < 1 << 6, (hex(0x300 + c), rel, ln, f)
        return rel | ln << 15 | f << 25
    words = []
    for c in sorted(psp):
        k = 0x300 + c - PSP_FIRST
        bits[k >> 3] |= 1 << (k & 7)
        words.append(word(psp, c))
    out = ["// Generated by mods/sotn-ultimate/tools/repack_xa.py, do not edit",
           "// PSP dub toggle: the voice ids with a PSP English version (see sound.c)",
           "#define MOD_PSP_XA_BASE %d" % base,
           "#define MOD_PSP_XA_COUNT %d" % len(words),
           "static const u8 g_ModPspXaBits[%d] = {%s};" % (len(bits), ", ".join("0x%02X" % b for b in bits)),
           "static const u32 g_ModPspXa[%d] = {" % max(1, len(words))]
    for i in range(0, len(words), 6):
        out.append("    " + ", ".join("0x%08X" % w for w in words[i:i + 6]) + ",")
    if not words:
        out.append("    0,")
    out.append("};")
    s = "\n".join(out) + "\n"
    if not os.path.exists(PSP_H) or open(PSP_H).read() != s:
        open(PSP_H, "w").write(s)
    print("xa repack: %d PSP dub versions (sectors from %d)" % (len(words), base))


def write_jp_h(jp, extra_index):
    """src/st/sel/mod_jp_xa.h: the JP versions' table (SEL copies it into
    DRA's g_ModDub.jp; src/dra/sound.c ModXaConfig plays it while
    MOD_JP_DUB_ON()). Empty (count 0) without --dub-jp"""
    n = PSP_LAST - PSP_FIRST + 1
    bits = [0] * ((n - 1) // 8 + 1)
    base = min((a + ch) & ~15 for a, _, _, ch, *_ in jp.values()) if jp else 0

    def word(c):
        a, ln, f, ch = jp[c][:4]
        rel = a + ch - base
        assert a % 16 == 0 and ch < 16, hex(0x300 + c)
        assert 0 <= rel < 1 << 15 and 0 < ln < 1 << 11 and 0 < f < 1 << 6, (
            hex(0x300 + c), rel, ln, f)
        return rel | ln << 15 | f << 26
    words = []
    extra = {}
    for c in sorted(jp):
        if c >= 563:  # a new config (MOD_VO_DID_YOU_NOT)
            extra[c] = word(c)
            continue
        k = 0x300 + c - PSP_FIRST
        bits[k >> 3] |= 1 << (k & 7)
        words.append(word(c))
    dyn = extra.get(extra_index.get("MOD_VO_DID_YOU_NOT"), 0)
    assert len(extra) == (1 if dyn else 0), extra
    out = ["// Generated by mods/sotn-ultimate/tools/repack_xa.py, do not edit",
           "// JP dub: the voice ids with a Japanese version (see src/dra/sound.c);",
           "// SEL copies the table into DRA's g_ModDub.jp (include/game.h)",
           "#define MOD_JP_XA_BASE %d" % base,
           "#define MOD_JP_XA_COUNT %d" % len(words),
           "#define MOD_JP_XA_DID_YOU_NOT 0x%08X" % dyn,
           "#define MOD_JP_XA_BITS {%s}" % ", ".join("0x%02X" % b for b in bits),
           "#define MOD_JP_XA_WORDS {\\"]
    for i in range(0, len(words), 6):
        out.append("    " + ", ".join("0x%08X" % w for w in words[i:i + 6]) + ",\\")
    if not words:
        out.append("    0,\\")
    out.append("}")
    s = "\n".join(out) + "\n"
    if not os.path.exists(JP_H) or open(JP_H).read() != s:
        open(JP_H, "w").write(s)
    print("xa repack: %d JP dub versions (sectors from %d)" % (len(words), base))


if __name__ == "__main__":
    main()
