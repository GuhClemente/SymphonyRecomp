// SPDX-License-Identifier: AGPL-3.0-or-later
// Maria's Saturn HUD art in F_GAME2's empty VRAM -> build/us/F_GAME2.BIN
// (the disc's BIN/F_GAME2.BIN): a byte-exact port of mods/sotn-ultimate/tools/gen_mar_hud.py
// (see its header for the layout). Reads the Saturn's MARIA.PRG and MAR_W.CHR
// and the PS1's BIN/F_GAME2.BIN; returns the whole new file.
export type DiscRead = (path: string) => Promise<Uint8Array>;

const PRG_BASE = 0x060a5000;
const RES_TABLE = 0x060c6d68;
const CHR_ORDER = [1, 2, 3, 8, 4, 6, 5, 7, 9, 10, 11, 12];
const CHUNK_X = [0x200, 0x220, 0x200, 0x220, 0x240, 0x260, 0x240, 0x260, 0x280,
    0x2a0, 0x280, 0x2a0, 0x2c0, 0x2e0, 0x2c0, 0x2e0, 0x300, 0x320,
    0x300, 0x320, 0x340, 0x360, 0x340, 0x360, 0x380, 0x3a0, 0x380,
    0x3a0, 0x3c0, 0x3e0, 0x3c0, 0x3e0];
const TPAGE_X = 0x200;
const TPAGE_Y = 0x100;
const PLATE_DROP = new Set([12, 13, 14, 15, 16, 44, 48, 58, 59, 60, 61, 62, 63, 77, 78, 79]);

type Rows = number[][];

const be16 = (d: Uint8Array, o: number): number => (d[o] << 8) | d[o + 1];
const be32 = (d: Uint8Array, o: number): number => ((d[o] << 24) | (d[o + 1] << 16) | (d[o + 2] << 8) | d[o + 3]) >>> 0;

function check(ok: boolean, what: string): void {
    if (!ok) {
        throw new Error("mar hud: " + what);
    }
}

function packageImages(prg: Uint8Array, chr: Uint8Array, res: number): [number, number, Rows][] {
    let base = 0;
    for (const r of CHR_ORDER) {
        let o = be32(prg, RES_TABLE - PRG_BASE + r * 12) - PRG_BASE;
        const recs: [number, number, number][] = [];
        let size: number;
        for (;;) {
            const w = prg[o];
            const h = prg[o + 1];
            const u = be16(prg, o + 2);
            o += 4;
            if (w === 0 && h === 0) {
                size = u * 8;
                break;
            }
            recs.push([w * 2, h * 2, u * 8]);
        }
        if (r === res) {
            return recs.map(([w, h, off]) => {
                const d = chr.subarray(base + off, base + off + (w * h) / 2);
                const px: number[] = [];
                for (const b of d) {
                    px.push(b >> 4, b & 15);
                }
                const rows: Rows = [];
                for (let y = 0; y < h; y++) {
                    rows.push(px.slice(y * w, (y + 1) * w));
                }
                return [w, h, rows] as [number, number, Rows];
            });
        }
        base += size;
    }
    throw new Error("mar hud: resource " + res);
}

const cols = (rows: Rows, keep: number[]): Rows => rows.map((row) => keep.map((c) => row[c]));
const range = (n: number): number[] => Array.from({ length: n }, (_, i) => i);

// (u, v, rows) blocks in tpage 0x18
export function marHudArt(prg: Uint8Array, chr: Uint8Array): [number, number, Rows][] {
    const img = packageImages(prg, chr, 5);
    const art: [number, number, Rows][] = [];
    const [hw, , holder] = img[5];
    art.push([128, 136, cols(holder, range(hw).filter((c) => (2 + c) % 5 !== 4))]);
    const [pw, , plate] = img[0];
    art.push([168, 136, cols(plate, range(pw).filter((c) => !PLATE_DROP.has(c)))]);
    for (let n = 0; n < 10; n++) {
        art.push([168 + 7 * n, 160, cols(img[6 + n][2], range(7))]);
    }
    const tile = img[17][2];
    const row = [3, 4, 6, 7].map((c) => tile[0][c]).concat([0], [3, 4, 6].map((c) => tile[0][c]));
    const bars: Rows = [];
    for (let i = 0; i < 96; i++) {
        bars.push(row);
    }
    art.push([240, 136, bars]);
    for (const [u, v, rows] of art) {
        check(u + rows[0].length <= 256 && v + rows.length <= 232, "block " + u + "," + v);
        for (const r of rows) {
            check(r.every((c) => c !== undefined), "short image");
        }
    }
    check(art[0][2][0].length === 38 && art[1][2][0].length === 64, "widths");
    return art;
}

function chunkOffset(hx: number, vy: number): number {
    for (let i = 0; i < CHUNK_X.length; i++) {
        const x = CHUNK_X[i];
        const y = ((i << 6) & 0x80) + 0x100;
        if (x <= hx && hx < x + 32 && y <= vy && vy < y + 128 && i !== 0x1a) {
            return i * 0x2000 + (vy - y) * 64 + (hx - x) * 2;
        }
    }
    throw new Error("mar hud: no chunk for " + hx + "," + vy);
}

export async function buildMarHud(saturn: DiscRead, ps1: DiscRead): Promise<Uint8Array> {
    const data = new Uint8Array(await ps1("BIN/F_GAME2.BIN"));
    check(data.length === 0x42000, "F_GAME2.BIN size");
    for (let vy = 136; vy < 232; vy++) {
        for (let hx = 32; hx < 64; hx++) {
            const off = chunkOffset(TPAGE_X + hx, TPAGE_Y + vy);
            check(data[off] === 0 && data[off + 1] === 0, "target block not empty");
        }
    }
    const px = new Map<number, number>(); // u * 256 + v
    for (const [u, v, rows] of marHudArt(await saturn("MARIA.PRG"), await saturn("MAR_W.CHR"))) {
        rows.forEach((row, y) => row.forEach((c, x) => px.set((u + x) * 256 + v + y, c)));
    }
    for (let vy = 136; vy < 232; vy++) {
        for (let hx = 32; hx < 64; hx++) {
            let val = 0;
            for (let k = 0; k < 4; k++) {
                val |= (px.get((hx * 4 + k) * 256 + vy) ?? 0) << (4 * k);
            }
            const off = chunkOffset(TPAGE_X + hx, TPAGE_Y + vy);
            data[off] = val & 0xff;
            data[off + 1] = val >> 8;
        }
    }
    return data;
}
