#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: the whole release from source, and its proof.
#
#   mods/sotn-ultimate/tools/make_release.sh PS1_TRACK1.bin SATURN_TRACK1.bin PSP.iso OUTDIR
#
# 1. mods/sotn-ultimate/tools/build_disc.sh: the full disc (build/sotn_ultimate.bin)
# 2. builder/node/make_hollow.ts: the patch (PS1 Track 1 -> the disc with
#    every Saturn / PSP span blanked) and manifest.json, in OUTDIR
# 3. builder/node/make_builder_page.ts: OUTDIR/builder.html
# 4. builder/node/build_disc.ts: the builder's whole flow run on the three
#    discs (node, from the modules); its disc must be the release's (the
#    manifest's md5, which the page checks too)
# 5. builder/node/test_builder_bundle.cjs: the same through the page's own
#    bundle (dist/builder_core.js, run as the page runs it), so a page that
#    fails to load or builds something else can't be released
# Discs: the PS1 US Track 1, the Saturn JP Track 1 and the PSP US .iso the
# builder page takes (mods/sotn-ultimate/README.md); build_disc.sh reads the extracted discs in
# disks/. NODE: node 22 or later (default node).
set -euo pipefail
cd "$(dirname "$0")/../../.."
if [ $# -ne 4 ]; then
    echo "usage: $0 PS1_TRACK1.bin SATURN_TRACK1.bin PSP.iso OUTDIR" >&2
    exit 2
fi
P1=$(realpath "$1") SAT=$(realpath "$2") PSP=$(realpath "$3")
mkdir -p "$4"
OUT=$(realpath "$4")
export NODE=${NODE:-node}
DISC=build/sotn_ultimate.bin

# 1. (build_disc.sh starts again by itself when the layout moved; when it
# asks for another run, once more)
mkdir -p build
for pass in 1 2; do
    if MOD_OUT=$DISC bash mods/sotn-ultimate/tools/build_disc.sh 2>&1 | tee build/make_release.log; then
        break
    fi
    if [ $pass = 2 ] || ! grep -q "run build_disc.sh again" build/make_release.log; then
        exit 1
    fi
done
want=$(md5sum "$DISC" | cut -d" " -f1)
echo "disc: $DISC md5 $want"
# 2. + 3.
$NODE --no-warnings mods/sotn-ultimate/tools/builder/node/make_hollow.ts "$DISC" "$P1" "$SAT" "$PSP" "$OUT"
rm -f "$OUT/hollow.bin"
$NODE --no-warnings mods/sotn-ultimate/tools/builder/node/make_builder_page.ts "$OUT" "$OUT/builder.html"
# 4.
proof=$(mktemp --suffix=.bin)
trap 'rm -f "$proof"' EXIT
$NODE --no-warnings mods/sotn-ultimate/tools/builder/node/build_disc.ts "$OUT" "$P1" "$SAT" "$PSP" "$proof"
got=$(md5sum "$proof" | cut -d" " -f1)
if [ "$got" != "$want" ]; then
    echo "FAIL the builder made $got, the build $want" >&2
    exit 1
fi
# 5.
page=$($NODE --no-warnings mods/sotn-ultimate/tools/builder/node/test_builder_bundle.cjs "$OUT" "$P1" "$SAT" "$PSP" | tail -1)
if [ "$page" != "result md5 $want expected $want" ]; then
    echo "FAIL the page's bundle: $page" >&2
    exit 1
fi
echo "OK   the builder (and its page bundle) makes the release disc ($want): $OUT/builder.html"
