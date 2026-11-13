#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: check that build/us/DRA.BIN is exactly the linked
# build/us/dra.elf, i.e. no retail dirt bytes were patched over it (see
# tools/dirt_patcher.py).
cd "$(dirname "$0")/../../.."
linked=$(mktemp)
trap 'rm -f "$linked"' EXIT
mipsel-linux-gnu-objcopy -O binary build/us/dra.elf "$linked"
if cmp -s "$linked" build/us/DRA.BIN; then
    echo "OK   DRA.BIN matches dra.elf (no dirt patches)"
else
    echo "FAIL DRA.BIN differs from dra.elf:"
    cmp -l "$linked" build/us/DRA.BIN | head -5
    exit 1
fi
