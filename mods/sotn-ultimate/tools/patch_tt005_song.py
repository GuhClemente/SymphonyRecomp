#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: the Song Lyrics Card (Saturn T_FAIRY2.PRG, the Yousei).
#
# TT_005 (the JP-only Sprite / Yousei familiar) is asm-only. While the Yousei
# sits on a seated Alucard's shoulder, UpdateServantSitOnShoulder decides
# whether she sings (MU_NOCTURNE, which is the Saturn's SD20 song). The US
# rule (level >= 50, then level >= 90 or rand() % 8 == 0, wait 2400 frames)
# becomes the Saturn's (helper 0x060D1DE4 called from T_FAIRY2's
# UpdateServantSitOnShoulder):
#   room flags & 0x60 == 0 (kept as is), room special state != 1,
#   level >= 10, PLAYER.step_s == 4 (sitting in a chair), then
#   Song Lyrics Card in an accessory slot (CheckEquipmentItemCount(card, 4))
#   -> always; else (rand() & 31) > 26 (level <= 22), 20 (23-43), 6 (>= 44);
#   then wait 1800 frames (0x708) and sing.
# The Saturn's 3600-frame wait for "not sitting" can't happen (sitting is
# required), so that branch's slot holds the new code. The rewrite is the
# same size as the original (48 instructions), so TT_005.BIN keeps its size
# and every other address (the overlay loads exactly 0xA000 bytes).
#
# Works from a pristine copy in $MOD_WORK/retail-asm, so reruns are safe.
import os
import re
import shutil
import sys
from mod_work import work_dir  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
REL = "servant/tt_005/nonmatchings/sprite/UpdateServantSitOnShoulder.s"
PATH = os.path.join(ROOT, "asm/us", REL)
PRISTINE = work_dir("retail-asm", REL)
SONG_LYRICS_CARD = 92  # ITEM_SONG_LYRICS_CARD (NUM_BODY_ITEMS + 2)

NEW = """\
    # Saturn port (mods/sotn-ultimate/tools/patch_tt005_song.py): Saturn singing rule
    lui        $v1, %hi(D_us_80179328)
    lw         $v1, %lo(D_us_80179328)($v1)
    lui        $a0, %hi(D_us_801792F0)
    lw         $a0, %lo(D_us_801792F0)($a0)
    ori        $v0, $zero, 0x1
    beq        $v1, $v0, .Lmod_song_no
     slti      $v0, $a0, 0xA
    bnez       $v0, .Lmod_song_no
     lui       $v1, %hi(PLAYER_step_s)
    lhu        $v1, %lo(PLAYER_step_s)($v1)
    ori        $v0, $zero, 0x4
    bne        $v1, $v0, .Lmod_song_no
     nop
    lui        $v0, 0x8004 # g_api.CheckEquipmentItemCount = 0x8003C864
    lw         $v0, -0x379C($v0)
    ori        $a0, $zero, {card}
    jalr       $v0
     ori       $a1, $zero, 0x4
    bnez       $v0, .Lus_80176FFC
     nop
    jal        rand
     nop
    lui        $a0, %hi(D_us_801792F0)
    lw         $a0, %lo(D_us_801792F0)($a0)
    andi       $v1, $v0, 0x1F
    ori        $a1, $zero, 0x1A
    slti       $v0, $a0, 0x17
    bnez       $v0, .Lmod_song_cmp
     slti      $v0, $a0, 0x2C
    bnez       $v0, .Lmod_song_cmp
     ori       $a1, $zero, 0x14
    ori        $a1, $zero, 0x6
  .Lmod_song_cmp:
    slt        $v0, $a1, $v1
    bnez       $v0, .Lus_80176FFC
  .Lmod_song_no:
     ori       $v0, $zero, 0x5
  .Lus_80176FF4:
    j          .Lus_80177340
     sh        $v0, 0x2C($s0)
  .Lus_80176FFC:
    sh         $zero, 0xB4($s0)
    ori        $v0, $zero, 0x708
    j          .Lus_801772DC
     sh        $v0, 0x8C($s0)
""".format(card=SONG_LYRICS_CARD)


def count_ins(lines):
    n = 0
    for l in lines:
        t = l.split("*/")[-1].strip() if "*/" in l else l.strip()
        if not t or t.startswith(".") or t.startswith("#") or t.endswith(":"):
            continue
        n += 1
    return n


if not os.path.exists(PRISTINE):
    os.makedirs(os.path.dirname(PRISTINE), exist_ok=True)
    text = open(PATH).read()
    if "Saturn port (mods/sotn-ultimate/tools/patch_tt005_song.py)" in text:
        sys.exit("FAIL patch_tt005_song: no pristine copy and the asm is already patched")
    shutil.copy2(PATH, PRISTINE)
lines = open(PRISTINE).read().split("\n")
start = next(i for i, l in enumerate(lines) if "/* 6F74 80176F74 1880033C */" in l)
end = next(i for i, l in enumerate(lines) if "/* 7030 80177030 8C0002A6 */" in l)
old = lines[start:end + 1]
new = NEW.rstrip("\n").split("\n")
pad = count_ins(old) - count_ins(new)
if count_ins(old) != 48 or pad < 0:
    sys.exit("FAIL patch_tt005_song: unexpected original block (%d instructions)" % count_ins(old))
new += ["    nop"] * pad
out = "\n".join(lines[:start] + new + lines[end + 1:])
if not os.path.exists(PATH) or open(PATH).read() != out:
    open(PATH, "w").write(out)
print("TT_005 song rule patched (%d padding nops)" % pad)

# ninja doesn't track INCLUDE_ASM'd .s files (sprite.c's depfile lists only
# headers): when the asm is newer than the object, make sprite.c newer too.
# (This also picks up mods/sotn-ultimate/tools/patch_status_asm.py's TT_005 offsets.)
OBJ = os.path.join(ROOT, "build/us/src/servant/tt_005/sprite.c.o")
SRC = os.path.join(ROOT, "src/servant/tt_005/sprite.c")
if not os.path.exists(OBJ) or os.path.getmtime(PATH) >= os.path.getmtime(OBJ):
    os.utime(SRC, None)
