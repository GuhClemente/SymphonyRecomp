#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: build the modded PS1 disc and run every safety check.
# Copies the result to MOD_OUT (default build/sotn_ultimate.bin).
set -euo pipefail
# Go and Rust may live in the user's home; make sure they are on PATH
if [ -f ~/.profile ]; then source ~/.profile; fi
cd "$(dirname "$0")/../../.."
OUT=${MOD_OUT:-build/sotn_ultimate.bin}
# the work folder (mods/sotn-ultimate/tools/mod_work.py) and the node the builder's
# TypeScript runs on (22 or later)
export MOD_WORK=${MOD_WORK:-$PWD/build/mod-work}
export NODE=${NODE:-node}
# Retail dirt bytes are patched at fixed offsets; on the mod's moved layout
# they corrupt live data (see tools/dirt_patcher.py)
export SOTN_MOD_NO_DIRT=1

# Run a noisy build step with its output kept in build/us/mod_step.log: on a
# failure show its errors (else its tail) and stop; compile errors printed by
# a step that still succeeds are shown as a warning (before, >/dev/null hid
# them and a failed compile could look like "DRA code grew past its pad")
quiet() {
    local log=build/us/mod_step.log
    mkdir -p build/us
    if ! "$@" > "$log" 2>&1; then
        grep -iE "error|undefined reference" "$log" | head -40 >&2 ||
            tail -40 "$log" >&2
        echo "FAIL: $* (full output: $log)" >&2
        exit 1
    fi
    # cc1 reports as "file.c:123: message" (-w hides its warnings, so these
    # are errors; e.g. "`X' undeclared" dropped a statement from UpdateGame
    # and the build still passed, as the cc1 | as pipe exits with as)
    if grep -qiE "(parse|syntax) error|error:|^[^ :]+\.[ch]:[0-9]+: " "$log"; then
        echo "WARN: $* printed errors:" >&2
        grep -iE "(parse|syntax) error|error:|^[^ :]+\.[ch]:[0-9]+: " "$log" | head -10 >&2
        grep -iE "(parse|syntax) error|error:|^[^ :]+\.[ch]:[0-9]+: " "$log" \
            >> build/us/cc1_errors.log
    fi
}
rm -f build/us/cc1_errors.log

# DRA and SEL compile against include/lba_us.h (the disc's file sectors),
# which only make disk regenerates: when a disc file was added or resized,
# this build used stale sectors, so it starts again once (below make disk)
LBA_BEFORE=$(sha1sum include/lba_us.h 2>/dev/null || true)

