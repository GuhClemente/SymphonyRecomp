#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Saturn port mod: BIN/RIC2.BIN, Richter's Saturn alternate costume.

The Saturn has a second Richter (RICHTER2.PRG/CHR + RIC_W2.CHR, chosen by
holding Up on the character select): the same code, animations and 191
images, redrawn in a long blue coat. RIC2.BIN is the PS1's own Richter
overlay (build/us/RIC.BIN) with only its sprite sheet replaced, so it has the
same code at the same addresses; src/dra/cd.c loads it instead of RIC.BIN
when MOD_RIC_COSTUME2() is true.

How the art is mapped:
- The PS1 Richter frames (spritebank0) are the Saturn's 213 frames in the same
  order, so frame f gives PS1 sprite i <-> Saturn image j. RICHTER2 uses the
  same image index for every frame.
- The Saturn art is the PS1's stretched 1.25x by repeating one column in five.
  Each PS1 sprite is matched against its RICHTER image with every column
  phase dropped (most match exactly) to find the phase and offset. RICHTER2's
  image is narrowed by dropping the same absolute columns (Saturn part x,y are
  the image centre; RICHTER2 moves centres and sizes for 71 images), and is
  placed by the same offsets, so unchanged parts land on exactly the PS1
  pixels. A column dropped there whose pixel has no neighbour keeps it (thin
  lines survive).
- Sprites that share one placeholder image on the PS1 (unused animations)
  get the RICHTER2 version of the Saturn image their art matches best.
- Palettes: RICHTER2 bank 0 replaces PAL_RICHTER (0x120); the PS1's other
  Richter body palettes (variants of 0x120: 0x121, 0x12B-0x12E, 0x134, 0x14E)
  get RICHTER2's colours wherever they equal 0x120's.
- RIC_W2.CHR's only redrawn art is the arm drawn while brandishing the whip
  (sleeved instead of bare). PS1: RicEntityArmBrandishWhip, spritebank2
  frames 1-27, in F_GAME2's VRAM (tpage 25, lower left, u 64-127, v 0-63).
  The RIC_W2 images were located by their silhouettes; they are narrowed
  the same way.

RIC2.BIN ends its sprite region with a header (magic "RIC2", pointer) that
DRA's ModRicCostumeApply (src/dra/cd.c) reads after the overlay is loaded to
upload these palettes (VRAM + g_Clut[1]) and arm pixels.

