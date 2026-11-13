#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: the Saturn's Maria boss fight in the Castle Center (CEN).

The Saturn's STAGE_14 (CEN) has Maria fight Alucard before she gives him the
Holy Glasses (entities 0x1D-0x25, 060E2F8C-060E69B8; ported to C in
mods/sotn-ultimate/src/maria_saturn/mod_maria_boss.h). This writes the tables that code reads,
mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h (rodata, included from e_maria.c):

  - her body: bank 0x060E9448 (317 images, 5 palettes), frame table
    0x060EAA2C (289 frames, up to 5 parts each). Each frame is composited
    into one image (parts in order, the last on top), narrowed to PS1
    proportions (x 4/5, saturn_sprites' column sampling), cropped and
    LZSS-packed (the MARIA.BIN format). The PS1 draws them as it draws boss
    Richter (BO6): a frame {0x8000 | image, x, y, clut} names an image of the
    overlay's sheet (g_api.o.unk30; ANIMSET_OVL(MB_ANIMSET)), which DRA
    LoadImages every frame into a g_PlayerDraw slot's VRAM. The code unpacks
    the image an entity shows into that entity's buffer and points the
    sheet entry there. A frame mixing palettes gets a merged palette when
    the colours fit in 15, else the minority colours go to their nearest.
  - her projectiles and effects: the fx bank 0x060E9608 (frames 0x060EB700
    1-20), the thrown blade (GAME bank 0x060457F0, frames 0x0604501C, 0xC0)
    and the flames (GAME bank 0x06046BF8, frames 0x060467E0 1-13): ordinary
    sprite parts in tpage 19's quadrants (free in CEN's shaft room: its
    entities draw from tpage 18 q0-q1), raw 4bpp, uploaded by the code.
  - palettes, animations (Saturn {u16 ticks, u16 frame}, 0 = loop, 0xFF =
    end), per-frame hitboxes and the other tables, X values at 4/5.
