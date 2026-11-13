#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: fix sector address headers in a rebuilt disc image.
#
# `make disk` copies the XA audio streams (SD/XA_STR1) from the retail image
# as raw 2352-byte sectors, keeping their original MSF headers. Once a modded
# file before them grows past its sector count, every following sector sits
# one or more sectors later than its header says, and the CD seek used for
# music and dialogue voices lands on the wrong place: no music, no voices.
#
# On Mode 2 sectors the header is not covered by EDC/ECC, so rewriting the
# MSF to the sector's real position is all that is needed.
import sys

path = sys.argv[1] if len(sys.argv) > 1 else "build/sotn.us.bin"
SYNC = b"\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00"


def bcd(n):
    return ((n // 10) << 4) | (n % 10)


fixed = 0
with open(path, "r+b") as f:
    f.seek(0, 2)
    count = f.tell() // 2352
    for lba in range(count):
        f.seek(lba * 2352)
        head = f.read(16)
        if head[:12] != SYNC or head[15] != 2:
            continue
        pos = lba + 150
        msf = bytes((bcd(pos // 4500), bcd(pos // 75 % 60), bcd(pos % 75)))
        if head[12:15] != msf:
            f.seek(lba * 2352 + 12)
            f.write(msf)
            fixed += 1
print(f"{path}: {count} sectors, rewrote {fixed} headers")
