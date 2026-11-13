#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the PSP (Dracula X Chronicles) English script on the PS1.

The PSP's English dub (mods/sotn-ultimate/tools/psp_dub.py) is a new recording of a revised
translation, so its lines only make sense with the PSP's text. This puts the
PSP English text of every voiced line into the PS1 cutscene scripts.

In-game toggle (include/game.h MOD_PSP_DUB_ON, chosen on the Player
Select): the retail scripts stay as they are, and each overlay gets a PSP
copy of every rewritten script in a rodata array (g_ModPspSpill, in the
overlay's mod_psp_script.h, filled in the built overlay by --apply because
the copies jump inside themselves) plus a map retail address -> copy that
the overlay's SetCutsceneScript uses while the dub is on (the C code's
entry offsets, DRE + 0x49D and RCEN + 0x129, are map entries too). CEN's
three scripts (the retail one and the Saturn fight's two, which CENP.BIN
shares) have no jumps: their copies are plain C arrays in the header.
The PSP scripts' own pointers mean nothing on the PS1, so only the PSP text
and its pauses (SET_WAIT) are taken, line by line (voice id: PS1 = PSP + 2),
and put on the PS1 skeleton: every non-text op (portraits, flags, events,
sound waits, jumps re-pointed into the copy) stays; ops in the middle of a
line's text keep their relative place.

Text: the PS1 font is fixed 8 px; a line holds MAX_COLS characters (the
text sprite is 192 px from x = 8 and the box ends at 248 on screen, so 22
touch the frame; 21 keeps a margin). PSP lines up to 29 characters are
rewrapped at word boundaries; more than 4 lines scroll as retail does.
NO4 (the ferryman, Dialogue3) does not scroll: 3 text sprites, then
g_Dialogue.prim[3] is the PORTRAIT, so a 4th line (nextCharY 3) turns the
portrait into a garbled text quad (CutsceneUnk4) and types into VRAM row
384 + 36. BOX_LIMITS caps its boxes at 3 lines (counted from SET_PORTRAIT,
which is what resets nextCharY there) of 22 columns (retail's own longest;
the text starts at x 56, the box ends at 248).
Typing speed: the PSP ignores SET_SPEED, so the PS1 line's own speed is
kept (the one most of its characters used), lowered when the text would
still be typing well after the voice ended.

A script has no PSP copy (text AND voices stay PS1 in both modes) when any
of its voiced lines has no PSP English text; NO_PSP lines (the Saturn
Maria fight's line with no PSP counterpart) are cut from the copies
(drop_lines). Those voice ids go to
build/us/psp_script/keep_ps1.json: psp_dub.py gives them no PSP version.

  psp_script.py          plan the copies (build/us/psp_script/patches.json,
                         report.txt, keep_ps1.json), write the headers
  psp_script.py --apply  fill the copies in the built overlays (build/us/*.BIN)
                         after the link; checks the retail scripts are intact
  psp_script.py --check  only check that (BO6RIC2.BIN too)
  psp_script.py --off    empty headers (MOD_PSP_DUB=0: no toggle, PS1 only)

JP dub (include/game.h MOD_JP_DUB_ON: Japanese voices, this PSP English
text): a third copy of each script (g_ModJpSpill / CEN's g_ModJpCopyN,
g_ModJpMap; ModPspScript looks there first while the JP dub is on) where
the Japanese lines (build/us/jp_dub/dub.json, so build_disc.sh runs this
again after psp_dub.py --set jp) need other timing (jp_adjust): a box is
held until its voice ends (SCRIPT_UNKNOWN_11 + a short wait before the
NEXT_DIALOG that would clear it early) and the typing is slowed where it
would end far ahead of the voice. CEN's Saturn fight copy also keeps the
cut NO_PSP line (the JP dub has a voice for it: the Saturn's take of
0x3C4) with its PS1 text.
"""
import argparse
import json
import math
import os
import re
import sys
from collections import Counter

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
PSP_BIN = os.path.join(ROOT, "disks/psp/PSPBIN")
WORK = os.path.join(ROOT, "build/us/psp_script")
PATCHES = os.path.join(WORK, "patches.json")
REPORT = os.path.join(WORK, "report.txt")
KEEP_JSON = os.path.join(WORK, "keep_ps1.json")
EDITS = os.path.join(WORK, "edits.json")  # check_stage_data.py allows these
DUB_JSON = os.path.join(ROOT, "build/us/psp_dub/dub.json")
MAX_COLS = 21
# overlay -> (columns, lines per box) where the box differs (see above)
BOX_LIMITS = {"no4": (22, 3)}


def box_limits(ovl):
    return BOX_LIMITS.get(ovl, (MAX_COLS, None))


BASE = 0x80180000  # stage / boss overlays load here; BIN offset = addr - BASE
VOICE_LO, VOICE_HI = 0x358, 0x532  # PS1 voice ids with a PSP counterpart

# overlay -> (retail BIN, built BIN, map name, scripts). A script is a yaml
# asset (its bytes are found in the retail BIN) or a RAM address (asm-only
# bosses: parsed from the retail BIN until END_CUTSCENE). "split": offsets
# the C code enters at (each part is rewritten within its own bytes).
OVERLAYS = {
    "no3": ("ST/NO3/NO3.BIN", "stno3", [{"yaml": "assets/st/no3/cutscene_data.yaml"}]),
    "st0": ("ST/ST0/ST0.BIN", "stst0", [{"yaml": "assets/st/st0/cutscene_script_psx.yaml"}]),
    "dai": ("ST/DAI/DAI.BIN", "stdai", [{"yaml": "assets/st/dai/cutscene_script_psx.yaml"}]),
    "dre": ("ST/DRE/DRE.BIN", "stdre", [{"yaml": "assets/st/dre/cutscene_script_psx.yaml",
                                         "split": [0x49D]}]),
    "lib": ("ST/LIB/LIB.BIN", "stlib", [{"yaml": "assets/st/lib/cutscene_data.yaml"}]),
    "nz0": ("ST/NZ0/NZ0.BIN", "stnz0", [{"yaml": "assets/st/nz0/cutscene_data.yaml"}]),
    "top": ("ST/TOP/TOP.BIN", "sttop", [{"yaml": "assets/st/top/cutscene_data.yaml"}]),
    # C arrays (no jumps): the retail script and the Saturn fight's two
    # (mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h; CENP.BIN compiles cutscene.c too)
    "cen": ("ST/CEN/CEN.BIN", "stcen", [
        {"yaml": "assets/st/cen/cutscene_data.yaml", "sym": "cutscene_data"},
        {"c": "mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h", "sym": "g_MbScriptPre"},
        {"c": "mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h", "sym": "g_MbScriptPost"}]),
    "rcen": ("ST/RCEN/RCEN.BIN", "strcen", [{"yaml": "assets/st/rcen/cutscene_script_psx.yaml",
                                             "split": [0x129]}]),
    "no4": ("ST/NO4/NO4.BIN", "stno4", [
        {"yaml": "assets/st/no4/cutscene_data_ferryman_left_psx.yaml"},
        {"yaml": "assets/st/no4/cutscene_data_ferryman_right_psx.yaml"}]),
    "sel": ("ST/SEL/SEL.BIN", "stsel", [{"yaml": "assets/st/sel/cutscene_endings.yaml"}]),
    "mar": ("BOSS/MAR/MAR.BIN", "bomar", [{"yaml": "assets/boss/mar/cutscene_data.yaml"}]),
    # SetCutsceneScript arguments in the asm (asm/us/boss/*/nonmatchings)
    "bo2": ("BOSS/BO2/BO2.BIN", "bobo2", [{"addr": 0x80181CCC}, {"addr": 0x80181DC5}]),
    "bo5": ("BOSS/BO5/BO5.BIN", "bobo5", [{"addr": 0x80181D5C}]),
    "bo6": ("BOSS/BO6/BO6.BIN", "bobo6", [{"addr": 0x8019A0D0}, {"addr": 0x8019A2E1},
                                          {"addr": 0x8019A2FB}]),
    "rbo2": ("BOSS/RBO2/RBO2.BIN", "borbo2", [{"addr": 0x80181878}]),
    "rbo6": ("BOSS/RBO6/RBO6.BIN", "borbo6", [{"addr": 0x80181720}, {"addr": 0x801819E7}]),
}
# the C file of each overlay whose SetCutsceneScript takes the copies: it
# includes its mod_psp_script.h before SetCutsceneScript
SPILL_C = {
    "no3": "src/st/no3/cutscene.c",
    "st0": "src/st/st0/cutscene.c",
    "dai": "src/st/dai/e_cutscene_dialogue.c",
    "dre": "src/st/dre/e_cutscene_dialogue.c",
    "lib": "src/st/lib/cutscene.c",
    "nz0": "src/st/nz0/cutscene.c",
    "top": "src/st/top/cutscene.c",
    "cen": "src/st/cen/cutscene.c",
    "rcen": "src/st/rcen/cutscene.c",
    "no4": "src/st/no4/cutscene.c",
    "sel": "src/st/sel/cutscene.c",
    "mar": "src/boss/mar/cutscene.c",
    "bo2": "src/boss/bo2/e_cutscene_dialogue.c",
    "bo5": "src/boss/bo5/e_cutscene_dialogue.c",
    "bo6": "src/boss/bo6/cutscene.c",
    "rbo2": "src/boss/rbo2/e_cutscene_dialogue.c",
    "rbo6": "src/boss/rbo6/e_cutscene_dialogue.c",
}
# overlays whose copies are plain C arrays (no jumps; scripts found by symbol)
C_ARRAYS = {"cen"}
# lines the PSP copies leave out (PS1 dub only). The Saturn Maria fight's
# lines (Japanese recordings 0x3C1-0x3C4, g_MbScriptPre/Post) take the PSP
# fight's English ones by the usual PS1 = PSP + 2: 0x3C0/0x3C1 (one box)
# <- 0x3BE 190g "Yes, but can you?" / 0x3BF 190g1 "Then for all our sakes,
# show me that you will!" (then the fight starts), 0x3C2 <- 0x3C0 "You're
# stronger than I am...", 0x3C3 <- 0x3C1 "I can't promise that..."; 0x3C4
# "I suppose that can't be helped either..." has none (the PSP goes on to
# "If it must be that way...", 0x3CC) and is cut.
NO_PSP = {0x3C4}
# voices whose clip goes on past the line into the next animation's sound:
# id -> (PSP, JP) seconds where the speech ends. Their box is held (a plain
# SET_WAIT, not SCRIPT_UNKNOWN_11, which would wait for the whole clip) only
# to there, so the box closes and the scene goes on under the rest of the
# clip, as the PS1's does. RCEN 0x419: Shaft's last line ends in his death
# scream (PSP 350e "Nooooo!" from 10.6 s, JP from 17.9 s), which the PS1
# plays over his death (the box closes at the text's end, SET_FLAG 8).
SPEECH_END = {0x419: (10.6, 17.9)}
# voices with a SET_FLAG inside their text that starts the next action (a
# fight): in the copies the flag comes as long before the box closes as on
# the PS1 (flag_leads), and the box is held with a plain SET_WAIT. RCEN
# 0x406: Shaft's "...your weak human side." (SET_FLAG 1 starts the fight
# 112 frames before the box goes; the PSP text put it 5.5 s early).
FLAG_LEAD = {0x406}
# ops a cut line may take with it (anything else, e.g. a flag, aborts)
DROP_OK = {"SET_PORTRAIT", "SCRIPT_UNKNOWN_11", "PLAY_SOUND", "WAIT_FOR_SOUND",
           "SET_SPEED", "TEXT", "LINE_BREAK", "SET_WAIT", "NEXT_DIALOG"}