Writes build/us/RIC2.BIN and build/us/ric2_preview.png.
"""
import os
import struct
import sys

import numpy as np

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "saturn"))
import character_viewer as cv  # noqa: E402

RAM = 0x8013C000
SAT = 0x060A5000
MAGIC = 0x32434952  # "RIC2", MOD_RIC_COSTUME2_MAGIC in include/game.h
SHEET_END = 0x170AC  # spritebank0: the end of the retail sprite region
MAX_W, MAX_H = 64, 64  # the PS1's largest Richter sprite (player VRAM slot)
P1 = dict(cv.CHARACTER_PROFILES["richter"])
P2 = dict(P1)
P2.update(image_table=0x060C120C, frame_table=0x060C0EA0,
          prg="disks/saturn/RICHTER2.PRG", chr="disks/saturn/RICHTER2.CHR")
# (PS1 rect in the F_GAME2 block (u0, v0, u1, v1), RIC_W2 offset, stride,
#  dropped column phase, x offset in the rect[, y offset in the rect, image
#  height])
ARMS = [
    ((112, 16, 128, 32), 21350, 24, 3, -2),
    ((112, 0, 128, 16), 21508, 24, 1, -5),
    ((64, 48, 88, 64), 21660, 24, 0, 5),
    # the forward (horizontal) arm: a 32-wide image, 9 rows (RIC_W.CHR's
    # bare one is 0x5500, 32x8, matching the PS1 exactly at phase 3, x 2,
    # y 4); the silhouette search had picked garbage inside arm 0 at stride
    # 24, which drew a stretched, detached arm
    ((88, 48, 112, 64), 0x5540, 32, 3, 2, 4, 9),
    ((80, 32, 96, 48), 21970, 24, 3, -2),
    ((112, 32, 128, 56), 22136, 16, 3, 2),
    ((96, 32, 112, 48), 22286, 16, 3, -2),
    ((96, 16, 112, 32), 22414, 16, 1, -2),
    ((96, 0, 112, 16), 22504, 16, 3, 1),
]
ARM_BLOCK = 6  # F_GAME2 0x2000-byte block: VRAM (0x240, 0x180)
ARM_VRAM = (0x240, 0x180)


def path(p):
    return os.path.join(ROOT, p)


def ric_syms():
    syms = {}
    for line in open(path("build/us/ric.map")):
        f = line.split()
        if len(f) == 2 and f[0].startswith("0x") and f[1].isidentifier():
            try:
                syms[f[1]] = int(f[0], 16)
            except ValueError:
                pass
    return syms


def unpack_psx(data, w, h):
    a = np.frombuffer(data, np.uint8)[: w * h // 2]
    out = np.empty(w * h, np.uint8)
    out[0::2] = a & 15
    out[1::2] = a >> 4
    return out.reshape(h, w)


def pack_psx(img):
    flat = img.astype(np.uint8).reshape(-1)
    return bytes((flat[0::2] | (flat[1::2] << 4)).tolist())


class Saturn:
    def __init__(self, prof):
        self.p = prof
        self.prg = open(path(prof["prg"]), "rb").read()
        self.chr = open(path(prof["chr"]), "rb").read()
        self.recs = cv.read_image_records(self.prg, prof)
        self.streams = cv.read_stream_records(self.prg, len(self.recs), prof)
        self.cache = {}

    def image(self, j):
        if j not in self.cache:
            wf, hf, _ = self.recs[j]
            o, s = self.streams[j]
            d = np.frombuffer(cv.decompress_lzss(self.chr[o:o + s]), np.uint8)
            w, h = wf * 2, hf * 2
            px = np.empty(w * h, np.uint8)
            px[0::2] = d[: w * h // 2] >> 4
            px[1::2] = d[: w * h // 2] & 15
            self.cache[j] = px.reshape(h, w)
        return self.cache[j]

    def frames(self):
        ft = self.p["frame_table"] - SAT
        it = self.p["image_table"] - SAT
        out = []
        for i in range((it - ft) // 4):
            o = struct.unpack_from(">I", self.prg, ft + i * 4)[0] - SAT
            pc = (struct.unpack_from(">H", self.prg, o)[0] >> 8) & 0x3F
            if pc != 1:
                out.append(None)
                continue
            fl, x, y, img = struct.unpack_from(">HhhH", self.prg, o + 4)
            out.append((fl >> 8, x, y, img))
        return out

    def palette(self, bank):
        t = self.p["palette_table"] - SAT
        return list(struct.unpack_from(">16H", self.prg, t + 2 + bank * 32))


def drop(img, keep):
    return img[:, keep]


def narrow(img, left, phase, rescue):
    """drop the columns whose absolute x (left + c) % 5 == phase"""
    w = img.shape[1]
    cols = [c for c in range(w) if (left + c) % 5 != phase]
    out = img[:, cols].copy()
    if rescue:
        # a dropped pixel with no neighbour in its row keeps its place
        for k, c in enumerate(range(w)):
            if (left + c) % 5 != phase:
                continue
            nl = img[:, c - 1] if c > 0 else np.zeros(img.shape[0], np.uint8)
            nr = img[:, c + 1] if c + 1 < w else np.zeros(img.shape[0], np.uint8)
            lost = (img[:, c] != 0) & (nl == 0) & (nr == 0)
            if lost.any():
                dst = sum(1 for cc in range(c) if (left + cc) % 5 != phase)
                dst = min(max(dst - 1, 0), out.shape[1] - 1)
                out[lost, dst] = img[lost, c]
    return out


def dropped_between(a, b, phase):
    """kept columns from absolute x a to b (signed)"""
    if b >= a:
        return (b - a) - sum(1 for x in range(a, b) if x % 5 == phase)
    return -dropped_between(b, a, phase)


def place_score(ps, nr, ox, oy):
    H, W = ps.shape
    h, w = nr.shape
    x0, y0 = min(0, ox), min(0, oy)
    x1, y1 = max(W, ox + w), max(H, oy + h)
    a = np.zeros((y1 - y0, x1 - x0), np.uint8)
    b = np.zeros_like(a)
    a[-y0:-y0 + H, -x0:-x0 + W] = ps
    b[oy - y0:oy - y0 + h, ox - x0:ox - x0 + w] = nr
    return int(np.count_nonzero(a != b))


def align(ps, sat):
    """best (mismatch, phase relative to the image, ox, oy)"""
    best = None
    for ph in range(5):
        nr = sat[:, [c for c in range(sat.shape[1]) if c % 5 != ph]]
        for oy in range(-4, 5):
            for ox in range(-10, 11):
                s = place_score(ps, nr, ox, oy)
                if best is None or s < best[0]:
                    best = (s, ph, ox, oy)
    return best


def crop(img):
    ys, xs = np.nonzero(img)
    if not len(ys):
        return img[:1, :4] * 0, 0, 0
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    c = img[y0:y1, x0:x1]
    w = (c.shape[1] + 3) & ~3
    if w != c.shape[1]:
        c = np.pad(c, ((0, 0), (0, w - c.shape[1])))
    return c, x0, y0


def largest_component(img):
    h, w = img.shape
    seen = np.zeros_like(img, bool)
    best = []
    for y in range(h):
        for x in range(w):
            if img[y, x] and not seen[y, x]:
                comp, stack = [], [(y, x)]
                seen[y, x] = True
                while stack:
                    cy, cx = stack.pop()
                    comp.append((cy, cx))
                    for dy in (-1, 0, 1):
                        for dx in (-1, 0, 1):
                            ny, nx = cy + dy, cx + dx
                            if 0 <= ny < h and 0 <= nx < w and img[ny, nx] \
                                    and not seen[ny, nx]:
                                seen[ny, nx] = True
                                stack.append((ny, nx))
                if len(comp) > len(best):
                    best = comp
    out = np.zeros_like(img)
    for y, x in best:
        out[y, x] = img[y, x]
    return out


def main():
    ric = bytearray(open(path("build/us/RIC.BIN"), "rb").read())
    syms = ric_syms()
    assert syms["spritebank0"] - RAM == SHEET_END, "RIC's sprite sheet moved"
    table = syms["richter_sprites"] - RAM
    # the PS1 sheet
    ptrs = []
    o = table
    while True:
        p = struct.unpack_from("<I", ric, o)[0]
        if not RAM <= p < RAM + SHEET_END or (ptrs and o >= min(ptrs) - RAM):
            break
        ptrs.append(p)
        o += 4
    n = len(ptrs)
    data_start = table + 4 * n
    sprites = []
    for p in ptrs:
        q = p - RAM
        w, h, px, py = ric[q:q + 4]
        sprites.append((w, h, px, py, unpack_psx(bytes(ric[q + 4:]), w, h), q))
    # PS1 frames (spritebank0, the {0x8000|sprite, x, y, clut} format)
    b0 = syms["spritebank0"] - RAM
    frames = []
    o = b0
    lim = syms["spritebank1"] - RAM
    while o < lim:
        p = struct.unpack_from("<I", ric, o)[0]
        if p == 0:
            frames.append(None)
        else:
            q = p - RAM
            lim = min(lim, q)
            f, x, y, c = struct.unpack_from("<Hhhh", ric, q)
            frames.append((f & 0x7FFF, x, y, c, q))
        o += 4
    s1, s2 = Saturn(P1), Saturn(P2)
    f1, f2 = s1.frames(), s2.frames()
    assert len(frames) == len(f1), (len(frames), len(f1))
    # Saturn placement per image: {j: (x1, y1, x2, y2)} (centres), checked
    # consistent over every frame that shows j
    delta = {}
    cand = {}
    for f, fr in enumerate(frames):
        if fr is None:
            continue
        a, b = f1[f], f2[f]
        assert a and b and a[3] == b[3], f
        j = a[3]
        cand.setdefault(fr[0], set()).add(j)
        w1, h1 = s1.image(j).shape[1], s1.image(j).shape[0]
        w2, h2 = s2.image(j).shape[1], s2.image(j).shape[0]
        d = (b[1] - b[2] * 0 - (a[1]), b[2] - a[2])
        # left/top edges: centre - size / 2
        e = ((a[1] - w1 // 2), (a[2] - h1 // 2), (b[1] - w2 // 2), (b[2] - h2 // 2))
        k = (e[2] - e[0], e[3] - e[1])
        if j in delta:
            assert delta[j][1] == k, ("inconsistent", j, delta[j], k)
        else:
            delta[j] = (e, k)
    # group sprites by shared data (placeholders share one image)
    groups = {}
    for i, s in enumerate(sprites):
        groups.setdefault(s[5], []).append(i)
    report = []
    new = {}  # data offset -> (bytes)
    frame_fix = {}  # frame data offset -> (dx, dy)
    for off, members in sorted(groups.items()):
        w, h, px, py, ps, _ = sprites[members[0]]
        js = set()
        for i in members:
            js |= cand.get(i, set())
        best = None
        for j in sorted(js):
            r = align(ps, s1.image(j))
            if best is None or r[0] < best[0][0]:
                best = (r, j)
        if best is None or best[0][0] > 0.15 * np.count_nonzero(ps):
            # placeholder art: the Saturn image it is a copy of
            for j in range(len(s1.recs)):
                if abs(s1.image(j).shape[0] - h) > 4:
                    continue
                r = align(ps, s1.image(j))
                if best is None or r[0] < best[0][0]:
                    best = (r, j)
        (mism, ph, ox, oy), j = best
        e, k = delta[j]
        left1, top1, left2, top2 = e
        phase = (left1 + ph) % 5
        img2 = narrow(s2.image(j), left2, phase, True)
        dx = dropped_between(left1, left2, phase)
        dy = top2 - top1
        c, cx, cy = crop(img2)
        assert c.shape[1] <= MAX_W and c.shape[0] <= MAX_H, (members, c.shape)
        npx = px + ox + dx + cx
        npy = py + oy + dy + cy
        fx = fy = 0
        if not 0 <= npx <= 255:
            fx = npx if npx < 0 else npx - 255
            npx -= fx
        if not 0 <= npy <= 255:
            fy = npy if npy < 0 else npy - 255
            npy -= fy
        if fx or fy:
            for f, fr in enumerate(frames):
                if fr and fr[0] in members:
                    frame_fix[fr[4]] = (fx, fy)
        new[off] = bytes([c.shape[1], c.shape[0], npx, npy]) + pack_psx(c)
        report.append((members, j, mism, int(np.count_nonzero(ps)),
                       (w, h), c.shape[::-1]))
    # palettes
    g2 = open(path("disks/us/BIN/F_GAME2.BIN"), "rb").read()

    def pspal(k):
        return list(struct.unpack_from("<16H", g2, 0x40000 + (k - 0x100) * 32))

    base, r1b0, r2b0 = pspal(0x120), s1.palette(0), s2.palette(0)
    assert all((base[i] & 0x7FFF) == (r1b0[i] & 0x7FFF) for i in range(1, 16))
    pals = []
    for k in range(0x100, 0x200):
        p = pspal(k)
        same = [i for i in range(1, 16) if (p[i] & 0x7FFF) == (base[i] & 0x7FFF)]
        if len(same) < 10:
            continue
        q = list(p)
        for i in same:
            q[i] = (r2b0[i] & 0x7FFF) | (p[i] & 0x8000)
        if q != p:
            pals.append((k, q))
    # the variants the Saturn has itself come from RICHTER2 as they are
    # (its frames select banks 2-4 and 6 where the PS1 frames select these;
    # bank 6 also differs in colour 12)
    sat = {0x12B: 2, 0x12C: 3, 0x12D: 4, 0x12E: 6}
    pals = [(k, [q[0]] + s2.palette(sat[k])[1:]) if k in sat else (k, q)
            for k, q in pals]
    # whip-brandish arms
    w2 = open(path("disks/saturn/RIC_W2.CHR"), "rb").read()
    blk = g2[ARM_BLOCK * 0x2000:(ARM_BLOCK + 1) * 0x2000]
    arms = []
    for arm in ARMS:
        (u0, v0, u1, v1), o, stride, ph, ox = arm[:5]
        h, w = v1 - v0, u1 - u0
        oy, ih = arm[5:] if len(arm) > 5 else (0, h)
        d = np.frombuffer(w2[o:o + ih * stride // 2], np.uint8)
        px = np.empty(ih * stride, np.uint8)
        px[0::2] = d >> 4
        px[1::2] = d & 15
        sat = px.reshape(ih, stride)
        nr = largest_component(narrow(sat, 0, ph, True))
        out = np.zeros((h, w), np.uint8)
        for y in range(ih):
            for x in range(nr.shape[1]):
                if nr[y, x] and 0 <= x + ox < w and 0 <= y + oy < h:
                    out[y + oy, x + ox] = nr[y, x]
        arms.append(((ARM_VRAM[0] + u0 // 4, ARM_VRAM[1] + v0, w // 4, h), out))
    # write RIC2.BIN
    out = bytearray(ric)
    end = SHEET_END - 8
    pos = data_start
    written = {}
    addr = {}
    for off in sorted(new):
        b = new[off]
        if b not in written:
            written[b] = pos
            out[pos:pos + len(b)] = b
            pos = (pos + len(b) + 3) & ~3
        addr[off] = written[b]
    for i, s in enumerate(sprites):
        struct.pack_into("<I", out, table + 4 * i, RAM + addr[s[5]])
    extra = pos
    blob = struct.pack("<I", len(pals))
    for k, q in pals:
        blob += struct.pack("<I16H", k, *q)
    blob += struct.pack("<I", len(arms))
    for (x, y, w, h), img in arms:
        blob += struct.pack("<4H", x, y, w, h) + pack_psx(img)
        blob += b"\0" * (-len(blob) % 4)
    out[pos:pos + len(blob)] = blob
    pos += len(blob)
    assert pos <= end, "costume sheet too big: %d > %d" % (pos, end)
    out[pos:end] = bytes(end - pos)
    struct.pack_into("<II", out, end, MAGIC, RAM + extra)
    for q, (fx, fy) in frame_fix.items():
        x, y = struct.unpack_from("<hh", out, q + 2)
        struct.pack_into("<hh", out, q + 2, x + fx, y + fy)
    assert len(out) == len(ric)
    open(path("build/us/RIC2.BIN"), "wb").write(out)
    bad = [r for r in report if r[2] > 0.1 * max(r[3], 1)]
    print("RIC2.BIN: %d sprite images (%d groups), %d bytes free; %d palettes, "
          "%d arm images; %d frames moved; weak matches: %s" % (
              n, len(groups), end - pos, len(pals), len(arms), len(frame_fix),
              [(r[0][0], r[1], r[2]) for r in bad]))
    if "-v" in sys.argv:
        for r in report:
            print("  sprites %s <- Saturn %d (R1 mismatch %d/%d px) %s -> %s" % r)
        for k, q in pals:
            print("  pal 0x%03X %s" % (k, " ".join("%04x" % v for v in q)))
    preview(ric, out, frames, syms, pals, base)


def preview(ric, out, frames, syms, pals, base):
    try:
        from PIL import Image
    except ImportError:
        return
    pal2 = dict(pals)[0x120]

    def rgb(v):
        return ((v & 31) * 8, ((v >> 5) & 31) * 8, ((v >> 10) & 31) * 8, 255)

    def draw(binary, f, pal):
        fr = frames[f]
        q = struct.unpack_from("<I", binary, syms["spritebank0"] - RAM + 4 * f)[0] - RAM
        idx, x, y = struct.unpack_from("<Hhh", binary, q)
        idx &= 0x7FFF
        p = struct.unpack_from("<I", binary, syms["richter_sprites"] - RAM + 4 * idx)[0] - RAM
        w, h, px, py = binary[p:p + 4]
        img = unpack_psx(bytes(binary[p + 4:]), w, h)
        im = Image.new("RGBA", (96, 112), (60, 60, 80, 255))
        for yy in range(h):
            for xx in range(w):
                if img[yy, xx]:
                    X, Y = 48 + x + px + xx, 56 + y + py + yy
                    if 0 <= X < 96 and 0 <= Y < 112:
                        im.putpixel((X, Y), rgb(pal[img[yy, xx]]))
        return im
    pick = [1, 20, 40, 56, 60, 130, 150, 200]
    pick = [f for f in pick if f < len(frames) and frames[f]]
    sheet = Image.new("RGBA", (96 * len(pick), 224), (30, 30, 40, 255))
    for k, f in enumerate(pick):
        sheet.paste(draw(ric, f, base), (96 * k, 0))
        sheet.paste(draw(out, f, pal2), (96 * k, 112))
    sheet = sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST)
    sheet.save(path("build/us/ric2_preview.png"))


if __name__ == "__main__":
    main()