python3 mods/sotn-ultimate/tools/gen_mod_layouts.py
# the Cursed Prison's doors in NO4 / NO0 (room table copies, doorway tiles)
python3 mods/sotn-ultimate/tools/gen_prs_links.py
# the Saturn's familiar card placements (NO2, ARE layouts, in place)
python3 mods/sotn-ultimate/tools/gen_card_layouts.py
.venv/bin/python mods/sotn-ultimate/tools/gen_mod_rooms.py
# the shaft's wall sconces (art, tables): src/st/np3/mod_sconce.h
.venv/bin/python mods/sotn-ultimate/tools/gen_np3_sconce.py
# Richter's / Maria's horse ride into the castle (art, tables):
# src/st/no3/mod_horse.h; its hoofbeats in NO3's bank: build/us/SD_ZKNO3;
# Richter's / Maria's NO3 bank with its neigh: build/us/NO3RIDE.BIN (its PSP
# dub part: gen_psp_sfx.py, below)
.venv/bin/python mods/sotn-ultimate/tools/gen_no3_horse.py
.venv/bin/python mods/sotn-ultimate/tools/gen_no3_sounds.py
# the Saturn's Maria boss fight in CEN (art, tables, scripts):
# mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h; her sounds in CEN's bank:
# build/us/SD_ZKCEN.VH/.VB, src/st/cen/mod_cen_sfx.h (the builder's
# TypeScript, mods/sotn-ultimate/tools/builder/src/cen_sounds.ts, as gen_cen_sounds.py)
.venv/bin/python mods/sotn-ultimate/tools/gen_cen_maria.py
$NODE --no-warnings mods/sotn-ultimate/tools/builder/node/cen_sounds.ts cen
.venv/bin/python mods/sotn-ultimate/tools/port_saturn_icons.py
python3 mods/sotn-ultimate/tools/gen_saturn_items.py
python3 mods/sotn-ultimate/tools/gen_saturn_enemies.py
# the PSP port's menu text picks: fixes in DRA's tables, and the strings that
# follow the Dub row (SEL's src/st/sel/mod_dub_text.h; checked below)
python3 mods/sotn-ultimate/tools/gen_menu_text.py
# the Saturn Librarian enemy list: src/st/lib/mod_bestiary.h
python3 mods/sotn-ultimate/tools/gen_saturn_bestiary.py
# Saturn-only music: build/us/XA_STR1.unpacked (retail + new tracks),
# mod_xa_music.h
.venv/bin/python mods/sotn-ultimate/tools/gen_saturn_music.py
# the Saturn-only stages' sound banks: the Garden's build/us/SD_ZKTE1.VH/.VB
# (mod_gdn_sfx.h), the Prison's SD_ZKTE2 (mod_prs_sfx.h), the Hell Garden's /
# the Soul Prison's SD_ZKTE3 / SD_ZKTE4 (mod_rgdn_sfx.h / mod_rprs_sfx.h).
# The builder's TypeScript (mods/sotn-ultimate/tools/builder/src/stage_sounds.ts, as
# gen_gdn / gen_prs / gen_rgdn / gen_rprs_sounds.py)
$NODE --no-warnings mods/sotn-ultimate/tools/builder/node/stage_sounds.ts
# the castle map blueprint with the Saturn's Garden cells: build/us/F_MAP.BIN
.venv/bin/python mods/sotn-ultimate/tools/paint_fmap.py disks/us/F_MAP.BIN disks/saturn/SUB_DISP.MAP build/us/F_MAP.BIN >/dev/null
.venv/bin/python mods/sotn-ultimate/tools/port_saturn_food.py
# the New Game character select's portraits: src/st/sel/mod_charsel_gfx.h
.venv/bin/python mods/sotn-ultimate/tools/gen_title_portraits.py
# the main menu's Sound mode button: src/st/sel/mod_sel_soundmode_gfx.h
.venv/bin/python mods/sotn-ultimate/tools/gen_sel_soundmode.py >/dev/null
python3 mods/sotn-ultimate/tools/patch_status_asm.py
# the Song Lyrics Card: the Saturn Yousei's singing rule in TT_005
python3 mods/sotn-ultimate/tools/patch_tt005_song.py
make build_assets >/dev/null
# The PSP (Dracula X Chronicles) English dub and script next to the PS1 ones,
# toggled in game (Player Select "Dub" row, include/game.h MOD_PSP_DUB_ON);
# MOD_PSP_DUB=0 builds the PS1 dub and script only. Voices:
# build/us/psp_dub/dub.json (encoded from disks/psp/snd_e/XA), taken by
# repack_xa.py --dub (second streams, src/dra/mod_psp_xa.h). Script:
# psp_script.py plans the PSP copies (and the voice ids with no PSP version,
# which the dub leaves alone: hence dub, script, dub) and writes the
# overlays' mod_psp_script.h; --apply fills the copies further down
MOD_PSP_DUB=${MOD_PSP_DUB:-1}
DUB_FLAGS=""
if [ "$MOD_PSP_DUB" = 1 ]; then
    .venv/bin/python mods/sotn-ultimate/tools/psp_dub.py >/dev/null
    python3 mods/sotn-ultimate/tools/psp_script.py
    .venv/bin/python mods/sotn-ultimate/tools/psp_dub.py | grep -v "^  skipped"
    DUB_FLAGS="--dub build/us/psp_dub/dub.json"
else
    python3 mods/sotn-ultimate/tools/psp_script.py --off
fi
# the PSP dub's player grunts: build/us/ALK_PSP.BIN, RIH_PSP.BIN (Alucard's /
# Richter's vab 1 with the PSP clips, from disks/psp sd_alk/sd_rih.spk;
# src/dra/cd.c loads them instead of VAB/SD_ALK / SD_RIH+SD_MAR when
# MOD_PSP_DUB_ON()); MOD_PSP_DUB=0: the retail banks
if [ "$MOD_PSP_DUB" = 1 ]; then
    .venv/bin/python mods/sotn-ultimate/tools/gen_psp_grunts.py | grep -v "^  "
else
    .venv/bin/python mods/sotn-ultimate/tools/gen_psp_grunts.py --off
