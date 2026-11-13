#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: build the Underground Garden stage overlay (mods/sotn-ultimate/src/garden/gdn).
#
# The decomp's build only knows the retail overlays, so this compiles the new
# stage the way tools/builds/gen.py compiles WRP (whose sources mods/sotn-ultimate/src/garden/gdn
# started from), links it at 0x80180000 with WRP's link script layout, and
# writes build/us/TE1.BIN. The Garden takes the unused TE1 disc slot (make
# disk prefers build/us files); its tiles go to build/us/F_TE1.BIN
# (mods/sotn-ultimate/tools/gdn_assets.py). Stage id MOD_STAGE_GDN, see include/game.h.
set -euo pipefail
# Go and Rust live in the user's home; make sure they are on PATH
source ~/.profile
cd "$(dirname "$0")/../../.."
OUT=build/us/mod/gdn
rm -rf "$OUT"; mkdir -p "$OUT"

.venv/bin/python mods/sotn-ultimate/tools/gdn_assets.py
# gdn_assets.py writes build/us/gdn_assets.us.yaml: the stage's asset
# config plus its compressed sprite blocks
./sotn.sh build-assets build/us/gdn_assets.us.yaml >/dev/null

# link script: WRP's, with the Garden's objects (and no warp.c)
[ -f build/us/stwrp.ld ] || { echo "FAIL build/us/stwrp.ld missing (build WRP first)"; exit 1; }
sed -e 's#build/us/src/st/wrp/#build/us/mod/gdn/#g' -e 's/stwrp/stgdn/g' \
    -e '/warp\.c\.o/d' -e '/^ *warp_c = \.;/d' build/us/stwrp.ld > "$OUT/stgdn.ld"

objs=$(grep -o 'build/us/mod/gdn/[^()]*\.o' "$OUT/stgdn.ld" | sort -u)
for o in $objs; do
    rel=${o#build/us/mod/gdn/}
    src=mods/sotn-ultimate/src/garden/gdn/${rel%.o}
    mkdir -p "$(dirname "$o")"
    # -Isrc/st/wrp: the shared stage headers (src/st/*.h) include "../x.h" from a
    # stage folder; WRP is the stage these are compiled like
    VERSION=us mipsel-linux-gnu-cpp -Imods/sotn-ultimate/src/garden/gdn -Isrc/st/wrp -lang-c -Iinclude -Iinclude/psxsdk -undef \
        -Wall -fno-builtin -Dmips -D__GNUC__=2 -D__OPTIMIZE__ -D__mips__ -D__mips -Dpsx \
        -D__psx__ -D__psx -D_PSYQ -D__EXTENSIONS__ -D_MIPSEL -D_LANGUAGE_C -DLANGUAGE_C \
        -DNO_LOGS -DHACKS -DUSE_INCLUDE_ASM -D_internal_version_us -DSOTN_STR "$src" |
        tools/sotn_str/target/release/sotn_str process |
        iconv --from-code=UTF-8 --to-code=Shift-JIS |
        bin/cc1-psx-26 -G0 -w -funsigned-char -fpeephole -ffunction-cse -fpcc-struct-return \
            -fcommon -fverbose-asm -msoft-float -g -quiet -mcpu=3000 -fgnu-linker -mgas \
            -gcoff -O2 |
        python3 tools/maspsx/maspsx.py --expand-div --aspsx-version=2.34 |
        mipsel-linux-gnu-as -Iinclude -march=r3000 -mtune=r3000 -no-pad-sections -O1 -G0 -o "$o"
done

# D_80097904: the entity graphics bank the game reloads after the menu, which
# the Garden's room 0 sets (mods/sotn-ultimate/src/garden/gdn/st_update.c); currentMusicId and
# stopMusicFlag: the boss room's music (mods/sotn-ultimate/src/garden/gdn/e_mod_boss.h)
mipsel-linux-gnu-ld -nostdlib --no-check-sections -Map "$OUT/stgdn.map" -T "$OUT/stgdn.ld" \
    -T config/undefined_syms.us.txt -T build/us/config/undefined_funcs_auto.us.stwrp.txt \
    -T build/us/config/undefined_syms_auto.us.stwrp.txt \
    --defsym D_80097904=0x80097904 --defsym currentMusicId=0x80097910 \
    --defsym stopMusicFlag=0x80097928 --defsym RotTransPers=0x8001824C \
    -o "$OUT/stgdn.elf"
mipsel-linux-gnu-objcopy -O binary "$OUT/stgdn.elf" build/us/TE1.BIN

# the overlay, BSS included, must end before CASTLE_MAP_PTR (0x801E0000)
end=$(grep -oE '0x[0-9a-f]+ +stgdn_BSS_END' "$OUT/stgdn.map" | awk '{print $1}')
if [ $((end)) -gt $((0x801E0000)) ]; then
    echo "FAIL gdn overlay ends at $end, past 0x801E0000"; exit 1
fi
echo "gdn: TE1.BIN $(stat -c %s build/us/TE1.BIN) bytes, ends at $end"
