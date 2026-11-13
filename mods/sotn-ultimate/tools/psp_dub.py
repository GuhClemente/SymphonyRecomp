#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the PSP (Dracula X Chronicles) English dub on the PS1.

Source: the PSP's English voice files (res/ps/snd_e/XA/*.at3, copied to
disks/psp/snd_e/XA) and their id table (src/dra_psp/63C08.c at3FileNames,
indexed by PSP sound id - 0x300). The PS1 US voice ids are the PSP ids + 2
for the whole voice range (PSP 0x356..0x530 -> PS1 0x358..0x532), checked
against the durations and the 162 cutscene lines of both scripts.

For every PS1 voice config (XA_STR1, mode 2) with an English PSP file:
decode, downmix to mono, resample to 37800 Hz, trim the silence
around the line, scale by GAIN (the PSP lines are mastered ~3.5 dB louder
than the PS1 ones: median active RMS ratio 0.67 over 42 lines; the PS1
lines peak at ~31000), encode as 37.8 kHz mono XA (satxa encmono). The encode is the
bring-your-own-discs builder's (mods/sotn-ultimate/tools/builder/src/audio.ts encodeVoice:
FFmpeg 7.1.1 + satxa as WebAssembly, run here through Node), so the player's
builder page remakes these streams byte for byte; a native ffmpeg's float
decode differs from CPU to CPU.
JP-only slots (repack_xa.JP_IDS) are skipped: nothing plays them on US.
The PS1 lines with no English PSP file (faerie hints, many demon lines,
some Librarian / Shaft / Maria extras) keep the PS1 audio.

PSP 0x531 (380e2 "...Did you not?", Dracula in the final scene) has no PS1
slot: it gets a new mod config after the Saturn music ones (MOD_VO_DID_YOU_NOT,
volume of the line before it).

