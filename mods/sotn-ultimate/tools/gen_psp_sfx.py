#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the PSP dub's stage and familiar sound banks (BIN/SFX_PSP.BIN).

Besides the XA lines and the player grunts (gen_psp_grunts.py), the PS1 US
release has English voices inside the sample banks: the bosses' in-fight
voices (Richter, the Doppleganger, Succubus, Death, Dracula, Shaft, Medusa,
Scylla, Akmodan, the Creature, Olrox...), many enemies' grunts (Owl Knight,
Gurkha, Discus Lord, Salem Witch...) and the familiars' (the Faerie, the
Demon, the Sword, the Yousei, the Nose Demon). The PSP replaces each bank by
res/ps/snd_e/SE/sd_<name>.spk (SEPK: one VAGp per used tone) and finds a
tone's entry through a per-bank (program, tone) list in PS.ELF (the key-on
code at 0x0892ECF4 picks the list by bank name; the index in the list is the
entry). Those lists give the exact PSP sound of every PS1 slot.

For every tone the PS1 plays (volume > 0) whose sample is another recording
than the PSP's (waveform correlation, also after a rate change, < 0.3) the
bank gets the PSP's: the English re-dub where the PSP has one (the 8 boss
banks and the familiars), the PSP's (Japanese) take where its English pack
kept the Japanese one. A PSP entry at volume 0 (record +12) is silent on the
PSP: the tone gets volume 0. Same programs/tones/ADSR/volumes; only the
sample and the centre note change (the clip plays at its own rate at the
note the game keys). Looping samples, the PS1-silent tones and the tones a
mod tool already changed (build/us/SD_ZKNO3, CEN, the Saturn stages) are
left alone. The VB keeps the retail length exactly (clips resampled down
together if needed; an unused padding sample at the end).

BIN/SFX_PSP.BIN = the changed banks back to back (VH padded to its retail
sectors, then VB), each like its SD_Z*.VH/VB pair on the disc. Writes
src/dra/mod_psp_sfx.h: per stage MOD_PSP_SFX_<OVL> = its VH sector in the
file + 1 (0: retail bank), which lba_stage.c stores in the Lba name field
(unused on the US mod build), and g_ModPspServantSfx[] for the familiars
(src/dra/cd.c loads them when MOD_PSP_DUB_ON()).

Usage: gen_psp_sfx.py [--off] [-v]   (--off: no banks, for MOD_PSP_DUB=0)

