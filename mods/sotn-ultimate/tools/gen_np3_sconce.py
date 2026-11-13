#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Saturn's candelabras in the Castle Entrance's shaft.

The three candles of the shaft extended to the Underground Garden (NP3's
layout copy, mods/sotn-ultimate/tools/gen_mod_layouts.py) are the Saturn's tall candelabras
with their glow, the Cursed Prison's corridor ones. As the Saturn Entrance
(STAGE_01) draws them: its candle type 3 (config 0x060F82D4: bank 0x060F610C,
frames 0x060F60BC, animation frames 1-4, glow records 0x060F8262,
hitboxOffY -40), whose frames stand on the entity's floor line (the Prison's
copy of the art is drawn 47 px higher); the stand left is frame 5,
breakable type 2 of NP3 (src/st/np3/e_breakable.c, mods/sotn-ultimate/src/castle/mod_candle.h).

NP3 is a retail overlay whose data section must not move, so the art and the
tables that name it are rodata copies the stage header points at, in
src/st/np3/mod_sconce.h (included from header.c, after the generated tables
whose static entries the copies list):
  - the art: an uncompressed 4bpp block (GFX_BANK_4BPP) for one quadrant of
    the stage sprite pages, as graphics bank SCONCE_GFX_BANK; the shaft room
    (which loads no sprite bank on the PS1) takes it as its entityGfxId
    (mods/sotn-ultimate/tools/gen_mod_rooms.py), so the engine loads it as the room is
    entered and reloads it after the pause menu;
  - the frames: sprite bank SCONCE_ANIMSET (1-4 the flame, 5 the stand);
  - the palettes: a copy of NP3's palette list with them appended at
    SCLUT_BASE (after the stage's last, PAL_HAMMER's 0x2F0-0x2F2).
