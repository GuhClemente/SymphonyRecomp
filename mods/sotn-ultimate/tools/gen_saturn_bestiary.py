#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port: the Saturn Librarian's enemy list (164 entries, PS1: 146).

The Saturn (LIBRARY.PRG 0x1825C, 164 BE u16 enemy ids) inserts its 18 new
enemies among the PS1 ones and shows the list in that order (the "No." is
the Saturn's). The bestiary bits (g_CastleFlags ENEMY_LIST_190 /
ENEMY_LIST_RAREDROP_1B0) stay the PS1's for the PS1 enemies, so retail and
mod saves keep their kills; the Saturn enemies get bits 146..163 in enemy id
order (src/st/collision.h MOD_SAT_BESTIARY_MASK computes the same numbers).
Bytes used: 0x1A2-0x1A4 and 0x1C2-0x1C4, which no overlay touches.

Writes src/st/lib/mod_bestiary.h:
  g_ModLibBit[display]  -> bestiary bit
  g_ModLibEnemy[bit]    -> enemy id shown (the Saturn's choice: it shows the
                           big Venus Weed / Blue Venus Weed flower records)
  g_ModLibDesc[bit-146] -> English descriptions (Knight0fDragon's fan
                           translation of the Saturn text, lines rejoined).
"""
import os
import re
import struct

from mod_work import read_src

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..")
LIB = os.path.join(ROOT, "disks/saturn/LIBRARY.PRG")
GAME = os.path.join(ROOT, "disks/saturn/GAME.PRG")
OUT = os.path.join(ROOT, "src/st/lib/mod_bestiary.h")
LIST, COUNT = 0x1825C, 164
SAT_LOOKUP = 0x060850F4 - 0x06066000  # GAME.PRG g_testCollEnemyLookup
RETAIL = 146

DESC = {  # fan translation; fitted to the PS1 detail box (2 lines, each
          # < 105 bytes of BIOS-font advance, see func_us_801B0C40); the
          # renderer reads non-letters as Shift-JIS pairs, so punctuation
          # must stay full-width (ASCII . , ' - would eat the next byte)
    0x190: "A living statue． The castle’s@crime prevention system．",
    0x191: "An antique piece that@is very valuable．",
    0x192: "Angry demon． Literally，@a man who burns．",
    0x194: "Leader of skeleton soldiers．@Its bones broke in a duel．",
    0x195: "The castle’s mad gardener@kills all who ruin the garden．",
    0x196: "Always smiling due to its@fruitful， human−faced crop．",
    0x198: "Vampire hunter looking for@Richter Belmont．",
    0x19D: "Jolly creature of the@river． Tastes like chicken．",
    0x19E: "Giant venus fly trap that@preys on humans， not flies．",
    0x19F: "A living statue． The castle’s@crime prevention system．",
    0x1A0: "A living statue． The castle’s@crime prevention system．",
    0x1A1: "An immaterial ghost． Only@holy weapons can kill it．",
    0x1A2: "A desecrated soul whose@hatred never fades．",
    0x1A4: "Skeleton of a tracker who@walks with Skeleton Beasts．",
    0x1A5: "Artificial beast made from@various animal bones．",
    0x1A7: "The light manipulating the@corpse is the true body．",
    0x1A8: "Beware of contact with this@terrible spirit of death．",
    0x1A9: "Skeleton with shield and@spear． Guards key places．",
}

# The cut Outer Wall skull (enemy record 0xB1, include/game.h
# MOD_OUTER_WALL_GHOST, named "Ghost Skull" by mods/sotn-ultimate/tools/gen_menu_text.py):
# only with the toggle, one more entry at the END of the list (No.165) with
# the next free bestiary bit (164: g_CastleFlags 0x1A4 / rare drop 0x1C4,
# mask 0x10); NO1's collision (src/st/collision.h) sets it. Its picture is
# MONSTER2.BIN record 19 (index 165, mods/sotn-ultimate/tools/ghost_portrait.py) because
# index 164 is the PSP Maria picture (src/st/lib/e_shop.c). With 165 entries
# the two-column list's last row has an empty right cell: g_ModLibBit[165] is
# bit 165, never set, whose g_ModLibEnemy is 0xFFFF (drawn blank, Cross
# refused).
GHOST_ID, GHOST_BIT = 0x0B1, COUNT
GHOST_DESC = "A burning skull that rides@the night winds of the wall．"
assert not any(c.isascii() and not (c.isalpha() or c in " @") for c in GHOST_DESC)

# the PSP port's own text for an enemy it lists too, shown with the PSP and JP
# dubs (src/st/lib/e_shop.c): its Maria entry. The PSP's "Vampire hunter.
# Uses 4 beasts of legend. Seeks Richter." is wider than the PS1 box's two
# lines (it wrapped and lost its end in game, 2026-10-24), so "Uses" goes;
# digits full-width like the rest
DESC_PSP = {0x198: "Vampire hunter． ４ beasts@of legend． Seeks Richter．"}
# the detail box's renderer (func_us_801B0C40) reads anything but a letter, a
# space or "@" as a Shift-JIS pair: an ASCII digit or . , ' - eats the next
# byte (a "4 " drew as a star)
for _t in list(DESC_PSP.values()):
    assert not any(c.isascii() and not (c.isalpha() or c in " @") for c in _t), _t

lib = open(LIB, "rb").read()
game = open(GAME, "rb").read()
sat = struct.unpack_from(">%dH" % COUNT, lib, LIST)
sat_lookup = struct.unpack_from(">426H", game, SAT_LOOKUP)
assert struct.unpack_from(">H", lib, LIST - 2)[0] == 0 and     struct.unpack_from(">I", lib, LIST + COUNT * 2)[0] >> 24 == 6

src = read_src("src/st/collision.h")
us = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", re.search(
    r"VERSION_US\)\s*static u16 g_testCollEnemyLookup\[\] = \{(.*?)\};",
    src, re.S).group(1))]
assert len(us) == 400 and max(us) == RETAIL

new_ids = sorted(x for x in sat if x >= 400)
assert len(new_ids) == COUNT - RETAIL == len(DESC) and set(new_ids) == set(DESC)
mask = 0
for x in new_ids:
    mask |= 1 << (x - 400)
m = re.search(r"#define MOD_SAT_BESTIARY_MASK (0x[0-9A-F]+)", src)
assert m and int(m.group(1), 16) == mask, "collision.h mask must be 0x%X" % mask

bits, enemy = [], [None] * COUNT
for k, x in enumerate(sat):
    if x < 400:
        b = us[x] - 1
        assert b >= 0, hex(x)
    else:
        b = RETAIL + new_ids.index(x)
    # the Saturn's own lookup sends this id to this very entry
    assert sat_lookup[x] == k + 1, (hex(x), k)
    assert enemy[b] is None, (hex(x), b)
    bits.append(b)
    enemy[b] = x
assert None not in enemy

out = ["// Generated by mods/sotn-ultimate/tools/gen_saturn_bestiary.py. Do not edit.",
       "#define MOD_LIB_SATURN_ENEMIES %d" % COUNT,
       "#define MOD_LIB_RETAIL_ENEMIES %d" % RETAIL,
       "#ifdef MOD_OUTER_WALL_GHOST",
       "// + the cut Outer Wall skull (0x%03X), No.%d, bit %d, picture %d" % (
           GHOST_ID, COUNT + 1, GHOST_BIT, COUNT + 1),
       "#define MOD_LIB_NUM_ENEMIES %d" % (COUNT + 1),
       "#define MOD_LIB_GHOST_BIT %d" % GHOST_BIT,
       "#define MOD_LIB_GHOST_PIC %d" % (COUNT + 1),
       "#else",
       "#define MOD_LIB_NUM_ENEMIES %d" % COUNT,
       "#endif", "",
       "// Saturn list position -> bestiary bit",
       "static const u8 g_ModLibBit[] = {"]
for i in range(0, COUNT, 16):
    out.append("    " + " ".join("%d," % b for b in bits[i:i + 16]))
out += ["#ifdef MOD_OUTER_WALL_GHOST",
        "    %d, %d, // the skull; the empty right cell of the last row" % (
            GHOST_BIT, GHOST_BIT + 1),
        "#endif"]
out += ["};", "", "// bestiary bit -> enemy id shown", "static const u16 g_ModLibEnemy[] = {"]
for i in range(0, COUNT, 10):
    out.append("    " + " ".join("0x%03X," % e for e in enemy[i:i + 10]))
out += ["#ifdef MOD_OUTER_WALL_GHOST",
        "    0x%03X, 0xFFFF," % GHOST_ID,
        "#endif"]
out += ["};", "", "// descriptions of bits %d.. (the PS1 ones are D_us_80180FB4)" % RETAIL,
        "static const char* const g_ModLibDesc[] = {"]
for x in new_ids:
    out.append('    "%s", // 0x%03X' % (DESC[x], x))
out += ["#ifdef MOD_OUTER_WALL_GHOST",
        '    "%s", // 0x%03X' % (GHOST_DESC, GHOST_ID),
        "#endif"]
out += ["};", ""]
(psp_id, psp_text), = DESC_PSP.items()
out += ["// the PSP port's description of bit %d (0x%03X), with the PSP / JP dubs" % (
            RETAIL + new_ids.index(psp_id), psp_id),
        "#define MOD_LIB_DESC_PSP_BIT %d" % (RETAIL + new_ids.index(psp_id)),
        'static const char g_ModLibDescPsp[] = "%s";' % psp_text, ""]
text = "\n".join(out)
if not os.path.exists(OUT) or open(OUT, encoding="utf-8").read() != text:
    open(OUT, "w", encoding="utf-8").write(text)
changed = [(hex(enemy[b]), b) for b in range(RETAIL) if enemy[b] != None]
print("mod_bestiary.h: %d entries, Saturn bits %d-%d, mask 0x%X" % (COUNT, RETAIL, COUNT - 1, mask))
