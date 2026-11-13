#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: BIN/MARIA2.BIN + BIN/MARIA2_SD.BIN, the PSP (Dracula X
# Chronicles) Maria, the select screen's Maria "PSP" version.
#
# MARIA2.BIN = src/maria (the PSP player overlay, fully decompiled) + the
# PS1 glue mods/sotn-ultimate/src/maria_psp/mar2.c, compiled with mipsel gcc as the upstream
# mods/mariapsp-forpsx does and linked at 0x8013C000 (mods/sotn-ultimate/src/maria_psp/mar2.ld).
# src/dra/cd.c loads it instead of MARIA.BIN when MOD_MARIA_PSP().
# MARIA2_SD.BIN = her PSP voice (tools/maria-sfx + gen_mar2_sounds.py).
#
# Needs the US UMD's files in disks/psp (mods/sotn-ultimate/tools/extract_psp.sh); the
# maria.bin assets are extracted once into src/maria/gen (its entry in
# mods/sotn-ultimate/tools/psp_us_assets.yaml).
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT=build/us/mod/mar2
mkdir -p "$OUT"
PSPBIN=disks/psp/PSPBIN/maria.bin
SPK=disks/psp/snd_e/SE/sd_mar2.spk
if [ ! -f "$PSPBIN" ] || [ ! -f "$SPK" ]; then
    echo "FAIL build_mar2: no $PSPBIN / $SPK (mods/sotn-ultimate/tools/extract_psp.sh: the US UMD into disks/psp)"
    exit 1
fi

# the PSP maria.bin assets (sprites, palettes, weapon / crash / HUD art)
if [ ! -f src/maria/gen/maria.h ] || [ ! -f src/maria/gen/hud_en.h ]; then
    { echo "version: pspeu"; echo "files:"
      awk '/^  - target: disks\/psp\/PSPBIN\/maria.bin/{p=1}
           /^  - target:/ && !/maria.bin/{p=0} p' mods/sotn-ultimate/tools/psp_us_assets.yaml; } > "$OUT/assets.yaml"
    ./sotn.sh extract-assets "$OUT/assets.yaml"
    ./sotn.sh build-assets "$OUT/assets.yaml"
fi
# the other-language font and HUD art pl_main.c includes but never links
# (not on the US UMD): empty
for n in font_eu hud_sp hud_it hud_ge hud_fr; do
    [ -f "src/maria/gen/$n.h" ] || echo "0, // not on the US UMD; unused" > "src/maria/gen/$n.h"
done

CC=mipsel-linux-gnu-gcc
CFLAGS="-mno-abicalls -G0 -fno-pic -g -O2 -Iinclude -Iinclude/psxsdk -Isrc/maria -msoft-float -march=3000
 -funsigned-char -fpeephole -ffunction-cse -fpcc-struct-return -fcommon -fno-stack-protector
 -D_internal_version_us -DMARIA_SFX_MOD=1 -w"
# her subweapon drop frames from the PSP DRA (mods/sotn-ultimate/src/maria_psp/mar2_items.h)
python3 mods/sotn-ultimate/tools/gen_mar2_items.py
objs="$OUT/mar2.o"
$CC $CFLAGS -c mods/sotn-ultimate/src/maria_psp/mar2.c -o "$OUT/mar2.o"
for c in src/maria/*.c; do
    b=$(basename "$c" .c)
    $CC $CFLAGS -c "$c" -o "$OUT/$b.o"
    objs="$objs $OUT/$b.o"
done

# DRA / main symbols: retail list, with the mod DRA's actual addresses
sed 's;//.*;;' config/symbols.us.txt > "$OUT/symbols.txt"
echo "g_SfxData = 0x800BF554;" >> "$OUT/symbols.txt"
# DRA symbols not in symbols.us.txt (build_disc.sh checks them again after
# make disk relinks DRA): her HUD icon table pointer, DRA's sprite banks,
# the HUD hook drawing her two subweapon icons
: > "$OUT/mod_syms.txt"
for s in g_ModSubwpnIcons D_800A3B70 g_ModMar2HudIcons; do
    a=$(mipsel-linux-gnu-nm build/us/dra.elf | awk -v s="$s" '$3==s{print $1}')
    if [ -z "$a" ]; then
        echo "FAIL build_mar2: $s not in build/us/dra.elf"
        exit 1
    fi
    echo "$s = 0x$a;" >> "$OUT/mod_syms.txt"
done
# any symbol this overlay takes from symbols.us.txt that DRA has moved
for s in $(for o in $objs; do mipsel-linux-gnu-nm -u "$o"; done | awk '{print $2}' | sort -u); do
    a=$(mipsel-linux-gnu-nm build/us/dra.elf | awk -v s="$s" '$3==s{print $1}')
    r=$(grep -E "^$s = " "$OUT/symbols.txt" | head -1 | grep -oE "0x[0-9A-Fa-f]+" || true)
    if [ -n "$a" ] && [ -n "$r" ] && [ $((16#$a)) -ne $((r)) ]; then
        echo "$s = 0x$a;" >> "$OUT/mod_syms.txt"
    fi
done
$CC -nostdlib -Wl,-Map="$OUT/mar2.map" -Wl,--build-id=none -Wl,--no-check-sections \
    -T mods/sotn-ultimate/src/maria_psp/mar2.ld -T "$OUT/mod_syms.txt" \
    -T config/undefined_syms.us.txt -T "$OUT/symbols.txt" -o "$OUT/mar2.elf" $objs
mipsel-linux-gnu-objcopy -O binary -j .data -j .rodata -j .text "$OUT/mar2.elf" build/us/MARIA2.BIN

nm_of() { mipsel-linux-gnu-nm "$OUT/mar2.elf" | awk -v s="$1" '$3==s{print $1}'; }
[ "$(nm_of MARIA_override)" = "8013c000" ] || { echo "FAIL MARIA_override at $(nm_of MARIA_override)"; exit 1; }
[ "$(nm_of maria_sprites)" = "8013c020" ] || { echo "FAIL maria_sprites at $(nm_of maria_sprites)"; exit 1; }
end=$(nm_of mar2_bss_end)
if [ $((16#$end)) -gt $((0x80180000)) ]; then
    echo "FAIL MARIA2 ends at 0x$end, past the player region (0x80180000)"
    exit 1
fi
size=$(stat -c %s build/us/MARIA2.BIN)
if [ $((0x8013C000 + size)) -ne $((16#$(nm_of end))) ]; then
    echo "FAIL MARIA2.BIN size $size does not end at 'end' 0x$(nm_of end)"
    exit 1
fi
echo "MARIA2.BIN $size bytes, BSS ends at 0x$end (free $((0x80180000 - 16#$end)))"

# her PSP voice
if [ ! -f build/us/MARIA2_SD.BIN ] || [ mods/sotn-ultimate/tools/gen_mar2_sounds.py -nt build/us/MARIA2_SD.BIN ] ||
    [ mods/sotn-ultimate/tools/gen_gdn_sounds.py -nt build/us/MARIA2_SD.BIN ] ||
    [ ! -f "$OUT/sfx/SD_RIH.VB" ]; then
    cargo run --quiet --release --manifest-path tools/maria-sfx/Cargo.toml -- build "$SPK" "$OUT/sfx"
    python3 mods/sotn-ultimate/tools/gen_mar2_sounds.py "$OUT/sfx"
fi