"""
import os
import struct
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from saturn_sprites import SaturnStage, PsxSpriteSheet  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
SAT = os.path.join(ROOT, "disks/saturn")
OUT = os.path.join(ROOT, "mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h")
B = 0x060DC000

BODY_BANK, BODY_FRAMES, NFRAMES = 0x060E9448, 0x060EAA2C, 289
FX_BANK, FX_FRAMES = 0x060E9608, 0x060EB700
BLADE_BANK, BLADE_FRAMES, BLADE_FRAME = 0x060457F0, 0x0604501C, 0xC0
FIRE_BANK, FIRE_FRAMES = 0x06046BF8, 0x060467E0
MB_PAL = 0x240  # body palettes (CEN loads 0x200-0x20F, 0x210, 0x223-0x224)
FX_QUADS = [(19, 0), (19, 1), (19, 2), (19, 3)]
MB_ANIMSET = 3  # CEN's spriteBanks 3 and 4 are empty
FX_ANIMSET = 4
# fx sprite bank (FX_ANIMSET) frame numbers
FX_FIRST_BLADE = 21
FX_FIRST_FIRE = 22  # 22..34 = Saturn fire frames 1..13

# animations used by the code (main body; frames are the Saturn's)
BODY_ANIMS = [0x60E8DFA, 0x60E8E2C, 0x60E8E3E, 0x60E8E64, 0x60E8E8E, 0x60E8EA4,
              0x60E8ECA, 0x60E8EE4, 0x60E8EF2, 0x60E8F08, 0x60E8F22, 0x60E8F58,
              0x60E8F7A, 0x60E8FA8, 0x60E8FC2, 0x60E8FEC, 0x60E900E, 0x60E9034,
              0x60E904E, 0x60E9060, 0x60E910C, 0x60E917E, 0x60E91A8, 0x60E923A,
              0x60E92EC, 0x60E930E, 0x60E93C0, 0x60E93E0, 0x60E940A]
FX_ANIMS = [0x60E95D0, 0x60E95E2, 0x60E9684]
FIRE_ANIM = 0x60E9640


def lzss(data):
    """flag byte (LSB first): 1 = literal, 0 = match u16 (off-1) | (len-3)<<12
    (gen_mar_assets.lzss's format, with a hash chain)"""
    out = bytearray()
    n = len(data)
    heads = {}
    i = 0
    while i < n:
        flag_pos = len(out)
        out.append(0)
        flags = 0
        for bit in range(8):
            if i >= n:
                break
            best_len, best_off = 0, 0
            if i + 2 < n:
                for j in reversed(heads.get(data[i:i + 3], [])):
                    if i - j > 4096:
                        break
                    k = 0
                    while k < 18 and i + k < n and data[j + k] == data[i + k]:
                        k += 1
                    if k > best_len:
                        best_len, best_off = k, i - j
                        if k == 18:
                            break
            if best_len >= 3:
                out += struct.pack("<H", (best_off - 1) | ((best_len - 3) << 12))
                step = best_len
            else:
                flags |= 1 << bit
                out.append(data[i])
                step = 1
            for k in range(step):
                if i + k + 2 < n:
                    heads.setdefault(data[i + k:i + k + 3], []).append(i + k)
            i += step
        out[flag_pos] = flags
    return bytes(out)


def unlzss(src, size):
    out = bytearray()
    i = 0
    while len(out) < size:
        flags = src[i]
        i += 1
        for bit in range(8):
            if len(out) >= size:
                break
            if flags & (1 << bit):
                out.append(src[i])
                i += 1
            else:
                v = src[i] | src[i + 1] << 8
                i += 2
                off, ln = (v & 0xFFF) + 1, (v >> 12) + 3
                for _ in range(ln):
                    out.append(out[-off])
    return bytes(out)


def ps1_colour(c, j):
    return 0 if j == 0 else ((c & 0x7FFF) or 0x8000)


def narrow(rows):
    """saturn_sprites._pixels' sampling: w -> round(w * 4 / 5) columns"""
    h, w = rows.shape
    nw = max(1, int(round(w * 4 / 5)))
    out = np.zeros((h, nw), np.uint8)
    for x in range(nw):
        a = x * w // nw
        b = max(a + 1, -(-(x + 1) * w // nw))
        c = (2 * x + 1) * w // (2 * nw)
        col = rows[:, c].copy()
        empty = col == 0
        for s in range(a, b):
            fill = empty & (rows[:, s] != 0)
            col[fill] = rows[fill, s]
            empty &= col == 0
        out[:, x] = col
    return out


def composite(st, parts, pals, cluts):
    """one PS1 image of the given parts -> (packed, raw size, x, y, clut
    offset, raw) or None. Parts of one palette keep it; several get a merged
    palette when they fit in 15 colours, else their nearest colours."""
    imgs = []
    for attr, cx, cy, img in parts:
        # stored facing left, as the PS1 draws enemies unflipped (her
        # Saturn frames face right: her sprite object draws them
        # mirrored, as her hitboxes show, e.g. the kicks' -24..-34)
        cx, attr = -cx, attr ^ 0x10
        w, h, rows = st.image(BODY_BANK, img)
        a = np.array(rows, np.uint8)
        if attr & 0x10:
            a = a[:, ::-1]
        if attr & 0x20:
            a = a[::-1, :]
        imgs.append((cx - w // 2, cy - h // 2, a, (attr >> 8) % len(pals)))
    x0 = min(x for x, y, a, p in imgs)
    y0 = min(y for x, y, a, p in imgs)
    x1 = max(x + a.shape[1] for x, y, a, p in imgs)
    y1 = max(y + a.shape[0] for x, y, a, p in imgs)
    canvas = np.zeros((y1 - y0, x1 - x0), np.uint32)  # 0 = transparent
    for x, y, a, p in imgs:
        m = a != 0
        col = np.array([cluts[p][k] | 0x10000 for k in range(16)], np.uint32)
        sub = canvas[y - y0:y - y0 + a.shape[0], x - x0:x - x0 + a.shape[1]]
        sub[m] = col[a[m]]
    used_pals = {p for x, y, a, p in imgs}
    counts = {p: sum(int(np.count_nonzero(a)) for x, y, a, pp in imgs if pp == p)
              for p in used_pals}
    main = max(counts, key=lambda p: counts[p])
    colours = sorted({int(v) for v in np.unique(canvas) if v})
    if len(used_pals) == 1:
        pal = tuple(cluts[main])
        clut = main
    else:
        if len(colours) <= 15:
            pal = tuple([0] + [c & 0xFFFF for c in colours] + [0] * (15 - len(colours)))
        else:
            pal = tuple(cluts[main])
        if pal not in cluts:
            cluts.append(pal)
        clut = cluts.index(pal)

    def rgb(c):
        return np.array([c & 31, c >> 5 & 31, c >> 10 & 31])
    lut = {}
    for c in colours:
        c16 = c & 0xFFFF
        if c16 in pal[1:]:
            lut[c] = pal.index(c16, 1)
        else:
            lut[c] = min(range(1, 16), key=lambda k: np.sum((rgb(pal[k]) - rgb(c16)) ** 2))
    idx = np.zeros(canvas.shape, np.uint8)
    for c, k in lut.items():
        idx[canvas == c] = k
    nar = narrow(idx)
    ys, xs = np.nonzero(nar)
    if not len(ys):
        return None
    cy0, cy1, cx0, cx1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    c = nar[cy0:cy1, cx0:cx1]
    w = (c.shape[1] + 3) & ~3
    if w != c.shape[1]:
        c = np.pad(c, ((0, 0), (0, w - c.shape[1])))
    h = c.shape[0]
    assert w <= 128 and h <= 77, ("frame too big for a g_PlayerDraw slot", w, h)
    flat = c.reshape(-1)
    px = bytes((flat[0::2] | (flat[1::2] << 4)).tolist())
    raw = bytes([w, h, 0, 0]) + px
    packed = lzss(raw)
    assert unlzss(packed, len(raw)) == raw
    px0 = int(round(x0 * 4 / 5)) + int(cx0)
    return (packed, len(raw), px0, int(y0 + cy0), clut)


def body(st):
    """her frames, and the effect overlays: a frame whose parts mix her
    palette 0 with effect palettes (the Guardian Knuckle shield, rings...) is
    split, her body in the frame's image and the effect parts in an overlay
    image (sheet index MB_NFRAMES + k) that a companion entity draws
    half-transparent over her, in its own palette"""
    pals = st.palettes(BODY_BANK)
    cluts = [tuple(ps1_colour(c, j) for j, c in enumerate(p)) for p in pals]
    frames = []  # (blob, raw size, x, y, clut offset) or None
    overlays = []  # the same, for the effect parts
    overlay_of = []  # per frame: 0 or 1 + overlay number
    maxraw = 0
    for f in range(NFRAMES):
        parts = st.frame(BODY_FRAMES, f)
        if not parts:
            frames.append(None)
            overlay_of.append(0)
            continue
        pal_of = [(a >> 8) % len(pals) for a, x, y, i in parts]
        if 0 in pal_of and len(set(pal_of)) > 1:
            body_parts = [p for p, q in zip(parts, pal_of) if q == 0]
            fx_parts = [p for p, q in zip(parts, pal_of) if q != 0]
            frames.append(composite(st, body_parts, pals, cluts))
            ov = composite(st, fx_parts, pals, cluts)
            if ov is None:
                overlay_of.append(0)
            else:
                overlays.append(ov)
                overlay_of.append(len(overlays))
        else:
            frames.append(composite(st, parts, pals, cluts))
            overlay_of.append(0)
    for fr in frames + overlays:
        if fr:
            maxraw = max(maxraw, fr[1])
    return frames, overlays, overlay_of, cluts, maxraw


def recolour(pal, mode):
    """the four guardian spirits: the fx bank's blue orb palette re-tinted
    (fire, thunder, ice, holy); the Saturn picks colour banks the PS1 bank
    doesn't have"""
    out = [0]
    for c in pal[1:]:
        r, g, b = c & 31, c >> 5 & 31, c >> 10 & 31
        lum = max(r, g, b)
        if mode == 0:  # fire: red-orange
            r, g, b = lum, lum * 2 // 5, lum // 8
        elif mode == 1:  # thunder: yellow
            r, g, b = lum, lum, lum // 4
        elif mode == 2:  # ice: as is
            pass
        else:  # holy: green
            r, g, b = lum // 3, lum, lum // 3
        out.append((r | g << 5 | b << 10) or 0x8000)
    return tuple(out)


def fx(st):
    def fill(sheet):
        frames = []
        sheet.palette_block(st, FX_BANK)
        for f in range(1, 21):
            frames.append(sheet.frame(st, FX_BANK, FX_FRAMES, f))
        frames.append(sheet.frame(st, BLADE_BANK, BLADE_FRAMES, BLADE_FRAME))
        for f in range(1, 14):
            frames.append(sheet.frame(st, FIRE_BANK, FIRE_FRAMES, f))
        return frames

    probe = PsxSpriteSheet(FX_QUADS * 2, 0)
    fill(probe)
    sheet = PsxSpriteSheet(FX_QUADS, 0)
    sheet.prepack(st, probe.requests)
    frames = fill(sheet)
    return sheet, frames


def anim(prg, a):
    out = []
    k = a - B
    while True:
        d, f = struct.unpack_from(">HH", prg, k)
        out += [d, f]
        k += 4
        if d in (0, 0xFF):
            return out


# the dialogue: the retail script (src/st/cen/gen/cutscene_data.h) split in
# two around the fight, as the Saturn's two scripts. The Saturn's lines the
# US disc has no subtitles for get the unused US recordings of them (XA
# 0x3C1-0x3C4 = Saturn 0x3BF-0x3C2; PS1 voice ids are the Saturn's + 2) and
# English renderings of the Saturn text (to check against the recordings).
NEW_PRE = [  # (portrait, voice, lines) after Maria's "I know." (same box)
    # Saturn 0x3BF: "dakara, warui kedo... anata no chikara, tamesasete morau wa."
    (1, 0x3C1, ["So... I'm sorry, but", "I'm going to test", "your strength."]),
]
NEW_POST = [
    # 0x3C0: "sa, sasuga ne. kore nara, kitto... onegai, Richter wo tasukete agete."
    (1, 0x3C2, ["Im... impressive.", "With this, surely...", "Please help Richter."]),
    # 0x3C1: "sono hoshou wa dekin. da ga, kanarazu yatsu wo tomete miseru."
    (0, 0x3C3, ["I cannot promise it.", "But I will stop him,", "without fail."]),
    # 0x3C2: "sore mo, shikata nai koto ne..."
    (1, 0x3C4, ["I suppose that can't", "be helped, either..."]),
]
# (the US disc keeps these four as Japanese recordings, XA files 0x50-0x51
# and 0x13-0x14 of the voice stream: the Saturn's lines; PS1 id = Saturn + 2)


def tokens(path):
    body = open(path).read()
    body = "\n".join(l for l in body.splitlines() if not l.startswith("//"))
    out, depth, cur, q = [], 0, "", False
    for ch in body:
        if ch == "'" and not cur.endswith("\\"):
            q = not q
        if not q and ch == "(":
            depth += 1
        if not q and ch == ")":
            depth -= 1
        if ch == "," and depth == 0 and not q:
            if cur.strip():
                out.append(cur.strip())
            cur = ""
        else:
            cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


# the end of a voiced box: a reading pause, then held until the voice ends
# (SCRIPT_UNKNOWN_11: CEN cutscene.c waits while the XA plays; each dub's
# voice has its own length), then a beat
HOLD = ["SET_WAIT(24)", "SCRIPT_UNKNOWN_11()", "SET_WAIT(24)"]


def say(portrait, voice, lines, first=False):
    t = [] if first else []
    t += ["SET_PORTRAIT(%d,%d)" % ((1, 0) if portrait else (0, 1)),
          "SCRIPT_UNKNOWN_11()", "PLAY_SOUND(0x%x)" % voice, "WAIT_FOR_SOUND()", "SET_SPEED(2)"]
    for k, line in enumerate(lines):
        if k:
            t += ["LINE_BREAK()", "SET_WAIT(4)"]
        t += ["'%s'" % ("\\'" if c == "'" else c) for c in line]
    t += HOLD + ["NEXT_DIALOG()"]
    return t


def scripts():
    tok = tokens(os.path.join(ROOT, "src/st/cen/gen/cutscene_data.h"))
    k = tok.index("PLAY_SOUND(0x3cc)") - 1  # SCRIPT_UNKNOWN_11() before it
    assert tok[k] == "SCRIPT_UNKNOWN_11()" and tok[k - 1] == "LINE_BREAK()"
    end = tok.index("CLOSE_DIALOG()")
    head = tok[:k - 1]  # up to Maria's "I know."
    # pre: ... "I know." then her new line in the same box
    pre = head + ["LINE_BREAK()", "SCRIPT_UNKNOWN_11()", "PLAY_SOUND(0x%x)" % NEW_PRE[0][1],
                  "WAIT_FOR_SOUND()", "SET_SPEED(2)"]
    for n, line in enumerate(NEW_PRE[0][2]):
        if n:
            pre += ["LINE_BREAK()", "SET_WAIT(4)"]
        pre += ["'%s'" % ("\\'" if c == "'" else c) for c in line]
    pre += HOLD + ["NEXT_DIALOG()", "CLOSE_DIALOG()", "SCRIPT_UNKNOWN_11()",
            "SET_FLAG(1)", "SCRIPT_UNKNOWN_20(0x11)", "END_CUTSCENE()", "0xff", "0xff"]
    # post: music stop, portraits, the new lines, then the retail "Well...
    # here. Take these with you." (its own box now) and the rest
    post = ["PLAY_SOUND(0x10)", "SCRIPT_UNKNOWN_11()", "SET_POS(60,28)",
            "LOAD_PORTRAIT(0x80182cc0,0)", "LOAD_PORTRAIT(0x80181f40,1)"]
    for portrait, voice, lines in NEW_POST:
        post += say(portrait, voice, lines)
    post += ["SET_PORTRAIT(1,0)", "SCRIPT_UNKNOWN_11()"] + tok[k + 1:]
    return pre, post


def main():
    st = SaturnStage(os.path.join(SAT, "STAGE_14.PRG"), os.path.join(SAT, "STAGE_14.CHR"),
                     os.path.join(SAT, "0.BIN"))
    prg = st.prg
    frames, overlays, overlay_of, cluts, maxraw = body(st)
    images = frames + overlays
    sheet, fxf = fx(st)
    fx_base = MB_PAL + len(cluts)
    fx_cluts = list(sheet.cluts)
    spirit_base = fx_base + len(fx_cluts)
    blue = fx_cluts[0]  # the filled orb (Saturn palette 0)
    spirits = [recolour(blue, m) for m in range(4)]
    assert spirit_base + 4 <= 0x2C0, ("palettes", hex(spirit_base))
    nq = sheet.used_quadrants()

    L = []
    w = L.append
    w("// Generated by mods/sotn-ultimate/tools/gen_cen_maria.py, do not edit")
    w("// clang-format off")
    w("// Saturn STAGE_14 (CEN) Maria boss: body frames, effects, tables")
    w("#define MB_NFRAMES %d" % NFRAMES)
    w("#define MB_NIMAGES %d // + the effect overlays" % (NFRAMES + len(overlays)))
    w("#define MB_ANIMSET %d" % MB_ANIMSET)
    w("#define MB_FX_ANIMSET %d" % FX_ANIMSET)
    w("#define MB_PAL 0x%X" % MB_PAL)
    w("#define MB_NPALS %d" % len(cluts))
    w("#define MB_FX_PAL 0x%X // fx sheet palettes (+ Saturn bank palette)" % fx_base)
    w("#define MB_FX_NPALS %d" % len(fx_cluts))
    w("#define MB_SPIRIT_PAL 0x%X // 4 guardian spirit tints" % spirit_base)
    w("#define MB_MAXRAW %d" % ((maxraw + 3) & ~3))
    w("#define MB_FX_NQUADS %d" % nq)
    w("#define MB_FX_BLADE %d" % FX_FIRST_BLADE)
    w("#define MB_FX_FIRE %d" % FX_FIRST_FIRE)
    w("#ifdef MOD_MB_TABLES")
    w("#ifndef MOD_RO")
    w("#define MOD_RO __attribute__((section(\".rodata\")))")
    w("#endif")
    blob = bytearray()
    offs = []
    for fr in images:
        if fr is None:
            offs.append((0, 0))
            continue
        offs.append((len(blob), fr[1]))
        blob += fr[0]
    w("u8 g_MbLz[] MOD_RO = {")
    for k in range(0, len(blob), 32):
        w("    " + ",".join("%d" % b for b in blob[k:k + 32]) + ",")
    w("};")
    w("// per frame: packed offset, unpacked size (0: no image)")
    w("u32 g_MbLzOff[] MOD_RO = {" + ",".join("%d" % o for o, s in offs) + "};")
    w("u16 g_MbRawSize[] MOD_RO = {" + ",".join("%d" % s for o, s in offs) + "};")
    w("// per frame: {0x8000 | sheet index, x, y, clut offset}")
    w("s16 g_MbFrameDesc[][4] MOD_RO = {")
    for f, fr in enumerate(images):
        if fr is None:
            w("    {0x%X, 0, 0, 0}," % (0x8000 | f))
        else:
            w("    {0x%X, %d, %d, %d}," % (0x8000 | f, fr[2], fr[3], fr[4]))
    w("};")
    w("s16* g_MbFrameTab[] MOD_RO = {")
    for f in range(len(images)):
        w("    g_MbFrameDesc[%d]," % f)
    w("};")
    w("// per frame: its effect overlay image (MB_NFRAMES + n - 1), 0 none")
    w("u16 g_MbOverlayOf[] MOD_RO = {" + ",".join(
        str(NFRAMES + k - 1 if k else 0) for k in overlay_of) + "};")
    w("u16 g_MbPals[] MOD_RO = {")
    for p in cluts:
        w("    " + ", ".join("0x%04X" % c for c in p) + ",")
    w("};")
    # fx
    w("u8 g_MbFxPix[] MOD_RO = {")
    for q in range(nq):
        px = sheet.quadrant_pixels(q)
        for k in range(0, len(px), 64):
            w("    " + ",".join("%d" % b for b in px[k:k + 64]) + ",")
    w("};")
    w("// VRAM x, y (halfwords, rows) of each fx quadrant")
    w("u16 g_MbFxQuad[][2] MOD_RO = {")
    for q in range(nq):
        tpage, quad = sheet.quadrants[q]
        w("    {%d, %d}," % ((tpage & 15) * 64 + (quad & 1) * 32,
                             (tpage >> 4) * 256 + (quad >> 1) * 128))
    w("};")
    for n, parts in enumerate(fxf):
        vals = [len(parts)]
        for p in parts:
            vals += [p["flags"], p["x"], p["y"], p["width"], p["height"], p["clut"] + fx_base,
                     p["tileset"], p["left"], p["top"], p["right"], p["bottom"]]
        vals.append(0)
        if len(vals) & 1:
            vals.append(0)
        w("s16 g_MbFx%d[] MOD_RO = {%s};" % (n + 1, ", ".join(map(str, vals))))
    w("s16* g_MbFxTab[] MOD_RO = {")
    w("    0,")
    for n in range(len(fxf)):
        w("    g_MbFx%d," % (n + 1))
    w("};")
    w("u16 g_MbFxPals[] MOD_RO = {")
    for p in fx_cluts + spirits:
        w("    " + ", ".join("0x%04X" % c for c in p) + ",")
    w("};")
    # animations
    for a in BODY_ANIMS + FX_ANIMS:
        v = anim(prg, a)
        w("u16 g_MbAnim%X[] MOD_RO = {%s};" % (a & 0xFFFF, ", ".join(map(str, v))))
    v = anim(prg, FIRE_ANIM)
    v = [x if i % 2 == 0 or v[i - 1] in (0, 0xFF) else x + FX_FIRST_FIRE - 1
         for i, x in enumerate(v)]
    w("u16 g_MbAnim%X[] MOD_RO = {%s};" % (FIRE_ANIM & 0xFFFF, ", ".join(map(str, v))))

    def sx(v):
        return int(round(v * 4 / 5))
    # per-frame hitbox: index table and {offX, offY, w, h}
    idx = list(prg[0x060E8CD8 - B:0x060E8CD8 - B + NFRAMES])
    hb = [struct.unpack_from(">bbBB", prg, 0x060E8B50 - B + 4 * i)
          for i in range(max(idx) + 1)]
    w("u8 g_MbHitboxIdx[] MOD_RO = {" + ",".join(map(str, idx)) + "};")
    w("s8 g_MbHitbox[][4] MOD_RO = {" + ",".join(
        "{%d,%d,%d,%d}" % (sx(x), y, sx(ww), h) for x, y, ww, h in hb) + "};")
    kick = [struct.unpack_from(">hhHH", prg, 0x060E95A8 - B + 8 * i) for i in range(5)]
    w("s16 g_MbKick[][4] MOD_RO = {" + ",".join(
        "{%d,%d,%d,%d}" % (sx(x), y, sx(ww), h) for x, y, ww, h in kick) + "};")
    s2 = struct.unpack_from(">4h", prg, 0x060E8B24 - B)
    s3 = struct.unpack_from(">8h", prg, 0x060E8B2C - B)
    w("s16 g_MbSensors2[] MOD_RO = {%d, %d, %d, %d};" % (sx(s2[0]), s2[1], sx(s2[2]), s2[3]))
    w("s16 g_MbSensors3[] MOD_RO = {%s};" % ", ".join(
        str(sx(v)) if i % 2 == 0 else str(v) for i, v in enumerate(s3)))
    b44 = [struct.unpack_from(">h", prg, 0x060E8B44 - B + 4 * i)[0] for i in range(3)]
    w("s16 g_MbBladeX[] MOD_RO = {%s};" % ", ".join(str(sx(v)) for v in b44))
    v1c = [struct.unpack_from(">ii", prg, 0x060E961C - B + 8 * i) for i in range(3)]
    w("s32 g_MbBladeV[][2] MOD_RO = {%s};" % ", ".join(
        "{%d, %d}" % (sx(a), b) for a, b in v1c))
    rays = [struct.unpack_from(">i", prg, 0x060E945C - B + 4 * i)[0] for i in range(32)]
    w("s32 g_MbRays[] MOD_RO = {%s};" % ", ".join(map(str, rays)))
    sp = [struct.unpack_from(">i", prg, 0x060E955C - B + 4 * i)[0] for i in range(8)]
    w("s32 g_MbSparkSpeed[] MOD_RO = {%s};" % ", ".join(map(str, sp)))
    pre, post = scripts()
    w("u8 g_MbScriptPre[] MOD_RO = {")
    w("    " + ", ".join(pre))
    w("};")
    w("u8 g_MbScriptPost[] MOD_RO = {")
    w("    " + ", ".join(post))
    w("};")
    w("#endif")
    w("// clang-format on")
    open(OUT, "w").write("\n".join(L) + "\n")
    print("cen maria: %d frames, %d bytes packed (%d max raw), %d palettes, "
          "fx %d frames in %d quadrants, %d fx palettes" % (
              sum(1 for f in frames if f), len(blob), maxraw, len(cluts),
              len(fxf), nq, len(fx_cluts) + 4))
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        preview(frames, cluts, sys.argv[2])


def preview(frames, cluts, path):
    from PIL import Image
    cols, cw, ch = 17, 100, 110
    img = Image.new("RGB", (cols * cw, (len(frames) // cols + 1) * ch), (40, 40, 60))
    for f, fr in enumerate(frames):
        if not fr:
            continue
        raw = unlzss(fr[0], fr[1])
        w, h = raw[0], raw[1]
        pal = cluts[fr[4]]
        ox, oy = (f % cols) * cw + 50 + fr[2], (f // cols) * ch + 80 + fr[3]
        for y in range(h):
            for x in range(w):
                b = raw[4 + (y * w + x) // 2]
                v = b & 15 if x % 2 == 0 else b >> 4
                if v:
                    c = pal[v]
                    px, py = ox + x, oy + y
                    if 0 <= px < img.size[0] and 0 <= py < img.size[1]:
                        img.putpixel((px, py), ((c & 31) << 3, (c >> 5 & 31) << 3, (c >> 10 & 31) << 3))
    img.save(path)


if __name__ == "__main__":
    main()
