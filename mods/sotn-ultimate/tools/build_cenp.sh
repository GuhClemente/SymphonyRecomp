#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: build/us/CENP.BIN (BIN/CENP.BIN on the disc), the Castle
# Center with the PSP (Dracula X Chronicles) Maria boss fight in place of
# the Saturn one. src/dra/cd.c loads it instead of ST/CEN/CEN.BIN when the
# select screen's Alucard panel has "Maria boss: PSP" (MOD_MARIA_BOSS_PSP);
# CEN.BIN (the Saturn fight) stays the default. Both fights don't fit in one
# overlay (0x80180000-0x801E0000).
#
# CENP = CEN's own sources compiled as the decomp does (cc1-psx) with
# -DMOD_CENP (src/st/cen/e_maria.c, e_init.c, cen.h), linked with CEN's link
# script, + the PSP boss (src/st/cen_psp/e_maria*.c, its player engine and
# fight logic, with the glue mods/sotn-ultimate/src/maria_psp/mod_cenp.c) compiled with mipsel gcc as
# MARIA2.BIN's src/maria is (mods/sotn-ultimate/tools/build_mar2.sh), linked after CEN's
# code: CEN's data keeps its addresses (the scripts' portrait pointers).
#
# Needs the US UMD's files (disks/psp, as build_mar2.sh): the PSP cen.bin
# assets (her sprites, weapon art, palettes, blueprint tables) are extracted
# once into build/us/mod/cenp/st/cen/gen. Needs build/us/stcen.ld and
# build/us/dra.elf (run after the main build).
set -euo pipefail
source ~/.profile
cd "$(dirname "$0")/../../.."
OUT=build/us/mod/cenp
PSPBIN=disks/psp/PSPBIN/cen.bin
if [ ! -f "$PSPBIN" ]; then
    echo "FAIL build_cenp: no $PSPBIN (mods/sotn-ultimate/tools/extract_psp.sh: the US UMD into disks/psp)"
    exit 1
fi
[ -f build/us/stcen.ld ] || { echo "FAIL build_cenp: build/us/stcen.ld missing"; exit 1; }
mkdir -p "$OUT/st/cen" "$OUT/st/cen_psp" "$OUT/obj" "$OUT/cen"

# the PSP cen.bin assets, in a mirror tree: the boss sources include
# "../cen/gen/..." and "../cen/cen.h" (forwarded to the real cen.h)
echo '#include "../../../../../../src/st/cen/cen.h"' > "$OUT/st/cen/cen.h"
cp src/st/cen/mod_cen_sfx.h "$OUT/st/cen/"
# the PSP dub's CEN bank (her PSP English clips; build/us/CENP_SD.bin, needs
# build/us/MARIA2_SD.BIN) and its ids: mods/sotn-ultimate/src/maria_psp/mod_cenp_sfx.h
.venv/bin/python mods/sotn-ultimate/tools/gen_cenp_sounds.py
cp mods/sotn-ultimate/src/maria_psp/mod_cenp_sfx.h "$OUT/st/cen_psp/"
if [ ! -f "$OUT/st/cen/gen/maria_spr.h" ] || [ ! -f "$OUT/st/cen/gen/blueprints.h" ]; then
    { echo "version: pspeu"; echo "files:"
      awk '/^  - target: disks\/psp\/PSPBIN\/cen.bin/{p=1}
           /^  - target:/ && !/\/cen.bin/{p=0} p' mods/sotn-ultimate/tools/psp_us_assets.yaml |
          sed -e "s#asset_path: assets/st/cen#asset_path: $OUT/assets/cen#" \
              -e "s#src_path: src/st/cen#src_path: $OUT/st/cen#"; } > "$OUT/assets.yaml"
    ./sotn.sh extract-assets "$OUT/assets.yaml" >/dev/null
    ./sotn.sh build-assets "$OUT/assets.yaml" >/dev/null
fi
# the Saturn fight's two scripts (the PS1 scene split around the fight)
awk '/^u8 g_MbScriptPre\[\]/{p=1} p{print} p && /^};/{n++; if (n == 2) exit}' \
    mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h > "$OUT/st/cen_psp/mb_scripts.h"
grep -q "g_MbScriptPost" "$OUT/st/cen_psp/mb_scripts.h" ||
    { echo "FAIL build_cenp: scripts not found in mod_maria_boss_data.h"; exit 1; }

# the PSP boss + glue (gcc, as build_mar2.sh)
boss=""
for c in src/st/cen_psp/e_maria*.c src/st/cen_psp/spritebanks.c mods/sotn-ultimate/src/maria_psp/mod_cenp.c; do
    cp "$c" "$OUT/st/cen_psp/"
