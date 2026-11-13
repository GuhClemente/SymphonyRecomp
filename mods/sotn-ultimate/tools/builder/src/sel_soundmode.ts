// SPDX-License-Identifier: AGPL-3.0-or-later
// The main menu's "Sound mode" button -> src/st/sel/mod_sel_soundmode_gfx.h:
// a byte-exact port of mods/sotn-ultimate/tools/gen_sel_soundmode.py (see its header). The
// button is assembled from the retail buttons in the PS1's ST/SEL/F_SEL.BIN
// plus the mod's own hand-traced capital S (CAP_MASK, mod data). The header's
// one array is g_ModSoundBtnPix (u16, 4bpp, 32 rows of 32).
export type Ps1Read = (path: string) => Promise<Uint8Array>;

export interface SoundBtnGfx {
    name: string; // "g_ModSoundBtnPix"
    size: number; // 2
    values: number[];
    wordEnd: number; // the generator's "text ends at x" print
    header: string; // the generator's whole header text
}

const CHUNK_X = [0x200, 0x220, 0x200, 0x220, 0x240, 0x260, 0x240, 0x260, 0x280,
    0x2a0, 0x280, 0x2a0, 0x2c0, 0x2e0, 0x2c0, 0x2e0, 0x300, 0x320,
    0x300, 0x320, 0x340, 0x360, 0x340, 0x360];
const W = 128;
const H = 32;
// insertion order matters (Counter ties)
const BUTTONS: [string, number, number][] = [
    ["filecopy", 0, 0x80], ["filedelete", 0, 0xa0], ["nameentry", 0, 0xc0],
    ["namechange", 0, 0xe0], ["fileselect", 0x80, 0x90],
];
const isWord = (v: number | null): boolean => v === 3 || v === 4 || v === 5;

const CAP_MASK = `
.........######........
......####...#####.....
.....###.......###.....
.....###.......##...#..
.##...###.........###..
.###..####.......###...
..###..#####....##.....
...##############......
....###.##########.....
.....#############.....
.....###############...
........#######..####..
..........######..###..
...........######..###.
............###...####.
............##....###..
...........##.....###..
...........##.....###..
.....#########...###...
....###########.###....
..#####.....######.....
`.split(/\s+/).filter((s) => s);

type Glyph = Map<string, number>; // "x,y" -> value, in Python dict order
const key = (x: number, y: number): string => x + "," + y;
const unkey = (k: string): [number, number] => k.split(",").map(Number) as [number, number];

function capShade(mask: string[]): string[] {
    const h = mask.length;
    const w = mask[0].length;
    const on = (x: number, y: number): boolean => x >= 0 && x < w && y >= 0 && y < h && mask[y][x] === "#";
    const rows: string[] = [];
    for (let y = 0; y < h; y++) {
        let row = "";
        for (let x = 0; x < w; x++) {
            if (!on(x, y)) {
                row += ".";
                continue;
            }
            const out = new Set<string>();
            for (const [d, dx, dy] of [["L", -1, 0], ["R", 1, 0], ["T", 0, -1], ["B", 0, 1]] as [string, number, number][]) {
                if (!on(x + dx, y + dy)) {
                    out.add(d);
                }
            }
            const onlyL = out.size === 1 && out.has("L");
            let v: number;
            if (out.size === 0 || onlyL) {
                v = 5;
                if (out.size === 0 && (!on(x + 2, y) || !on(x, y + 2))
                    && on(x - 1, y) && on(x, y - 1) && on(x - 1, y - 1)) {
                    v = 4;
                }
            } else if ([...out].filter((d) => d !== "L").length >= 2 || (out.has("L") && out.has("T"))) {
                v = 3;
            } else {
                v = 4;
            }
            row += String(v);
        }
        rows.push(row);
    }
    const lstrip = (r: string): number => r.length - r.replace(/^\.+/, "").length;
    const lo = Math.min(...rows.map(lstrip));
    const hi = Math.max(...rows.map((r) => r.replace(/\.+$/, "").length));
    return rows.map((r) => r.slice(lo, hi));
}