def drop_lines(toks, ids, where):
    """toks without the lines whose PLAY_SOUND is in ids: a line sharing the
    previous line's box loses its LINE_BREAK and leaves its end wait; a line
    with a box of its own goes with its portrait and NEXT_DIALOG"""
    out = list(toks)
    while True:
        i = next((k for k, t in enumerate(out) if t[0] == "PLAY_SOUND" and t[1] in ids), None)
        if i is None:
            return out
        j = seg_bounds(out, i)
        a = i
        while a > 0 and out[a - 1][0] in ("SCRIPT_UNKNOWN_11", "SET_PORTRAIT"):
            a -= 1
        if a > 0 and out[a - 1][0] == "LINE_BREAK":
            a -= 1
            new = [out[j - 1]] if out[j - 1][0] == "SET_WAIT" else []
            b = j
        else:
            assert out[j][0] == "NEXT_DIALOG", (where, hex(out[i][1]))
            new, b = [], j + 1
        bad = [t for t in out[a:b] if t[0] not in DROP_OK]
        if bad:
            sys.exit("psp_script: %s: cutting 0x%X would drop %r" % (where, out[i][1], bad))
        out[a:b] = new
# PS1 voice id -> the PSP line (PSP id) that matches it, where not PS1 - 2:
# DRE's "You broke free of my spell" (PS1 0x38B) is the PSP's 080b "Strong
# enough to break my spell" (PSP 0x38A; PS1 slot 0x38C is a silent unused
# one); the PSP's 080a (0x389) is in no script. psp_dub.py VOICE_OVERRIDE
# gives slot 0x38B that recording.
# BO6: the PSP's first branch plays its 0x3D4 (232b, "Richter. Why would a
# Belmont...") where the PS1's first branch plays 0x3D4 ("So it is you..."),
# and its second branch 0x3D2 (231b, "Answer me!") where the PS1 plays
# 0x3D6 ("Answer me!"): the two swap (psp_dub.py VOICE_OVERRIDE too)
# CEN: the PSP restructured Maria's scene around its own fight; the PS1
# skeleton's lines take the PSP lines that say the same thing, by the same
# speaker: 0x3CC "Well... here. Take these" <- 0x3C9 "If it must be that
# way... Here. I found these.", 0x3CD "What are these?" <- 0x3C4 "And these
# are?", 0x3CF "Thank you. 'Tis best then if you pray" <- 0x3C6 "Thank you.
# I go to find Richter. Stay, and pray for him." (psp_dub.py VOICE_OVERRIDE);
TEXT_OVERRIDE = {0x38B: 0x38A, 0x3D4: 0x3D4, 0x3D6: 0x3D2,
                 0x3CC: 0x3C9, 0x3CD: 0x3C4, 0x3CF: 0x3C6}
# PSP lines with no PS1 slot, added after a PS1 line: PS1 voice id -> (PSP
# id of the new line, mod XA id, where: None = the next line of the same
# box; text = it replaces the next box's unvoiced text, this one (the other
# speaker's box))
APPEND = {
    # rbo6: Alucard's "...Did you not?" (380e2) answers Dracula's "...did I?"
    # in Alucard's next box, the PS1's silent "........."
    0x427: (0x531, "MOD_VO_DID_YOU_NOT", "........."),
}

# opcode table: name -> (code, param sizes in script bytes)
OPS = {
    "END_CUTSCENE": (0, []), "LINE_BREAK": (1, []), "SET_SPEED": (2, [1]),
    "SET_WAIT": (3, [1]), "HIDE_DIALOG": (4, []), "SET_PORTRAIT": (5, [1, 1]),
    "NEXT_DIALOG": (6, []), "SET_POS": (7, [1, 1]), "CLOSE_DIALOG": (8, []),
    "PLAY_SOUND": (9, [2]), "WAIT_FOR_SOUND": (10, []), "SCRIPT_UNKNOWN_11": (11, []),
    "SET_EVENTS": (12, [4]), "SCRIPT_UNKNOWN_13": (13, []), "SCRIPT_SWITCH": (14, None),
    "SCRIPT_UNKNOWN_15": (15, [4]), "WAIT_FOR_FLAG": (16, [1]), "SET_FLAG": (17, [1]),
    "SCRIPT_UNKNOWN_18": (18, []), "LOAD_PORTRAIT": (19, [4, 1]),
    "SCRIPT_UNKNOWN_20": (20, [2]), "SCRIPT_UNKNOWN_21": (21, []), "RESET_FLAG": (22, [1]),
    "SCRIPT_UNKNOWN_23": (23, []), "WAIT_FOR_FLAG_RESET": (24, [1]),
}
NAMES = {code: name for name, (code, _) in OPS.items()}
TEXT_OPS = {"TEXT", "LINE_BREAK", "SET_SPEED", "SET_WAIT"}
SEG_END = {"NEXT_DIALOG", "CLOSE_DIALOG", "END_CUTSCENE", "PLAY_SOUND", "HIDE_DIALOG"}


