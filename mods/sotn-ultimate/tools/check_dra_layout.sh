#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: check that the modded DRA keeps the retail addresses other
# files depend on. main.exe and the sound banks use fixed addresses from
# dra_BSS_START onwards, and stages use g_SfxScripts (inside sound_data's
# data) at a fixed address, so all three boundaries must match retail.
#
# Two pads balance the mod's changes: MOD_DATA_PAD (src/dra/config_us.c, in
# DRA's data before sound_data) and MOD_DRA_PAD (src/dra/42398.c, rodata).
set -euo pipefail
cd "$(dirname "$0")/../../.."

MAP=build/us/dra.map
RETAIL_DATA_END=0x800db3b4
RETAIL_SOUND_DATA=0x800bd1e0
RETAIL_BSS_START=0x801362ac

data_end=$(grep -oE "0x[0-9a-f]+ +dra_DATA_END = " "$MAP" | awk '{print $1}')
sound_data=$(grep -E "^ \.data +0x[0-9a-f]+ +0x[0-9a-f]+ build/us/src/dra/sound_data\.c\.o" "$MAP" | awk '{print $2}')
bss_start=$(grep -oE "0x[0-9a-f]+ +dra_BSS_START = " "$MAP" | awk '{print $1}')
pad=$(grep -oE "#define MOD_DRA_PAD [0-9]+" src/dra/42398.c | awk '{print $3}')
dpad=$(grep -oE "#define MOD_DATA_PAD [0-9]+" src/dra/config_us.c | awk '{print $3}' || echo 0)

status=0
diff=$((RETAIL_SOUND_DATA - sound_data))
if [ $diff -ne 0 ]; then
    echo "FAIL sound_data .data is $sound_data, retail is $RETAIL_SOUND_DATA"
    echo "     set MOD_DATA_PAD in src/dra/config_us.c to $((dpad + diff)) and rebuild"
    status=1
else
    echo "OK   sound_data .data $sound_data (MOD_DATA_PAD $dpad)"
fi

if [ $((data_end)) -ne $((RETAIL_DATA_END)) ]; then
    echo "FAIL dra_DATA_END is $data_end, retail is $RETAIL_DATA_END"
    status=1
else
    echo "OK   dra_DATA_END $data_end"
fi

diff=$((RETAIL_BSS_START - bss_start))
if [ $diff -ne 0 ]; then
    echo "FAIL dra_BSS_START is $bss_start, retail is $RETAIL_BSS_START"
    echo "     set MOD_DRA_PAD in src/dra/42398.c to $((pad + diff)) and rebuild"
    status=1
else
    echo "OK   dra_BSS_START $bss_start (MOD_DRA_PAD $pad)"
fi
exit $status
