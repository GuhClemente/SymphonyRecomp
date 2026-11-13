#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Cursed Prison's stage sound bank.

As the Garden's (mods/sotn-ultimate/tools/gen_gdn_sounds.py, whose builder this reuses): the
Saturn's Prison sounds (SD/SD_ROUG.CRT, its stage bank 0xB36) rendered to
WAVs by mods/sotn-ultimate/tools/saturn_sfx.py, in a PS1 VAB of their own, written as
build/us/SD_ZKTE2.VH/.VB (the unused TE2 stage's bank slot; lba_stage.c's
PRS row loads TE2's).

  - the Saturn-only sounds (Saturn ids 0x8F0+, all priority 50 there) take
    PS1 stage ids of priority 50 whose slots are free here (MOD_SFX_* in the
    generated mods/sotn-ultimate/src/prison/prs/mod_prs_sfx.h); the Red Gargoyle's are the Garden's
    ids, as its code is shared (mods/sotn-ultimate/src/enemies/e_mod_gargoyle.h);
  - the Spear Guard (the PS1's code) plays the Underground Caverns' stage ids
    0x770, 0x771 and 0x773 (a sequence of 0x772's tone). The Saturn's Spear
    Guard sounds (0x768, 0x7C1, 0x7C2) are the same: user rule, keep the PS1
    sound then, so the Caverns' tones are copied into the same slots, with
    their pair voice (tone + 1: the same sample, detuned and quieter, which
    the game starts with each tone), as the Caverns play them.
0x902 (unreferenced by the Saturn's Prison) is left out.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_gdn_sounds as gs  # noqa: E402

gs.OUT = os.path.join(gs.ROOT, "build/us/SD_ZKTE2")
gs.HEADER = os.path.join(gs.ROOT, "mods/sotn-ultimate/src/prison/prs/mod_prs_sfx.h")
gs.LABEL = "prs"
gs.DESCRIPTION = "the Prison's own sounds (its stage bank, build/us/SD_ZKTE2)"
gs.UNCAPPED = set()
gs.SOUNDS = [
    ("GARGOYLE_CLAW", 0x8FE, 0x725),
    ("GARGOYLE_DEBRIS", 0x8FF, 0x728),
    ("SPECTER_A", 0x900, 0x709),
    ("SPECTER_B", 0x901, 0x70A),
    ("WISP", 0x903, 0x70B),
]
gs.SATURN_PRIORITY = {}
gs.COPIES = []
gs.INSTRUMENTS = []
# the Spear Guard's tones (NO4 program 6: 0x770 tone 10, 0x771 tone 12, 0x772
# and the 0x773 sequence tone 14)
NO4 = "disks/us/ST/NO4/SD_ZKNO4"
gs.PS1_TONES = [(NO4, 6, 10, 1), (NO4, 6, 12, 1), (NO4, 6, 14, 1)]
gs.PS1_PAIRS = True  # and tones 11, 13, 15 (same samples: the bank's VB is unchanged)
STAGE = "SD_ROUG.CRT"


def render(sid, cap):
    import saturn_sfx
    path = os.path.join(gs.WORK, "prs_%X_%d.wav" % (sid, cap))
    return saturn_sfx.render_to_wav(sid, path=path, max_rate=cap, stage=STAGE)


if __name__ == "__main__":
    for cap in gs.RATE_CAPS:
        if gs.build(render, gs.instrument, cap) is not None:
            break
    else:
        raise SystemExit("the Prison's sounds don't fit its bank at any rate cap")