"""
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from saturn_sprites import SaturnStage, PsxSpriteSheet  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
SAT = os.path.join(ROOT, "disks/saturn")
GEN = os.path.join(ROOT, "src/st/np3/gen")
OUT = os.path.join(ROOT, "src/st/np3/mod_sconce.h")
STAGE = "np3"
SPRITE_BANKS_H = os.path.join(GEN, "sprite_banks.h")

PRG = "STAGE_01"
BANK16 = (0x060F610C, 0x060F60BC)  # STAGE_01's candelabra bank
FRAMES = [1, 2, 3, 4, 5]           # the flame, then the stand
GLOW = 0x060F8262                  # the candelabra's glow records {dx, dy, image}
GLOW_COLOURS = [0] * 10 + [0x8044, 0x8067, 0x808A, 0x80CE, 0x8112, 0]  # gdn_assets
QUADRANT = (18, 0)                 # VRAM (128, 256): the shaft room has no other sprites
SCONCE_GFX_BANK = 8                # gfxBanks index (NP3's 8-18 are empty)
SCONCE_ANIMSET = 13                # spriteBanks index (NP3's 13+ are empty)
# more spriteBanks entries (index -> extern symbol): the moved save room's
# picture with a taller doorway (src/st/<stage>/e_red_door.c, e_room_fg.c)
EXTRA_SPRITES = {14: "g_ModSaveRoomSprites"}
SCLUT_BASE = 0x2F4
COMMENT = "the Saturn's tall candelabra (Cursed Prison art) for the shaft's candles"
GLOW_FLIP = 0

# The first visit's Entrance (NO3) has the same shaft (gen_mod_rooms.py):
# the same candelabra, from STAGE_01's bank (the Saturn's STAGE_0X draws the
# same art). Same slots: NO3's gfxBanks 7-19 and spriteBanks 10+ are empty,
# its stage palettes end at 0x2EF (and the horse ride's, 0x2D9.., are only
# loaded in the forest run).
NO3 = {
    "GEN": os.path.join(ROOT, "src/st/no3/gen"),
    "OUT": os.path.join(ROOT, "src/st/no3/mod_sconce.h"),
    "STAGE": "no3",
    "SPRITE_BANKS_H": os.path.join(ROOT, "src/st/no3/gen/sprite_banks.h"),
}

# The Reverse Entrance (RNO3)'s shaft, as the Saturn's inverted one
# (RSTAGE01 candle type 3, config 0x060F0BC4: bank 0x060EED94, frames
# 0x060EED44, glow 0x060F0B52; its art hangs from the ceiling, the flames
# below). Same slots: RNO3's gfxBanks 3-14 and spriteBanks 9+ are empty, its
# stage palettes end at 0x2EF (pal_2e0).
RNO3 = {
    "GEN": os.path.join(ROOT, "src/st/rno3/gen"),
    "OUT": os.path.join(ROOT, "src/st/rno3/mod_sconce.h"),
    "STAGE": "rno3",
    "SPRITE_BANKS_H": os.path.join(ROOT, "src/st/rno3/gen/us/sprite_banks.h"),
    "PRG": "RSTAGE01",
    "BANK16": (0x060EED94, 0x060EED44),
    "GLOW": 0x060F0B52,
    "SCLUT_BASE": 0x2F0,
    "COMMENT": "the Saturn's inverted Entrance candelabra (hanging) for the shaft's candles",
    # the candle is drawn turned around (ROT(180)) and so is its glow on the
    # Saturn, whose records place it so: the glow sprite is stored turned
    # around
    "GLOW_FLIP": 3,
}


def table(text, start):
    """the entries of a generated C array, from its opening line"""
    body = text[text.index(start) + len(start):]
    body = body[:body.index("};")]
    return [e.strip() for e in body.split(",\n") if e.strip()]


def main():
    st = SaturnStage(os.path.join(SAT, PRG + ".PRG"), os.path.join(SAT, PRG + ".CHR"),
                     os.path.join(SAT, "0.BIN"))
    glow_recs = [struct.unpack_from(">hhh", st.prg, GLOW - st.base + f * 10) for f in range(4)]

    def fill(sheet):
        frames = [sheet.frame(st, *BANK16, f) for f in FRAMES]
        glow = [sheet.image_part(st, BANK16[0], img, dx, dy, GLOW_COLOURS, GLOW_FLIP)
                for dx, dy, img in glow_recs]
        return frames, glow

    probe = PsxSpriteSheet([(0, 0)] * 4, SCLUT_BASE)
    fill(probe)
    sheet = PsxSpriteSheet([QUADRANT], SCLUT_BASE)
    sheet.prepack(st, probe.requests)
    frames, glow = fill(sheet)
    assert sheet.used_quadrants() == 1
    assert SCLUT_BASE + len(sheet.cluts) <= 0x300, "palettes past the stage's range"
    rows = max(y + h for _, _, y, _, h in sheet.placed.values())
    rows = (rows + 1) & ~1
    art = sheet.quadrant_pixels(0)[:rows * 64]

    gb = open(os.path.join(GEN, "graphics_banks.h")).read()
    names = table(gb, "u_long** gfxBanks[] = {")
    assert names[SCONCE_GFX_BANK] == STAGE + "_gfxbank_0", names
    # The copies live in e_red_door.c (the link script keeps header.c's data
    # only), where the banks, static to header.c, can't be named: their
    # addresses are the retail ones (the data section is retail's), read from
    # the retail gfxBanks table
    m = re.search(r"0x([0-9a-f]+)\s+gfxBanks\n", open(os.path.join(ROOT, f"build/us/st{STAGE}.map")).read())
    assert m, f"build {STAGE.upper()} once first (build/us/st{STAGE}.map)"
    retail = open(os.path.join(ROOT, f"disks/us/ST/{STAGE.upper()}/{STAGE.upper()}.BIN"), "rb").read()
    o = int(m[1], 16) - 0x80180000
    banks = ["(u_long**)0x%08X" % v for v in struct.unpack_from("<%dI" % len(names), retail, o)]
    assert len(set(banks[k] for k, n in enumerate(names) if n == STAGE + "_gfxbank_0")) == 1
    sb = open(SPRITE_BANKS_H).read()
    sprites = table(sb, "s16** spriteBanks[] = {")
    assert sprites[SCONCE_ANIMSET] == "0", sprites
    assert all(sprites[k] == "0" for k in EXTRA_SPRITES), sprites
    pd = open(os.path.join(GEN, "palette_def.h")).read()
    pals = table(pd, "static u_long* pal_def_0[] = {")
    assert pals[-1] == "PAL_TERMINATE()" and len(re.findall(r"pal_def_\d+\[\]", pd)) == 1

    tpage, quad = QUADRANT
    vx, vy = (tpage & 15) * 64 + (quad & 1) * 32, (tpage >> 4) * 256 + (quad >> 1) * 128
    with open(OUT, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gen_np3_sconce.py, do not edit\n// clang-format off\n")
        f.write("// %s\n" % COMMENT)
        f.write("#define MOD_SCONCE_ANIMSET ANIMSET_OVL(%d)\n" % SCONCE_ANIMSET)
        f.write("#define MOD_SCONCE_GFX_ID %d // the shaft room's entityGfxId\n"
                % (SCONCE_GFX_BANK + 1))
        f.write("#ifdef MOD_SCONCE_TABLES\n")
        # rodata: the data section must not grow (mods/sotn-ultimate/tools/check_stage_data.py)
        f.write("#define MOD_RO __attribute__((section(\".rodata\")))\n")
        for s in sprites:
            if s != "0":
                f.write("extern s16* %s[];\n" % s)
        for sym in sorted(set(re.findall(r"PAL_BULK_\(0x[0-9A-F]+, (\w+),", "\n".join(pals)))):
            f.write("extern u16 %s[];\n" % sym)
        f.write("u8 g_ModSconceArt[] MOD_RO __attribute__((aligned(4))) = {\n")
        for k in range(0, len(art), 32):
            f.write("    " + ", ".join("0x%02X" % b for b in art[k:k + 32]) + ",\n")
        f.write("};\n")
        f.write("u_long* g_ModSconceBank[] MOD_RO = {\n    (u_long*)GFX_BANK_4BPP,\n"
                "    GFX_ENTRY(0x%03X, 0x%03X, 128, %d, g_ModSconceArt),\n"
                "    GFX_TERMINATE(),\n};\n" % (vy, vx, rows))
        f.write("u_long** g_ModGfxBanks[] MOD_RO = {\n")
        for k, b in enumerate(banks):
            f.write("    %s,\n" % ("g_ModSconceBank" if k == SCONCE_GFX_BANK else b))
        f.write("};\n")
        for n, parts in enumerate(frames):
            vals = [len(parts)]
            for p in parts:
                vals += [p["flags"], p["x"], p["y"], p["width"], p["height"], p["clut"],
                         p["tileset"], p["left"], p["top"], p["right"], p["bottom"]]
            vals.append(0)
            if len(vals) & 1:
                vals.append(0)
            f.write("s16 g_ModSconceFrame%d[] MOD_RO = {%s};\n" % (n + 1, ", ".join(map(str, vals))))
        f.write("s16* g_ModSconceSprites[] MOD_RO = {\n    0,\n")
        for n in range(len(frames)):
            f.write("    g_ModSconceFrame%d,\n" % (n + 1))
        f.write("};\n")
        for s in EXTRA_SPRITES.values():
            f.write("extern s16* %s[];\n" % s)
        f.write("s16** g_ModSpriteBanks[] MOD_RO = {\n")
        for k, s in enumerate(sprites):
            f.write("    %s,\n" % ("g_ModSconceSprites" if k == SCONCE_ANIMSET
                                  else EXTRA_SPRITES.get(k, s)))
        f.write("};\n")
        f.write("u16 g_ModSconcePals[] MOD_RO = {\n")
        for pal in sheet.cluts:
            f.write("    " + ", ".join("0x%04X" % c for c in pal) + ",\n")
        f.write("};\nu_long* g_ModPalDef[] MOD_RO = {\n")
        for p in pals[:-1]:
            f.write("    %s,\n" % p)
        f.write("    PAL_BULK_(0x%04X, g_ModSconcePals, 0x%X),\n    PAL_TERMINATE(),\n};\n"
                % (SCLUT_BASE * 16, 16 * len(sheet.cluts)))
        f.write("u_long* g_ModCluts[] MOD_RO = {\n    (u_long*)&g_ModPalDef,\n};\n")
        f.write("#endif\n")
        f.write("#ifdef MOD_SCONCE_GLOW\n")
        f.write("// per flame frame (1-4): x, y, w, h, u, v, clut, tpage\n")
        f.write("static const s16 g_ModCandleGlow[][8] = {\n")
        for p in glow:
            q = p["tileset"] & 3
            f.write("    {%d, %d, %d, %d, %d, %d, 0x%X, %d},\n" % (
                p["x"], p["y"], p["width"], p["height"], p["left"] + (q & 1) * 128,
                p["top"] + (q >> 1) * 128, p["clut"], p["tileset"] >> 2))
        f.write("};\n#endif\n// clang-format on\n")
    print(STAGE + " sconce: %d images, %d rows of art, %d palettes from 0x%X"
          % (len(sheet.placed), rows, len(sheet.cluts), SCLUT_BASE))


if __name__ == "__main__":
    main()
    globals().update(NO3)
    main()
    globals().update(RNO3)
    main()