def word(x):
    return [(x & 0xFF000) >> 12, (x & 0xFF00) >> 8, (x & 0xFF0) >> 4, x & 0xFF]


def unword(b):
    v = 0
    for x in b:
        v = (v << 4) | x
    return v & 0xFFFFF


def assemble(toks):
    out = []
    for t in toks:
        op = t[0]
        if op == "TEXT":
            out += list(t[1].encode("ascii"))
        elif op == "BYTE":
            out.append(t[1])
        elif op == "SCRIPT_SWITCH":
            out.append(14)
            for a in t[1:]:
                out += word(a)
        else:
            code, params = OPS[op]
            out.append(code)
            for size, a in zip(params, t[1:]):
                if size == 1:
                    out.append(a)
                elif size == 2:
                    out += [(a & 0xFF0) >> 4, a & 0xFF]
                else:
                    out += word(a)
    assert all(0 <= b < 256 for b in out), toks
    return bytes(out)


def read_c_array(path, sym):
    """tokens of a script written as a C array of cutscene.h macros"""
    src = open(os.path.join(ROOT, path)).read()
    m = re.search(r"^u8 %s\[\][^=]*= \{(.*?)\};" % sym, src, re.S | re.M)
    assert m, (path, sym)
    items = re.findall(r"(\w+)\(([^)]*)\)|'(\\.|[^'])'|(0x[0-9a-fA-F]+|\d+)", m.group(1))
    toks = []
    for name, args, ch, num in items:
        if name:
            a = [int(x, 0) for x in args.split(",")] if args.strip() else []
            toks.append(tuple([name] + a))
        elif ch:
            c = ch[1] if ch.startswith("\\") else ch
            if toks and toks[-1][0] == "TEXT":
                toks[-1] = ("TEXT", toks[-1][1] + c)
            else:
                toks.append(("TEXT", c))
        else:
            toks.append(("BYTE", int(num, 0)))
    return toks


def read_yaml(path):
    toks = []
    for line in open(os.path.join(ROOT, path)):
        m = re.match(r'\s*- \[TEXT, "(.*)"\]\s*$', line)
        if m:
            toks.append(("TEXT", m.group(1)))
            continue
        m = re.match(r"\s*- \[(\w+)(?:, (.*))?\]\s*$", line)
        if m:
            args = [int(a, 0) for a in m.group(2).split(",")] if m.group(2) else []
            toks.append(tuple([m.group(1)] + args))
    return toks


def disassemble(b, start, stop_at_end=True):
    """script ops from b[start:], until END_CUTSCENE; (tokens, end offset)"""
    toks, i = [], start
    while True:
        c = b[i]
        if c >= 0x20:
            j = i
            while b[j] >= 0x20:
                j += 1
            toks.append(("TEXT", b[i:j].decode("latin-1")))
            i = j
            continue
        name = NAMES[c]
        if name == "SCRIPT_SWITCH":
            # the variable, then the cases: words of overlay addresses (an
            # op or text never starts with 0x80-0xBF)
            k, args = i + 1, []
            while 0x80 <= b[k] < 0xC0 and bytes(word(unword(b[k:k + 4]))) == b[k:k + 4]:
                args.append(unword(b[k:k + 4]) | 0x80100000)
                k += 4
            assert len(args) >= 3, hex(i)
            toks.append(tuple([name] + args))
            i = k
            continue
        params = OPS[name][1]
        args, k = [], i + 1
        for size in params:
            if size == 1:
                args.append(b[k])
            elif size == 2:
                args.append((b[k] << 4) | b[k + 1])
            else:
                args.append(unword(b[k:k + 4]) | 0x80100000)
            k += size
        toks.append(tuple([name] + args))
        i = k
        if name == "END_CUTSCENE" and stop_at_end:
            return toks, i


def tok_size(t):
    return len(assemble([t]))


def jump_targets(t):
    """the script addresses a SCRIPT_SWITCH (cases, not its variable) or a
    SCRIPT_UNKNOWN_15 (goto) jumps to"""
    if t[0] == "SCRIPT_SWITCH":
        return list(t[2:])
    if t[0] == "SCRIPT_UNKNOWN_15":
        return [t[1]]
    return []


def retail_script(retail, sc, where):
    """(tokens, BIN offset) of a yaml / addr script in the retail overlay,
    with every part its jumps reach: BO6 0x8019A2FB's SCRIPT_SWITCH sends the
    good route (orb destroyed) to Shaft's lines right after the script's
    END_CUTSCENE (0x8019A432), which the copy must hold too, else its jump
    lands in the retail (PS1) text. A jump anywhere else outside: error."""
    if "yaml" in sc:
        toks = read_yaml(sc["yaml"])
        blob = assemble(toks)
        off = retail.find(blob)
        assert off >= 0 and retail.find(blob, off + 1) < 0, (where, sc["yaml"])
    else:
        off = sc["addr"] - BASE
        toks, end = disassemble(retail, off)
        assert retail[off:end] == assemble(toks), where
    while True:
        lo = BASE + off
        hi = lo + len(assemble(toks))
        out = [a for t in toks for a in jump_targets(t) if not lo <= a < hi]
        if not out:
            return toks, off
        if hi not in out:
            sys.exit("psp_script: %s 0x%X: a jump leaves the script (%s)" % (
                where, lo, " ".join("0x%X" % a for a in out)))
        more, end = disassemble(retail, hi - BASE)
        assert retail[hi - BASE:end] == assemble(more), where
        toks = toks + more


# ---- PSP side --------------------------------------------------------------
EN_WORDS = set("the you i to of a is and it my your what me this be that not are "
               "have will for in do with so no all can was he she his her we us "
               "am an on but".split())


def en_score(text):
    w = re.findall(r"[a-z']+", text.lower())
    return sum(x in EN_WORDS for x in w) / max(1, len(w))


def psp_segments(path):
    """PSP id -> [(score, items)], items: ('T', str) / ('BR',) / ('W', n)"""
    b = open(path, "rb").read()
    found = {}
    for m in re.finditer(rb"\x09([\x30-\x53])(.)", b, re.S):
        sid = (m.group(1)[0] << 4) | m.group(2)[0]
        if not (VOICE_LO - 2 <= sid <= 0x531):
            continue
        i, items = m.end(), []
        try:
            while i < len(b):
                c = b[i]
                if c >= 0x20:
                    j = i
                    while j < len(b) and b[j] >= 0x20:
                        j += 1
                    items.append(("T", b[i:j].decode("latin-1")))
                    i = j
                    continue
                name = NAMES.get(c)
                if name is None or name in SEG_END:
                    break
                if name == "LINE_BREAK":
                    items.append(("BR",))
                elif name == "SET_WAIT":
                    items.append(("W", b[i + 1]))
                if name == "SCRIPT_SWITCH":
                    break
                i += 1 + sum(OPS[name][1])
        except IndexError:
            continue
        text = "".join(x[1] for x in items if x[0] == "T")
        if len(text) < 2 or any(ord(ch) > 0x7E for ch in text):
            continue
        found.setdefault(sid, []).append((en_score(text), items))
    return found


def psp_lines(items):
    """items -> lines: [[(char, [waits before it])...], ...] + trailing waits"""
    lines, cur, pend = [], [], []
    for it in items:
        if it[0] == "T":
            for ch in it[1]:
                cur.append((ch, pend))
                pend = []
        elif it[0] == "BR":
            lines.append(cur)
            cur = []
        else:
            pend = pend + [it[1]]
    lines.append(cur)
    return lines, pend