done
CC=mipsel-linux-gnu-gcc
CFLAGS="-mno-abicalls -G0 -fno-pic -g -O2 -Iinclude -Iinclude/psxsdk -msoft-float -march=3000
 -funsigned-char -fpeephole -ffunction-cse -fpcc-struct-return -fcommon -fno-stack-protector
 -D_internal_version_us -DMOD_CENP -DMOD_CENP_BOSS -w"
for c in "$OUT"/st/cen_psp/*.c; do
    b=$(basename "$c" .c)
    $CC $CFLAGS -c "$c" -o "$OUT/obj/$b.o"
    boss="$boss $OUT/obj/$b.o"
done

# CEN's own sources with -DMOD_CENP (cc1-psx, as build.ninja's psx-cc)
sed -e "s#build/us/src/st/cen/#$OUT/cen/#g" -e 's/stcen_/stcenp_/g' \
    -e 's/\.stcen\b/.stcenp/g' build/us/stcen.ld > "$OUT/stcenp.ld"
objs=$(grep -o "$OUT/cen/[^()]*\.o" "$OUT/stcenp.ld" | sort -u)
for o in $objs; do
    rel=${o#$OUT/cen/}
    src=src/st/cen/${rel%.o}
    mkdir -p "$(dirname "$o")"
    VERSION=us mipsel-linux-gnu-cpp -Isrc/st/cen -lang-c -Iinclude -Iinclude/psxsdk -undef \
        -Wall -fno-builtin -Dmips -D__GNUC__=2 -D__OPTIMIZE__ -D__mips__ -D__mips -Dpsx \
        -D__psx__ -D__psx -D_PSYQ -D__EXTENSIONS__ -D_MIPSEL -D_LANGUAGE_C -DLANGUAGE_C \
        -DNO_LOGS -DHACKS -DUSE_INCLUDE_ASM -D_internal_version_us -DSOTN_STR -DMOD_CENP \
        "$src" 2>/dev/null |
        tools/sotn_str/target/release/sotn_str process |
        iconv --from-code=UTF-8 --to-code=Shift-JIS |
        bin/cc1-psx-26 -G0 -w -funsigned-char -fpeephole -ffunction-cse -fpcc-struct-return \
            -fcommon -fverbose-asm -msoft-float -g -quiet -mcpu=3000 -fgnu-linker -mgas \
            -gcoff -O2 |
        python3 tools/maspsx/maspsx.py --expand-div --aspsx-version=2.34 |
        mipsel-linux-gnu-as -Iinclude -march=r3000 -mtune=r3000 -no-pad-sections -O1 -G0 -o "$o"
done

# the boss after CEN's code (data, rodata, text) and BSS
ins_text=""
ins_bss=""
for o in $boss; do
    ins_text="$ins_text        $o(.text);\n"
done
for o in $boss; do
    ins_text="$ins_text        $o(.rodata);\n        $o(.rodata.*);\n        $o(.data);\n        $o(.data.*);\n        $o(.sdata);\n"
    ins_bss="$ins_bss        $o(.sbss);\n        $o(.bss);\n        $o(.bss.*);\n        $o(.scommon);\n        $o(COMMON);\n"
done
python3 - "$OUT/stcenp.ld" "$ins_text" "$ins_bss" <<'EOF'
import sys
p, t, b = sys.argv[1], sys.argv[2].replace("\\n", "\n"), sys.argv[3].replace("\\n", "\n")
s = open(p).read()
a = "        . = ALIGN(., 4);\n        stcenp_TEXT_END = .;"
assert s.count(a) == 1
s = s.replace(a, "        . = ALIGN(., 8);\n        cenp_text_start = .;\n" + t + a)
a = "        . = ALIGN(., 4);\n        stcenp_BSS_END = .;"
assert s.count(a) == 1
s = s.replace(a, "        . = ALIGN(., 8);\n        cenp_bss_start = .;\n" + b +
              "        . = ALIGN(., 4);\n        cenp_bss_end = .;\n" + a)
open(p, "w").write(s)
EOF

# DRA / main symbols the boss takes that CEN's own lists lack: the mod
# DRA's address when it has the symbol, else the retail list
sed 's;//.*;;' config/symbols.us.txt > "$OUT/symbols.txt"
known=$(cat config/undefined_syms.us.txt build/us/config/undefined_funcs_auto.us.stcen.txt \
    build/us/config/undefined_syms_auto.us.stcen.txt | grep -oE '^ *[A-Za-z_][A-Za-z0-9_]*' | tr -d ' ' | sort -u)
defined=$(for o in $boss $objs; do mipsel-linux-gnu-nm --defined-only "$o"; done | awk '{print $3}' | sort -u)
: > "$OUT/boss_syms.txt"
for s in $(for o in $boss; do mipsel-linux-gnu-nm -u "$o"; done | awk '{print $2}' | sort -u); do
    case "$s" in cenp_*) continue ;; esac # the link script's
    if grep -qx "$s" <<< "$defined"; then continue; fi
    if grep -qx "$s" <<< "$known"; then continue; fi
    a=$(mipsel-linux-gnu-nm build/us/dra.elf | awk -v s="$s" '$3==s{print $1, $2}' | head -1)
    if [ -n "$a" ]; then
        # no direct calls into DRA's code (make disk relinks it): g_api
        case "${a#* }" in T|t)
            echo "FAIL build_cenp: the boss calls DRA's $s directly (use g_api / mod_cenp.c)"
            exit 1 ;;
        esac
        echo "$s = 0x${a% *};" >> "$OUT/boss_syms.txt"
        continue
    fi
    r=$(grep -E "^$s = " "$OUT/symbols.txt" | head -1 || true)
    if [ -n "$r" ]; then
        echo "$r" >> "$OUT/boss_syms.txt"
        continue
    fi
    echo "FAIL build_cenp: no address for $s"
    exit 1
done

mipsel-linux-gnu-ld -nostdlib --no-check-sections -Map "$OUT/stcenp.map" -T "$OUT/stcenp.ld" \
    -T config/undefined_syms.us.txt -T build/us/config/undefined_funcs_auto.us.stcen.txt \
    -T build/us/config/undefined_syms_auto.us.stcen.txt -T "$OUT/boss_syms.txt" \
    -o "$OUT/stcenp.elf"
mipsel-linux-gnu-objcopy -O binary "$OUT/stcenp.elf" build/us/CENP.BIN

nm_of() { mipsel-linux-gnu-nm "$1" | awk -v s="$2" '$3==s{print $1}' | head -1; }
end=$(nm_of "$OUT/stcenp.elf" stcenp_BSS_END)
if [ $((16#$end)) -gt $((0x801E0000)) ]; then
    echo "FAIL CENP ends at 0x$end, past 0x801E0000"; exit 1
fi
size=$(stat -c %s build/us/CENP.BIN)
if [ $((0x80180000 + size)) -ne $((16#$end)) ]; then
    echo "FAIL CENP.BIN size $size does not end at stcenp_BSS_END 0x$end"; exit 1
fi
# padded (zeros) to the whole stage area: a constant size keeps the disc
# layout (XA_STR1's sectors, lba_us.h) stable when the fight's code changes
truncate -s $((0x60000)) build/us/CENP.BIN
# + the PSP dub's CEN bank (VH 4 sectors + VB, as ST/CEN/SD_ZKCEN.VH/.VB) at
# 0x60000: src/dra/cd.c loads it when MOD_PSP_DUB_ON() (the stage load
# reads the first 0x60000 bytes only)
cat build/us/CENP_SD.bin >> build/us/CENP.BIN
# + the PS1 dub's CEN banks, same layout: the Saturn boss's at 0x7C800, the
# PSP boss's at 0x99000 (src/dra/cd.c; hers in the PSP dub's slot layout).
# The Saturn boss's sounds (the Saturn fight's bank as is; for her,
# gen_cenp_sounds.py --ps1sat, as the builder's TypeScript
# mods/sotn-ultimate/tools/builder/src/cenp_sounds.ts)
${NODE:-node} --no-warnings         mods/sotn-ultimate/tools/builder/node/cen_sounds.ts cenp-ps1sat
for f in CENP_SD CEN_SD_PS1 CENP_SD_PS1; do
    if [ "$(stat -c %s build/us/$f.bin)" -ne $((0x1C800)) ]; then
        echo "FAIL build_cenp: build/us/$f.bin is not 0x1C800 bytes"; exit 1
    fi
done
cat build/us/CEN_SD_PS1.bin build/us/CENP_SD_PS1.bin >> build/us/CENP.BIN
# CEN's data must keep its addresses (scripts point into it)
rod=$(nm_of build/us/stcen.elf stcen_RODATA_START)
bad=$(comm -23 <(mipsel-linux-gnu-nm build/us/stcen.elf | awk -v r=$((16#$rod)) \
        'strtonum("0x"$1) < r && strtonum("0x"$1) >= 0x80180000 && $3 !~ /^stcen_/ {print $1, $3}' | sort -u) \
      <(mipsel-linux-gnu-nm "$OUT/stcenp.elf" | awk '{print $1, $3}' | sort -u) | head -5)
if [ -n "$bad" ]; then
    echo "FAIL CENP moved CEN data symbols:"; echo "$bad"; exit 1
fi
for s in EntityMariaPsp g_MbScriptPre maria_sprites cenp_bss_start; do
    [ -n "$(nm_of "$OUT/stcenp.elf" $s)" ] || { echo "FAIL CENP lacks $s"; exit 1; }
done
echo "cenp: CENP.BIN $size bytes, ends at 0x$end ($(( (0x801E0000 - 16#$end) / 1024 )) KB free)"
