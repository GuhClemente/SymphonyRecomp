#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Saturn-only music tracks as PS1 XA streams.

Encodes Saturn SD/SDxx.PCM files with bin/satxa (mods/sotn-ultimate/tools/satxa.c) and
appends them to a copy of the retail SD/XA_STR1 (build/us/XA_STR1.unpacked,
which mods/sotn-ultimate/tools/repack_xa.py packs into build/us/XA_STR1., the file
`make disk` takes over the retail one). The retail stream interleaves 8
channels a sector each (music files 0-70, voices 1-99); the new tracks use
file MOD_XA_FILE, one channel each, in one appended region whose unused
slots are the retail filler sector (submode 0).

Writes src/dra/mod_xa_music.h: the XaMusicConfig entries (intro + loop per
track, read by src/dra/sound.c past the retail table) and the MU_MOD_* ids.
The PS1 player times the intro and loop with VSync counts and, when the
loop's runs out, seeks back to the loop entry's sector (CdSoundCommand6).

After the tracks, ONESHOTS: streams with one config each (no loop), from a
PSP .at3 (mods/sotn-ultimate/tools/builder/src/audio.ts encodeSong: the builder's decode +
satxa encstereo, through Node): the PSP's English fairy
song, which src/dra/sound.c ModXaConfig plays for MU_NOCTURNE while the PSP
dub is on.
"""
import json
import math
import os
import subprocess
import sys
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from psp_dub import audio_cli, ffmpeg  # noqa: E402
from mod_work import work_dir  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
RETAIL = os.path.join(ROOT, "disks/us/SD/XA_STR1.")
# mods/sotn-ultimate/tools/repack_xa.py packs it into build/us/XA_STR1. (the disc's file)
OUT = os.path.join(ROOT, "build/us/XA_STR1.unpacked")
# the mod configs as written to HEADER, for repack_xa.py (which rewrites them)
OUT_JSON = os.path.join(ROOT, "build/us/mod_xa_music.json")
WORK = work_dir("music")
HEADER = os.path.join(ROOT, "src/dra/mod_xa_music.h")
SATXA = os.path.join(ROOT, "bin/satxa")

RETAIL_CONFIGS = 563  # assets/dra/us/xa_music_config.yaml
MOD_XA_FILE = 100  # unused by the retail stream (files 0-99)
VOLUME = 80
# the player's timers count VSyncs: SOTN's display is 240p NTSC, whose
# refresh is 53693175 / (3413 * 263) = 59.826 Hz, not 59.94 (the interlaced
# rate). At 59.94 the loop timer ran out 0.2% late (0.13 s for SD2C's 64 s
# loop, plus the intro's): the loop start's copy after the loop end played,
# then the seek back and that start again, an audible stutter + gap. (Retail
# uses 60 per second, later still, but its data ends at the loop end.)
# the data repeats each loop body until the track lasts this long (satxa
# enc): the seek back to the loop entry stops the XA stream for the seek's
# time, unavoidable with this player (retail tracks too: their data ends at
# the loop end, nothing duplicated, and their timers run slightly past it),
# so it comes every 4-5 minutes instead of every loop. For every track:
# mods/sotn-ultimate/tools/repack_xa.py drops the region's unused interleave slots, so a
# track costs only its own length (the unpacked region is 8 x the longest)
MIN_SECONDS = 270  # SD2C x4, SD25 x3, the others x2: a seek every ~4.5 min
# and that seek goes to the loop's quietest stretch (satxa enc's fade_ms):
# the loop entry moves there, the data plays on to the same point a pass
# later and fades out over JUMP_FADE_MS, so the pause falls where the music
# is already quiet (Cursed Prison: a silent bar) instead of mid-phrase
JUMP_FADE_MS = 100
# not for the tracks only the Sound Mode player plays (no game music):
# they keep the seek at the loop end
JUMP_SKIP = ("SD24.PCM", "SD28.PCM")


def jump_fade_ms(pcm):
    return 0 if pcm in JUMP_SKIP else JUMP_FADE_MS
VSYNC_HZ = 53693175 / (3413 * 263)

# (MU_ name, Saturn file, gain): gains bring the Saturn mixes (RMS 9195,
# 4987...; `bin/satxa wav` decodes one to measure it) to the retail tracks'
# level (RMS ~2400-3500 at volume 64-84): ~3200
TRACKS = [
    ("MU_MOD_SKELETON_LEADER", "SD2B.PCM", 0.35),  # Saturn 0x12B, boss
    ("MU_MOD_UNDERGROUND_GARDEN", "SD25.PCM", 0.64),  # Saturn 0x125
    ("MU_MOD_CURSED_PRISON", "SD26.PCM", 0.73),  # Saturn 0x126 (RMS 4344)
    # the inverted castle's (Saturn stage table rows 0x62 / 0x63)
    ("MU_MOD_HELL_GARDEN", "SD29.PCM", 0.57),  # Saturn 0x129 (RMS 5630)
    ("MU_MOD_SOUL_PRISON", "SD2C.PCM", 0.33),  # Saturn 0x12C (RMS 9803)
    # the Entrance in Richter mode (Saturn game.c func_060727DC; Maria's
    # is 0x12C, the Soul Prison's track)
    ("MU_MOD_RICHTER_ENTRANCE", "SD2A.PCM", 0.40),  # Saturn 0x12A (RMS 7931)
    # the Maria boss fight in the Castle Center (Saturn STAGE_14 boss step 2
    # plays 0xE0000123; no PS1 track matches it: chroma check against every
    # US XA track, best 0.88 where the same pieces score ~1.0)
    ("MU_MOD_MARIA_BOSS", "SD23.PCM", 0.49),  # Saturn 0x123 (RMS 6491)
    # only in the Saturn's sound mode (TITLE.PRG playlist 0x060B2CF4,
    # src/st/sel/2C048.c ModSound*): the Master Librarian's theme (on PS1 a
    # sequence, MU_SEQ_LIBRARIAN, LIB only), the Cursed Prison theme's long
    # version (SD26 + more, chroma 0.995) and SD28, used nowhere else
    ("MU_MOD_OLD_LIBRARIAN", "SD24.PCM", 0.40),  # Saturn 0x124 (RMS 8062)
    ("MU_MOD_SOUND_2D", "SD2D.PCM", 0.70),  # Saturn 0x12D (RMS 4597)
    ("MU_MOD_SOUND_28", "SD28.PCM", 0.49),  # Saturn 0x128 (RMS 6584)
]
# (MU_ name, PSP .at3, gain, retail config it stands in for, that config's
# volume): one config each, mode 2 (a one-shot, timer + 20) as retail's.
# PS_HEKIRU.at3 ("Hekiru" = Hekiru Shiina, the singer) is the fairy's
# Nocturne: res/ps/snd_e/PS_BGM the English recording (2007), res/ps/sound
# the Japanese one (the PS1's). Same arrangement as the PS1 XA (config 0x3F,
# 139 s timer); the gain brings its active RMS (8711) to the PS1 one's (3856)
PSP_BGM = os.path.join(ROOT, "disks/psp/snd_e/PS_BGM")
ONESHOTS = [
    ("MU_MOD_NOCTURNE_EN", "PS_HEKIRU.at3", 0.44, 0x3F, 64),
]
# a one-shot's timer ends this many VSyncs after its last sound
ONESHOT_MARGIN = 3
ONESHOT_SILENCE = 300  # a sample under this (both channels) is silence
# 8 channels (a sector each) per appended region: stream k (the tracks, then
# the one-shots) is file MOD_XA_FILE + k // 8, channel k % 8 (repack_xa.py
# repacks them anyway)
assert len(TRACKS) + len(ONESHOTS) <= 16
SECTOR = 2352


def build():
    os.makedirs(WORK, exist_ok=True)
    # only when satxa.c changed: a fresh binary is newer than every encode
    # cached against it (psp_dub.py), which re-encoded all the dub lines
    src = os.path.join(ROOT, "mods/sotn-ultimate/tools/satxa.c")
    if not os.path.exists(SATXA) or os.path.getmtime(SATXA) < os.path.getmtime(src):
        subprocess.run(["gcc", "-O2", "-Wall", "-o", SATXA, src, "-lm"], check=True)
    infos = []
    for k, (name, pcm, gain) in enumerate(TRACKS):
        xa = os.path.join(WORK, pcm + ".xa")
        out = subprocess.run([SATXA, "enc", os.path.join(ROOT, "disks/saturn/SD", pcm),
                              xa, str(MOD_XA_FILE + k // 8), str(k % 8), str(gain),
                              str(MIN_SECONDS), str(jump_fade_ms(pcm))],
                             check=True, capture_output=True, text=True).stdout.split()
        info = dict(zip(out[0::2], out[1::2]))
        info["xa"] = xa
        infos.append(info)
    exe, path_of = ffmpeg()
    for j, (name, at3, gain, like, vol) in enumerate(ONESHOTS):
        k = len(TRACKS) + j
        xa = os.path.join(WORK, at3 + ".xa")
        # the gain in the float decode: a 16-bit decode first clipped the
        # peaks over full scale (as psp_dub.py HEADROOM); the end: the last
        # sample (either channel) over ONESHOT_SILENCE
        r = audio_cli("song", os.path.join(PSP_BGM, at3), xa, repr(gain), ONESHOT_SILENCE,
                      MOD_XA_FILE + k // 8, k % 8)
        info = {"sectors": str(r["sectors"]), "xa": xa, "seconds": r["last_sample"] / 37800}
        infos.append(info)

    retail_size = os.path.getsize(RETAIL)
    assert retail_size % (SECTOR * 8) == 0
    region = retail_size // SECTOR  # first sector of the region, in the file
    with open(RETAIL, "rb") as f:
        f.seek(0)
        # a retail filler sector: submode 0 (no audio)
        filler = None
        for i in range(64):
            s = f.read(SECTOR)
            if s[18] == 0:
                filler = s
                break
    if filler is None:
        with open(RETAIL, "rb") as f:
            for i in range(retail_size // SECTOR):
                s = f.read(SECTOR)
                if s[16:20] == b"\0\0\0\0":
                    filler = s
                    break
    assert filler is not None
    streams = [open(i["xa"], "rb").read() for i in infos]
    # one region per 8 tracks, one after the other: rows of 8 sectors
    groups = [list(range(g, min(g + 8, len(infos)))) for g in range(0, len(infos), 8)]
    regions = []  # first sector of each region, in the file
    start = region
    tmp = OUT + ".tmp"
    with open(RETAIL, "rb") as src, open(tmp, "wb") as dst:
        while True:
            b = src.read(1 << 24)
            if not b:
                break
            dst.write(b)
        for g in groups:
            regions.append(start)
            count = max(len(streams[k]) // SECTOR for k in g)
            for r in range(count):
                for ch in range(8):
                    k = g[0] + ch
                    if k in g and r * SECTOR < len(streams[k]):
                        dst.write(streams[k][r * SECTOR:(r + 1) * SECTOR])
                    else:
                        dst.write(filler)
            start += count * 8
    os.replace(tmp, OUT)

    def entries(k, info):
        reg = regions[k // 8]
        intro = round(float(info["intro_seconds"]) * VSYNC_HZ)
        reps = int(info["repeats"])
        loop = round(reps * float(info["loop_seconds"]) * VSYNC_HZ)
        loop_addr = reg + int(info["intro_sectors"]) * 8
        f, ch = MOD_XA_FILE + k // 8, k % 8
        return ([reg, intro, f, ch, VOLUME, 0, 1],
                [loop_addr, loop, f, ch, VOLUME, 0, 0])

    def oneshot(j):
        k = len(TRACKS) + j
        timer = math.ceil(infos[k]["seconds"] * VSYNC_HZ) + ONESHOT_MARGIN
        return [regions[k // 8], timer, MOD_XA_FILE + k // 8, k % 8, ONESHOTS[j][4], 0, 2]

    configs = []
    for k, info in enumerate(infos[:len(TRACKS)]):
        configs.extend(entries(k, info))
    for j in range(len(ONESHOTS)):
        configs.append(oneshot(j))
    with open(OUT_JSON, "w") as f:
        json.dump(configs, f)
    with open(HEADER, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gen_saturn_music.py, do not edit\n")
        f.write("// Saturn-only music, appended to SD/XA_STR1 (files %d+)\n" % MOD_XA_FILE)
        f.write("#define MOD_XA_RETAIL_CONFIGS %d\n" % RETAIL_CONFIGS)
        for k, ((name, pcm, gain), info) in enumerate(zip(TRACKS, infos)):
            f.write("#define %s 0x%X // %s\n" % (name, 0x300 + RETAIL_CONFIGS + k * 2, pcm))
        for j, (name, at3, gain, like, vol) in enumerate(ONESHOTS):
            f.write("#define %s 0x%X // PSP %s\n" % (
                name, 0x300 + RETAIL_CONFIGS + len(TRACKS) * 2 + j, at3))
        f.write("#define MOD_XA_CONFIGS %d\n" % (len(TRACKS) * 2 + len(ONESHOTS)))
        f.write("#ifdef MOD_XA_MUSIC_TABLE\n")
        f.write("static const XaMusicConfig g_ModXaMusicConfigs[] = {\n")
        for k, ((name, pcm, gain), info) in enumerate(zip(TRACKS, infos)):
            f.write("    // %s: to the loop entry %.2f s (%d sectors into the loop), loop %.2f s x %d\n" % (
                pcm, float(info["intro_seconds"]), int(info["quiet_sectors"]),
                float(info["loop_seconds"]), int(info["repeats"])))
            for e in entries(k, info):
                f.write("    {%d, %d, %d, %d, %d, %d, %d},\n" % tuple(e))
        for j, (name, at3, gain, like, vol) in enumerate(ONESHOTS):
            f.write("    // PSP %s: %.2f s, one-shot (as config 0x%X)\n" % (
                at3, infos[len(TRACKS) + j]["seconds"], 0x300 + like))
            f.write("    {%d, %d, %d, %d, %d, %d, %d},\n" % tuple(configs[len(TRACKS) * 2 + j]))
        f.write("};\n#endif\n")
    count = start - region
    print("saturn music: %d tracks + %d one-shots, %d sectors appended to XA_STR1" % (
        len(TRACKS), len(ONESHOTS), count))


if __name__ == "__main__":
    build()