--set jp (after the PSP run: reads src/dra/mod_psp_sfx.h): the JP dub's
(include/game.h MOD_JP_DUB_ON) banks. The packs whose English and Japanese
files are the same (41 of 57: the English pack kept the Japanese takes) keep
the PSP dub's bank; the others' banks are built again from the Japanese
pack (clips and volumes: e.g. Death's attack lines, silent in English, are
voiced in Japanese) into build/us/SFX_JP.BIN (in BIN/JP_SD.BIN,
mods/sotn-ultimate/tools/gen_jp_sd.py). src/st/sel/mod_jp_sfx.h: MOD_JP_SFX_PAIRS =
{PSP bank value (its SFX_PSP.BIN sector + 1), JP bank: its JP_SD.BIN sector
+ 1, or 0xFFFF when the Japanese clips leave the retail bank as it is},
which SEL copies into g_ModDub.jp.sfx for src/dra/cd.c.
"""
import glob
import hashlib
import json
import math
import os
import re
import struct
import subprocess
import sys

import numpy as np
import yaml

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vab import Vab  # noqa: E402
import gen_gdn_sounds as G  # noqa: E402
from gen_psp_grunts import active_rms, trim  # noqa: E402
from jp_sfx_pitch import JP_SFX_PITCH  # noqa: E402

PSP = os.path.join(ROOT, "disks/psp")
ELF = os.path.join(PSP, "PS.ELF")
SPK_EN = os.path.join(PSP, "snd_e/SE")
SPK_JP = os.path.join(PSP, "sound/SE")
WORK = os.path.join(ROOT, "build/us/psp_sfx")
OUT = os.path.join(ROOT, "build/us/SFX_PSP.BIN")
HDR = os.path.join(ROOT, "src/dra/mod_psp_sfx.h")
SECTOR = 2048
SAME = 0.3  # correlation from which a PS1 sample is the PSP's recording
SKIP_STAGES = {"CEN", "MAR", "TE1", "TE2", "TE3", "TE4"}  # Maria / Saturn
MODDED = {"NO3": "build/us/SD_ZKNO3"}  # banks a mod tool rewrote
# The Saturn-only stages' banks (gen_prs_sounds.py, gen_rgdn_sounds.py,
# gen_rprs_sounds.py: build/us/<name>) copy some PS1 stage tones as they are
# (gen_gdn_sounds.PS1_TONES, each with its pair, tone + 1): with a dub those
# copies follow the source stage's dub bank (saturn_bank; e.g. the Prison's
# Spear Guard, NO4's). tok -> (its bank, [(source stage, its bank, program,
# tones)]), disc paths
SATURN_COPIES = {
    "TE2": ("ST/TE2/SD_ZKTE2", [("NO4", "ST/NO4/SD_ZKNO4", 6, (10, 12, 14))]),
    "TE3": ("ST/TE3/SD_ZKTE3", [("RNO3", "ST/RNO3/SD_ZRNO3", 2, (4, 6)),
                                ("RNZ1", "ST/RNZ1/SD_ZRNZ1", 5, (0, 4))]),
    "TE4": ("ST/TE4/SD_ZKTE4", [("RNO2", "ST/RNO2/SD_ZRNO2", 2, (8, 10, 12))]),
}
# familiars: g_LbaServant* index -> (bank, PSP pack). Bat, ghost and the
# Yousei share one bank (SD_BAT = SD_GHOST = SD_FAIR2, PSP sd_tuka2)
SERVANTS = [("SD_BAT", "tuka2"), ("SD_GHOST", "tuka2"), ("SD_FAIRY", "tuka1"),
            ("SD_DEVIL", "koa1"), ("SD_SWORD", "mak"), ("SD_FAIR2", "tuka2"),
            ("SD_DEVI2", "koa2")]
VERBOSE = "-v" in sys.argv[1:]


def elf_sections(elf):
    """section name -> (vma, size, file offset), objdump -h"""
    text = subprocess.run(["mipsel-linux-gnu-objdump", "-h", elf],
                          capture_output=True, text=True, check=True).stdout
    return {m.group(1): (int(m.group(3), 16), int(m.group(2), 16), int(m.group(4), 16))
            for m in re.finditer(r"^\s*\d+\s+(\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)",
                                 text, re.M)}


def psp_tables():
    """bank name -> [(prog, tone)...] (index = spk entry), from PS.ELF: the
    code that looks each bank's list up loads its name ("sd_" + more, a1)
    and then its list's address in .data (a0); found anywhere in .text"""
    d = open(ELF, "rb").read()
    sec = elf_sections(ELF)
    tv, ts, _ = sec[".text"]
    dv, dsz, doff = sec[".data"]
    asm = subprocess.run(
        ["mipsel-linux-gnu-objdump", "-d", "--start-address=0x%x" % tv,
         "--stop-address=0x%x" % (tv + ts), ELF],
        capture_output=True, text=True, check=True).stdout.splitlines()
    ins = []
    for line in asm:
        m = re.match(r"\s*[0-9a-f]+:\s+[0-9a-f]+\s+(\S+)\s*(.*)", line)
        if m:
            ins.append((m.group(1), m.group(2)))
    out, consts = {}, []
    for i, (op, a) in enumerate(ins):
        if op == "lui" and a.startswith("a1,"):
            hi, lo = int(a.split(",")[1], 16), 0
            if ins[i + 1][0] == "ori" and ins[i + 1][1].startswith("a1,a1,"):
                lo = int(ins[i + 1][1].split(",")[2], 16)
            consts.append(((hi << 16) | lo).to_bytes(4, "big").decode("latin1"))
        if op == "lui" and a.startswith("a0,0x") and consts:
            j = i + 1 if ins[i + 1][0] == "addiu" else i + 2
            if ins[j][0] != "addiu" or not ins[j][1].startswith("a0,a0,"):
                continue
            addr = (int(a.split(",")[1], 16) << 16) + int(ins[j][1].split(",")[2])
            if not dv <= addr < dv + dsz:
                continue
            full = [c for c in consts if c.startswith("sd_") and "\0" not in c]
            if not full:
                consts = []
                continue
            name = (full[-1] + consts[-1]).replace("\0", "")
            k, tab = addr - dv + doff, []
            while d[k] != 0xFF:
                tab.append((d[k], d[k + 1]))
                k += 2
            out[name] = tab
            consts = []
    assert "sd_rihi" in out and len(out["sd_rihi"]) == 52, sorted(out)
    # PS.ELF's sd_olst list (NO2) is 6 short of its 63 clips: entries 41-46
    # (zu1o5y, PSP volume 0) have no PS1 slot and 47-62 are p8t0-15 (the Owl
    # Knight, the owl and the Gurkha, as on both PS1 banks). Taken as listed,
    # the Owl Knight's taunt / attack / react were silenced and the others
    # played the wrong lines (v1.3)
    t = out["sd_olst"]
    assert len(t) == 57 and t[41:] == [(8, i) for i in range(16)], t
    out["sd_olst"] = t[:41] + [(0xFF, 0xFF)] * 6 + [(8, i) for i in range(16)]
    return out


