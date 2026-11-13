#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the PSP port's menu text changes the user picked
(2026-10-24). DRA's tables and strings stay exactly retail (DRA has no room):
everything is handed to DRA by SEL, which every way into a game passes
through, in g_ModDub (DRA's dead 4 KB buffers, BSS: no DRA file bytes).

FIXED, the same with every dub (Natto, "[2 handed]", "Restores HP", the
Silver Ring's "...in...Room", the Medal's "ATT+1", the Faerie Scroll's stray
space, the enemy name fixes): SEL builds each new string in
g_ModDub.fixedText and points DRA's table entry at it (src/st/sel/2C048.c
ModDubTextCopy). SEL has little room, so a description change is kept as an
edit of DRA's retail string (at byte pos: drop del bytes, insert these) and
only names are stored whole.

FOLLOWS THE DUB ROW (PSP text with the PSP and JP dubs, the PS1's with the
PS1 dub): every PSP string fits in the bytes of the PS1 string it replaces,
so DRA keeps one copy and writes the chosen bytes over it: SEL leaves both
versions and each string's DRA address in g_ModDub.text; DRA's
ModApplyDubText (src/dra/game_handlers.c) writes the chosen one every frame
of play, so a new game, a loaded save and the JP dub all show the right one.

Writes src/st/sel/mod_dub_text.h, and build/us/menu_text.json for
mods/sotn-ultimate/tools/check_menu_text.py (the retail bytes each entry replaces, checked
against the built DRA).