Writes build/us/psp_dub/dub.json, read by mods/sotn-ultimate/tools/repack_xa.py --dub:
the replacement stream (raw mono sectors) and the new length (unk228, in
VSyncs) per config. Encodes are cached (build/us/psp_dub/*.xa).

--set jp: the third dub, "JP" (Japanese voices, the PSP English script as
subtitles; include/game.h MOD_JP_DUB_ON): the PSP's Japanese voice files
(res/ps/sound/XA, the 1997 PS1 Japanese cast; same file names and id table)
for the same PS1 slots as the English set (its dub.json, so run --set psp
first; no English placeholder redirects: every JP file is a real take),
plus Maria's CEN scene (slots 0x3BA-0x3CF, Saturn ids 0x3B8-0x3CD) with the
Saturn's own takes (disks/saturn/SD/SDF0.PCM, 4-bit IMA 22,050 Hz; its
fight lines 0x3C0/0x3C4/0x3C5/0x3CC are other takes than the PSP's)
including 0x3C4, which has no English PSP line. The JP files are
peak-normalised (median peak 32,400) with ~4.5 dB less body than the PSP
English ones: GAIN_JP 0.95 keeps their peaks where the PS1 lines' are; the
Saturn takes are first matched to the PSP JP ones (median active RMS of the
same takes). Writes build/us/jp_dub/dub.json (repack_xa.py --dub-jp).
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import wave
from concurrent.futures import ThreadPoolExecutor

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import repack_xa  # noqa: E402
from gen_psp_grunts import active_rms  # noqa: E402
from mod_work import work_dir  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
AT3_DIR = os.path.join(ROOT, "disks/psp/snd_e/XA")
NAMES_C = os.path.join(ROOT, "src/dra_psp/63C08.c")
WORK = os.path.join(ROOT, "build/us/psp_dub")
OUT_JSON = os.path.join(WORK, "dub.json")
SATXA = os.path.join(ROOT, "bin/satxa")
# the builder's audio code (see the top); NODE runs .ts directly (Node 24)
NODE = os.environ.get("NODE", "node")
AUDIO_CLI = os.path.join(ROOT, "mods/sotn-ultimate/tools/builder/node/audio_cli.ts")
AUDIO_DEPS = [AUDIO_CLI, os.path.join(ROOT, "mods/sotn-ultimate/tools/builder/src/audio.ts"),
              work_dir("builder", "audio", "audio_core.js")]
RATE = 37800
GAIN = 0.70
# --set jp
JP_AT3_DIR = os.path.join(ROOT, "disks/psp/sound/XA")
JP_WORK = os.path.join(ROOT, "build/us/jp_dub")
GAIN_JP = 0.95
SDF0 = os.path.join(ROOT, "disks/saturn/SD/SDF0.PCM")
SAT_INDEX = os.path.join(ROOT, "disks/saturn/0.BIN")  # base 0x06004080
SAT_SLOTS = range(0x3BA, 0x3D0)  # PS1 slots = Saturn ids + 2
SAT_RATE = 22050
# Saturn ids whose take is not the PSP JP one (no loudness reference)
SAT_OTHER_TAKES = {0x3BE, 0x3C2, 0x3C3, 0x3CA}
# silence trim: window RMS under this is silence; keep a little around it
SILENCE = 250
LEAD_S, TAIL_S = 0.03, 0.12
# the stop timer (unk228, VSyncs) ends this many frames after the sound
TIMER_MARGIN = 3
# the decode is at this scale, the gain is applied after: ATRAC3plus
# decodes to float and many PSP lines peak over full scale (up to +1.4 dB,
# boyz4h), which a straight 16-bit decode clipped flat (155 of 238 lines).
# (audio.ts HEADROOM: the encode itself lives there now)
HEADROOM = 0.5
FIRST_PSP, LAST_PSP = 0x356, 0x530  # PS1 id = PSP id + 2
# PS1 slots whose matching PSP line is not PSP id = PS1 id - 2 (see
# psp_script.py TEXT_OVERRIDE, which gives them the same lines' text)
VOICE_OVERRIDE = {
    0x38B: "080b",  # DRE "You broke free of my spell" (PSP 080a is unused)
    0x3D4: "232b",  # BO6: the PSP's branches play 232b / 231b the other
    0x3D6: "231b",  # way round
    # CEN, Maria's scene (psp_script.py TEXT_OVERRIDE): the PSP lines that
    # say the same as the PS1 skeleton's, by the same speaker
    0x3CC: "202c1",  # "If it must be that way... Here. I found these."
    0x3CD: "201d",  # "And these are?"
    0x3CF: "201f",  # "Thank you. I go to find Richter. Stay, and pray..."
    # LIB: the Librarian's greeting when you talk to him. PS1 script plays
    # 0x3B9, the PSP's (lib.bin, same SCRIPT_SWITCH spot) plays 0x3B5 =
    # 180a; 180a2 (PSP 0x3B7) is a silent EN placeholder
    0x3B9: "180a",
}
# voice ids with no PSP version (psp_script.py: lines without PSP text, and
# NO_PSP, the Saturn Maria fight's 0x3C4, cut from the PSP copy)
KEEP_JSON = os.path.join(ROOT, "build/us/psp_script/keep_ps1.json")
EXTRA = [  # (define, PSP id, config whose volume it takes)
    ("MOD_VO_DID_YOU_NOT", 0x531, 0x427),
]


def ffmpeg():
    """the decoder: the builder's (AUDIO_CLI), no native ffmpeg any more;
    (exe, path mapper) kept for the callers' signatures"""
    for p in [NODE] + AUDIO_DEPS:
        if not os.path.exists(p):
            sys.exit("psp_dub: missing %s (mods/sotn-ultimate/tools/builder/native/build.sh)" % p)
    return NODE, lambda p: p


def audio_cli(*args):
    """run mods/sotn-ultimate/tools/builder/node/audio_cli.ts; its JSON line, if any"""
    out = subprocess.run([NODE, AUDIO_CLI] + [str(a) for a in args], check=True,
                         capture_output=True, text=True, stdin=subprocess.DEVNULL).stdout.strip()
    return json.loads(out) if out else None


def psp_names():
    s = open(NAMES_C).read()
    body = re.search(r"at3FileNames\[\] = \{(.*?)\};", s, re.S).group(1)
    return re.findall(r'"([^"]+)"', body)


def psp_en_remap():
    """the PSP's English voice redirects (63C08.c func_psp_09141608,
    D_psp_09189D40: {id, id played instead} when the voice language is not
    JP), e.g. 0x4FA maks1b (silent EN placeholder) -> 0x4F9 maks1a"""
    s = open(NAMES_C).read()
    body = re.search(r"D_psp_09189D40\[\]\[2\] = \{(.*?)\};", s, re.S).group(1)
    return {int(a, 16): int(b, 16)
            for a, b in re.findall(r"\{(0x[0-9A-Fa-f]+), (0x[0-9A-Fa-f]+)\}", body)}


def encode(name, exe, path_of, src=None, work=WORK, gain=GAIN):
    """at3 (or wav) -> trimmed, scaled 37800 Hz mono wav -> mono XA; cached"""
    src = src or os.path.join(AT3_DIR, name + ".at3")
    xa = os.path.join(work, name + ".xa")
    meta = os.path.join(work, name + ".json")
    deps = [src, os.path.abspath(__file__)] + AUDIO_DEPS
    if os.path.exists(meta) and all(os.path.getmtime(meta) >= os.path.getmtime(d) for d in deps):
        res = json.load(open(meta))
        if res.get("gain") == repr(gain):  # (the Saturn takes' gain is computed)
            return res
    # mods/sotn-ultimate/tools/builder/src/audio.ts encodeVoice (the decode at HEADROOM,
    # highpass, silence trim with SILENCE / LEAD_S / TAIL_S, gain, satxa
    # encmono), as the player's builder runs it
    info = audio_cli("voice", src, xa, repr(gain))
    # src and gain: what the builder's manifest remakes the stream from
    res = {"name": name, "xa": xa, "sectors": info["sectors"],
           "seconds": info["seconds"], "unk228": info["unk228"],
           "src": os.path.relpath(src, ROOT), "gain": repr(gain)}
    json.dump(res, open(meta, "w"))
    return res


IMA_STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
            50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
            253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
            1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
            3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
            11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794,
            32767]
IMA_IDX = [-1, -1, -1, -1, 2, 4, 6, 8] * 2


def ima_decode(buf):
    """the Saturn's voice ADPCM: 4-bit IMA, high nibble first, no header"""
    out = np.empty(len(buf) * 2, np.int16)
    p = i = k = 0
    for byte in buf:
        for n in (byte >> 4, byte & 15):
            s = IMA_STEP[i]
            d = s >> 3
            if n & 4:
                d += s
            if n & 2:
                d += s >> 1
            if n & 1:
                d += s >> 2
            p = max(-32768, min(32767, p - d if n & 8 else p + d))
            i = max(0, min(88, i + IMA_IDX[n]))
            out[k] = p
            k += 1
    return out


def saturn_clip(sid):
    """Saturn voice id -> its PCM (SDF0.PCM; index in 0.BIN at 0x06039414:
    header {0x7FFF, 0x3FF, 0}, then {u32 offset, u32 size} BE per id from
    0x33E; clips are padded to 0x800 with silence, trimmed later)"""
    z = open(SAT_INDEX, "rb").read()
    hdr = 0x06039414 - 0x06004080
    assert z[hdr:hdr + 12] == bytes.fromhex("00007fff000003ff00000000"), "Saturn 0.BIN"
    o = 0x0603941C - 0x06004080 + (sid - 0x33D) * 8
    off, size = int.from_bytes(z[o:o + 4], "big"), int.from_bytes(z[o + 4:o + 8], "big")
    with open(SDF0, "rb") as f:
        f.seek(off)
        return ima_decode(f.read(size))


def write_wav(path, x, rate):
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(np.asarray(x, np.int16).tobytes())


def read_wav(path):
    with wave.open(path) as w:
        return np.frombuffer(w.readframes(w.getnframes()), np.int16).astype(np.float64)


def main_jp():
    """--set jp: build/us/jp_dub/dub.json (see the module doc)"""
    os.makedirs(JP_WORK, exist_ok=True)
    en = json.load(open(OUT_JSON))
    names = psp_names()
    retail, _ = repack_xa.read_configs()
    have = {f[:-4] for f in os.listdir(JP_AT3_DIR) if f.endswith(".at3")}
    jobs = {}  # config index -> JP file name
    for c in sorted(int(k) for k in en["replace"]):
        ps1 = 0x300 + c
        if ps1 in SAT_SLOTS:
            continue
        name = VOICE_OVERRIDE.get(ps1, names[ps1 - 2 - 0x300][:-4])
        assert name in have, (hex(ps1), name)
        jobs[c] = name
    exe, path_of = ffmpeg()
    # the Saturn takes: decoded to 22,050 Hz wavs, scaled to the PSP JP
    # files' loudness (median over the same takes), then as the others
    sat = {}
    for ps1 in SAT_SLOTS:
        c = ps1 - 0x300
        if retail[c][6] != 2 or retail[c][1] == 0:
            continue
        sid = ps1 - 2
        path = os.path.join(JP_WORK, "sat_%X.src.wav" % sid)
        if not os.path.exists(path) or os.path.getmtime(path) < os.path.getmtime(__file__):
            write_wav(path, saturn_clip(sid), SAT_RATE)
        sat[c] = (sid, path)
    ratios = []
    for c, (sid, path) in sat.items():
        name = names[sid - 0x300][:-4]
        if sid in SAT_OTHER_TAKES or name not in have:
            continue
        ref = os.path.join(JP_WORK, name + ".ref_ts.wav")
        if not os.path.exists(ref):
            audio_cli("decode", os.path.join(JP_AT3_DIR, name + ".at3"), ref,
                      "aresample=%d,aformat=sample_fmts=s16:channel_layouts=mono" % SAT_RATE)
        a, b = active_rms(read_wav(ref), SAT_RATE), active_rms(read_wav(path), SAT_RATE)
        if a and b:
            ratios.append(a / b)
    sat_gain = float(np.median(ratios))
    extra_names = [names[p - 0x300][:-4] for _, p, _ in EXTRA]
    todo = sorted(set(jobs.values()) | set(extra_names))
    with ThreadPoolExecutor(8) as ex:
        enc = dict(zip(todo, ex.map(lambda n: encode(
            n, exe, path_of, os.path.join(JP_AT3_DIR, n + ".at3"), JP_WORK, GAIN_JP), todo)))
    with ThreadPoolExecutor(8) as ex:
        senc = dict(zip(sat, ex.map(lambda c: encode(
            "sat_%X" % sat[c][0], exe, path_of, sat[c][1], JP_WORK,
            GAIN_JP * sat_gain), sat)))
    replace = {}
    for c, name in jobs.items():
        r = enc[name]
        replace[str(c)] = {"name": name, "xa": r["xa"], "sectors": r["sectors"],
                           "unk228": r["unk228"], "seconds": r["seconds"],
                           "old_unk228": retail[c][1]}
    for c, r in senc.items():
        replace[str(c)] = {"name": r["name"], "xa": r["xa"], "sectors": r["sectors"],
                           "unk228": r["unk228"], "seconds": r["seconds"],
                           "old_unk228": retail[c][1], "saturn": True}
    extra = []
    for (define, psp, like), name in zip(EXTRA, extra_names):
        r = enc[name]
        extra.append({"define": define, "name": r["name"], "xa": r["xa"],
                      "sectors": r["sectors"], "unk228": r["unk228"],
                      "seconds": r["seconds"], "like": like - 0x300})
    out = os.path.join(JP_WORK, "dub.json")
    json.dump({"replace": replace, "extra": extra}, open(out, "w"), indent=0)
    longest = sorted(list(replace.values()) + extra, key=lambda r: -r["seconds"])[:6]
    print("jp dub: %d PS1 voice slots get a Japanese version (%d Saturn takes, x%.2f to "
          "the PSP's), %.0f s, %d mono sectors, %d extra; longest %s" % (
              len(replace), len(senc), sat_gain,
              sum(r["seconds"] for r in replace.values()),
              sum(r["sectors"] for r in replace.values()) + sum(r["sectors"] for r in extra),
              len(extra), ", ".join("%s %.1f s (unk228 %d)" % (
                  r["name"], r["seconds"], r["unk228"]) for r in longest)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--set", choices=("psp", "jp"), default="psp")
    if ap.parse_args().set == "jp":
        return main_jp()
    os.makedirs(WORK, exist_ok=True)
    names = psp_names()
    remap = psp_en_remap()
    assert len(remap) == 5, remap
    retail, _ = repack_xa.read_configs()
    jp = set(repack_xa.JP_IDS)
    have = {f[:-4] for f in os.listdir(AT3_DIR) if f.endswith(".at3")}
    jobs = {}  # config index -> PSP file
    skipped = []
    keep = set(json.load(open(KEEP_JSON))) if os.path.exists(KEEP_JSON) else set()
    for psp in range(FIRST_PSP, LAST_PSP + 1):
        ps1 = psp + 2
        name = VOICE_OVERRIDE.get(ps1, names[remap.get(psp, psp) - 0x300][:-4])
        c = ps1 - 0x300
        addr, ln, f, ch, vol, u, mode = retail[c]
        if name not in have:
            continue  # no English file: the PS1 line stays
        if ps1 in jp:
            skipped.append("0x%X %s (JP-only slot)" % (ps1, name))
            continue
        if mode != 2 or ln == 0:
            skipped.append("0x%X %s (not a voice config)" % (ps1, name))
            continue
        jobs[c] = name
    for define, psp, _ in EXTRA:
        assert names[psp - 0x300][:-4] in have
    exe, path_of = ffmpeg()
    todo = sorted(set(jobs.values()) | {names[p - 0x300][:-4] for _, p, _ in EXTRA})
    with ThreadPoolExecutor(8) as ex:
        enc = dict(zip(todo, ex.map(lambda n: encode(n, exe, path_of), todo)))
    replace, durations = {}, {}
    for c, name in sorted(jobs.items()):
        e = enc[name]
        # every encoded line's length, for psp_script.py's typing speeds
        durations["%d" % (0x300 + c)] = e["unk228"]
        if 0x300 + c in keep:
            skipped.append("0x%X %s (no PSP version: psp_script.py keep list)" % (0x300 + c, name))
            continue
        replace[str(c)] = {"name": name, "xa": e["xa"], "sectors": e["sectors"],
                           "unk228": e["unk228"], "seconds": e["seconds"],
                           "old_unk228": retail[c][1]}
    extra = []
    for define, psp, like in EXTRA:
        e = enc[names[psp - 0x300][:-4]]
        extra.append({"define": define, "name": e["name"], "xa": e["xa"],
                      "sectors": e["sectors"], "unk228": e["unk228"],
                      "seconds": e["seconds"], "like": like - 0x300})
    json.dump({"replace": replace, "extra": extra, "durations": durations},
              open(OUT_JSON, "w"), indent=0)
    old = sum(r["old_unk228"] for r in replace.values()) / 60
    new = sum(r["seconds"] for r in replace.values())
    print("psp dub: %d PS1 voice slots get a PSP English version (%.0f s, was %.0f s), "
          "%d new, %d mono sectors" % (len(replace), new, old, len(extra),
                                       sum(r["sectors"] for r in replace.values())))
    for s in skipped:
        print("  skipped " + s)


if __name__ == "__main__":
    main()