def spk_entries(path):
    """[(name, rate, VAG body)] (sd_togi is a SEPK1.10, same layout)"""
    d = open(path, "rb").read()
    assert d[:6] == b"SEPK1.", path
    out = []
    for i in range(struct.unpack_from("<I", d, 8)[0]):
        off = struct.unpack_from("<I", d, 0x20 + i * 0x20)[0]
        v = d[off:]
        assert v[:4] == b"VAGp", (path, i)
        size, rate = struct.unpack_from(">II", v, 12)
        name = v[0x20:0x30].split(b"\0")[0].decode("latin1")
        out.append((name, rate, v[0x30:0x30 + size]))
    return out


def spk_vols(path):
    d = open(path, "rb").read()
    n = struct.unpack_from("<I", d, 8)[0]
    return [struct.unpack_from("<H", d, 0x20 + i * 0x20 + 12)[0] for i in range(n)]


def pcm(b):
    return np.asarray(G.decode_vag(b), dtype=np.float64)


def corr(a, b):
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]
    d = math.sqrt(float((a * a).sum() * (b * b).sum()))
    return float((a * b).sum()) / d if d else 0.0


def same_recording(x, y):
    if len(x) < 64 or len(y) < 64:
        return False
    s = corr(x, y)
    if 0.25 < len(y) / len(x) < 4:
        r = np.interp(np.linspace(0, len(y) - 1, len(x)), np.arange(len(y)), y)
        s = max(s, corr(x, r))
    return s >= SAME


def loops(b):
    """an audible loop: the first end flag repeats (3) back to a loop start
    (4) over blocks with sound (one-shots end (1), or repeat silence)"""
    blocks = [b[k:k + 16] for k in range(0, len(b) - 15, 16)]
    start = 0
    for i, blk in enumerate(blocks):
        if blk[1] & 4:
            start = i
        if blk[1] & 1:
            return bool(blk[1] & 2) and any(any(x[2:]) for x in blocks[start:i + 1])
    return False


def pitch(t):
    """the tone's effective centre in 1/128 semitones (the shift raises the
    pitch, so it lowers the note that plays 44100 Hz)"""
    return t["center"] * 128 - t["shift"]


