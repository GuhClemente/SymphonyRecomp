#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the hoofbeats of Richter's and Maria's horse ride (NO3).

The Saturn's ride (STAGE_0X 0x060DC80C) plays id 0x5D9 (Richter) / 0x5DB
(Maria) every 32 frames while galloping and 0x5DA / 0x5DC at the gate: clips
1 and 2 of its in-RAM PCM set, SDE8.PCM / SDD2.PCM (the two files are the
same; signed 8-bit mono, 22,050 Hz). Clip 1 (bytes 0-0x1AB0, 0.31 s) is the
hoofbeat: here at 11,025 Hz (22,050 doesn't fit), every 32 frames as the
Saturn's code (src/st/no3/e_outdoor_ents.c). (Before v1.3: the Valhalla
Knight's gallop.) Clip 2, the neigh at the gate (1.1 s, at its 22,050 Hz),
doesn't fit NO3's bank: Richter and Maria get a bank of their own,
BIN/NO3RIDE.BIN (src/dra/cd.c loads it for NO3 when the player isn't
Alucard), where it takes the place of the two Alucard-only samples (Death's
laugh and "Death takes your items", next to each other; their tones are
silenced), with a silent filler after it so every other sample keeps its
sound RAM address and the bank its size. It plays as SFX_UNUSED_79E, whose
empty slot (program 1 tone 6) gets a tone on it.

It goes in the slot of SFX_SCIFI_BLAST (0x7D1, program 0 tone 10, empty in
NO3's bank; no code the mod builds plays it), with the envelope, pitch slot
and loudness of the PS1's own horse, the Valhalla Knight's gallop
(SFX_VALHALLA_KNIGHT_GALLOP 0x719, Olrox's Quarters' bank NO2, program 2
tone 14). The builder makes the same bank from the player's Saturn disc
(mods/sotn-ultimate/tools/builder/src/no3_sounds.ts).
Written as build/us/SD_ZKNO3.VH/.VB (`make disk` takes them over the
retail files) and build/us/NO3RIDE.BIN: the ride's bank (ride_bank: its VH
in as many sectors as SD_ZKNO3.VH, then its VB, as stage banks load), made
from this bank, then from the PSP dub's NO3 bank (its zombies' and Death's
PSP clips; mods/sotn-ultimate/tools/gen_psp_sfx.py writes that second part). The bank must stay within VB_LIMIT (the largest retail stage
bank, mods/sotn-ultimate/tools/gen_gdn_sounds.py); the Valhalla Knight's neigh (7 KB) does
not fit with it.
"""
import os
import sys

import yaml

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vab import Vab  # noqa: E402
import gen_gdn_sounds as G  # noqa: E402
import gen_psp_grunts as P  # noqa: E402

VB_LIMIT = 0x1A610  # as gen_gdn_sounds.py
NO3 = os.path.join(ROOT, "disks/us/ST/NO3/SD_ZKNO3")
NO2 = os.path.join(ROOT, "disks/us/ST/NO2/SD_ZKNO2")
OUT = os.path.join(ROOT, "build/us/SD_ZKNO3")
RIDE_OUT = os.path.join(ROOT, "build/us/NO3RIDE.BIN")
SECTOR = 2048
# (name, source bank, PS1 id whose sound is copied, id played in NO3)
COPIES = [("HORSE_GALLOP", NO2, 0x719, 0x7D1)]
# the Saturn's hoofbeat: SDD2.PCM clip 1, at RATE
SAT_PCM = os.path.join(ROOT, "disks/saturn/SD/SDD2.PCM")
CLIP = (0, 0x1AB0)
RATE = 11025
# the neigh: SDD2.PCM clip 2 at its own rate, where the Alucard-only samples
# of SFX_DEATH_TAKES_ITEMS (0x7A0) and SFX_DEATH_LAUGH (0x7A1) are
NEIGH = (0x1AC0, 0x7930)
NEIGH_RATE = 22050
NEIGH_ID = 0x79E
NEIGH_OVER = (0x7A0, 0x7A1)


def hoofbeat():
    d = open(SAT_PCM, "rb").read()[CLIP[0]:CLIP[1]]
    x = [(b - 256 if b >= 128 else b) * 256 for b in d]
    y, _ = G.resample(x, 22050, RATE)
    return P.encode("no3_hoofbeat", y, RATE)


def neigh():
    d = open(SAT_PCM, "rb").read()[NEIGH[0]:NEIGH[1]]
    return P.encode("no3_neigh", [(b - 256 if b >= 128 else b) * 256 for b in d], NEIGH_RATE)


def load(path):
    return Vab(open(path + ".VH", "rb").read(), open(path + ".VB", "rb").read())


def main():
    cfg = yaml.safe_load(open(os.path.join(ROOT, "assets/dra/us/sfx_config.yaml")))
    vab = load(NO3)
    for name, src, orig, new in COPIES:
        sv = load(src)
        o, e = cfg[orig - 0x600], cfg[new - 0x600]
        assert o["vabid"] == 3 and e["vabid"] == 3, name
        slot = vab.tones[e["prog"]]
        # empty here: NO3's silent placeholder (vag 1 at volume 0)
        assert slot[e["tone"]]["vol"] == 0 and slot[e["tone"] + 1]["vol"] == 0, name
        os.makedirs(P.WORK, exist_ok=True)
        vab.vags.append(hoofbeat())
        # the Valhalla Knight's tone (envelope, loudness at the new id's
        # volume), playing the Saturn clip at its own rate at the new id's
        # note; one voice, as the Saturn's PCM channel (its pair stays silent)
        st = sv.tones[o["prog"]][o["tone"]]
        c, f = G.centre(e["note"], RATE)
        slot[e["tone"]] = dict(
            st, prog=e["prog"], vag=len(vab.vags), center=c, shift=f,
            vol=min(127, round(st["vol"] * o["volume"] / e["volume"])))
    vh, vb = vab.write()
    assert len(vb) <= VB_LIMIT, ("NO3's bank too big", len(vb), VB_LIMIT)
    open(OUT + ".VH", "wb").write(vh)
    open(OUT + ".VB", "wb").write(vb)
    part = ride_bank(vh, vb)
    # the PSP dub's part: mods/sotn-ultimate/tools/gen_psp_sfx.py, from its NO3 bank (this
    # bank until then)
    open(RIDE_OUT, "wb").write(part + part)
    print("no3 sounds: %d copied, bank %d bytes (limit %d); the ride's bank: neigh %d bytes" % (
        len(COPIES), len(vb), VB_LIMIT, len(neigh())))


def ride_bank(vh, vb):
    """-> a part of BIN/NO3RIDE.BIN: NO3's bank vh + vb (this tool's, or the
    PSP dub's) with the neigh over the two Alucard-only samples, then a
    silent filler as long as the rest; their tones silenced, the neigh's tone
    on it with the envelope and loudness of the hoofbeat's (the Valhalla
    Knight's). Its VH in whole sectors, then its VB in whole sectors"""
    cfg = yaml.safe_load(open(os.path.join(ROOT, "assets/dra/us/sfx_config.yaml")))
    _, src, orig, _ = COPIES[0]
    o = cfg[orig - 0x600]
    st = load(src).tones[o["prog"]][o["tone"]]
    ride = Vab(vh, vb)
    over = [cfg[i - 0x600] for i in NEIGH_OVER]
    vags = [ride.tones[x["prog"]][x["tone"]]["vag"] for x in over]
    assert vags[1] == vags[0] + 1, vags
    room = len(ride.vags[vags[0] - 1]) + len(ride.vags[vags[1] - 1])
    body = neigh()
    assert len(body) < room, (len(body), room)
    ride.vags[vags[0] - 1] = body
    ride.vags[vags[1] - 1] = bytes(room - len(body))
    for p in ride.tones:
        for tn in ride.tones[p]:
            if tn["vag"] in vags:
                tn["vol"] = 0
    n = cfg[NEIGH_ID - 0x600]
    slot = ride.tones[n["prog"]]
    assert n["vabid"] == 3 and slot[n["tone"]]["vol"] == 0 and slot[n["tone"] + 1]["vol"] == 0
    c, f = G.centre(n["note"], NEIGH_RATE)
    slot[n["tone"]] = dict(st, prog=n["prog"], vag=vags[0], center=c, shift=f,
                           vol=min(127, round(st["vol"] * o["volume"] / n["volume"])))
    rvh, rvb = ride.write()
    assert len(rvh) == len(vh) and len(rvb) == len(vb), (len(rvh), len(rvb))
    return rvh + bytes(-len(rvh) % SECTOR) + rvb + bytes(-len(rvb) % SECTOR)


if __name__ == "__main__":
    main()