def wrap(lines, cols=MAX_COLS, max_lines=None):
    """PSP lines -> lines of at most cols (char, waits). Lines that fit
    stay as the PSP broke them; otherwise the whole line is refilled: fewest,
    most even lines, preferring breaks where the PSP had one or after a
    sentence (a wait on a dropped space moves to the next character).
    max_lines (a box that cannot scroll): refilled within that many lines."""
    words = []  # [[(char, waits)], hard break after]
    for ln in lines:
        pend, cur = [], []
        for ch, w in ln:
            if ch == " ":
                if cur:
                    words.append([cur, False])
                    cur = []
                pend = pend + w
            else:
                cur.append((ch, pend + w))
                pend = []
        if cur:
            words.append([cur, False])
        if words:
            words[-1][1] = True
    if not words:
        return [[]]

    def join(ws):
        out = []
        for k, (wd, _) in enumerate(ws):
            if k:
                out.append((" ", []))
            out += wd
        return out
    if all(len(join(w)) <= cols for w in split_hard(words)):
        out = [join(w) for w in split_hard(words)]
        if max_lines is None or len(out) <= max_lines:
            return out
    n = len(words)
    nl = max_lines if max_lines is not None else n
    # best[c][j]: words[:j] in c lines (the cost as before, per line)
    best = [[math.inf] * (n + 1) for _ in range(nl + 1)]
    prev = [[0] * (n + 1) for _ in range(nl + 1)]
    best[0][0] = 0
    for c in range(nl):
        for i in range(n):
            if best[c][i] == math.inf:
                continue
            length = -1
            for j in range(i + 1, n + 1):
                length += 1 + len(words[j - 1][0])
                if length > cols:
                    if j == i + 1:
                        raise ValueError("word longer than a line")
                    break
                cost = 40
                if j == n and i > 0:
                    cost += 2 * max(0, 8 - length) ** 2  # no orphan at the end
                if j < n:
                    cost += (cols - length) ** 2
                    end = words[j - 1][0][-1][0]
                    if words[j - 1][1]:
                        cost -= 30
                    elif end in ".!?":
                        cost -= 15
                    elif end in ",;":
                        cost -= 5
                if best[c][i] + cost < best[c + 1][j]:
                    best[c + 1][j], prev[c + 1][j] = best[c][i] + cost, i
    c = min(range(1, nl + 1), key=lambda k: (best[k][n], k))
    if best[c][n] == math.inf:
        raise ValueError("text does not fit %d lines of %d" % (nl, cols))
    cuts, j = [], n
    while j:
        cuts.append((prev[c][j], j))
        j, c = prev[c][j], c - 1
    return [join(words[a:b]) for a, b in reversed(cuts)]


def split_hard(words):
    out, cur = [], []
    for w in words:
        cur.append(w)
        if w[1]:
            out.append(cur)
            cur = []
    if cur:
        out.append(cur)
    return out


# ---- the transplant ----------------------------------------------------------
def seg_bounds(toks, i):
    j = i + 1
    while j < len(toks) and toks[j][0] not in SEG_END:
        j += 1
    return j


def new_segment(seg, lines, end_waits, speed, break_wait, drop_mid=False):
    """seg: [(index, token)] of the PS1 tokens after PLAY_SOUND up to the
    segment end; returns [(token, index or None)] (None: new)"""
    tpos = [k for k, (_, t) in enumerate(seg) if t[0] == "TEXT"]
    a, b = tpos[0], tpos[-1]
    pre = [(t, n) for n, t in seg[:a] if t[0] not in TEXT_OPS]
    post = [(t, n) for n, t in seg[b + 1:]]
    # ops in the middle of the PS1 text: keep, at the same share of the text
    mid, typed = [], 0
    total = sum(len(t[1]) for _, t in seg if t[0] == "TEXT")
    for n, t in seg[a:b + 1]:
        if t[0] == "TEXT":
            typed += len(t[1])
        elif t[0] not in TEXT_OPS:
            mid.append((typed / total, t, n))
    nchars = [len(ln) for ln in lines]
    cum = [sum(nchars[:k]) / max(1, sum(nchars)) for k in range(len(lines))]
    at = {}
    for f, t, n in mid:
        k = min(range(len(lines)), key=lambda k: abs(cum[k] - f))
        at.setdefault(k, []).append((t, n))
    out = pre + [(("SET_SPEED", speed), None)]
    for k, ln in enumerate(lines):
        if k:
            out.append((("LINE_BREAK",), None))
        out += at.get(k, [])
        if k and break_wait and not (ln and ln[0][1]):
            out.append((("SET_WAIT", break_wait), None))
        buf = ""
        for ch, waits in ln:
            if waits and not drop_mid:
                if buf:
                    out.append((("TEXT", buf), None))
                    buf = ""
                out += [(("SET_WAIT", w), None) for w in waits]
            buf += ch
        if buf:
            out.append((("TEXT", buf), None))
    # the pause after the text: the PSP's if it has one
    if end_waits:
        pw = [k for k, (t, _) in enumerate(post) if t[0] == "SET_WAIT"]
        if pw:
            # (same op, new value: a jump may land on it)
            post[pw[-1]] = (("SET_WAIT", end_waits[-1]), post[pw[-1]][1])
        else:
            post = [(("SET_WAIT", w), None) for w in end_waits] + post
    return out + post


def typing_frames(lines, speed, waits):
    return sum(len(ln) for ln in lines) * (speed + 1) + waits


def pick(psp, sid):
    return max(psp[sid], key=lambda c: c[0])[1]


# ---- JP dub timing -----------------------------------------------------------
# The JP dub plays the Japanese voices over these English copies, which were
# timed for the English lines (Japanese ones run 1.14x longer at the median,
# up to 2.5x). How a box ends (interpreter, e.g. src/st/no3/cutscene.c):
# every voiced line ends its box with NEXT_DIALOG (then CLOSE_DIALOG at the
# scene's end) as soon as its text and pauses are typed; the next line's
# SCRIPT_UNKNOWN_11 (func_80131F68: wait until no XA plays, i.e. until the
# voice's unk228 timer ran out) only holds the NEXT, already cleared box;
# WAIT_FOR_SOUND waits for the start. So the JP copies (jp_adjust) get,
# where the voice outlasts the box, SCRIPT_UNKNOWN_11 + SET_WAIT(JP_TAIL)
# right before the op that ends it, and a slower typing speed where the
# text would finish far ahead of the voice.
JP_DUB_JSON = os.path.join(ROOT, "build/us/jp_dub/dub.json")
JP_TYPE_FRAC = 0.75  # slower typing while it takes less of the voice than this
JP_TYPE_GAP = 15     # ... and still ends this many frames before the voice
JP_SPEED_MAX = 5     # (frames per character = speed + 1)
JP_TAIL = 10         # a beat after the voice before the box goes
JP_MARGIN = 6        # the frame estimate's slack (box_time)
BOX_END = ("NEXT_DIALOG", "CLOSE_DIALOG", "END_CUTSCENE", "HIDE_DIALOG")
INF = 10 ** 9


def jp_durations(extra_ids):
    """voice id -> JP line length in frames (build/us/jp_dub/dub.json)"""
    if not os.path.exists(JP_DUB_JSON):
        return {}
    d = json.load(open(JP_DUB_JSON))
    out = {0x300 + int(c): e["seconds"] * 60 for c, e in d["replace"].items()}
    for e in d.get("extra", []):
        if e["define"] in extra_ids:
            out[extra_ids[e["define"]]] = e["seconds"] * 60
    return out


def voiced_lines(toks, dur):
    """indices of the PLAY_SOUNDs with a duration whose segment has text"""
    return [i for i, t in enumerate(toks) if t[0] == "PLAY_SOUND" and t[1] in dur and
            any(x[0] == "TEXT" for x in toks[i + 1:seg_bounds(toks, i)])]


def speed_at(toks, i):
    sp = 2
    for t in toks[:i]:
        if t[0] == "SET_SPEED":
            sp = t[1]
    return sp