def set_pitch(t, total):
    total = int(round(total))
    t["center"] = -(-total // 128)
    t["shift"] = t["center"] * 128 - total
    assert 0 <= t["center"] <= 127, t


def encode(name, x, rate):
    wav = os.path.join(WORK, name + ".wav")
    body = os.path.join(WORK, name + ".vag")
    G.write_wav(wav, [int(max(-32768, min(32767, round(v)))) for v in x], rate)
    subprocess.run([G.SATXA, "vag", wav, body], check=True, capture_output=True)
    return open(body, "rb").read()


CFG = None


def notes_of(vabid, p, t):
    global CFG
    if CFG is None:
        CFG = yaml.safe_load(open(os.path.join(ROOT, "assets/dra/us/sfx_config.yaml")))
    return sorted({e["note"] for e in CFG
                   if e["vabid"] == vabid and e["prog"] == p and e["tone"] == t})


def key_note(vabid, p, t, tone):
    """the note the game keys this tone with: of the sound ids on the slot,
    the one at which the retail sample plays at a sane rate (60 first).
    Only notes in the tone's key range play it (Doppleganger's voices: one
    tone per range, keyed 2, 14, 26 ..., while other stages key the same
    slot with 60)"""
    ns = notes_of(vabid, p, t)
    ns = [n for n in ns if tone["min"] <= n <= tone["max"]] or ns
    ok = [n for n in ns
          if 3000 <= 44100 * 2 ** ((n * 128 - pitch(tone)) / 1536) <= 48000]
    return 60 if 60 in ok or not ok else min(ok, key=lambda n: abs(n - 60))


def build_bank(key, src, base, pack, vabid, tables, clip_dir=SPK_EN, prefix=""):
    """-> (vh, vb, report) or None when nothing changes. clip_dir: the
    pack the clips (and their PSP volumes) come from (--set jp: SPK_JP);
    prefix: only the clips whose name starts with it"""
    vh0 = open(os.path.join(ROOT, src + ".VH"), "rb").read()
    vb0 = open(os.path.join(ROOT, src + ".VB"), "rb").read()
    retail = Vab(vh0, vb0)
    vhb = open(os.path.join(ROOT, base + ".VH"), "rb").read()
    vbb = open(os.path.join(ROOT, base + ".VB"), "rb").read()
    vab = Vab(vhb, vbb)
    assert vab.write() == (vhb, vbb), key
    tab = tables["sd_" + pack]
    en = spk_entries(os.path.join(SPK_EN, "sd_%s.spk" % pack))
    jp = spk_entries(os.path.join(SPK_JP, "sd_%s.spk" % pack))
    vols = spk_vols(os.path.join(clip_dir, "sd_%s.spk" % pack))
    if clip_dir == SPK_JP:
        en = jp
    # --set jp: the JP PS1's pitch for the slots its samples play elsewhere
    jpt = JP_SFX_PITCH.get(key[3:] if key.startswith("jp_") else key[:-3], {})         if clip_dir == SPK_JP else {}
    groups, silent, report = {}, [], []
    for k, (p, t) in enumerate(tab):
        if k >= len(en) or p not in vab.tones or t > 15:
            continue
        tone = vab.tones[p][t]
        if not tone["vag"] or not tone["vol"]:
            continue  # the PS1 is silent here: stays so
        rt = retail.tones[p][t] if p in retail.tones else None
        if rt is None or rt["vag"] != tone["vag"] or \
                retail.vags[rt["vag"] - 1] != vab.vags[tone["vag"] - 1]:
            continue  # a mod tool's sound
        body = vab.vags[tone["vag"] - 1]
        x = pcm(body)
        name, rate, clip = en[k]
        if not name.startswith(prefix):
            continue
        if vols[k] == 0:
            silent.append((p, t))
            report.append("p%dt%d silent (PSP %s volume 0)" % (p, t, name))
            continue
        y = pcm(clip)
        if same_recording(x, y):
            continue
        if loops(body) or loops(clip):
            report.append("p%dt%d kept: looping sample" % (p, t))
            continue
        h = hashlib.md5(clip).hexdigest()
        g = groups.setdefault((tone["vag"], h), {"tones": [], "name": name,
                                                 "rate": rate, "pcm": y,
                                                 "ps1": x, "en": clip != jp[k][2]})
        g["tones"].append((p, t))
    # a played tone the PSP's list leaves out (Succubus p3t7: the list has
    # p3t9 there) follows its pair, so no retail recording is left playing
    moved = {pt: g for g in groups.values() for pt in g["tones"]}
    for p in vab.tones:
        for t in range(16):
            tn = vab.tones[p][t]
            if (p, t) in tab or not tn["vag"] or not tn["vol"]:
                continue
            pals = [g for g in groups.values()
                    if vab.tones[g["tones"][0][0]][g["tones"][0][1]]["vag"] == tn["vag"]]
            if not pals:
                continue
            g = next((g for g in pals if (p, t - 1) in g["tones"]), pals[0])
            g["tones"].append((p, t))
            report.append("p%dt%d follows %s (not in the PSP list)" % (p, t, g["name"]))
    if not groups and not silent:
        return None
    for p, t in silent:
        vab.tones[p][t]["vol"] = 0
    # the retail samples no tone plays any more are dropped
    moved = {pt for g in groups.values() for pt in g["tones"]}
    used = sorted({vab.tones[p][t]["vag"] for p in vab.tones for t in range(16)
                   if vab.tones[p][t]["vag"] and (p, t) not in moved})
    budget = len(vbb) - sum(len(vab.vags[i - 1]) for i in used)
    ratios = []
    for g in groups.values():
        p, t = g["tones"][0]
        tone = vab.tones[p][t]
        g["note"] = key_note(vabid, p, t, tone)
        r_ps1 = 44100 * 2 ** ((g["note"] * 128 - pitch(tone)) / 1536)
        g["pcm"] = trim(g["pcm"], g["rate"])
        a, b = active_rms(g["ps1"], int(r_ps1)), active_rms(g["pcm"], g["rate"])
        if a and b:
            ratios.append(a / b)
    gain = min(1.0, float(np.median(ratios))) if ratios else 1.0
    os.makedirs(WORK, exist_ok=True)
    scale = 1.0
    while True:
        used_bytes = 0
        for i, g in enumerate(groups.values()):
            r = g["rate"] if scale == 1.0 else max(4000, int(g["rate"] * scale))
            x = g["pcm"] * gain
            peak = np.abs(x).max() if len(x) else 0
            if peak > 32000:
                x *= 32000 / peak
            if r != g["rate"]:
                x = np.asarray(G.resample(x, g["rate"], r)[0], dtype=np.float64)
            g["body"], g["r"] = encode("%s_%d_%d" % (key, i, r), x, r), r
            used_bytes += len(g["body"])
        if used_bytes <= budget:
            break
        scale -= 0.05
        if scale < 0.5:
            print("psp sfx %s (sd_%s): SKIPPED, the clips do not fit (%d > %d bytes)"
                  % (key, pack, used_bytes, budget))
            return None
    # rebuild the sample list: kept retail samples, then the PSP clips
    remap = {old: n + 1 for n, old in enumerate(used)}
    vags = [vab.vags[i - 1] for i in used]
    for g in groups.values():
        vags.append(g["body"])
        g["vag"] = len(vags)
    for p in vab.tones:
        for t in range(16):
            tn = vab.tones[p][t]
            if tn["vag"] and (p, t) not in moved:
                tn["vag"] = remap[tn["vag"]]
    for g in groups.values():
        p0, t0 = g["tones"][0]
        ref = pitch(vab.tones[p0][t0])
        # a JP PS1 pitch is for its sample at the PSP header's rate: lowered
        # as the clip was resampled
        adj = int(round(1536 * math.log2(g["rate"] / g["r"])))
        if (p0, t0) in jpt:
            c, s = jpt[(p0, t0)]
            delta = c * 128 - s + adj - ref
        else:
            c, f = G.centre(g["note"], g["r"])
            delta = c * 128 - f - ref
        for p, t in g["tones"]:
            tn = vab.tones[p][t]
            tn["vag"] = g["vag"]
            if (p, t) in jpt:
                c, s = jpt[(p, t)]
                set_pitch(tn, c * 128 - s + adj)
            else:
                set_pitch(tn, pitch(tn) + delta)
        report.append("%s <- %s%s (%d Hz)" % (
            " ".join("p%dt%d" % pt for pt in g["tones"]), g["name"],
            " EN" if g["en"] else " (PSP JP take)", g["r"]))
    vab.vags = vags
    vh, vb = vab.write()
    if len(vb) < len(vbb):
        vab.vags.append(bytes(len(vbb) - len(vb)))
        vh, vb = vab.write()
    assert len(vh) == len(vhb) and len(vb) == len(vbb), (key, len(vh), len(vb))
    report.insert(0, "%d clips, %d silenced, gain %.2f, rate x%.2f, %d/%d bytes" % (
        len(groups), len(silent), gain, scale, used_bytes, budget))
    return vh, vb, report


def saturn_bank(tok, dubs):
    """-> (vh, vb) of the Saturn stage tok's bank (SATURN_COPIES) with its
    copied PS1 tones as the source stages' dub banks have them (dubs: source
    tok -> (vh, vb); a source missing there keeps the PS1 tones), or None
    when nothing changes. The VB keeps its length (the stage table's)"""
    base, srcs = SATURN_COPIES[tok]
    path = os.path.join(ROOT, "build/us", os.path.basename(base))
    vh0, vb0 = (open(path + x, "rb").read() for x in (".VH", ".VB"))
    vab = Vab(vh0, vb0)
    assert vab.write() == (vh0, vb0), tok
    orig, changed = list(vab.vags), False
    for stok, src, p, tones in srcs:
        if stok not in dubs:
            continue
        ps1 = Vab(*(open(os.path.join(ROOT, "disks/us", src + x), "rb").read()
                    for x in (".VH", ".VB")))
        dub = Vab(*dubs[stok])
        done = {}
        for t in [u for t in tones for u in (t, t + 1)]:
            bt, rt, dt = vab.tones[p][t], ps1.tones[p][t], dub.tones[p][t]
            if bt["vag"] < 2:
                continue  # a pair the copy has not (vag 1: the blank)
            assert orig[bt["vag"] - 1] == ps1.vags[rt["vag"] - 1], (tok, p, t)
            body = dub.vags[dt["vag"] - 1] if dt["vag"] else orig[bt["vag"] - 1]
            if dict(dt, vag=0) == dict(rt, vag=0) and body == ps1.vags[rt["vag"] - 1]:
                continue
            assert done.get(bt["vag"], body) == body, (tok, p, t)  # a pair shares it
            done[bt["vag"]] = body
            vab.vags[bt["vag"] - 1] = body
            vab.tones[p][t] = dict(dt, vag=bt["vag"])
            changed = True
    if not changed:
        return None
    grow = len(vb0) - sum(map(len, vab.vags))
    assert grow >= 0 and grow % 16 == 0, (tok, grow)
    vab.vags[-1] += bytes(grow)  # never played: past the last sample's end
    vh, vb = vab.write()
    assert len(vh) == len(vh0) and len(vb) == len(vb0), tok
    return vh, vb


def pad(b, n=None):
    n = n if n is not None else -(-len(b) // SECTOR) * SECTOR
    assert len(b) <= n, (len(b), n)
    return b + bytes(n - len(b))


def psp_bank(data, val, src):
    """(vh, vb) of the PSP bank at SFX_PSP.BIN sector val - 1 (data), sized
    as the PS1 bank src (disc path) it replaces"""
    vhl, vbl = (os.path.getsize(os.path.join(ROOT, "disks/us", src + x)) for x in (".VH", ".VB"))
    o = (val - 1) * SECTOR
    vhs = -(-vhl // SECTOR) * SECTOR
    return data[o:o + vhl], data[o + vhs:o + vhs + vbl]


def stages():
    """US rows of lba_stage.c: ovl token -> pack name. The token is
    LBA_NAMES' last argument, the one lba_stage.c pastes into
    MOD_PSP_SFX_##tok (MAR's row borrows NO0's sound header, so its VH_OFF
    name is NO0's)"""
    src = open(os.path.join(ROOT, "src/dra/lba_stage.c")).read()
    out = {}  # only the US rows use LBA_NAMES
    for m in re.finditer(r"LBA_NAMES\(\s*\"[^\"]*\",\s*\"[^\"]*\",\s*\"(\w+)\",\s*(\w+)\)", src):
        out.setdefault(m.group(2), m.group(1))
    assert len(out) > 40, len(out)
    return out


JP_HDR = os.path.join(ROOT, "src/st/sel/mod_jp_sfx.h")
JP_OUT = os.path.join(ROOT, "build/us/SFX_JP.BIN")


def main_jp(off):
    import gen_jp_sd
    src = open(HDR).read()
    psp_val = {m.group(1): int(m.group(2))
               for m in re.finditer(r"#define MOD_PSP_SFX_(\w+) (\d+)", src)}
    m = re.search(r"#define MOD_PSP_SFX_SERVANTS \{([^}]*)\}", src)
    srv_val = [int(x) for x in m.group(1).split(",")]
    toks = stages()
    data, seen, pairs = bytearray(), {}, {}
    jp_dubs, jp_retail = {}, set()
    if not off:
        tables = psp_tables()

        def differs(pack):
            a = open(os.path.join(SPK_EN, "sd_%s.spk" % pack), "rb").read()
            b = open(os.path.join(SPK_JP, "sd_%s.spk" % pack), "rb").read()
            return a != b
        jobs = []
        for tok, pack in sorted(toks.items()):
            f = glob.glob(os.path.join(ROOT, "disks/us/*/%s/SD_Z*.VH" % tok))
            if tok in SKIP_STAGES or not f or "sd_" + pack not in tables or                     not (differs(pack) or tok in JP_SFX_PITCH):
                continue
            src_ = os.path.relpath(f[0][:-3], ROOT)
            jobs.append((tok, src_, MODDED.get(tok, src_), pack, 3, psp_val.get(tok, 0)))
        for i, (name, pack) in enumerate(SERVANTS):
            if differs(pack) or "srv%d" % i in JP_SFX_PITCH:
                src_ = "disks/us/SERVANT/" + name
                jobs.append(("srv%d" % i, src_, src_, pack, 2, srv_val[i]))
        for tok, src_, base, pack, vabid, psp in jobs:
            if not psp:
                print("jp sfx %s (sd_%s): no PSP bank to stand in for: skipped" % (tok, pack))
                continue
            r = build_bank("jp_" + tok, src_, base, pack, vabid, tables, SPK_JP)
            if r is None:
                jp_retail.add(tok)
                pairs.setdefault(psp, 0xFFFF)
                print("jp sfx %s (sd_%s): no change: the retail bank" % (tok, pack))
                continue
            # Richter's / Maria's NO3 bank (BIN/NO3RIDE.BIN) has no JP part
            assert tok != "NO3", "a JP NO3 bank: give NO3RIDE.BIN a JP part (src/dra/cd.c)"
            vh, vb, report = r
            jp_dubs[tok] = (vh, vb)
            blob = pad(vh, -(-len(vh) // SECTOR) * SECTOR) + pad(vb)
            h = hashlib.md5(blob).hexdigest()
            if h not in seen:
                seen[h] = len(data) // SECTOR
                data += blob
            val = gen_jp_sd.SFX_BASE + seen[h] + 1
            assert pairs.get(psp, val) == val, (tok, psp)
            pairs[psp] = val
            print("jp sfx %s (sd_%s): %s" % (tok, pack, report[0]))
            if VERBOSE:
                for line in report[1:]:
                    print("    " + line)
        # the Saturn stages (SATURN_COPIES) with a PSP bank: their copies as
        # each source's JP bank has them, its PSP one when the JP dub keeps
        # that, the PS1 tones when it plays the retail bank
        psp_data = open(OUT, "rb").read()
        for tok in sorted(SATURN_COPIES):
            psp = psp_val.get(tok, 0)
            if not psp:
                continue
            dubs = {}
            for stok, src_, _, _ in SATURN_COPIES[tok][1]:
                if stok in jp_dubs:
                    dubs[stok] = jp_dubs[stok]
                elif stok not in jp_retail and psp_val.get(stok):
                    dubs[stok] = psp_bank(psp_data, psp_val[stok], src_)
            r = saturn_bank(tok, dubs)
            if r is None:
                pairs[psp] = 0xFFFF
                print("jp sfx %s: no change: the retail bank" % tok)
                continue
            blob = pad(r[0], -(-len(r[0]) // SECTOR) * SECTOR) + pad(r[1])
            h = hashlib.md5(blob).hexdigest()
            if h not in seen:
                seen[h] = len(data) // SECTOR
                data += blob
            pairs[psp] = gen_jp_sd.SFX_BASE + seen[h] + 1
            print("jp sfx %s: its PS1 copies from %s" % (tok, ", ".join(sorted(dubs))))
        print("jp sfx: %d banks, SFX_JP.BIN %d sectors" % (len(pairs), len(data) // SECTOR))
    assert len(pairs) < 28, len(pairs)  # include/game.h MOD_JP_SFX_MAX (+ the end)
    if not data:
        data = bytearray(SECTOR)
    open(JP_OUT, "wb").write(bytes(data))
    rows = ["{%d, %d}" % kv for kv in sorted(pairs.items())] + ["{0, 0}"]
    s = ("// generated by mods/sotn-ultimate/tools/gen_psp_sfx.py --set jp: do not edit\n"
         "// the JP dub's banks: {PSP bank (SFX_PSP.BIN sector + 1), JP bank\n"
         "// (BIN/JP_SD.BIN sector + 1, 0xFFFF: the retail bank)}\n"
         "#define MOD_JP_SFX_PAIRS {%s}\n" % ", ".join(rows))
    if not os.path.exists(JP_HDR) or open(JP_HDR).read() != s:
        open(JP_HDR, "w").write(s)


def main():
    off = "--off" in sys.argv[1:]
    if "--set" in sys.argv[1:] and sys.argv[sys.argv.index("--set") + 1] == "jp":
        return main_jp(off)
    toks = stages()
    data, offs, seen, srv = bytearray(), {}, {}, [0] * len(SERVANTS)
    if not off:
        tables = psp_tables()
        jobs = []
        for tok, pack in sorted(toks.items()):
            f = glob.glob(os.path.join(ROOT, "disks/us/*/%s/SD_Z*.VH" % tok))
            if tok in SKIP_STAGES or not f or "sd_" + pack not in tables:
                continue
            src = os.path.relpath(f[0][:-3], ROOT)
            jobs.append((tok, src, MODDED.get(tok, src), pack, 3))
        for i, (name, pack) in enumerate(SERVANTS):
            src = "disks/us/SERVANT/" + name
            jobs.append(("srv%d" % i, src, src, pack, 2))
        total, dubs = 0, {}
        for tok, src, base, pack, vabid in jobs:
            r = build_bank(tok, src, base, pack, vabid, tables)
            if r is None:
                continue
            vh, vb, report = r
            dubs[tok] = (vh, vb)
            vhs = -(-len(vh) // SECTOR)
            blob = pad(vh, vhs * SECTOR) + pad(vb)
            h = hashlib.md5(blob).hexdigest()
            if h not in seen:
                seen[h] = len(data) // SECTOR
                data += blob
            sec = seen[h]
            if tok.startswith("srv"):
                srv[int(tok[3:])] = sec + 1
            else:
                offs[tok] = sec + 1
            total += 1
            print("psp sfx %s (sd_%s): %s" % (tok, pack, report[0]))
            if VERBOSE:
                for line in report[1:]:
                    print("    " + line)
        # the Saturn stages' copies of PS1 tones (SATURN_COPIES)
        for tok in sorted(SATURN_COPIES):
            r = saturn_bank(tok, dubs)
            if r is None:
                continue
            blob = pad(r[0], -(-len(r[0]) // SECTOR) * SECTOR) + pad(r[1])
            h = hashlib.md5(blob).hexdigest()
            if h not in seen:
                seen[h] = len(data) // SECTOR
                data += blob
            offs[tok] = seen[h] + 1
            total += 1
            print("psp sfx %s: its PS1 copies from %s" % (tok, ", ".join(
                s for s, _, _, _ in SATURN_COPIES[tok][1] if s in dubs)))
        print("psp sfx: %d banks, SFX_PSP.BIN %d sectors" % (total, len(data) // SECTOR))
    if not data:
        data = bytearray(SECTOR)
    open(OUT, "wb").write(bytes(data))
    # BIN/NO3RIDE.BIN's second part, Richter's / Maria's NO3 bank with the
    # PSP dub: the ride's bank made from this NO3 bank (gen_no3_sounds.py
    # ride_bank; the first part's copy when the PSP changes nothing there)
    import gen_no3_sounds as N
    ride = open(N.RIDE_OUT, "rb").read()
    part = ride[:len(ride) // 2]
    if "NO3" in offs:
        vhl, vbl = (os.path.getsize(N.OUT + x) for x in (".VH", ".VB"))
        b = bytes(data[(offs["NO3"] - 1) * SECTOR:])
        vhs = -(-vhl // SECTOR) * SECTOR
        part = N.ride_bank(b[:vhl], b[vhs:vhs + vbl])
    open(N.RIDE_OUT, "wb").write(ride[:len(part)] + part)
    lines = [
        "// generated by mods/sotn-ultimate/tools/gen_psp_sfx.py: do not edit",
        "// the PSP dub's sound banks in BIN/SFX_PSP.BIN: sector + 1 (0: retail)",
        "#ifndef MOD_PSP_SFX_H",
        "#define MOD_PSP_SFX_H",
    ]
    for tok in sorted(toks):
        lines.append("#define MOD_PSP_SFX_%s %d" % (tok, offs.get(tok, 0)))
    # src/dra/cd.c loads every familiar's PSP bank without checking for 0
    assert all(srv), ("a familiar without a PSP bank", srv)
    lines.append("#define MOD_PSP_SFX_SERVANTS {%s}" % ", ".join(map(str, srv)))
    lines.append("#endif")
    open(HDR, "w").write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
