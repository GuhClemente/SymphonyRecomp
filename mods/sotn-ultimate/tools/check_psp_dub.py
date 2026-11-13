#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: offline checks of the PSP dub toggle (PS1 and PSP dub +
script both on the disc, include/game.h MOD_PSP_DUB_ON).

From the built files only (build/us/*.BIN, CENP.BIN, XA_STR1., the XA config
tables, src/dra/mod_psp_xa.h):
  - every script psp_script.py lists: the PS1 version in the built overlay is
    the retail script, byte for byte; it plays PS1 lines only;
  - its PSP copy, read back from the built overlay (g_ModPspSpill, or CEN's
    g_ModPspCopyN; CENP.BIN too): no text line wider than
    psp_script.MAX_COLS, every voiced line plays an id with a PSP version
    (or a PSP-only mod line; the NO_PSP lines are cut), every PLAY_SOUND has an XA
    config, and the overlay's map (retail address -> copy) is in it;
  - every voice id with a PSP version: its PS1 config still plays a PS1
    stream (not the PSP one), and the PSP stream (mod_psp_xa.h) is the
    encoded line (mono, as long as it), its length (unk228) covers the line
    and ends at most a few frames after it; the ids with a PSP version are
    exactly psp_dub.py's.
  - JP dub (MOD_JP_DUB_ON): every id with a JP version (src/st/sel/
    mod_jp_xa.h, as SEL's built table) is psp_dub.py --set jp's, its stream
    is the encoded line, its length (11 bits: the 25 s lines) covers it, and
    the PS1 / PSP configs keep their own streams; MOD_VO_DID_YOU_NOT's too;
    CEN's JP copies (g_ModJpCopyN, CENP too) are in their map, fit, and
    play only ids with a JP version (0x3C4 included); SEL.BIN holds the
    table; DRA's g_ModDub is where SEL writes it; BIN/JP_SD.BIN's banks
    (MOD_JP_SFX_PAIRS) are VABs.
"""
import json
import math
import os
import re
import struct
import sys

from mod_work import read_src

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import psp_script as P  # noqa: E402

ROOT = P.ROOT
S = 2352
fail = 0


def bad(msg):
    global fail
    fail += 1
    print("FAIL " + msg)


def configs():
    gen = open(os.path.join(ROOT, "src/dra/gen/us/xa_music_config.h")).read()
    rows = [list(map(lambda x: int(x, 0), m)) for m in re.findall(
        r"\{(0x[0-9A-F]+), (0x[0-9A-F]+), (0x[0-9A-F]+), (\d+), (\d+), (\d+), (\d+)\},", gen)]
    mod = open(os.path.join(ROOT, "src/dra/mod_xa_music.h")).read()
    rows += [list(map(int, m)) for m in re.findall(
        r"\{(\d+), (\d+), (\d+), (\d+), (\d+), (\d+), (\d+)\},", mod)]
    return rows


def psp_table():
    """mod_psp_xa.h -> {voice id: [cd_addr, unk228, file, channel]}"""
    h = open(os.path.join(ROOT, "src/dra/mod_psp_xa.h")).read()
    base = int(re.search(r"#define MOD_PSP_XA_BASE (\d+)", h)[1])
    bits = [int(x, 0) for x in re.search(r"g_ModPspXaBits\[\d+\] = \{([^}]*)\}", h)[1].split(",") if x.strip()]
    words = [int(x, 0) for x in re.search(r"g_ModPspXa\[\d+\] = \{([^}]*)\}", h)[1].split(",") if x.strip()]
    first = int(re.search(r"#define MOD_PSP_XA_FIRST (0x[0-9A-Fa-f]+)",
                          read_src("include/game.h"))[1], 16)
    out, n = {}, 0
    for k in range(len(bits) * 8):
        if bits[k >> 3] >> (k & 7) & 1:
            w = words[n]
            n += 1
            sec = (w & 0x7FFF) + base
            out[first + k] = [sec & ~15, (w >> 15) & 0x3FF, w >> 25, sec & 15]
    assert n == int(re.search(r"#define MOD_PSP_XA_COUNT (\d+)", h)[1])
    return out


JP_JSON = os.path.join(ROOT, "build/us/jp_dub/dub.json")
JP_H = os.path.join(ROOT, "src/st/sel/mod_jp_xa.h")
JP_MAGIC = 0x4A504455  # include/game.h MOD_JP_MAGIC
MOD_DUB_ADDR = 0x8013644C  # src/st/sel/2C048.c MOD_DUB_STATE


def jp_table():
    """mod_jp_xa.h -> ({voice id: [cd_addr, unk228, file, channel]}, the
    MOD_VO_DID_YOU_NOT entry or None, base, its word, the words)"""
    h = open(JP_H).read()
    base = int(re.search(r"#define MOD_JP_XA_BASE (\d+)", h)[1])
    dyn = int(re.search(r"#define MOD_JP_XA_DID_YOU_NOT (0x[0-9A-F]+)", h)[1], 16)
    bits = [int(x, 0) for x in re.search(r"MOD_JP_XA_BITS \{([^}]*)\}", h)[1].split(",") if x.strip()]
    body = re.search(r"MOD_JP_XA_WORDS \{(.*?)\}", h, re.S)[1].replace("\\", "")
    words = [int(x, 0) for x in body.split(",") if x.strip()]

    def dec(w):
        sec = (w & 0x7FFF) + base
        return [sec & ~15, (w >> 15) & 0x7FF, w >> 26, sec & 15]
    out, n = {}, 0
    for k in range(len(bits) * 8):
        if bits[k >> 3] >> (k & 7) & 1:
            out[P.VOICE_LO + k] = dec(words[n])
            n += 1
    assert n == int(re.search(r"#define MOD_JP_XA_COUNT (\d+)", h)[1])
    return out, (dec(dyn) if dyn else None), base, dyn, words


def sym(mapfile, name):
    m = re.search(r"0x([0-9a-f]+)\s+%s\n" % name, open(os.path.join(ROOT, mapfile)).read())
    return int(m[1], 16) if m else None


def read_pos(b, i, end=None):
    """read_script, and each token's offset in b"""
    toks, pos = [], []
    while True:
        pos.append(i)
        t, i = read_one(b, i)
        toks.append(t)
        if (end is None and t[0] == "END_CUTSCENE") or (end is not None and i >= end):
            return toks, pos


def read_script(b, i, end=None):
    toks = []
    while True:
        t, i = read_one(b, i)
        toks.append(t)
        if end is None and t[0] == "END_CUTSCENE":
            break
        if end is not None and i >= end:
            break
    return toks


def main():
    dub = json.load(open(P.DUB_JSON))
    dubbed = {0x300 + int(c) for c in dub["replace"]}
    extra = {}
    xid = json.load(open(os.path.join(P.WORK, "extra_ids.json")))
    for e in dub["extra"]:
        extra[xid[e["define"]]] = e
    psp = psp_table()
    if set(psp) != dubbed:
        bad("mod_psp_xa.h ids != psp_dub.py's (%d vs %d)" % (len(psp), len(dubbed)))
    jdub = json.load(open(JP_JSON))
    jp, jp_dyn, jp_base, jp_dyn_word, jp_words = jp_table()
    if set(jp) != {0x300 + int(c) for c in jdub["replace"]}:
        bad("mod_jp_xa.h ids != psp_dub.py --set jp's (%d vs %d)" % (len(jp), len(jdub["replace"])))
    njcopies = njlines = 0
    jworst = -1e9
    jdur = P.jp_durations(xid)
    for vid_, (_, sec_) in P.SPEECH_END.items():
        if vid_ in jdur:
            jdur[vid_] = min(jdur[vid_], sec_ * 60)  # (as psp_script.py)
    cfg = configs()
    plans = {}
    for p in json.load(open(P.PATCHES)):
        plans[p["ovl"]] = p
    nscripts = ncopies = nvoices = nptrs = 0
    for ovl, (retail_rel, mapname, scripts) in P.OVERLAYS.items():
        bins = [("build/us/%s.BIN" % ovl.upper(), "build/us/%s.map" % mapname)]
        if ovl == "cen":
            bins.append(("build/us/CENP.BIN", "build/us/mod/cenp/stcenp.map"))
        if ovl == "bo6":
            bins.append(("build/us/BO6RIC2.BIN", "build/us/bobo6.map"))
        if ovl == "sel":  # (the coat endings, mods/sotn-ultimate/tools/build_selric2.py)
            bins.append(("build/us/SELRIC2.BIN", "build/us/stsel.map"))
        retail = open(os.path.join(ROOT, "disks/us", retail_rel), "rb").read()
        for binpath, mapfile in bins:
            b = open(os.path.join(ROOT, binpath), "rb").read()
            spill = sym(mapfile, "g_ModPspSpill")
            plan = plans.get(ovl, {}).get("spill")
            copies = 0
            ranges = []  # (retail lo, hi, map keys, sc) of the scripts with a copy
            for k, sc in enumerate(scripts):
                # the PS1 version: retail
                if "c" in sc:
                    blob = P.assemble(P.read_c_array(sc["c"], sc["sym"]))
                    a = sym(mapfile, sc["sym"])
                    off = a - P.BASE if a else None
                else:
                    # (with the parts its jumps reach past its END: BO6 0x8019A2FB)
                    rt, off = P.retail_script(retail, sc, ovl)
                    blob = P.assemble(rt)
                if off is None or b[off:off + len(blob)] != blob:
                    bad("%s: %s: the PS1 script %s is not the retail one" % (binpath, ovl, sc.get("sym", hex(off or 0))))
                    continue
                nscripts += 1
                ps1 = read_script(b, off, off + len(blob))
                for vid in text_voices(ps1):
                    if vid in extra:
                        bad("%s: PS1 script plays the PSP-only line 0x%X" % (ovl, vid))
                # the PSP copy
                ent = None
                if ovl in P.C_ARRAYS:
                    a = sym(mapfile, "g_ModPspCopy%d" % k)
                    if a is None:
                        bad("%s: %s: no g_ModPspCopy%d" % (binpath, ovl, k))
                        continue
                    copy = a - P.BASE
                    pairs = [(P.BASE + off, a)]
                else:
                    ent = next((x for x in (plan or {}).get("scripts", []) if x["off"] == off), None)
                    if ent is None:
                        if spill is not None or any(v in dubbed for v in text_voices(ps1)):
                            print("     %s: %s @0x%X has no PSP copy" % (binpath, ovl, off))
                        continue
                    copy = spill + ent["at"] - P.BASE
                    pairs = [(P.BASE + off + po, spill + so) for po, so in ent["entries"]]
                # the overlay's map holds the pairs (SetCutsceneScript's lookup)
                for ra, ca in pairs:
                    if struct.pack("<II", ra, ca) not in b:
                        bad("%s: %s: no map entry 0x%X -> 0x%X" % (binpath, ovl, ra, ca))
                # (a copy ending in a jump has no END: read it to its length below)
                if ent is None:
                    toks = read_script(b, copy)
                if ent is not None:
                    want = P.assemble(P.resolve([tuple(tuple(x) if isinstance(x, list) else x for x in t)
                                                 for t in ent["tokens"]], spill + ent["at"]))
                    if b[copy:copy + len(want)] != want:
                        bad("%s: %s: the PSP copy at 0x%X is not filled" % (binpath, ovl, copy))
                        continue
                    toks = read_script(b, copy, copy + len(want))
                copies += 1
                ncopies += 1
                ranges.append((P.BASE + off, P.BASE + off + len(blob), {ra for ra, _ in pairs}, sc))
                jump_inside(binpath, ovl, "PSP", toks, P.BASE + copy)
                try:
                    P.check_widths(toks, ovl)
                except SystemExit as e:
                    bad(str(e))
                for vid in [t[1] for t in toks if t[0] == "PLAY_SOUND" and t[1] > 0x300]:
                    if vid - 0x300 >= len(cfg) or (cfg[vid - 0x300][0] == 0 and
                                                   cfg[vid - 0x300][2:4] == [0, 0]):
                        bad("%s: plays 0x%X, which has no XA config" % (ovl, vid))
                for vid in [t[1] for t in toks if t[0] == "PLAY_SOUND" and t[1] in P.NO_PSP]:
                    bad("%s: the PSP copy plays the Japanese-only line 0x%X" % (ovl, vid))
                for vid in text_voices(toks):
                    nvoices += 1
                    if not (P.VOICE_LO <= vid <= P.VOICE_HI) or vid in P.NO_PSP:
                        continue
                    if vid not in psp and vid not in extra:
                        bad("%s: PSP text with the PS1-only voice 0x%X" % (ovl, vid))
                # the JP dub's copy (boxes held for the Japanese voices; CEN:
                # the NO_PSP line kept), else the PSP copy plays under JP too
                jcopy, jend, jpairs = None, None, []
                if ovl in P.C_ARRAYS:
                    ja = sym(mapfile, "g_ModJpCopy%d" % k)
                    if ja is not None:
                        jcopy, jpairs = ja - P.BASE, [(P.BASE + off, ja)]
                elif ent is not None and ent.get("jp_tokens"):
                    jsp = sym(mapfile, "g_ModJpSpill")
                    if jsp is None:
                        bad("%s: %s: no g_ModJpSpill" % (binpath, ovl))
                        continue
                    want = P.assemble(P.resolve([tuple(tuple(x) if isinstance(x, list) else x
                                                       for x in t) for t in ent["jp_tokens"]],
                                                jsp + ent["jp_at"]))
                    jcopy = jsp + ent["jp_at"] - P.BASE
                    jend = jcopy + len(want)
                    if b[jcopy:jend] != want:
                        bad("%s: %s: the JP copy at 0x%X is not filled" % (binpath, ovl, jcopy))
                        continue
                    jpairs = [(P.BASE + off + po, jsp + so) for po, so in ent["jp_entries"]]
                if jcopy is not None:
                    for ra, ca in jpairs:
                        if struct.pack("<II", ra, ca) not in b:
                            bad("%s: %s: no JP map entry 0x%X -> 0x%X" % (binpath, ovl, ra, ca))
                    jt, jpos = read_pos(b, jcopy, jend)
                    jump_inside(binpath, ovl, "JP", jt, P.BASE + jcopy)
                    ranges[-1][2].update(ra for ra, _ in jpairs)
                    try:
                        P.check_widths(jt, ovl)
                    except SystemExit as x:
                        bad(str(x))
                    jv = text_voices(jt)
                    for vid in jv:
                        if P.VOICE_LO <= vid <= P.VOICE_HI and vid not in jp:
                            bad("%s: JP copy plays 0x%X, which has no JP version" % (ovl, vid))
                    if not set(P.NO_PSP) & set(text_voices(ps1)) <= set(jv):
                        bad("%s: the JP copy %d lacks %s" % (binpath, k, P.NO_PSP))
                    njcopies += 1
                else:
                    jt, jpos = read_pos(b, copy, copy + len(P.assemble(P.resolve(
                        [tuple(tuple(x) if isinstance(x, list) else x for x in t)
                         for t in ent["tokens"]], 0))) if ent is not None else None)
                # every voiced line's box stays up until its JP voice ends
                at = {a: i for i, a in enumerate(jpos)}
                for vid, v, up, typed in P.jp_lines(jt, jdur, lambda a: at.get(a - P.BASE)
                                                    if isinstance(a, int) else None):
                    njlines += 1
                    if up < v:
                        bad("%s: %s: JP 0x%X: box up %.1f s, voice %.1f s" % (
                            binpath, ovl, vid, up / 60, v / 60))
                    jworst = max(jworst, v - up)
            nptrs += script_pointers(binpath, b, ranges)
    print("%s   script starts: %d script pointers in the overlays' code / data, each a "
          "PSP (and JP) map entry; no copy jumps out of itself" % ("OK" if not fail else "FAIL", nptrs))
    # the voices
    xa = open(os.path.join(ROOT, "build/us/XA_STR1."), "rb").read()
    n = len(xa) // S

    def run_of(addr, f, ch):
        s, out = addr + ch, []
        while s < n and xa[s * S + 16] == f and xa[s * S + 17] == ch:
            out.append(s)
            s += 16 if not xa[s * S + 19] & 1 else 8
        return out
    nd = 0
    items = [(vid, psp[vid], e) for vid, e in
             ((0x300 + int(c), e) for c, e in dub["replace"].items()) if vid in psp]
    items += [(vid, cfg[vid - 0x300][:4], e) for vid, e in extra.items()]
    for vid, (addr, ln, f, ch), e in items:
        need = math.ceil(e["seconds"] * 60)
        if not need <= ln <= need + 6:
            bad("0x%X: unk228 %d for a %.2f s line" % (vid, ln, e["seconds"]))
        run = run_of(addr, f, ch)
        enc = open(e["xa"], "rb").read()
        if len(run) != e["sectors"] or any(xa[s * S + 19] & 1 for s in run) or any(
                xa[s * S + 24:s * S + 2328] != enc[k * S + 24:k * S + 2328] for k, s in enumerate(run)):
            bad("0x%X: the PSP XA stream is not the encoded line" % vid)
        if len(run) * 4032 / 37800 * 60 + 2 < ln - 6:
            bad("0x%X: timer runs past the stream" % vid)
        if vid in psp:
            c = cfg[vid - 0x300]
            r1 = run_of(c[0], c[2], c[3])
            if not r1 or (c[0], c[2], c[3]) == (addr, f, ch) or set(r1) & set(run):
                bad("0x%X: the PS1 config does not play its own PS1 stream" % vid)
        nd += 1
    # the JP versions
    nj = 0
    jitems = [(vid, jp[vid], e) for vid, e in
              ((0x300 + int(c), e) for c, e in jdub["replace"].items()) if vid in jp]
    for e in jdub["extra"]:
        if jp_dyn is None:
            bad("JP: no %s version" % e["define"])
        else:
            jitems.append((xid[e["define"]], jp_dyn, e))
    for vid, (addr, ln, f, ch), e in jitems:
        need = math.ceil(e["seconds"] * 60)
        if not need <= ln <= need + 6:
            bad("JP 0x%X: unk228 %d for a %.2f s line" % (vid, ln, e["seconds"]))
        run = run_of(addr, f, ch)
        enc = open(e["xa"], "rb").read()
        if len(run) != e["sectors"] or any(xa[s * S + 19] & 1 for s in run) or any(
                xa[s * S + 24:s * S + 2328] != enc[k * S + 24:k * S + 2328] for k, s in enumerate(run)):
            bad("JP 0x%X: the JP XA stream is not the encoded line" % vid)
        if len(run) * 4032 / 37800 * 60 + 2 < ln - 6:
            bad("JP 0x%X: timer runs past the stream" % vid)
        c = cfg[vid - 0x300]
        r1 = run_of(c[0], c[2], c[3])
        if not r1 or set(r1) & set(run):
            bad("JP 0x%X: the PS1 config does not play its own stream" % vid)
        if vid in psp and set(run_of(psp[vid][0], psp[vid][2], psp[vid][3])) & set(run):
            bad("JP 0x%X: shares the PSP stream" % vid)
        nj += 1
    longest = max((e["seconds"] for _, _, e in jitems), default=0)
    # SEL's built table (ModJpDubTable: magic, base, MOD_VO_DID_YOU_NOT, sfx
    # pairs (MOD_JP_SFX_MAX x 2 u16), bits (60), words)
    sfx_max = int(re.search(r"#define MOD_JP_SFX_MAX (\d+)",
                            read_src("include/game.h")).group(1))
    sel = open(os.path.join(ROOT, "build/us/SEL.BIN"), "rb").read()
    tbl = sel.find(struct.pack("<III", JP_MAGIC, jp_base, jp_dyn_word))
    if tbl < 0:
        bad("SEL.BIN: no JP dub table")
    else:
        w = struct.unpack_from("<%dI" % len(jp_words), sel, tbl + 12 + sfx_max * 4 + 60)
        if list(w) != jp_words:
            bad("SEL.BIN: the JP table's words differ from mod_jp_xa.h")
    a = sym("build/us/dra.map", "g_ModDub")
    if a != MOD_DUB_ADDR:
        bad("DRA g_ModDub at %s, SEL writes 0x%X (src/st/sel/2C048.c MOD_DUB_STATE)" % (
            hex(a) if a else None, MOD_DUB_ADDR))
    # BIN/JP_SD.BIN's stage / familiar banks and the player ones
    jsd = open(os.path.join(ROOT, "build/us/JP_SD.BIN"), "rb").read()
    pairs = re.search(r"MOD_JP_SFX_PAIRS \{(.*)\}", open(os.path.join(
        ROOT, "src/st/sel/mod_jp_sfx.h")).read())[1]
    nb = 0
    for p_, j_ in re.findall(r"\{(\d+), (\d+)\}", pairs):
        j_ = int(j_)
        if j_ in (0, 0xFFFF):
            continue
        if jsd[(j_ - 1) * 2048:(j_ - 1) * 2048 + 4] != b"pBAV":
            bad("JP_SD.BIN: no VAB at sector %d" % (j_ - 1))
        nb += 1
    for off_ in (0, 32, 92, 158):
        if jsd[off_ * 2048:off_ * 2048 + 4] != b"pBAV":
            bad("JP_SD.BIN: no VAB at sector %d" % off_)
    print("%s   psp dub toggle: %d PS1 scripts, %d PSP copies (%d voiced lines), "
          "%d PSP voice versions checked" % ("OK" if not fail else "FAIL", nscripts, ncopies,
                                             nvoices, nd))
    print("%s   jp dub: %d JP voice versions (longest %.1f s), %d JP script copies, "
          "%d JP_SD banks + 4 player banks" % ("OK" if not fail else "FAIL", nj, longest,
                                               njcopies, nb))
    print("%s   jp dub timing: %d voiced lines, every box up until its JP voice ends "
          "(closest: %.1f s to spare)" % ("OK" if not fail else "FAIL", njlines, -jworst / 60))
    sys.exit(1 if fail else 0)


def jump_inside(binpath, ovl, which, toks, lo):
    """a copy's jumps (SCRIPT_SWITCH cases, SCRIPT_UNKNOWN_15) stay in it: one
    out of it would play the retail (PS1) text (BO6's Shaft lines did)"""
    hi = lo + len(P.assemble([("TEXT", "x" * len(t[1])) if t[0] == "TEXT" else t for t in toks]))
    for t in toks:
        for a in P.jump_targets(t):
            if not lo <= a < hi:
                bad("%s: %s: the %s copy at 0x%X jumps out of itself (to 0x%X)" % (
                    binpath, ovl, which, lo, a))


def script_pointers(binpath, b, ranges):
    """every address into a script with a copy that the overlay can load as a
    pointer must be one of its map entries, or SetCutsceneScript / the
    script-text start would play the retail (PS1) script from there: MIPS
    lui + addiu / ori (any distance, per register, as a linear sweep; an
    addiu on a known value too: DRE + 0x49D) and aligned data words, over
    the whole built overlay. Returns how many it found."""
    if not ranges:
        return 0
    found = set()

    def hit(v, where):
        for lo, hi, keys, sc in ranges:
            if lo <= v < hi:
                found.add(v)
                if v not in keys:
                    bad("%s: %s 0x%X (%s): a pointer into script 0x%X with no map entry" % (
                        binpath, where, P.BASE + i * 4, "0x%X" % v, lo))
    reg = [None] * 32
    for i in range(len(b) // 4):
        w = struct.unpack_from("<I", b, i * 4)[0]
        op, rs, rt, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
        if op == 0x0F:  # lui
            reg[rt] = imm << 16
        elif op in (0x09, 0x0D) and reg[rs] is not None:  # addiu / ori
            v = (reg[rs] + (imm - 0x10000 if imm & 0x8000 else imm)) & 0xFFFFFFFF                 if op == 0x09 else reg[rs] | imm
            hit(v, "code")
            reg[rt] = v
        else:
            # any other write ends what the register held
            if op in (0x08, 0x0A, 0x0B, 0x0C, 0x0E, 0x09, 0x0D) or 0x20 <= op <= 0x26 or \
                    (op == 0x12 and rs in (0, 2)):
                reg[rt] = None
            elif op == 0:
                reg[(w >> 11) & 31] = None
            elif op == 0x03:  # jal
                reg[31] = None
        hit(w, "word")
    return len(found)


def read_one(b, i):
    c = b[i]
    if c >= 0x20:
        j = i
        while b[j] >= 0x20:
            j += 1
        return ("TEXT", b[i:j].decode("latin-1")), j
    name = P.NAMES[c]
    if name == "SCRIPT_SWITCH":
        k, args = i + 1, []
        while 0x80 <= b[k] < 0xC0 and bytes(P.word(P.unword(b[k:k + 4]))) == b[k:k + 4]:
            args.append(P.unword(b[k:k + 4]) | 0x80100000)
            k += 4
        return tuple([name] + args), k
    args, k = [], i + 1
    for size in P.OPS[name][1]:
        if size == 1:
            args.append(b[k])
        elif size == 2:
            args.append((b[k] << 4) | b[k + 1])
        else:
            args.append(P.unword(b[k:k + 4]) | 0x80100000)
        k += size
    return tuple([name] + args), k


def text_voices(toks):
    """voice ids followed by text before the next voice / dialog end"""
    out = []
    for i, t in enumerate(toks):
        if t[0] != "PLAY_SOUND" or t[1] < 0x300:
            continue
        j = i + 1
        while j < len(toks) and toks[j][0] not in P.SEG_END:
            if toks[j][0] == "TEXT":
                out.append(t[1])
                break
            j += 1
    return out


if __name__ == "__main__":
    main()
