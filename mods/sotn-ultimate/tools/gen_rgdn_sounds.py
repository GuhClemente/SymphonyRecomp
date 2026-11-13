#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Hell Garden's stage sound bank (the inverted castle's
Underground Garden, stage 0x31).

As the Garden's (mods/sotn-ultimate/tools/gen_gdn_sounds.py, whose builder this reuses, as
gen_prs_sounds.py does): the Saturn's Hell Garden sounds (SD/SD_TEIX.CRT, its
stage bank 0xB34) rendered to WAVs by mods/sotn-ultimate/tools/saturn_sfx.py, in a PS1 VAB
of their own, written as build/us/SD_ZKTE3.VH/.VB (the unused TE3 stage's
bank slot; lba_stage.c's 0x31 row loads TE3's).

  - the Saturn-only sounds (Saturn ids 0x8F0+, priority 50 there: the Copper
    Armor's and the Ifrit's) take PS1 stage ids of priority 50 whose slots
    are free here (MOD_SFX_* in the generated mods/sotn-ultimate/src/garden/rgdn/mod_rgdn_sfx.h).
    0x8F1 (the bank's voice 1) is unreferenced by the Saturn's stage: left out;
  - the enemies that run the PS1's code play their PS1 stage ids, so their
    PS1 tones are copied into the same slots (with their pair voice). The
    Saturn's own versions are the same recordings (tools: inv/step7/cmp.py,
    envelope / spectrum correlation >= 0.94 / 0.97): user rule, keep the
    PS1 sound then.
      Blue Venus Weed (RNO3, program 2): 0x70F tone 4, 0x710 tone 6
        (Saturn 0x74A, 0x74B);
      Cloaked Knight (RNZ1, program 5): 0x71E tone 0, 0x720 tone 4 (Saturn
        0x7BC, 0x7BE); its 0x71F (tone 2) is a silent dummy in NZ1 and RNZ1
        alike and stays silent.
    The Cave Troll, Bitterfly and the Skeleton Guardian play common sounds
    only.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_gdn_sounds as gs  # noqa: E402

gs.OUT = os.path.join(gs.ROOT, "build/us/SD_ZKTE3")
gs.HEADER = os.path.join(gs.ROOT, "mods/sotn-ultimate/src/garden/rgdn/mod_rgdn_sfx.h")
gs.LABEL = "rgdn"
gs.DESCRIPTION = "the Hell Garden's own sounds (its stage bank, build/us/SD_ZKTE3)"
gs.UNCAPPED = set()
gs.SOUNDS = [
    ("COPPER_ARMOR_DEATH", 0x8F0, 0x72D),  # program 3 tone 8 (it falls apart)
    ("IFRIT_DEATH", 0x8F2, 0x731),  # 3/10 (it burns up)
    ("IFRIT_FIRE_BREATH", 0x8F3, 0x733),  # 3/12 (the flames it spits)
    ("IFRIT_FIRE_RING", 0x8F4, 0x734),  # 4/0 (it throws the ring)
    ("IFRIT_FIRE_PILLAR", 0x8F5, 0x735),  # 4/2 (the ring erupts, Saturn 0x060E7E60)
    ("IFRIT_CHARGE", 0x8F6, 0x73B),  # 4/4 (embers gather)
]
gs.SATURN_PRIORITY = {}
gs.COPIES = []
gs.INSTRUMENTS = []
RNO3 = "disks/us/ST/RNO3/SD_ZRNO3"
RNZ1 = "disks/us/ST/RNZ1/SD_ZRNZ1"
gs.PS1_TONES = [(RNO3, 2, 4, 1), (RNO3, 2, 6, 1), (RNZ1, 5, 0, 1), (RNZ1, 5, 4, 1)]
gs.PS1_PAIRS = True
STAGE = "SD_TEIX.CRT"


def render(sid, cap):
    import saturn_sfx
    path = os.path.join(gs.WORK, "rgdn_%X_%d.wav" % (sid, cap))
    return saturn_sfx.render_to_wav(sid, path=path, max_rate=cap, stage=STAGE)


if __name__ == "__main__":
    for cap in gs.RATE_CAPS:
        if gs.build(render, gs.instrument, cap) is not None:
            break
    else:
        raise SystemExit("the Hell Garden's sounds don't fit its bank at any rate cap")
