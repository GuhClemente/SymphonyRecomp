#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Wight's PS1-style spirit and glow (test switches).

include/game.h MOD_WIGHT_PS1_SPIRIT / MOD_WIGHT_PS1_GLOW (off by default)
select them; rprs_assets.py asks switches() and appends palettes() to the
Soul Prison's sprite palettes, mods/sotn-ultimate/src/enemies/e_mod_wight.h draws them.

The Wight's palette block (MOD_WIGHT_PAL, 0x22B..0x22E): its body frames use
the first palette (frames 69/70 the second and third, the sword's frames
85-96 the fourth) and cycle +0, +2, +1, +2 (WightPalCycle). Indices 14 and 15
are the olive spirit inside and around the skeleton (frame 71, the Saturn's
death spirit, is only those two): the glow.

- WIGHT_BODY (glow): 5 copies of the block's 0x22B..0x22F with 14/15 clear,
  the body without its spirit (same layout, so the cycle offsets still work).
- WIGHT_GLOW (glow): the same 5 with only 14/15 left, turned to yellows of
  their brightness (stronger), drawn additively in a second copy of the frame.
- WIGHT_SPIRIT (spirit): 3 palettes, every colour of the first palette as a
  yellow of its brightness (normal, dimmer, brighter: the cycle's 0, 1, 2),
  drawn additively for the death spirit (the standing frame, stretched).
All colours keep the STP bit: the PS1 blends only texels that have it.
"""
import os
import re

from mod_work import read_src

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
SPIRIT, GLOW = "MOD_WIGHT_PS1_SPIRIT", "MOD_WIGHT_PS1_GLOW"
SPIRIT_GAINS = (1.25, 0.95, 1.55)
GLOW_GAIN = 2.2


def switches():
    """the switches defined (not commented out) in include/game.h"""
    src = read_src("include/game.h")
    return {s for s in (SPIRIT, GLOW) if re.search(r"^#define %s\b" % s, src, re.M)}


def split(c):
    return c & 31, c >> 5 & 31, c >> 10 & 31


def yellow(c, gain):
    """a colour as a yellow of its brightness (RGB555, STP set)"""
    if not c & 0x7FFF:
        return 0
    r, g, b = split(c)
    lum = (r * 30 + g * 59 + b * 11) / 100.0
    v = min(31, int(round(lum * gain)))
    w = min(31, int(round(lum * gain * 0.28)))
    return 0x8000 | v | v << 5 | w << 10


def palettes(block, on):
    """[(name, [16-colour palettes])] for the switches in on; block = the
    Wight's palettes 0x22B..0x22F (5 x 16 RGB555)"""
    out = []
    if GLOW in on:
        out.append(("WIGHT_BODY", [[0 if i in (14, 15) else c for i, c in enumerate(p)]
                                   for p in block]))
        out.append(("WIGHT_GLOW", [[yellow(c, GLOW_GAIN) if i in (14, 15) else 0
                                    for i, c in enumerate(p)] for p in block]))
    if SPIRIT in on:
        out.append(("WIGHT_SPIRIT", [[yellow(c, g) for c in block[0]] for g in SPIRIT_GAINS]))
    return out
