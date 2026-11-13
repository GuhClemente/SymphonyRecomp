#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: build the Saturn's new weapons (60-63) for BIN/WEAPON2.BIN.
#
# For each ID with a mods/sotn-ultimate/src/items/weapon/w_0ID.c (all of them when none are given):
# mods/sotn-ultimate/tools/port_saturn_weapon.py converts its Saturn art and data
# (assets/weapon/f_0ID.png, mods/sotn-ultimate/src/items/weapon/mod/w_0ID_data.h), then the weapon is
# compiled once per hand, as tools/builds/gen.py compiles retail weapons for
# WEAPON0.BIN and mods/sotn-ultimate/tools/build_weapon1.sh for WEAPON1.BIN: hand 0 with
# -DWEAPON0 linked by weapon0.ld (0x8017A000), hand 1 with -DWEAPON1 linked
# by weapon1.ld (0x8017D000; HAND_ID makes g_HandId 1 there). These weapons
# are all C (no asm data), so nothing else changes between the hands.
# Each hand's slot is written to build/us/mod/weapons/wHAND_0ID.bin: 0x4000
# bytes of art (png2bin of the page) then the code, data and bss in 0x3000.
# mods/sotn-ultimate/tools/build_saturn_weapons.py puts the slots in WEAPON2.BIN.
#
# Usage: build_saturn_weapon.sh [ID...]   (e.g. 063)
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT=build/us/mod/weapons
mkdir -p "$OUT"
for f in config/undefined_syms.us.txt build/us/config/undefined_funcs_auto.us.weapon.txt \
    build/us/config/undefined_syms_auto.us.weapon.txt; do
    [ -f "$f" ] || { echo "missing $f: build the game once first" >&2; exit 1; }
done

ids=("$@")
if [ ${#ids[@]} = 0 ]; then
    for c in mods/sotn-ultimate/src/items/weapon/w_06[0-3].c; do
        [ -e "$c" ] || continue
        c=${c##*/w_}
        ids+=("${c%.c}")
    done
fi

for id in "${ids[@]}"; do
    .venv/bin/python mods/sotn-ultimate/tools/port_saturn_weapon.py "$((10#$id))"
    .venv/bin/python tools/png2bin.py "assets/weapon/f_$id.png" "$OUT/f_$id.bin"
    [ "$(stat -c %s "$OUT/f_$id.bin")" = 16384 ] || { echo "FAIL art of $id is not 0x4000 bytes"; exit 1; }
    for hand in 0 1; do
        VERSION=us mipsel-linux-gnu-cpp -DW_$id -DWEAPON$hand -lang-c -Iinclude -Iinclude/psxsdk \
            -undef -Wall -fno-builtin -Dmips -D__GNUC__=2 -D__OPTIMIZE__ -D__mips__ -D__mips \
            -Dpsx -D__psx__ -D__psx -D_PSYQ -D__EXTENSIONS__ -D_MIPSEL -D_LANGUAGE_C \
            -DLANGUAGE_C -DNO_LOGS -DHACKS -DUSE_INCLUDE_ASM -D_internal_version_us -DSOTN_STR \
            mods/sotn-ultimate/src/items/weapon/w_$id.c | tools/sotn_str/target/release/sotn_str process |
            iconv --from-code=UTF-8 --to-code=Shift-JIS |
            bin/cc1-psx-26 -G0 -w -funsigned-char -fpeephole -ffunction-cse \
                -fpcc-struct-return -fcommon -fverbose-asm -msoft-float -g -quiet \
                -mcpu=3000 -fgnu-linker -mgas -gcoff -O2 |
            python3 tools/maspsx/maspsx.py --expand-div --aspsx-version=2.34 |
            mipsel-linux-gnu-as -Iinclude -march=r3000 -mtune=r3000 -no-pad-sections \
                -O1 -G0 -o "$OUT/w${hand}_$id.c.o"
        # (weapon1.ld lists no .bss: any lands in an orphan section after the
        # overlay, which the size check below still counts)
        mipsel-linux-gnu-ld -nostdlib --no-check-sections -Map "$OUT/w${hand}_$id.map" \
            -T weapon$hand.ld -T config/undefined_syms.us.txt \
            -T build/us/config/undefined_funcs_auto.us.weapon.txt \
            -T build/us/config/undefined_syms_auto.us.weapon.txt \
            -o "$OUT/w${hand}_$id.elf" "$OUT/w${hand}_$id.c.o"
        mipsel-linux-gnu-objcopy -O binary "$OUT/w${hand}_$id.elf" "$OUT/w${hand}_$id.code"
        base=$((0x8017A000 + hand * 0x3000))
        # everything the overlay uses, bss included, must end in its 0x3000
        used=$(mipsel-linux-gnu-objdump -h "$OUT/w${hand}_$id.elf" | python3 -c '
import sys
base, end, lines = int(sys.argv[1]), int(sys.argv[1]), sys.stdin.read().splitlines()
for i, l in enumerate(lines):
    f = l.split()
    if len(f) >= 7 and f[0].isdigit() and "ALLOC" in lines[i + 1]:
        size, vma = int(f[2], 16), int(f[3], 16)
        if size and base <= vma < base + 0x10000:
            end = max(end, vma + size)
print(end - base)' $base)
        code=$(stat -c %s "$OUT/w${hand}_$id.code")
        if [ "$code" -gt 12288 ] || [ "$used" -gt 12288 ]; then
            echo "FAIL weapon $id hand $hand uses $used bytes (code+data $code) > 0x3000"
            exit 1
        fi
        # the header's first pointer (EntityWeaponAttack) is in this hand's range
        first=$(od -An -tx4 -N4 "$OUT/w${hand}_$id.code" | tr -d ' ')
        if [ $((0x$first)) -lt $base ] || [ $((0x$first)) -ge $((base + 0x3000)) ]; then
            echo "FAIL weapon $id hand $hand header starts with 0x$first"
            exit 1
        fi
        cp "$OUT/f_$id.bin" "$OUT/w${hand}_$id.bin"
        cat "$OUT/w${hand}_$id.code" >>"$OUT/w${hand}_$id.bin"
        truncate -s $((0x7000)) "$OUT/w${hand}_$id.bin"
        printf "weapon %s hand %d: 0x%x bytes used (code+data 0x%x)\n" "$id" "$hand" "$used" "$code"
    done
done
