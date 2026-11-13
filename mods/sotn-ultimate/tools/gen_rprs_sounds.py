#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Soul Prison's stage sound bank (the inverted castle's
Cursed Prison, stage 0x30).

As the Prison's (mods/sotn-ultimate/tools/gen_prs_sounds.py; builder: gen_gdn_sounds.py):
the Saturn's Soul Prison sounds (SD/SD_ROUGX.CRT, its stage bank 0xB37)
rendered to WAVs by mods/sotn-ultimate/tools/saturn_sfx.py, in a PS1 VAB of their own,
written as build/us/SD_ZKTE4.VH/.VB (the unused TE4 stage's bank slot;
lba_stage.c's 0x30 row loads TE4's).

  - the Saturn-only sounds (Saturn ids 0x8FE+, priority 50 there) take PS1
    stage ids of priority 50 whose slots are free here (MOD_SFX_* in the
    generated mods/sotn-ultimate/src/prison/rprs/mod_rprs_sfx.h); the Gargoyle's (params 2) are the
    Garden's ids, as its code is shared (mods/sotn-ultimate/src/enemies/e_mod_gargoyle.h);
  - the Flying Zombie runs the PS1's code (src/st/e_flying_zombie.h) and plays
    its PS1 stage ids 0x714/0x715/0x716 (RNO2's program 2 tones 8/10/12; LIB's
    bank has only tone 8): copied into the same slots, with their pair voice.
    The Saturn's 0x784/0x785 are the same recordings as 0x714/0x715
    (inv/step7/cmp.py): user rule, keep the PS1 sound then.
    Jack O'Bones and the Nova Skeleton play common sounds only.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_gdn_sounds as gs  # noqa: E402

gs.OUT = os.path.join(gs.ROOT, "build/us/SD_ZKTE4")
gs.HEADER = os.path.join(gs.ROOT, "mods/sotn-ultimate/src/prison/rprs/mod_rprs_sfx.h")
gs.LABEL = "rprs"
gs.DESCRIPTION = "the Soul Prison's own sounds (its stage bank, build/us/SD_ZKTE4)"
gs.UNCAPPED = set()
gs.SOUNDS = [
    ("GARGOYLE_CLAW", 0x8FE, 0x725),  # program 3 tone 0
    ("GARGOYLE_DEBRIS", 0x8FF, 0x728),  # 3/2
    ("WRAITH_GRAB", 0x904, 0x72D),  # 3/8, it seizes the player (short)
    ("WRAITH_DRAIN", 0x905, 0x731),  # 3/10, each soul-drain burst
    ("WRAITH_DEATH", 0x906, 0x733),  # 3/12, its death cry
    ("WRAITH_VANISH", 0x907, 0x734),  # 4/0, the wisp vanishing (long)
    ("WIGHT_SLASH", 0x908, 0x735),  # 4/2, the sword swing (short)
    ("WIGHT_DEATH", 0x909, 0x73B),  # 4/4, its death (with the explosion)
]
gs.SATURN_PRIORITY = {}
gs.COPIES = []
gs.INSTRUMENTS = []
RNO2 = "disks/us/ST/RNO2/SD_ZRNO2"
gs.PS1_TONES = [(RNO2, 2, 8, 1), (RNO2, 2, 10, 1), (RNO2, 2, 12, 1)]
gs.PS1_PAIRS = True
STAGE = "SD_ROUGX.CRT"


def render(sid, cap):
    import saturn_sfx
    path = os.path.join(gs.WORK, "rprs_%X_%d.wav" % (sid, cap))
    return saturn_sfx.render_to_wav(sid, path=path, max_rate=cap, stage=STAGE)


if __name__ == "__main__":
    for cap in gs.RATE_CAPS:
        if gs.build(render, gs.instrument, cap) is not None:
            break
    else:
        raise SystemExit("the Soul Prison's sounds don't fit its bank at any rate cap")