def box_time(toks, i, target, speed, t=0, typed=0, lines=1, seen=None):
    """frames from the start of voice i (its PLAY_SOUND; WAIT_FOR_SOUND = the
    start) to the end of its box: (frames, index of the op ending it, typing
    end); INF frames when a SCRIPT_UNKNOWN_11 holds the box first (a next
    line in the same box, or the JP wait). target(jump arg) -> token index
    or None. Branches: the shortest. Frame costs as the interpreter's: a
    character speed + 1, SET_WAIT n + 1; about: a scroll 8, SET_PORTRAIT 7,
    SET_POS 25; WAIT_FOR_FLAG 0 (event time: never more than that)."""
    seen = set() if seen is None else seen
    k = i + 1
    while k < len(toks):
        tk = toks[k]
        op = tk[0]
        if op == "TEXT":
            t += len(tk[1]) * (speed + 1)
            typed = t
        elif op == "SET_SPEED":
            speed = tk[1]
        elif op == "SET_WAIT":
            t += tk[1] + 1
        elif op == "LINE_BREAK":
            lines += 1
            if lines > 4:
                t += 8
        elif op == "PLAY_SOUND" and tk[1] > 0x300:
            return t, k, typed  # (another voice cuts this one)
        elif op == "SCRIPT_UNKNOWN_11":
            return INF, k, typed
        elif op in BOX_END:
            return t, k, typed
        elif op == "SET_PORTRAIT":
            t += 7
        elif op == "SET_POS":
            t += 25
        elif op == "SCRIPT_UNKNOWN_23":
            t += 1
        elif op in ("SCRIPT_UNKNOWN_15", "SCRIPT_SWITCH"):
            worst = None
            for a in tk[1 if op == "SCRIPT_UNKNOWN_15" else 2:]:
                j = target(a)
                if j is None or j in seen:
                    continue
                r = box_time(toks, j - 1, target, speed, t, typed, lines, seen | {j})
                if worst is None or r[0] < worst[0]:
                    worst = r
            return worst if worst else (t, k, typed)
        k += 1
    return t, k - 1, typed


def offsets_of(toks):
    o, out = 0, []
    for t in toks:
        out.append(o)
        o += tok_size(tuple(a[1] if isinstance(a, (tuple, list)) else a for a in t))
    return out, o


def flag_leads(toks):
    """FLAG_LEAD voice id -> frames from its in-text SET_FLAG to the end of
    its box in these (PS1) tokens"""
    out = {}
    for i, t in enumerate(toks):
        if t[0] != "PLAY_SOUND" or t[1] not in FLAG_LEAD:
            continue
        j = seg_bounds(toks, i)
        fl = [k for k in range(i + 1, j) if toks[k][0] == "SET_FLAG"]
        if len(fl) == 1:
            out[t[1]] = box_time(toks, fl[0], lambda a: None, speed_at(toks, fl[0]))[0]
    return out


def jp_adjust(toks, entries, dur, typing_too=True, leads=None):
    """the JP dub's version of a copy's tokens (jumps inside it as ("S",
    offset in the copy)) and of its entry offsets: (tokens, entries, ids of
    the lines changed). typing_too=False: only the box holds (the PSP copies,
    whose typing transplant() already fits to the English voices)"""
    offs, _ = offsets_of(toks)
    at = {o: k for k, o in enumerate(offs)}
    # jump targets / entries -> ("L", token index)
    toks = [list(t) for t in toks]
    for t in toks:
        for n, a in enumerate(t):
            if n and isinstance(a, (tuple, list)):
                t[n] = ("L", at[a[1]])
    ents = [at[e] for e in entries]

    def target(a):
        return a[1] if isinstance(a, tuple) else None
    changed = set()
    # typing: the voiced segments' SET_SPEEDs, slower while the text would
    # be typed in under JP_TYPE_FRAC of the voice (and still end ahead of it)
    for i in voiced_lines(toks, dur) if typing_too else []:
        v = dur[toks[i][1]]
        # (up to the last character: not the pause after it)
        j = max(k for k in range(i + 1, seg_bounds(toks, i)) if toks[k][0] == "TEXT") + 1
        sps = [k for k in range(i + 1, j) if toks[k][0] == "SET_SPEED"]
        if not sps:
            continue

        def typing(dl):
            sp, f = speed_at(toks, i) + dl, 0
            for k in range(i + 1, j):
                if toks[k][0] == "SET_SPEED":
                    sp = toks[k][1] + dl
                elif toks[k][0] == "TEXT":
                    f += len(toks[k][1]) * (sp + 1)
                elif toks[k][0] == "SET_WAIT":
                    f += toks[k][1] + 1
            return f
        dl = 0
        while (typing(dl) < JP_TYPE_FRAC * v and
               max(toks[k][1] for k in sps) + dl < JP_SPEED_MAX and
               typing(dl + 1) <= v - JP_TYPE_GAP):
            dl += 1
        if dl:
            for k in sps:
                toks[k][1] += dl
            changed.add(toks[i][1])
    # the box: held until the voice ends (simulated again after each insert:
    # through a jump a line may have more than one end)
    for _ in range(1000):
        tt = [tuple(t) for t in toks]
        short = None
        for i in voiced_lines(tt, dur):
            up, end, _ = box_time(tt, i, target, speed_at(tt, i))
            if up < dur[tt[i][1]] + JP_MARGIN and tt[end][0] in BOX_END + ("PLAY_SOUND",):
                short = (i, end)
                break
        if short is None:
            break
        i, end = short
        new = [["SCRIPT_UNKNOWN_11"], ["SET_WAIT", JP_TAIL]]
        if tt[i][1] in SPEECH_END or tt[i][1] in (leads or {}):
            # (the clip outlasts the speech: wait for the speech only)
            up = box_time(tt, i, target, speed_at(tt, i))[0]
            need = int(dur[tt[i][1]] + JP_MARGIN - up) + JP_TAIL
            new = [["SET_WAIT", min(255, max(0, need - 1))]]
        toks[end:end] = new
        m = len(new)
        # (a jump to the op now lands on the wait)
        for t in toks:
            for n, a in enumerate(t):
                if n and isinstance(a, tuple) and a[1] > end:
                    t[n] = ("L", a[1] + m)
        ents = [e + m if e > end else e for e in ents]
        changed.add(tt[i][1])
    else:
        sys.exit("psp_script: jp_adjust does not converge")

    def move(at, old, new):
        # toks[at:at + old] -> new; jumps / entries after it follow
        toks[at:at + old] = new
        m = len(new) - old
        for t in toks:
            for n, a in enumerate(t):
                if n and isinstance(a, tuple) and a[1] > at:
                    t[n] = ("L", max(at, a[1] + m))
        ents[:] = [max(at, e + m) if e > at else e for e in ents]
    # the in-text flag of a FLAG_LEAD line: leads[id] frames before its box
    # ends, as on the PS1
    for vid, lead in (leads or {}).items():
        for i in [k for k, t in enumerate(toks) if t[0] == "PLAY_SOUND" and t[1] == vid]:
            end = i + 1
            while toks[end][0] not in BOX_END:
                end += 1
            fl = [k for k in range(i + 1, end) if toks[k][0] == "SET_FLAG"]
            if len(fl) != 1:
                continue
            flag = list(toks[fl[0]])
            move(fl[0], 1, [])
            end -= 1
            t, k = 0, end
            while k > i + 1 and t < lead:
                k -= 1
                op = toks[k]
                if op[0] == "SET_WAIT" and t + op[1] + 1 > lead:
                    b = lead - t - 1  # the flag, then b + 1 frames of this wait
                    a = op[1] - b - 1
                    move(k, 1, ([["SET_WAIT", a]] if a >= 0 else []) +
                         [flag, ["SET_WAIT", b]])
                    break
                if op[0] == "SET_WAIT":
                    t += op[1] + 1
                elif op[0] == "TEXT":
                    t += len(op[1]) * (speed_at(toks, k) + 1)
            else:
                move(k, 0, [flag])
            changed.add(vid)
    plain = [tuple(t) for t in toks]
    offs, _ = offsets_of([tuple(0 if isinstance(a, tuple) else a for a in t) for t in plain])
    out = [tuple(("S", offs[a[1]]) if isinstance(a, tuple) else a for a in t) for t in plain]
    return out, [offs[e] for e in ents], changed


def offset_target(toks):
    """box_time's target() for tokens with ("S", offset in the copy) jumps"""
    at = {o: k for k, o in enumerate(offsets_of(toks)[0])}
    return lambda a: at.get(a[1]) if isinstance(a, (tuple, list)) else None