export async function buildSelSoundMode(ps1: Ps1Read): Promise<SoundBtnGfx> {
    const data = await ps1("ST/SEL/F_SEL.BIN");
    const vram: number[][] = [];
    for (let y = 0; y < 256; y++) {
        vram.push(new Array<number>(1024).fill(0));
    }
    CHUNK_X.forEach((x0, n) => {
        const y0 = (n << 6) & 0x80;
        for (let y = 0; y < 128; y++) {
            for (let x = 0; x < 32; x++) {
                const o = n * 0x2000 + (y * 32 + x) * 2;
                if (o + 1 >= data.length) {
                    throw new Error("F_SEL.BIN too short");
                }
                vram[y0 + y][x0 + x] = data[o] | (data[o + 1] << 8);
            }
        }
    });
    const crop = (xb: number, u: number, v: number, w: number, h: number): number[][] => {
        const out: number[][] = [];
        for (let y = 0; y < h; y++) {
            const row: number[] = [];
            for (let x = 0; x < w; x++) {
                const uu = u + x;
                row.push((vram[v + y][xb + (uu >> 2)] >> ((uu & 3) * 4)) & 15);
            }
            out.push(row);
        }
        return out;
    };
    const btn = new Map<string, number[][]>();
    for (const [k, u, v] of BUTTONS) {
        btn.set(k, crop(0x300, u, v, W, H));
    }
    const wordMask = (b: number[][]): boolean[][] => {
        const m: boolean[][] = [];
        for (let y = 0; y < H; y++) {
            const row: boolean[] = [];
            for (let x = 0; x < W; x++) {
                let hit = false;
                if (!(x < 8 || y < 7)) {
                    for (let yy = Math.max(0, y - 1); yy < Math.min(H, y + 2) && !hit; yy++) {
                        for (let xx = Math.max(0, x - 1); xx < Math.min(W, x + 2); xx++) {
                            if (isWord(b[yy][xx])) {
                                hit = true;
                                break;
                            }
                        }
                    }
                }
                row.push(hit);
            }
            m.push(row);
        }
        return m;
    };
    const names = BUTTONS.map(([k]) => k);
    const masks = new Map<string, boolean[][]>();
    for (const k of names) {
        masks.set(k, wordMask(btn.get(k)!));
    }
    const tmpl: (number | null)[][] = [];
    for (let y = 0; y < H; y++) {
        const row: (number | null)[] = [];
        for (let x = 0; x < W; x++) {
            const counts = new Map<number, number>();
            for (const k of names) {
                if (!masks.get(k)![y][x]) {
                    const v = btn.get(k)![y][x];
                    counts.set(v, (counts.get(v) ?? 0) + 1);
                }
            }
            if (counts.size) {
                let best = -1;
                let bestN = 0;
                for (const [v, n] of counts) { // first of the most common
                    if (n > bestN) {
                        best = v;
                        bestN = n;
                    }
                }
                row.push(best);
            } else if (x < 31) {
                row.push(null);
            } else {
                row.push(0);
            }
        }
        tmpl.push(row);
    }
    for (let y = 0; y < H; y++) {
        for (let x = 0; x < W; x++) {
            if (tmpl[y][x] !== null) {
                continue;
            }
            let found = false;
            for (const dx of [-7, 7, -14, 14, -21, 21]) {
                if (x + dx >= 0 && x + dx < W && tmpl[y][x + dx] !== null
                    && !names.some((k) => masks.get(k)![y][x + dx])) {
                    tmpl[y][x] = tmpl[y][x + dx];
                    found = true;
                    break;
                }
            }
            if (!found) {
                tmpl[y][x] = 6;
            }
        }
    }
    const img = tmpl.map((r) => r.slice() as number[]);
    const placed = new Set<string>();
    const put = (gl: Glyph, ox: number, oy: number): void => {
        for (const [k, v] of gl) {
            let [x, y] = unkey(k);
            x += ox;
            y += oy;
            if (x >= 0 && x < W && y >= 0 && y < H) {
                if (v === 1 && isWord(img[y][x]) && placed.has(key(x, y))) {
                    continue;
                }
                img[y][x] = v;
                placed.add(key(x, y));
            }
        }
    };

    const capArt = capShade(CAP_MASK);
    const cap: Glyph = new Map();
    capArt.forEach((row, y) => {
        for (let x = 0; x < row.length; x++) {
            if (row[x] !== ".") {
                cap.set(key(x, y), Number(row[x]));
            }
        }
    });
    for (const k of [...cap.keys()]) {
        const [x, y] = unkey(k);
        for (const dy of [-1, 0, 1]) {
            for (const dx of [-1, 0, 1]) {
                if (!cap.has(key(x + dx, y + dy))) {
                    cap.set(key(x + dx, y + dy), 1);
                }
            }
        }
    }
    put(cap, 31 - capArt[0].length, 29 - capArt.length);

    const glyph = (name: string, x0: number, x1: number, rot = false, y0 = 12, y1 = 31): Glyph => {
        const b = btn.get(name)!;
        let g: Glyph = new Map();
        for (let y = y0; y < y1; y++) {
            for (let x = x0; x <= x1; x++) {
                const v = b[y][x];
                let edge = false;
                if (v === 1) {
                    for (let yy = y - 1; yy < y + 2 && !edge; yy++) {
                        // Python's b[-1] wraps to the last row
                        const r = b[yy < 0 ? yy + H : yy];
                        if (r === undefined) {
                            throw new Error("glyph row out of range");
                        }
                        for (let xx = Math.max(x0, x - 1); xx < Math.min(x1, x + 1) + 1; xx++) {
                            if (isWord(r[xx])) {
                                edge = true;
                                break;
                            }
                        }
                    }
                }
                if (isWord(v) || edge) {
                    g.set(key(x - x0, y), v);
                }
            }
        }
        if (rot) {
            const r: Glyph = new Map();
            for (const [k, v] of g) {
                const [x, y] = unkey(k);
                r.set(key(x1 - x0 - x, 50 - y), v);
            }
            g = r;
        }
        return g;
    };
    const glyphs = new Map<string, Glyph>([
        ["o", glyph("filecopy", 85, 95)],
        ["n", glyph("namechange", 96, 107)],
        ["d", glyph("filedelete", 71, 82)],
        ["m", glyph("nameentry", 41, 57)],
        ["e", glyph("nameentry", 57, 67)],
    ]);
    glyphs.set("u", glyph("namechange", 96, 107, true));
    const o = glyphs.get("o")!;
    o.set(key(5, 22), 4);
    o.set(key(5, 28), 4);
    o.set(key(6, 28), 4);

    let x = 31;
    for (const word of ["ound", "mode"]) {
        for (const ch of word) {
            const g = glyphs.get(ch)!;
            const xs = [...g.keys()].map((k) => unkey(k)[0]);
            const lo = Math.min(...xs);
            const hi = Math.max(...xs);
            put(g, x - lo, 0);
            x += hi - lo;
        }
        x += 5;
    }
    const wordEnd = x - 5;

    const values: number[] = [];
    const lines = ["// generated by mods/sotn-ultimate/tools/gen_sel_soundmode.py, do not edit",
        "#define MOD_SOUND_BTN_W " + W, "#define MOD_SOUND_BTN_H " + H,
        "static const u16 g_ModSoundBtnPix[] MOD_RO __attribute__((aligned(4))) = {"];
    for (let y = 0; y < H; y++) {
        const words: string[] = [];
        for (let xx = 0; xx < W; xx += 4) {
            let v = 0;
            for (let i = 0; i < 4; i++) {
                v += img[y][xx + i] << (i * 4);
            }
            values.push(v);
            words.push("0x" + v.toString(16).toUpperCase().padStart(4, "0"));
        }
        lines.push("    " + words.join(", ") + ",");
    }
    lines.push("};");
    return { name: "g_ModSoundBtnPix", size: 2, values, wordEnd, header: lines.join("\n") + "\n" };
}
