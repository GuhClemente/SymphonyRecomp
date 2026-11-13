#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: check that modded stage overlays keep their retail data.
#
# Stage cutscene scripts (and possibly other data) hold absolute addresses
# into the overlay's data section, so the mod may only add code and rodata,
# which the linker places after it. The only allowed differences inside the
# data section are relinked pointers into the overlay's own rodata/text.
import re
import struct
import sys
import os

os.chdir(os.path.join(os.path.dirname(__file__), "../../.."))
# overlay map name -> retail file
STAGES = {
    "sttop": "disks/us/ST/TOP/TOP.BIN",
    "stsel": "disks/us/ST/SEL/SEL.BIN",
    "stno4": "disks/us/ST/NO4/NO4.BIN",
    "bobo3": "disks/us/BOSS/BO3/BO3.BIN",
    "stnz1": "disks/us/ST/NZ1/NZ1.BIN",
    "stcat": "disks/us/ST/CAT/CAT.BIN",
    "stnp3": "disks/us/ST/NP3/NP3.BIN",
    "stno3": "disks/us/ST/NO3/NO3.BIN",
    "stno0": "disks/us/ST/NO0/NO0.BIN",
    "stwrp": "disks/us/ST/WRP/WRP.BIN",
    # familiar cards moved back to the Saturn's rooms (gen_card_layouts.py)
    # the cut Outer Wall Ghost's tower layout copy (gen_mod_layouts.py)
    "stno1": "disks/us/ST/NO1/NO1.BIN",
    "stno2": "disks/us/ST/NO2/NO2.BIN",
    "stare": "disks/us/ST/ARE/ARE.BIN",
    # the inverted castle's neighbours of the Hell Garden / Soul Prison
    "strno3": "disks/us/ST/RNO3/RNO3.BIN",
    "strwrp": "disks/us/ST/RWRP/RWRP.BIN",
    "strno4": "disks/us/ST/RNO4/RNO4.BIN",
    "strno0": "disks/us/ST/RNO0/RNO0.BIN",
    # the Saturn's Maria boss fight (mods/sotn-ultimate/src/maria_saturn/mod_maria_boss.h)
    "stcen": "disks/us/ST/CEN/CEN.BIN",
    # the Saturn's 164-entry enemy list (src/st/lib/mod_bestiary.h)
    "stlib": "disks/us/ST/LIB/LIB.BIN",
    # the PSP English script (mods/sotn-ultimate/tools/psp_script.py): scripts rewritten in
    # place, or a jump to the rest in the overlay's rodata
    "stst0": "disks/us/ST/ST0/ST0.BIN",
    "stdai": "disks/us/ST/DAI/DAI.BIN",
    "stdre": "disks/us/ST/DRE/DRE.BIN",
    "stnz0": "disks/us/ST/NZ0/NZ0.BIN",
    "strcen": "disks/us/ST/RCEN/RCEN.BIN",
    "bomar": "disks/us/BOSS/MAR/MAR.BIN",
    "bobo2": "disks/us/BOSS/BO2/BO2.BIN",
    "bobo5": "disks/us/BOSS/BO5/BO5.BIN",
    "bobo6": "disks/us/BOSS/BO6/BO6.BIN",
    "borbo2": "disks/us/BOSS/RBO2/RBO2.BIN",
    "borbo6": "disks/us/BOSS/RBO6/RBO6.BIN",
}

# Intended value changes: SEL's own stage table has TE1-TE4 rows whose
# VH/VB lengths come from lba_us.h (the same retail values in all), and the
# Garden's, the Prison's, the Hell Garden's and the Soul Prison's sound banks
# (TE1-TE4's slots, mods/sotn-ultimate/tools/gen_gdn_sounds.py, gen_prs_sounds.py,
# gen_rgdn_sounds.py, gen_rprs_sounds.py) changed them. Allowed:
# (overlay, retail value) -> the lba_us.h constants the built value may equal.
lba = dict(re.findall(r"#define (\w+) (0x[0-9A-Fa-f]+)", open("include/lba_us.h").read()))
INTENDED = {
    ("stsel", 0x1820): {int(lba["ST_TE%d_VH_LEN" % i], 16) for i in (1, 2, 3, 4)},
    ("stsel", 0x1A700): {int(lba["ST_TE%d_VB_LEN" % i], 16) for i in (1, 2, 3, 4)},
    # NO3's row: its bank gained the horse ride's hoofbeats
    # (mods/sotn-ultimate/tools/gen_no3_sounds.py)
    ("stsel", 0x19AD0): {int(lba["ST_NO3_VB_LEN"], 16)},
}