def jp_lines(toks, dur, target):
    """[(voice id, JP voice frames, box frames (INF: held), typing frames)]"""
    out = []
    for i in voiced_lines(toks, dur):
        up, _, typed = box_time(toks, i, target, speed_at(toks, i))
        out.append((toks[i][1], dur[toks[i][1]], up, typed))
    return out


def transplant(part, psp, dub, report, ovl=None):
    """plan for one script part: [("tok", index) | ("seg", ...) | ("app", ...)],
    replaced ids, ids with no PSP text"""
    plan, replaced, missing = [], [], []
    cols, max_lines = box_limits(ovl)
    i = 0
    while i < len(part):
        t = part[i]
        if t[0] != "PLAY_SOUND" or not (VOICE_LO <= t[1] <= VOICE_HI):
            plan.append(("tok", i))
            i += 1
            continue
        vid = t[1]
        j = seg_bounds(part, i)
        seg = [(n, part[n]) for n in range(i + 1, j)]
        if vid in NO_PSP:
            plan += [("tok", n) for n in range(i, j)]
            i = j
            continue
        if not any(x[0] == "TEXT" for _, x in seg):
            plan += [("tok", n) for n in range(i, j)]  # a voice with no text
            i = j
            continue
        sid = TEXT_OVERRIDE.get(vid, vid - 2)
        if sid not in psp:
            missing.append(vid)
            plan += [("tok", n) for n in range(i, j)]
            i = j
            continue
        lines, end_waits = psp_lines(pick(psp, sid))
        lines = wrap(lines, cols, max_lines)
        # speed: the PS1 line's, by characters typed
        sp, cnt = 2, Counter()
        for _, x in seg:
            if x[0] == "SET_SPEED":
                sp = x[1]
            elif x[0] == "TEXT":
                cnt[sp] += len(x[1])
        speed = cnt.most_common(1)[0][0]
        mid_waits = sum(w for ln in lines for _, ws in ln for w in ws)
        voice = dub.get(vid)
        if voice:
            while speed > 1 and typing_frames(lines, speed, mid_waits) > voice + 30:
                speed -= 1
        plan.append(("tok", i))
        plan.append(("seg", seg, lines, end_waits, speed))
        replaced.append(vid)
        report.append("  %X speed %d typing %.1fs voice %s | PS1 %r | %s" % (
            vid, speed, typing_frames(lines, speed, mid_waits) / 60,
            "%.1fs" % (voice / 60) if voice else "-",
            "".join(x[1] for _, x in seg if x[0] == "TEXT")[:24],
            " / ".join("".join(c for c, _ in ln) for ln in lines)))
        if vid in APPEND:
            psp_id, define, who = APPEND[vid]
            l2, w2 = psp_lines(pick(psp, psp_id))
            l2 = wrap(l2, cols)
            plan.append(("app", l2, speed, define, who))
            report.append("  + %s speed %d | %s" % (define, speed, " / ".join(
                "".join(c for c, _ in ln) for ln in l2)))
        i = j
    return plan, replaced, missing


# shrink levels, until the script fits its retail bytes: (wait after a line
# break, drop repeated SET_SPEED, drop the PSP's pauses inside the text)
LEVELS = [(4, False, False), (0, False, False), (0, True, False), (0, True, True)]


def render(part, plan, level, extra_ids):
    """[(token, original index or None)]"""
    break_wait, elide, drop_mid = LEVELS[level]
    out, into = [], None
    for p in plan:
        if p[0] == "tok":
            t = part[p[1]]
            if into and t[0] == "TEXT" and t[1] == into[0]:
                # (the voice before the text; a jump to it lands on it)
                out += [(into[1][0], p[1])] + [(x, None) for x in into[1][1:]]
                into = None
                continue
            out.append((t, p[1]))
        elif p[0] == "seg":
            out += new_segment(p[1], p[2], p[3], p[4], break_wait, drop_mid)
        else:
            # wait for the line's voice, play the new one: the same box's
            # next line (before the segment's closing pause), or in place of
            # the next box's unvoiced text
            _, lines, speed, define, where = p
            seg = [("PLAY_SOUND", extra_ids[define]), ("WAIT_FOR_SOUND",),
                   ("SET_SPEED", speed)]
            for n, ln in enumerate(lines):
                if n:
                    seg.append(("LINE_BREAK",))
                seg.append(("TEXT", "".join(c for c, _ in ln)))
            if where:
                into = (where, seg)
                continue
            k = len(out)
            while k and out[k - 1][0][0] == "SET_WAIT":
                k -= 1
            seg = [("LINE_BREAK",), ("SCRIPT_UNKNOWN_11",)] + seg
            out[k:k] = [(t, None) for t in seg]
    # the typing speed stays set across lines: drop a new SET_SPEED that
    # repeats the one before it (only in scripts without jumps, where the
    # order of the lines is the order of the bytes)
    if elide and not any(t[0] in ("SCRIPT_SWITCH", "SCRIPT_UNKNOWN_15") for t in part):
        cur, kept = None, []
        for t, idx in out:
            if t[0] == "SET_SPEED":
                if idx is None and t[1] == cur:
                    continue
                cur = t[1]
            kept.append((t, idx))
        out = kept
    return out


