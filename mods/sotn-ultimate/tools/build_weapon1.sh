#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: build the left-hand (WEAPON1) copy of modded weapons.
#
# The decomp only builds WEAPON0.BIN (right hand). The left hand loads the
# same weapon from WEAPON1.BIN: identical graphics, code linked by weapon1.ld
# (0x8017D000) with g_HandId = 1. Retail WEAPON1 would mismatch any weapon the
# mod changes (e.g. weapon 21's food frames), so this compiles those weapons
# for hand 1 the way tools/builds/gen.py compiles hand 0, and splices them
# into a copy of the retail file as build/us/WEAPON1.BIN (make disk prefers
# build/us). Each weapon slot is 0x7000 bytes: 0x4000 graphics, 0x3000 code.
#
# Usage: build_weapon1.sh [--check] ID...   (--check: compare with retail)
set -euo pipefail
cd "$(dirname "$0")/../../.."
CHECK=0
if [ "${1:-}" = "--check" ]; then CHECK=1; shift; fi
OUT=build/us/mod/weapon1
mkdir -p "$OUT"
# always start from the retail file so slots the mod leaves alone stay retail
if [ $CHECK = 0 ]; then cp disks/us/BIN/WEAPON1.BIN "$OUT/WEAPON1.BIN"; fi

for id in "$@"; do
    VERSION=us mipsel-linux-gnu-cpp -DW_$id -DWEAPON1 -lang-c -Iinclude -Iinclude/psxsdk \
        -undef -Wall -fno-builtin -Dmips -D__GNUC__=2 -D__OPTIMIZE__ -D__mips__ -D__mips \
        -Dpsx -D__psx__ -D__psx -D_PSYQ -D__EXTENSIONS__ -D_MIPSEL -D_LANGUAGE_C \
        -DLANGUAGE_C -DNO_LOGS -DHACKS -DUSE_INCLUDE_ASM -D_internal_version_us -DSOTN_STR \
        src/weapon/w_$id.c | tools/sotn_str/target/release/sotn_str process |
        iconv --from-code=UTF-8 --to-code=Shift-JIS |
        bin/cc1-psx-26 -G0 -w -funsigned-char -fpeephole -ffunction-cse \
            -fpcc-struct-return -fcommon -fverbose-asm -msoft-float -g -quiet \
            -mcpu=3000 -fgnu-linker -mgas -gcoff -O2 |
        python3 tools/maspsx/maspsx.py --expand-div --aspsx-version=2.34 |
        mipsel-linux-gnu-as -Iinclude -march=r3000 -mtune=r3000 -no-pad-sections \
            -O1 -G0 -o "$OUT/w_$id.c.o"
    # same data, but g_HandId = 1 (weapons whose data is all C, as w_017,
    # get HAND_ID 1 from -DWEAPON1 and have no data .s)
    DATA_O=""
    if [ -f "asm/us/weapon/data/w_$id.data.s" ]; then
    DATA_O="$OUT/w_$id.data.s.o"
    python3 - "$id" "$OUT" <<'EOF'
import re, sys
wid, out = sys.argv[1:]
s = open(f"asm/us/weapon/data/w_{wid}.data.s").read()
s, n = re.subn(r"(glabel g_HandId\n\s*/\*[^*]*\*/ \.word )0x00000000", r"\g<1>0x00000001", s)
assert n == 1, "g_HandId not found"
open(f"{out}/w_{wid}.data.s", "w").write(s)
EOF
    mipsel-linux-gnu-as -Iinclude -march=r3000 -mtune=r3000 -no-pad-sections -O1 -G0 \
        -o "$OUT/w_$id.data.s.o" "$OUT/w_$id.data.s"
    fi
    # the weapon's own BSS that splat lists at its hand-0 address (e.g.
    # w_017's D_7B000_8017B0F4): the hand-1 copy has it 0x3000 higher
    DEFSYMS=$(mipsel-linux-gnu-nm -u "$OUT/w_$id.c.o" | awk '{print $2}' |
        python3 -c '
import re, sys
want = set(sys.stdin.read().split())
for l in open("build/us/config/undefined_syms_auto.us.weapon.txt"):
    m = re.match(r"(\w+) = 0x([0-9A-Fa-f]+);", l.strip())
    if m and m.group(1) in want and 0x8017A000 <= int(m.group(2), 16) < 0x8017D000:
        print("--defsym=%s=0x%X" % (m.group(1), int(m.group(2), 16) + 0x3000))
')
    mipsel-linux-gnu-ld -nostdlib --no-check-sections -Map "$OUT/w1_$id.map" -T weapon1.ld \
        -T config/undefined_syms.us.txt -T build/us/config/undefined_funcs_auto.us.weapon.txt \
        -T build/us/config/undefined_syms_auto.us.weapon.txt -o "$OUT/w1_$id.elf" \
        "$OUT/w_$id.c.o" $DATA_O $DEFSYMS
    mipsel-linux-gnu-objcopy -O binary "$OUT/w1_$id.elf" "$OUT/w1_$id.bin"
    size=$(stat -c %s "$OUT/w1_$id.bin")
    if [ "$size" -gt 12288 ]; then echo "FAIL weapon $id hand 1 code is $size bytes (> 0x3000)"; exit 1; fi
    truncate -c -s 12288 "$OUT/w1_$id.bin"
    gfx=build/us/weapon/f0_$id.bin
    [ "$(stat -c %s "$gfx")" = 16384 ] || { echo "FAIL $gfx is not 0x4000 bytes"; exit 1; }
    slot=$((10#$id * 0x7000))
    if [ $CHECK = 1 ]; then
        if cmp -s <(tail -c +$((slot + 0x4000 + 1)) disks/us/BIN/WEAPON1.BIN | head -c 12288) "$OUT/w1_$id.bin"; then
            echo "OK   weapon $id hand 1 code matches retail WEAPON1"
        else
            echo "DIFF weapon $id hand 1 code differs from retail WEAPON1:"
            cmp -l <(tail -c +$((slot + 0x4000 + 1)) disks/us/BIN/WEAPON1.BIN | head -c 12288) "$OUT/w1_$id.bin" | head -5
        fi
        continue
    fi
    dd if="$gfx" of="$OUT/WEAPON1.BIN" bs=1 seek=$slot conv=notrunc status=none
    dd if="$OUT/w1_$id.bin" of="$OUT/WEAPON1.BIN" bs=1 seek=$((slot + 0x4000)) conv=notrunc status=none
    echo "weapon $id: hand 1 copy spliced into WEAPON1.BIN"
done
[ $CHECK = 1 ] || cp "$OUT/WEAPON1.BIN" build/us/WEAPON1.BIN