fi
# the PSP dub's stage / familiar banks: build/us/SFX_PSP.BIN (boss, enemy and
# familiar voices as the PSP plays them, from disks/psp snd_e/SE + PS.ELF's
# tone lists; cd.c loads them when MOD_PSP_DUB_ON()) + src/dra/mod_psp_sfx.h
if [ "$MOD_PSP_DUB" = 1 ]; then
    .venv/bin/python mods/sotn-ultimate/tools/gen_psp_sfx.py
else
    .venv/bin/python mods/sotn-ultimate/tools/gen_psp_sfx.py --off
fi
# The third dub, "JP" (Player Select Dub row; include/game.h MOD_JP_DUB_ON):
# Japanese voices with the PSP English script. Voices: build/us/jp_dub/dub.json
# (the PSP's Japanese files, disks/psp sound/XA; Maria's CEN scene: the
# Saturn's takes), repack_xa.py --dub-jp (third streams, table in SEL:
# src/st/sel/mod_jp_xa.h); banks: build/us/JP_SD.BIN (its grunts, the PSP
# Maria's voice, the PSP boss Maria's CEN bank, the stage / familiar banks
# whose Japanese clips differ: src/st/sel/mod_jp_sfx.h). MOD_PSP_DUB=0: no
# JP versions (the banks are still built: the file is on the disc)
if [ "$MOD_PSP_DUB" = 1 ]; then
    .venv/bin/python mods/sotn-ultimate/tools/psp_dub.py --set jp
    # again, now with the Japanese lines' lengths: the JP script copies (boxes
    # held for the Japanese voices, slower typing; psp_script.py jp_adjust)
    python3 mods/sotn-ultimate/tools/psp_script.py >/dev/null
    DUB_FLAGS="$DUB_FLAGS --dub-jp build/us/jp_dub/dub.json"
    .venv/bin/python mods/sotn-ultimate/tools/gen_psp_sfx.py --set jp | grep "^jp sfx:"
else
    .venv/bin/python mods/sotn-ultimate/tools/gen_psp_sfx.py --set jp --off
fi
.venv/bin/python mods/sotn-ultimate/tools/gen_psp_grunts.py --set jp | grep -v "^  "
JP_MAR2=build/us/mod/mar2/sfx_jp
if [ ! -f "$JP_MAR2/SD_RIH.VB" ]; then
    cargo run --quiet --release --manifest-path tools/maria-sfx/Cargo.toml -- build \
        disks/psp/sound/SE/sd_mar2.spk "$JP_MAR2"
fi
python3 mods/sotn-ultimate/tools/gen_mar2_sounds.py "$JP_MAR2" build/us/MARIA2_SD_JP.BIN
.venv/bin/python mods/sotn-ultimate/tools/gen_cenp_sounds.py --set jp
python3 mods/sotn-ultimate/tools/gen_jp_sd.py
# Pack XA_STR1 (needs the fresh xa_music_config.h): build/us/XA_STR1.unpacked
# -> build/us/XA_STR1., rewrites the retail and mod XA configs; verifies
# itself (fails the build on any mismatch). --keep-all keeps the JP-only
# lines, --no-mono keeps dual-mono voices stereo
.venv/bin/python mods/sotn-ultimate/tools/repack_xa.py $DUB_FLAGS ${REPACK_XA_FLAGS:-}
# Saturn balance (equipment + enemy fields) as a runtime patch list for
# ModApplyBalance (src/dra/game_handlers.c, the Player Select Balance row)
python3 mods/sotn-ultimate/tools/gen_saturn_balance.py

# Build, then rebalance DRA's two pads: MOD_DATA_PAD keeps sound_data (and
# g_SfxScripts) at its retail address, MOD_DRA_PAD keeps the BSS there.
# The data pad goes first because it also moves everything after it.
SKIP_CHECK=1 quiet ./sotn.sh build us
# A rebuilt asset tool (its Go source changed) makes that build extract the
# generated headers again, xa_music_config.h too, over repack_xa.py's
# configs: start again once (the tool is current then)
if [ src/.assets_build_done_us -nt build/us/XA_STR1. ]; then
    echo "the asset build replaced the XA configs: building again"
    exec bash mods/sotn-ultimate/tools/build_disc.sh "$@"
fi
# Never balance pads blind: stop if the checker itself can't run
if ! { bash mods/sotn-ultimate/tools/check_dra_layout.sh 2>&1 || true; } | grep -q "dra_BSS_START"; then
    echo "check_dra_layout.sh did not run; refusing to continue" >&2
    exit 1