Names are stored in the game's _S encoding (each character - 0x20, a space
is 0, then 0xFF 0); descriptions as raw bytes, punctuation as Shift-JIS
pairs."""
import json
import os
import re

from mod_work import read_src

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
GEN = os.path.join(ROOT, "src/dra/gen/us")
OUT_SEL = os.path.join(ROOT, "src/st/sel/mod_dub_text.h")
OUT_JSON = os.path.join(ROOT, "build/us/menu_text.json")

# tables: 0 g_EquipDefs (0x34 per record), 1 g_AccessoryDefs (0x20),
# 2 g_EnemyDefs (0x28), 3 g_RelicDefs (0x10), 4 g_MenuStr (4)
STRIDE = {0: 0x34, 1: 0x20, 2: 0x28, 3: 0x10, 4: 4}
HEADER = {0: "equipment.h", 1: "accessories.h", 2: "enemydefs.h"}

# --- fixed: (table, index, field offset, change) -------------------------
# change: a new name, or [(old bytes, new bytes)] in the retail description
HAND2 = [(b" hand\x81\x6E", b" handed\x81\x6E")]  # "[2 hand]" -> "[2 handed]"
FIXED = [
    (0, 0x03B, 0, "Natto"),
    *[(0, k, 4, HAND2) for k in (0x15, 0x1A, 0x1B, 0x54, 0x5F, 0x64, 0x65, 0x8B, 0x8D)],
    (1, 0x2A, 4, [(b"Heals", b"Restores")]),
    (1, 0x49, 4, [(b"Tower", b"Room")]),
    (1, 0x52, 4, [(b"ATT\x20\x81\x7B", b"ATT\x81\x7B")]),
    (3, 15, 4, [(b"name ", b"name")]),  # Faerie Scroll: the stray space
    (2, 0x059, 0, "Medusa Head"),  # unused record (no stage uses it)
    (2, 0x08E, 0, "Orobourous"),
    (2, 0x08F, 0, "Orobourous Rider"),  # 16: fits BottomCornerText; not listed
    (2, 0x108, 0, "Tin Man"),
    (2, 0x10F, 0, "Cloaked Knight"),
    (2, 0x11D, 0, "Balloon Pod"),
    (2, 0x126, 0, "Scylla Wyrm"),
    (2, 0x144, 0, "Beelzebub"),
    (2, 0x16B, 0, "Cerberus"),
]
# The cut Outer Wall skull (record 0xB1), placed only when include/game.h
# defines MOD_OUTER_WALL_GHOST: retail leaves its name empty, so the Faerie
# Scroll would show an empty box when it is hit. The PSP port names it
# "Ghost", but retail already has a Ghost (0x09C, the Library one), so the
# mod calls it "Ghost Skull" (Faerie Scroll + its Library entry No.165,
# mods/sotn-ultimate/tools/gen_saturn_bestiary.py).
if re.search(r"^#define MOD_OUTER_WALL_GHOST *$",
             read_src("include/game.h"), re.M):
    FIXED.append((2, 0x0B1, 0, "Ghost Skull"))

# --- follows the dub row: (table, index, field, PS1 text, PSP text, kind) --
# kind: "s" _S name, "c" plain C string
DUB = [
    (0, 0x000, 0, "Empty hand", "Unarmed", "s"),
    (1, 0x00, 0, "----", "none", "s"),  # one string for the 4 empty slots
    (2, 0x0CB, 0, "Minotaurus", "Minotaur", "s"),
    (3, 23, 0, "Sprite Card", "Fairy Card", "c"),
    (3, 23, 4, "Summons ””Pixie”” familiar", "Summons ””Fairy”” familiar", "c"),
    (3, 24, 0, " Nosedevil Card", " N Demon Card", "c"),
    (3, 24, 4, "Summons ””Nose−devil”” familiar", "Summons ””N Demon”” familiar", "c"),
    (4, 51, 0, "Sprite", "Fairy", "s"),
    (4, 52, 0, "N.Devil", "N Demon", "s"),
]
SJIS = {"”": b"\x81\x68", "−": b"\x81\x7C", "’": b"\x81\x66"}
RELIC_DESC = {15: "Displays enemy’s name "}  # retail text of the relic fixes


def s_bytes(text):
    """the _S encoding: each character - 0x20 (a space is 0), then 0xFF 0 (a
    name ends at 0xFF followed by 0: src/st/popup.h BottomCornerText)"""
    assert all(0x20 <= ord(c) < 0x80 for c in text), text
    return bytes(ord(c) - 0x20 for c in text) + b"\xff\0"


def c_bytes(text):
    return b"".join(SJIS[c] if c in SJIS else c.encode("ascii") for c in text) + b"\0"


def retail(table, idx, field):
    """the retail bytes of a name / description (gen header or relic text)"""
    if table == 3:
        return c_bytes(RELIC_DESC[idx])
    src = open(os.path.join(GEN, HEADER[table]), encoding="latin-1").read()
    m = re.search(r'^/\* 0x0*%X \*/ \{_S\("((?:[^"\\]|\\.)*)"\)(?:, "((?:\\x[0-9A-F]{2})*)")?' % idx,
                  src, re.M)
    assert m, (table, hex(idx))
    if field == 0:
        return s_bytes(m.group(1))
    return bytes.fromhex(m.group(2).replace("\\x", "")) + b"\0"


def main():
    check = {"fixed": [], "dub": []}
    fixed_rows, fixed_blob = [], b""
    for table, idx, field, change in FIXED:
        old = retail(table, idx, field)
        if isinstance(change, str):
            if table == 2:
                assert len(change) <= (16 if idx == 0x08F else 15), change
            new = s_bytes(change)
            pos, dele, ins = 0, 0xFF, new  # stored whole
            note = change
        else:
            (a, b), = change
            assert old.count(a) == 1, (table, hex(idx), a, old)
            k = old.index(a)
            # the shortest edit: the bytes a and b share at their ends stay
            pre = 0
            while pre < min(len(a), len(b)) and a[pre] == b[pre]:
                pre += 1
            suf = 0
            while suf < min(len(a), len(b)) - pre and a[-1 - suf] == b[-1 - suf]:
                suf += 1
            pos, dele, ins = k + pre, len(a) - pre - suf, b[pre:len(b) - suf]
            new = old[:pos] + ins + old[pos + dele:]
            assert new == old.replace(a, b) and pos < 256 and dele < 255, (hex(idx), pos, dele)
            note = "%s: %r" % (hex(idx), b.decode("latin-1"))
        room = len(new) + (-len(new) % 4)  # each string word-aligned
        off = idx * STRIDE[table] + field
        fixed_rows.append((table, pos, off, dele, len(ins), room, note))
        fixed_blob += ins
        check["fixed"].append([table, off, old.hex()])
    dub_rows, dub_blob = [], b""
    for table, idx, field, ps1, psp, kind in DUB:
        enc = s_bytes if kind == "s" else c_bytes
        a, b = enc(ps1), enc(psp)
        assert len(b) <= len(a), (ps1, psp)
        b += bytes(len(a) - len(b))  # zeros: a name ends at 0xFF then 0
        # only the span where they differ is kept and written (SEL has
        # little room); its start goes in the table byte's bits 3-7
        diff = [k for k in range(len(a)) if a[k] != b[k]]
        lo, hi = diff[0], diff[-1] + 1
        assert lo < 32, (ps1, lo)
        off = idx * STRIDE[table] + field
        dub_rows.append((table | lo << 3, off, hi - lo, ps1 + " -> " + psp))
        dub_blob += a[lo:hi] + b[lo:hi]
        check["dub"].append([table, off, a.hex()])

    out = ["// Generated by mods/sotn-ultimate/tools/gen_menu_text.py, do not edit",
           "// The PSP port's menu text: per string, the DRA table, the byte",
           "// offset of its pointer in that table, its length in the bytes below.",
           "// Fixed (every dub): {table, pos, pointer offset, bytes dropped at",
           "// pos (0xFF: the whole string is the inserted bytes), bytes inserted,",
           "// room in g_ModDub.fixedText}; the inserted bytes follow, in order.",
           "#define MOD_FIXED_TEXT_COUNT %d" % len(fixed_rows),
           "#define MOD_FIXED_TEXT_SIZE %d // room needed" % sum(r[5] for r in fixed_rows),
           "static const ModFixedTextSrc g_ModFixedTextSrc[] MOD_RO = {"]
    out += ["    {%d, %d, 0x%04X, %d, %d, %d}, // %s" % r for r in fixed_rows]
    out += ["};", "static const u8 g_ModFixedTextBytes[] MOD_RO = {"]
    out += ["    " + " ".join("0x%02X," % c for c in fixed_blob[i:i + 16])
            for i in range(0, len(fixed_blob), 16)]
    out += ["};", "// Following the Dub row: {table | where the PS1 / PSP versions",
            "// start to differ << 3, pointer offset, length of the differing span};",
            "// then per string the span's PS1 bytes, its PSP bytes, written over",
            "// DRA's string by ModApplyDubText.",
            "#define MOD_DUB_TEXT_COUNT %d" % len(dub_rows),
            "#define MOD_DUB_TEXT_SIZE %d" % len(dub_blob),
            "static const ModDubTextSrc g_ModDubTextSrc[] MOD_RO = {"]
    out += ["    {%d, 0x%04X, %d}, // %s" % r for r in dub_rows]
    out += ["};", "static const u8 g_ModDubTextBytes[] MOD_RO = {"]
    out += ["    " + " ".join("0x%02X," % c for c in dub_blob[i:i + 16])
            for i in range(0, len(dub_blob), 16)]
    out += ["};", ""]
    text = "\n".join(out)
    if not os.path.exists(OUT_SEL) or open(OUT_SEL, encoding="utf-8").read() != text:
        open(OUT_SEL, "w", encoding="utf-8").write(text)
    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    json.dump(check, open(OUT_JSON, "w"), indent=1)
    print("menu text: %d fixed strings (%d bytes in SEL, %d built in g_ModDub), "
          "%d dub-row strings (%d bytes)" % (len(fixed_rows), len(fixed_blob),
                                             sum(r[5] for r in fixed_rows), len(dub_rows), len(dub_blob)))


if __name__ == "__main__":
    main()
