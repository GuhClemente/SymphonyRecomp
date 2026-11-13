#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: copy equipment/relic icons and icon palettes from the
# Saturn disc into the PSX icon banks, in place (DRA's data keeps its size).
#
# The Saturn's up-to-date icon bank is in the character CHR files
# (ALUCARD.CHR: icons at 0x20000 + n*128, 4bpp high nibble first; palettes at
# 0x2A000 + n*32, big-endian RGB555 in the same bit layout as PSX). ITEM.CHR
# is an older copy that lacks the new items. Icon and palette numbers match
# the PSX banks; the Saturn put its new art in slots that are empty or
# placeholder boxes on PSX. Only port slots listed below.
#
# Idempotent; edits the extracted assets, so run it after `make extract`.
import os
import struct
import zlib

from PIL import Image

# icon -> what uses it
ICONS = {
    275: "God Speed Shoes",
    231: "Glaring Cloth",
    236: "Rainbow Mantle",
    2: "Smelly Rice",
    3: "Delicious Meal",
    156: "Signed Pencil Board",
    210: "Alucard Spear",
    233: "Woodland Shield",
    212: "Astral Dagger",
    214: "Double Blade Axe",
    138: "Song Lyrics Card",
}
# PSX palette slot -> (Saturn palette slot, what uses it)
PALETTES = {
    281: (281, "God Speed Shoes"),
    # The Saturn recoloured Crystal cloak's palette 219 for Glaring Cloth.
    # Both cloaks exist in the port, so Glaring Cloth gets those colours in
    # the free slot 231 and 219 stays the Crystal cloak's.
    231: (219, "Glaring Cloth"),
    236: (236, "Rainbow Mantle"),
    # The Saturn's slots where they are free on PSX, else a free one
    2: (62, "Smelly Rice"),
    3: (54, "Delicious Meal"),
    141: (141, "Signed Pencil Board"),
    138: (138, "Alucard Spear"),
    211: (211, "Woodland Shield"),
    140: (140, "Astral Dagger"),
    136: (136, "Double Blade Axe"),
    139: (74, "Song Lyrics Card"),
}

ROOT = os.path.join(os.path.dirname(__file__), "../../..")
CHR = os.path.join(ROOT, "disks/saturn/ALUCARD.CHR")
GFX = os.path.join(ROOT, "assets/dra/g_GfxEquipIcon.png")
PAL = os.path.join(ROOT, "assets/dra/g_PalEquipIcon.bin")

chr_data = open(CHR, "rb").read()

img = Image.open(GFX)
assert img.mode == "P" and img.size == (16, 5120), (img.mode, img.size)
w, h = img.size
pixels = bytearray(img.tobytes())
for icon in ICONS:
    src = chr_data[0x20000 + icon * 128 : 0x20000 + (icon + 1) * 128]
    for y in range(16):
        for x in range(16):
            pixels[(icon * 16 + y) * w + x] = (
                src[y * 8 + x // 2] >> (4 if x % 2 == 0 else 0)
            ) & 15

# Rewrite only IDAT and keep every other chunk byte-identical: sotn-assets
# sizes the icon table from the PNG's depth and palette, and PIL's encoder
# changes those (4-bit or a 256-entry PLTE), which moves DRA's data section.
png = open(GFX, "rb").read()
raw = b"".join(b"\x00" + bytes(pixels[y * w : (y + 1) * w]) for y in range(h))
idat = zlib.compress(raw, 9)
out = bytearray(png[:8])
pos, wrote_idat = 8, False
while pos < len(png):
    (length,) = struct.unpack(">I", png[pos : pos + 4])
    kind = png[pos + 4 : pos + 8]
    if kind == b"IDAT":
        if not wrote_idat:
            out += struct.pack(">I", len(idat)) + b"IDAT" + idat
            out += struct.pack(">I", zlib.crc32(b"IDAT" + idat))
            wrote_idat = True
    else:
        out += png[pos : pos + 12 + length]
    pos += 12 + length
open(GFX, "wb").write(out)

pal = bytearray(open(PAL, "rb").read())
for n, (sat, _) in PALETTES.items():
    colors = list(
        struct.unpack(">16H", chr_data[0x2A000 + sat * 32 : 0x2A000 + (sat + 1) * 32])
    )
    colors[0] = 0  # index 0 is transparent; PSX banks keep it as 0x0000
    pal[n * 32 : (n + 1) * 32] = struct.pack("<16H", *colors)
open(PAL, "wb").write(pal)
print("ported icons", sorted(ICONS), "palettes", sorted(PALETTES))