# Tilemaps edited in place (same size, contents only), listed by their
# generators (mods/sotn-ultimate/tools/gen_prs_links.py): overlay -> [[symbol, bytes]]
import json
EDITS_PATH = "build/us/mod_data_edits.json"
DATA_EDITS = json.load(open(EDITS_PATH)) if os.path.exists(EDITS_PATH) else {}
# the PSP English script's byte ranges (overlay offsets), from
# mods/sotn-ultimate/tools/psp_script.py: overlay -> [[offset, size]]
PSP_PATH = "build/us/psp_script/edits.json"
PSP_EDITS = json.load(open(PSP_PATH)) if os.path.exists(PSP_PATH) else {}

# Overlays whose end (BSS included) must stay below a fixed buffer: the
# castle map / save buffers at 0x801E0000+ (CASTLE_MAP_PTR), for every stage
# (v1.3: NO3's horse ride neigh once ran past it unnoticed)
VRAM_LIMITS = {name: 0x801E0000 for name in STAGES}

status = 0
for name, retail_path in STAGES.items():
    text = open(f"build/us/{name}.map").read()
    sym = lambda s: int(re.search(rf"0x([0-9a-f]+)\s+{name}_{s} = ", text)[1], 16)
    base, data_end, bss_start = sym("DATA_START"), sym("DATA_END"), sym("BSS_START")
    if name in VRAM_LIMITS:
        end = sym("BSS_END")
        if end > VRAM_LIMITS[name]:
            print(f"FAIL {name}: overlay ends at {end:#x}, past {VRAM_LIMITS[name]:#x}")
            status = 1
        else:
            print(f"OK   {name}: overlay ends at {end:#x} ({VRAM_LIMITS[name] - end} bytes left)")
    retail = open(retail_path, "rb").read()
    built = open(f"build/us/{name.upper()[2:]}.BIN", "rb").read()
    n = data_end - base
    if len(retail) < n:
        print(f"FAIL {name}: data section grew past the retail file")
        status = 1
        continue
    edited = []
    for sym_name, size in DATA_EDITS.get(name, []):
        m = re.search(r"0x([0-9a-f]+)\s+%s\n" % sym_name, text)
        assert m, (name, sym_name)
        edited.append((int(m[1], 16) - base, int(m[1], 16) - base + size))
    for off, size in PSP_EDITS.get(name, []):
        # word-aligned cover of the range (the check walks words)
        edited.append((off & ~3, off + size))
    bad = []
    for i in range(0, n, 4):
        if retail[i : i + 4] == built[i : i + 4]:
            continue
        if any(a <= i < b for a, b in edited):
            continue
        ptr = struct.unpack("<I", built[i : i + 4])[0]
        was = struct.unpack("<I", retail[i : i + 4])[0]
        if ptr in INTENDED.get((name, was), ()):
            continue
        if not (data_end <= ptr < bss_start):
            bad.append(hex(base + i))
    if bad:
        print(f"FAIL {name}: data changed at {', '.join(bad[:8])}")
        status = 1
    else:
        print(f"OK   {name}: data section matches retail (relinked pointers only)")

    # The mod's layout copy is only switched in when the stage header's
    # MOD_LAYOUT_ID reaches InitRoomEntities (src/st/create_entity.h); make
    # sure the built code loads g_ModLayoutX (lui + addiu of its address).
    m = re.search(r"0x([0-9a-f]+)\s+g_ModLayoutX\n", text)
    if m:
        addr = int(m[1], 16)
        hi, lo = ((addr + 0x8000) >> 16) & 0xFFFF, addr & 0xFFFF
        words = struct.unpack("<%dI" % (len(built) // 4), built[: len(built) // 4 * 4])
        used = any(
            w >> 26 == 0xF and w & 0xFFFF == hi
            and any(v >> 26 == 9 and v & 0xFFFF == lo for v in words[i + 1 : i + 12])
            for i, w in enumerate(words))
        if used:
            print(f"OK   {name}: InitRoomEntities uses the mod layout")
        else:
            print(f"FAIL {name}: g_ModLayoutX is never loaded (MOD_LAYOUT_ID missing?)")
            status = 1
sys.exit(status)