def check_widths(toks, where):
    """where = the overlay: its BOX_LIMITS (columns; lines since SET_PORTRAIT)"""
    cols, max_lines = box_limits(where)
    line = 0
    nlines = 1
    for t in toks:
        if t[0] == "TEXT":
            line += len(t[1])
            if line > cols:
                sys.exit("psp_script: %s: line over %d: %r" % (where, cols, t[1]))
        elif t[0] in ("LINE_BREAK", "NEXT_DIALOG", "CLOSE_DIALOG", "SCRIPT_SWITCH",
                      "SCRIPT_UNKNOWN_15"):
            line = 0  # (after a jump the bytes are another branch)
        if t[0] == "LINE_BREAK":
            nlines += 1
            if max_lines is not None and nlines > max_lines:
                sys.exit("psp_script: %s: box over %d lines" % (where, max_lines))
        elif t[0] in ("SET_PORTRAIT", "SCRIPT_SWITCH", "SCRIPT_UNKNOWN_15"):
            nlines = 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--off", action="store_true")
    ap.add_argument("--check", action="store_true", help="--apply, but only check")
    args = ap.parse_args()
    if args.apply or args.off or args.check:
        return apply(args.off, args.check)
    os.makedirs(WORK, exist_ok=True)
    dub = {}
    if os.path.exists(DUB_JSON):
        d = json.load(open(DUB_JSON))
        dub = {int(k): v for k, v in d.get("durations", {}).items()}
    for vid, (sec, _) in SPEECH_END.items():
        if vid in dub:
            dub[vid] = min(dub[vid], int(sec * 60))
    # the new lines' ids: after the retail and Saturn music configs (as
    # repack_xa.py appends psp_dub.py's EXTRA)
    src = open(os.path.join(ROOT, "src/dra/mod_xa_music.h")).read()
    n_retail = int(re.search(r"#define MOD_XA_RETAIL_CONFIGS (\d+)", src).group(1))
    n_mod = len(re.findall(r"\{\d+, \d+, \d+, \d+, \d+, \d+, \d+\},", src))
    n_mod -= len(re.findall(r"    // PSP dub ", src))  # a rerun on a dubbed header
    extra_ids = {define: 0x300 + n_retail + n_mod + k
                 for k, (_, (_, define, _)) in enumerate(sorted(APPEND.items()))}
    jdur = jp_durations(extra_ids)
    for vid, (_, sec) in SPEECH_END.items():
        if vid in jdur:
            jdur[vid] = min(jdur[vid], sec * 60)
    report, patches, keep, edits, spills = [], [], set(), {}, {}
    maxcol = 0
    for ovl, (retail_rel, mapname, scripts) in OVERLAYS.items():
        retail = open(os.path.join(ROOT, "disks/us", retail_rel), "rb").read()
        psp = psp_segments(os.path.join(PSP_BIN, ovl + ".bin"))
        for sc in scripts:
            if "c" in sc:
                toks = read_c_array(sc["c"], sc["sym"])
                blob = assemble(toks)
                off = None
                label = sc["sym"]
                # (no jumps and no retail offset: the copy may differ in size)
                toks_jp = toks
                toks = drop_lines(toks, NO_PSP, label)
            else:
                toks, off = retail_script(retail, sc, ovl)
                blob = assemble(toks)
                label = os.path.basename(sc["yaml"]) if "yaml" in sc else "0x%X" % sc["addr"]
            line = 0
            for t in toks:
                if t[0] == "TEXT":
                    line += len(t[1])
                    maxcol = max(maxcol, line)
                elif t[0] in ("LINE_BREAK", "NEXT_DIALOG", "CLOSE_DIALOG"):
                    line = 0
            # parts: split at the C code's entry offsets
            cuts, o, parts, cur = sc.get("split", []), 0, [], []
            for t in toks:
                if o in cuts and cur:
                    parts.append(cur)
                    cur = []
                assert not any(o < c < o + tok_size(t) for c in cuts), (ovl, hex(o))
                cur.append(t)
                o += tok_size(t)
            parts.append(cur)
            assert len(parts) == len(cuts) + 1, (ovl, "entry offsets")
            voiced = [t[1] for t in toks if t[0] == "PLAY_SOUND" and VOICE_LO <= t[1] <= VOICE_HI]
            report.append("%s %s @%s (%d bytes, %d parts) voices %s" % (
                ovl, label, "0x%X" % off if off is not None else "C", len(blob), len(parts),
                " ".join("%X" % v for v in voiced)))
            rendered, why = [], ""
            for part in parts:
                plan, rep_, miss = transplant(part, psp, dub, report, ovl)
                if miss:
                    why = "no PSP text for " + " ".join("%X" % v for v in miss)
                    break
                rendered.append((part, render(part, plan, 0, extra_ids)))
            if why:
                report.append("  PS1 only: " + why)
                keep.update(voiced)
                continue
            # the copy: its parts in a row; jump targets inside the script
            # move with the token they hit, ("S", offset in the copy)
            old_map, starts, o, part_off = {}, [], 0, 0
            for part, newt in rendered:
                old_at, q = [], 0
                for t in part:
                    old_at.append(q)
                    q += tok_size(t)
                starts.append((part_off, o))
                for t, idx in newt:
                    if idx is not None and off is not None:
                        old_map[BASE + off + part_off + old_at[idx]] = ("S", o)
                    o += tok_size(t)
                part_off += q
            lo = BASE + off if off is not None else 0
            hi = lo + len(blob) if off is not None else 0
            fixed = []
            for part, newt in rendered:
                for t, _ in newt:
                    if t[0] in ("SCRIPT_SWITCH", "SCRIPT_UNKNOWN_15"):
                        assert off is not None, (ovl, label, "jump in a C array script")
                        head = 2 if t[0] == "SCRIPT_SWITCH" else 1
                        tg = []
                        for a_ in t[head:]:
                            # (a jump out of the copy would play retail text)
                            assert lo <= a_ < hi, (ovl, label, "jump out of the script", hex(a_))
                            assert a_ in old_map, (ovl, "jump into replaced text", hex(a_))
                            a_ = old_map[a_]
                            tg.append(a_)
                        t = tuple(list(t[:head]) + tg)
                    fixed.append(t)
            # boxes held until the English voice ends (as the JP copies)
            leads = flag_leads(toks)
            fixed, ents, held = jp_adjust(fixed, [so for _, so in starts], dub, False, leads)
            starts = [(po, so) for (po, _), so in zip(starts, ents)]
            check_widths(fixed, ovl)
            n = len(assemble(resolve(fixed, BASE)))
            sp = spills.setdefault(ovl, {"size": 0, "scripts": []})
            sp["scripts"].append({
                "off": off, "sym": sc.get("sym"), "at": sp["size"], "tokens": fixed,
                "old": blob.hex(), "entries": [[po, sp["size"] + so] for po, so in starts]})
            sp["size"] += n
            report.append("  PSP copy: %d bytes at +%d (PS1 %d bytes), %d boxes held%s" % (
                n, sp["size"] - n, len(blob), len(held),
                ", entries " + " ".join("+0x%X" % po for po, _ in starts) if len(starts) > 1 else ""))
            # the JP dub's copy: boxes held for the Japanese voices, slower
            # typing (jp_adjust); CEN's C arrays: the cut NO_PSP line back
            # (its PS1 text)
            base_jp, keeps = fixed, ""
            if "c" in sc and toks_jp != toks:
                plan, _, miss = transplant(toks_jp, psp, dub, [], ovl)
                assert not miss, (ovl, label, miss)
                base_jp = [t for t, _ in render(toks_jp, plan, 0, extra_ids)]
                keeps = ", keeps " + " ".join("%X" % v for v in sorted(NO_PSP))
            jt, jents, jchanged = jp_adjust(base_jp, [so for _, so in starts], jdur, True, leads)
            if jchanged or keeps:
                check_widths(jt, ovl)
                rec = sp["scripts"][-1]
                rec["jp_tokens"] = jt
                jn = len(assemble(resolve(jt, BASE)))
                if ovl not in C_ARRAYS:
                    js = sp.get("jp_size", 0)
                    rec["jp_at"] = js
                    rec["jp_entries"] = [[po, js + so] for (po, _), so in zip(starts, jents)]
                    sp["jp_size"] = js + jn
                report.append("  JP copy: %d bytes, %d lines retimed%s" % (jn, len(jchanged), keeps))
                for vid, v, up, typed in jp_lines(jt, jdur, offset_target(jt)):
                    report.append("    JP %X voice %.1fs typing %.1fs box %s%s" % (
                        vid, v / 60, typed / 60, "held" if up >= INF else "%.1fs" % (up / 60),
                        " *" if vid in jchanged else ""))
    for ovl, sp in spills.items():
        if ovl not in C_ARRAYS:
            patches.append({"ovl": ovl, "bin": "build/us/%s.BIN" % ovl.upper(),
                            "map": "build/us/%s.map" % OVERLAYS[ovl][1], "spill": sp})
    keep |= NO_PSP
    write_spill_headers(spills)
    json.dump(patches, open(PATCHES, "w"), indent=0)
    json.dump({}, open(EDITS, "w"))
    json.dump(sorted(keep), open(KEEP_JSON, "w"))
    json.dump(extra_ids, open(os.path.join(WORK, "extra_ids.json"), "w"))
    report.insert(0, "retail longest line %d chars; limit %d" % (maxcol, MAX_COLS))
    open(REPORT, "w").write("\n".join(report) + "\n")
    print("psp script: %d scripts with a PSP copy in %d overlays, %d voice ids "
          "without a PSP version (%s)" % (
              sum(len(sp["scripts"]) for sp in spills.values()), len(spills), len(keep),
              os.path.relpath(REPORT, ROOT)))
    for line in report:
        if "PS1 only" in line:
            print(line)


def resolve(toks, base):
    """("S", offset) jump targets -> addresses in a spill array at base"""
    out = []
    for t in toks:
        t = tuple(base + a[1] if isinstance(a, (tuple, list)) else a for a in t)
        out.append(t)
    return out


HDR_FN_JP = """static u8* ModPspScript(u8* script) {
    u32 i;

    if (MOD_JP_DUB_ON()) {
        for (i = 0; i < sizeof(g_ModJpMap) / sizeof(g_ModJpMap[0]); i++) {
            if (script == g_ModJpMap[i][0]) {
                return g_ModJpMap[i][1];
            }
        }
    }
    if (MOD_PSP_DUB_ON()) {
        for (i = 0; i < sizeof(g_ModPspMap) / sizeof(g_ModPspMap[0]); i++) {
            if (script == g_ModPspMap[i][0]) {
                return g_ModPspMap[i][1];
            }
        }
    }
    return script;
}
"""
HDR_FN = """static u8* ModPspScript(u8* script) {
    u32 i;

    if (MOD_PSP_DUB_ON()) {
        for (i = 0; i < sizeof(g_ModPspMap) / sizeof(g_ModPspMap[0]); i++) {
            if (script == g_ModPspMap[i][0]) {
                return g_ModPspMap[i][1];
            }
        }
    }
    return script;
}
"""


