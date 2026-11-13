#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: the US UMD's (Castlevania: The Dracula X Chronicles,
# ULUS-10277) PSP_GAME/USRDIR/res/ps into disks/psp, which the build reads.
#
#   mods/sotn-ultimate/tools/extract_psp.sh PSP.iso
set -euo pipefail
cd "$(dirname "$0")/../../.."
if [ $# -ne 1 ]; then
    echo "usage: $0 PSP.iso (the US UMD, ULUS-10277)" >&2
    exit 2
fi
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
7z x -y -o"$tmp" "$1" "PSP_GAME/USRDIR/res/ps/*" >/dev/null
[ -f "$tmp/PSP_GAME/USRDIR/res/ps/PS.ELF" ] || { echo "no res/ps/PS.ELF in $1" >&2; exit 1; }
rm -rf disks/psp
mv "$tmp/PSP_GAME/USRDIR/res/ps" disks/psp
echo "disks/psp: $(find disks/psp -type f | wc -l) files from $1"
