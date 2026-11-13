#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: BIN/MAR.BIN, the Saturn's playable Maria.
#
# The Saturn Maria (MARIA.PRG) is a fork of the Saturn Richter player code
# with her own sprites, animations and attacks. The same here: MAR.BIN is
# src/ric compiled a second time with -DMOD_MARIA_SAT (the deltas are under
# that define, generated data in build/us/mod/mar/) and linked by a copy of
# RIC's linker script. src/dra/cd.c loads it instead of RIC.BIN when
# g_PlayableCharacter == PLAYER_MARIA.
#
# Usage: build_mar.sh [--same]   (--same: no define; must equal RIC.BIN)
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT=build/us/mod/mar
mkdir -p "$OUT"
DEF=-DMOD_MARIA_SAT
if [ "${1:-}" = "--same" ]; then DEF=; fi

objs=""
for c in src/ric/*.c; do
    b=$(basename "$c")
    VERSION=us mipsel-linux-gnu-cpp $DEF -I"$OUT" -Isrc/ric -lang-c -Iinclude -Iinclude/psxsdk \
        -undef -Wall -fno-builtin -Dmips -D__GNUC__=2 -D__OPTIMIZE__ -D__mips__ -D__mips \
        -Dpsx -D__psx__ -D__psx -D_PSYQ -D__EXTENSIONS__ -D_MIPSEL -D_LANGUAGE_C \
        -DLANGUAGE_C -DNO_LOGS -DHACKS -DUSE_INCLUDE_ASM -D_internal_version_us -DSOTN_STR \
        "$c" | tools/sotn_str/target/release/sotn_str process |
        iconv --from-code=UTF-8 --to-code=Shift-JIS |
        bin/cc1-psx-26 -G0 -w -funsigned-char -fpeephole -ffunction-cse \
            -fpcc-struct-return -fcommon -fverbose-asm -msoft-float -g -quiet \
            -mcpu=3000 -fgnu-linker -mgas -gcoff -O2 |
        python3 tools/maspsx/maspsx.py --expand-div --aspsx-version=2.34 |
        python3 mods/sotn-ultimate/tools/align_bss.py |
        mipsel-linux-gnu-as -Iinclude -march=r3000 -mtune=r3000 -no-pad-sections \
            -O1 -G0 -o "$OUT/$b.o"
done
sed "s#build/us/src/ric/#$OUT/#g; s/\bric_/mar_/g" build/us/ric.ld > "$OUT/mar.ld"
# DRA data Maria's code reads that Richter's never did (US addresses)
echo "g_ClutIds = 0x8003C104;" > "$OUT/mar_syms.txt"
# DRA's XA volumes (BSS, fixed: check_dra_layout.sh), for her music ducking
echo "g_CdVolume = 0x8013B668;" >> "$OUT/mar_syms.txt"
echo "g_XaMusicVolume = 0x80139820;" >> "$OUT/mar_syms.txt"
# DRA's stopwatch music-mute state (main BSS), for her time stop
echo "D_8006BB00 = 0x8006BB00;" >> "$OUT/mar_syms.txt"
mipsel-linux-gnu-ld -nostdlib --no-check-sections -Map "$OUT/mar.map" -T "$OUT/mar.ld" \
    -T config/undefined_syms.us.txt -T build/us/config/undefined_funcs_auto.us.ric.txt \
    -T build/us/config/undefined_syms_auto.us.ric.txt -T "$OUT/mar_syms.txt" -o "$OUT/mar.elf"
mipsel-linux-gnu-objcopy -O binary "$OUT/mar.elf" build/us/MARIA.BIN
# no misaligned access to her statics (mods/sotn-ultimate/tools/align_bss.py)
python3 mods/sotn-ultimate/tools/check_align.py "$OUT/mar.elf" | tail -1
# the player region ends where stage overlays start
end=$(mipsel-linux-gnu-nm "$OUT/mar.elf" | awk '$3=="mar_BSS_END"{print $1}')
if [ -z "$end" ]; then end=$(mipsel-linux-gnu-nm -n "$OUT/mar.elf" | tail -1 | cut -c1-8); fi
if [ $((16#$end)) -gt $((0x80180000)) ]; then
    echo "FAIL MAR ends at 0x$end, past the player region (0x80180000)"
    exit 1
fi
echo "MARIA.BIN $(stat -c %s build/us/MARIA.BIN) bytes, ends at 0x$end (free $((0x80180000 - 16#$end)))"