def write_spill_headers(spills):
    for ovl, c in SPILL_C.items():
        path = os.path.join(ROOT, os.path.dirname(c), "mod_psp_script.h")
        s = ("// Generated by mods/sotn-ultimate/tools/psp_script.py, do not edit\n"
             "// PSP dub toggle: the PSP English copies of this overlay's scripts;\n"
             "// SetCutsceneScript plays them while MOD_PSP_DUB_ON() (include/game.h)\n")
        sp = spills.get(ovl)
        if sp and ovl in C_ARRAYS:
            s += "#define MOD_PSP_SCRIPT\n"
            rows = []
            for k, sc in enumerate(sp["scripts"]):
                code = assemble(sc["tokens"])
                s += "extern u8 %s[];\n" % sc["sym"]
                s += "u8 g_ModPspCopy%d[] __attribute__((section(\".rodata\"))) = {\n" % k
                for i in range(0, len(code), 16):
                    s += "    " + ", ".join("0x%02X" % x for x in code[i:i + 16]) + ",\n"
                s += "};\n"
                rows.append("    {%s, g_ModPspCopy%d}," % (sc["sym"], k))
            jrows = []
            for k, sc in enumerate(sp["scripts"]):
                if not sc.get("jp_tokens"):
                    continue
                code = assemble(sc["jp_tokens"])
                s += "u8 g_ModJpCopy%d[] __attribute__((section(\".rodata\"))) = {\n" % k
                for i in range(0, len(code), 16):
                    s += "    " + ", ".join("0x%02X" % x for x in code[i:i + 16]) + ",\n"
                s += "};\n"
                jrows.append("    {%s, g_ModJpCopy%d}," % (sc["sym"], k))
            s += "static u8* const g_ModPspMap[][2] = {\n" + "\n".join(rows) + "\n};\n"
            if jrows:
                s += ("// the JP dub's copies (boxes held for the Japanese voices; a NO_PSP\n"
                      "// line kept)\n"
                      "static u8* const g_ModJpMap[][2] = {\n" + "\n".join(jrows) +
                      "\n};\n" + HDR_FN_JP)
            else:
                s += HDR_FN
        elif sp:
            s += ("// (g_ModPspSpill is filled in the built overlay by --apply)\n"
                  "#define MOD_PSP_SCRIPT\n"
                  "u8 g_ModPspSpill[%d] __attribute__((section(\".rodata\"))) = {0};\n"
                  "static u8* const g_ModPspMap[][2] = {\n" % sp["size"])
            for sc in sp["scripts"]:
                for po, so in sc["entries"]:
                    s += "    {(u8*)0x%08X, g_ModPspSpill + %d},\n" % (BASE + sc["off"] + po, so)
            s += "};\n"
            if sp.get("jp_size"):
                s += ("// the JP dub's copies (boxes held for the Japanese voices; filled by\n"
                      "// --apply too)\n"
                      "u8 g_ModJpSpill[%d] __attribute__((section(\".rodata\"))) = {0};\n"
                      "static u8* const g_ModJpMap[][2] = {\n" % sp["jp_size"])
                for sc in sp["scripts"]:
                    for po, so in sc.get("jp_entries", []):
                        s += "    {(u8*)0x%08X, g_ModJpSpill + %d},\n" % (BASE + sc["off"] + po, so)
                s += "};\n" + HDR_FN_JP
            else:
                s += HDR_FN
        if sp:
            s = s.replace("#define MOD_PSP_SCRIPT\n",
                          "#if defined(MOD_SATURN) && defined(VERSION_US)\n"
                          "#define MOD_PSP_SCRIPT\n", 1) + "#endif\n"
        if not os.path.exists(path) or open(path).read() != s:
            open(path, "w").write(s)


def disassemble_all(b):
    toks, i = [], 0
    while i < len(b):
        t, i2 = disassemble_one(b, i)
        toks.append(t)
        i = i2
    return toks


def disassemble_one(b, i):
    c = b[i]
    if c >= 0x20:
        j = i
        while j < len(b) and b[j] >= 0x20:
            j += 1
        return ("TEXT", b[i:j].decode("latin-1")), j
    name = NAMES[c]
    if name == "SCRIPT_SWITCH":
        # var + cases: as many words as up to the next op we know; the
        # scripts here have 2 cases (checked by the caller's round trip)
        return (name,), i + 1 + 12
    return (name,), i + 1 + sum(OPS[name][1])


def apply(off, check=False):
    if off:
        write_spill_headers({})
        for f in (PATCHES, EDITS):
            if os.path.exists(f):
                os.remove(f)
        json.dump([], open(KEEP_JSON, "w"))
        print("psp script: no PSP copies (retail scripts only)")
        return
    if not os.path.exists(PATCHES):
        sys.exit("psp_script: run it without --apply first")
    patches = json.load(open(PATCHES))
    # BO6RIC2.BIN is a copy of BO6.BIN (mods/sotn-ultimate/tools/build_bo6ric2.py): check it too
    patches += [dict(p, bin="build/us/BO6RIC2.BIN") for p in patches
                if check and p["ovl"] == "bo6"]
    count, jcount, touched = 0, 0, []
    for p in patches:
        path = os.path.join(ROOT, p["bin"])
        if not os.path.exists(path):
            continue
        b = bytearray(open(path, "rb").read())
        m = re.search(r"0x([0-9a-f]+)\s+g_ModPspSpill\n", open(os.path.join(ROOT, p["map"])).read())
        assert m, "%s: g_ModPspSpill not linked (mod_psp_script.h?)" % p["map"]
        mj = re.search(r"0x([0-9a-f]+)\s+g_ModJpSpill\n", open(os.path.join(ROOT, p["map"])).read())
        assert mj or not p["spill"].get("jp_size"), "%s: g_ModJpSpill not linked" % p["map"]
        changed = False
        for sc in p["spill"]["scripts"]:
            if sc.get("jp_tokens"):
                # the JP dub's copy
                addr = int(mj[1], 16) + sc["jp_at"]
                toks = [tuple(tuple(a) if isinstance(a, list) else a for a in t)
                        for t in sc["jp_tokens"]]
                code = assemble(resolve(toks, addr))
                assert sc["jp_at"] + len(code) <= p["spill"]["jp_size"]
                o = addr - BASE
                if bytes(b[o:o + len(code)]) != code:
                    if check:
                        sys.exit("psp_script: %s at 0x%X: the JP copy is not filled" % (p["bin"], o))
                    b[o:o + len(code)] = code
                    changed = True
                jcount += 1
            old = bytes.fromhex(sc["old"])
            if bytes(b[sc["off"]:sc["off"] + len(old)]) != old:
                sys.exit("psp_script: %s at 0x%X: the retail script changed" % (p["bin"], sc["off"]))
            addr = int(m[1], 16) + sc["at"]
            toks = [tuple(tuple(a) if isinstance(a, list) else a for a in t) for t in sc["tokens"]]
            code = assemble(resolve(toks, addr))
            assert sc["at"] + len(code) <= p["spill"]["size"]
            o = addr - BASE
            if bytes(b[o:o + len(code)]) != code:
                if check:
                    sys.exit("psp_script: %s at 0x%X: the PSP copy is not filled" % (p["bin"], o))
                b[o:o + len(code)] = code
                changed = True
            count += 1
        if changed:
            open(path, "wb").write(b)
            touched.append(p["bin"])
    if check:
        # the new lines' ids in the scripts = the ones repack_xa.py gave them
        src = open(os.path.join(ROOT, "src/dra/mod_xa_music.h")).read()
        for define, vid in json.load(open(os.path.join(WORK, "extra_ids.json"))).items():
            m = re.search(r"#define %s (0x[0-9A-F]+)" % define, src)
            if not m or int(m.group(1), 16) != vid:
                sys.exit("psp_script: %s is not 0x%X in mod_xa_music.h" % (define, vid))
        print("psp script: %d PSP copies, %d JP copies in the built overlays" % (count, jcount))
    else:
        print("psp script: %d PSP copies, %d JP copies filled (%s)" % (
            count, jcount, " ".join(sorted(set(touched))) or "already"))


if __name__ == "__main__":
    main()