fi
dpad=$(bash mods/sotn-ultimate/tools/check_dra_layout.sh | grep -oE "MOD_DATA_PAD in [^ ]+ to -?[0-9]+" | grep -oE "\-?[0-9]+$" || true)
if [ -n "$dpad" ]; then
    if [ "$dpad" -lt 0 ]; then
        echo "DRA data grew past its pad: free more data first" >&2
        exit 1
    fi
    sed -i "s/#define MOD_DATA_PAD [0-9]*/#define MOD_DATA_PAD $dpad/" src/dra/config_us.c
    echo "MOD_DATA_PAD -> $dpad"
    SKIP_CHECK=1 quiet ./sotn.sh build us
fi
if bash mods/sotn-ultimate/tools/check_dra_layout.sh | grep -q "FAIL dra_DATA_END"; then
    bash mods/sotn-ultimate/tools/check_dra_layout.sh || true
    echo "DRA data section changed size after the data pad: investigate" >&2
    exit 1
fi
pad=$(bash mods/sotn-ultimate/tools/check_dra_layout.sh | grep -oE "MOD_DRA_PAD in [^ ]+ to -?[0-9]+" | grep -oE "\-?[0-9]+$" || true)
if [ -n "$pad" ]; then
    if [ "$pad" -lt 0 ]; then
        echo "DRA code grew past its pad: free more dead code first" >&2
        exit 1
    fi
    sed -i "s/#define MOD_DRA_PAD [0-9]*/#define MOD_DRA_PAD $pad/" src/dra/42398.c
    echo "MOD_DRA_PAD -> $pad"
fi
# the PSP English script into the built overlays (before BO6RIC2 copies BO6)
if [ "$MOD_PSP_DUB" = 1 ]; then
    python3 mods/sotn-ultimate/tools/psp_script.py --apply
fi

# The Saturn's new stages (TE1.BIN / F_TE1.BIN slots); needs the WRP build
bash mods/sotn-ultimate/tools/build_gdn.sh
# the Cursed Prison (TE2.BIN / F_TE2.BIN slots)
bash mods/sotn-ultimate/tools/build_prs.sh
# the Soul Prison, the inverted castle's (TE4.BIN / F_TE4.BIN slots)
bash mods/sotn-ultimate/tools/build_rprs.sh
# the Hell Garden, the inverted castle's Garden (TE3.BIN / F_TE3.BIN slots)
bash mods/sotn-ultimate/tools/build_rgdn.sh
# Left-hand copies of the weapons the mod changes (WEAPON1.BIN; the decomp
# only builds WEAPON0.BIN). Weapon 21 throws food; weapon 17 (Pentagram /
# Bat Pentagram) follows the Balance row. 37 (Jewel / Stone sword: gold),
# 44 (Heart Refresh: hearts) and 52 (Alucard Shield: its Shield Rod spell
# spends MP) touch g_Status fields the mod moved by 0x14 (HAND_ITEMS_CAP):
# the retail left-hand copies would read the old offsets
# 000 (swords): the Saturn's Terminus Est / Mourneblade subtypes
bash mods/sotn-ultimate/tools/build_weapon1.sh 021 017 037 044 052 001 004 005 014 031 033 000
python3 mods/sotn-ultimate/tools/check_weapon_hands.py
# the Saturn's new weapons 60-63: build/us/WEAPON2.BIN
python3 mods/sotn-ultimate/tools/build_saturn_weapons.py
# the Saturn's bestiary pictures: build/us/MONSTER2.BIN (its 18 new enemies) and
# build/us/MONSTER.BIN (retail, same size, 3 Saturn touch-ups). The builder's
# TypeScript (mods/sotn-ultimate/tools/builder, also run by the player's builder page); the
# internal Ghost Skull build still needs the Python (its extra record)
if grep -q "^#define MOD_OUTER_WALL_GHOST *$" include/game.h mods/sotn-ultimate/include/game_mod.h; then
    python3 mods/sotn-ultimate/tools/build_monster2.py
else
    $NODE mods/sotn-ultimate/tools/builder/node/monster2.ts
fi
# Richter's Saturn alternate costume: build/us/RIC2.BIN (RIC.BIN + new sheet)
.venv/bin/python mods/sotn-ultimate/tools/build_ric2.py
# boss Richter in that coat: build/us/BO6RIC2.BIN (BO6.BIN + the RIC2 sheet)
.venv/bin/python mods/sotn-ultimate/tools/build_bo6ric2.py
# ... and the kneeling Richter after Shaft's orb (TOP): g_ModTopRic2
(cd mods/sotn-ultimate/tools && python3 build_topric2.py)
# the Saturn's playable Maria: build/us/MARIA.BIN (src/ric + -DMOD_MARIA_SAT);
# her packed graphics only when the generator or Saturn data changed (slow)
if [ ! -f build/us/mod/mar/mar_sprites.h ] ||
    [ mods/sotn-ultimate/tools/gen_mar_assets.py -nt build/us/mod/mar/mar_sprites.h ]; then
    .venv/bin/python mods/sotn-ultimate/tools/gen_mar_assets.py
fi
# her Saturn voice and sounds: build/us/MARIA_SD.BIN (her vab 1 and vab 2,
# loaded by src/dra/cd.c) and build/us/mod/mar/mar_sfx.h. The builder's
# TypeScript (mods/sotn-ultimate/tools/builder/src/mar_sounds.ts, also run by the player's
# builder page; ~1 s), ported from gen_mar_sounds.py
$NODE --no-warnings mods/sotn-ultimate/tools/builder/node/mar_sounds.ts build/us/MARIA_SD.BIN \
    build/us/mod/mar/mar_sfx.h | tail -1
# the same banks with the PSP dub's English lines: build/us/MARIA_SD_PSP.BIN
$NODE --no-warnings mods/sotn-ultimate/tools/builder/node/mar_sounds.ts --psp build/us/MARIA_SD_PSP.BIN | tail -1
# her PS1-dub bank: a copy of the PSP one (her select panel offers no PS1 dub)
cp build/us/MARIA_SD_PSP.BIN build/us/MARIA_SD_PS1.BIN
bash mods/sotn-ultimate/tools/build_mar.sh
# the PSP Maria (Maria "PSP" on the select screen): build/us/MARIA2.BIN
# (src/maria + mods/sotn-ultimate/src/maria_psp glue) and her PSP voice build/us/MARIA2_SD.BIN
bash mods/sotn-ultimate/tools/build_mar2.sh
# MARIA_SD_PS1 + the PSP Maria's PS1-dub bank after it, once build_mar2.sh
# has made MARIA2_SD.BIN (same layout, 0x42 sectors each;
# src/dra/cd.c MOD_MAR_SD_SECTORS): a copy of her PSP bank.
# The file is cut back to one bank first
cp build/us/MARIA2_SD.BIN build/us/MARIA2_SD_PS1.BIN
for f in MARIA_SD_PSP MARIA2_SD_PS1; do
    if [ "$(stat -c %s build/us/$f.BIN)" -ne $((0x42 * 0x800)) ]; then
        echo "FAIL build/us/$f.BIN is not 0x42 sectors"; exit 1
    fi
done
truncate -s $((0x42 * 0x800)) build/us/MARIA_SD_PS1.BIN
cat build/us/MARIA2_SD_PS1.BIN >> build/us/MARIA_SD_PS1.BIN
# the PSP Maria boss fight (Alucard's "Boss Maria: PSP" on the select
# screen): build/us/CENP.BIN, CEN with it in the Saturn fight's place
bash mods/sotn-ultimate/tools/build_cenp.sh
# her Saturn HUD art in F_GAME2's empty VRAM: build/us/F_GAME2.BIN
.venv/bin/python mods/sotn-ultimate/tools/gen_mar_hud.py
# the pause menu System screen's "Ultimate settings" (Alucard games): code
# after Alucard's sprites in build/us/ARC_F2.BIN (mods/sotn-ultimate/src/menu/sysopt.c),
# against DRA's addresses (checked again after make disk below)
bash mods/sotn-ultimate/tools/build_sysopt.sh
# Richter in the coat in the endings: build/us/SELRIC2.BIN (SEL.BIN + the
# RIC2 art); made again below from the final SEL
(cd mods/sotn-ultimate/tools && python3 build_selric2.py)
quiet make disk
if [ "$(sha1sum include/lba_us.h)" != "$LBA_BEFORE" ]; then
    if [ -z "${MOD_BUILD_RERUN:-}" ]; then
        echo "include/lba_us.h changed (disc files moved): building again"
        MOD_BUILD_RERUN=1 exec bash "$0" "$@"
    fi
    echo "FAIL include/lba_us.h changed again on the second run" >&2
    exit 1
fi
# make disk relinks the overlays that include lba_us.h (SEL): patch them
# again and remake the image (BO6, copied into BO6RIC2 above, must not be
# among them: --check compares both)
if [ "$MOD_PSP_DUB" = 1 ]; then
    out=$(python3 mods/sotn-ultimate/tools/psp_script.py --apply)
    echo "$out"
    if echo "$out" | grep -q "BIN"; then
        VERSION=us quiet ./sotn.sh disk
    fi
    python3 mods/sotn-ultimate/tools/psp_script.py --check
fi
# SELRIC2 from the final SEL (relinked and patched above)
out=$(cd mods/sotn-ultimate/tools && python3 build_selric2.py)
echo "$out"
if echo "$out" | grep -q changed; then
    VERSION=us quiet ./sotn.sh disk
fi
# Raw-copied XA sectors keep retail addresses; fix them or music/voices break
python3 mods/sotn-ultimate/tools/fix_sector_headers.py build/sotn.us.bin
# cd.c's / SEL's literal sector numbers (not from lba_us.h) vs the image
python3 mods/sotn-ultimate/tools/check_cd_lba.py build/sotn.us.bin

bash mods/sotn-ultimate/tools/check_dra_layout.sh
# MARIA2.BIN was linked against DRA symbols (build/us/mod/mar2/mod_syms.txt:
# g_ModSubwpnIcons, D_800A3B70, any moved one) before make disk relinked
# DRA: none may have moved
while read -r s _ a; do
    a=${a%;}
    want=$(mipsel-linux-gnu-nm build/us/dra.elf | awk -v s="$s" '$3==s{print "0x" $1}')
    if [ "$want" != "$a" ]; then
        echo "FAIL MARIA2 linked against $s $a (DRA has $want): run build_disc.sh again"
        exit 1
    fi
done < build/us/mod/mar2/mod_syms.txt
echo "OK   MARIA2 DRA symbols ($(wc -l < build/us/mod/mar2/mod_syms.txt))"
# the same for the System screen options' code in ARC_F2.BIN
while read -r s _ a; do
    a=${a%;}
    want=$(mipsel-linux-gnu-nm build/us/dra.elf | awk -v s="$s" '$3==s{print "0x" $1}')
    if [ -z "$want" ]; then # (a main SDK function: config/symbols.us.txt)
        want=$(awk -v s="$s" '$1==s{sub(/;/,"",$3); print tolower($3)}' config/symbols.us.txt)
        a=$(echo "$a" | tr A-F a-f)
    fi
    if [ "$want" != "$a" ]; then
        echo "FAIL sysopt linked against $s $a (DRA has $want): run build_disc.sh again"
        exit 1
    fi
done < build/us/mod/sysopt/syms.txt
echo "OK   sysopt DRA symbols ($(wc -l < build/us/mod/sysopt/syms.txt)), ARC_F2.BIN $(stat -c %s build/us/ARC_F2.BIN) bytes"
bash mods/sotn-ultimate/tools/check_no_dirt.sh
python3 mods/sotn-ultimate/tools/check_stage_data.py
if [ "$MOD_PSP_DUB" = 1 ]; then
    python3 mods/sotn-ultimate/tools/check_psp_dub.py
fi
python3 mods/sotn-ultimate/tools/check_status_shift.py | tail -1
python3 mods/sotn-ultimate/tools/check_menu_text.py
# splat's absolute addresses inside moved symbols (stale = broken asm
# cutscenes; fix: check_abs_syms.py --write, then build again)
python3 mods/sotn-ultimate/tools/check_abs_syms.py || true
echo "--- files that differ from retail:"
sha1sum -c config/check.us.sha 2>/dev/null | grep -v ": OK$" || true

# an emulator holding the disc open blocks the copy: write a second name
if ! cp build/sotn.us.bin "$OUT" 2>/dev/null; then
    echo "$OUT is in use: writing ${OUT%.bin}2.bin instead"
    OUT="${OUT%.bin}2.bin"
    cp build/sotn.us.bin "$OUT"
fi
# its cue sheet names the bin (the main disc's .cue already exists)
cue="${OUT%.bin}.cue"
if [ ! -f "$cue" ]; then
    printf 'FILE "%s" BINARY\r\n  TRACK 01 MODE2/2352\r\n    FLAGS DCP\r\n    INDEX 01 00:00:00\r\n' \
        "$(basename "$OUT")" > "$cue"
fi
echo "copied to $OUT"
if [ -s build/us/cc1_errors.log ]; then
    echo "CC1 ERRORS: $(sort -u build/us/cc1_errors.log | wc -l) lines (build/us/cc1_errors.log)"
else
    echo "OK   no compiler errors in the build steps"
fi
