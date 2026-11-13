// SPDX-License-Identifier: AGPL-3.0-or-later
// zlib / raw DEFLATE decompression (RFC 1950 / 1951), synchronous, for the
// converter's preview PNGs (gdn_assets.read_png_rgba reads them with
// zlib.decompress). Decoding is exact by definition: no options.

class BitReader {
    d: Uint8Array;
    pos: number;
    bit: number;
    cnt: number;

    constructor(d: Uint8Array, pos: number) {
        this.d = d;
        this.pos = pos;
        this.bit = 0;
        this.cnt = 0;
    }

    bits(n: number): number {
        while (this.cnt < n) {
            if (this.pos >= this.d.length) {
                throw new Error("inflate: unexpected end of data");
            }
            this.bit |= this.d[this.pos++] << this.cnt;
            this.cnt += 8;
        }
        const v = this.bit & ((1 << n) - 1);
        this.bit >>>= n;
        this.cnt -= n;
        return v;
    }

    align(): void {
        this.bit = 0;
        this.cnt = 0;
    }
}

// canonical Huffman table: counts per length, symbols in code order
interface Huff {
    count: Int32Array;
    symbol: Int32Array;
}

function huff(lengths: ArrayLike<number>, n: number): Huff {
    const count = new Int32Array(16);
    for (let i = 0; i < n; i++) {
        count[lengths[i]]++;
    }
    count[0] = 0;
    const offs = new Int32Array(16);
    for (let len = 1; len < 15; len++) {
        offs[len + 1] = offs[len] + count[len];
    }
    const symbol = new Int32Array(n);
    for (let i = 0; i < n; i++) {
        if (lengths[i] !== 0) {
            symbol[offs[lengths[i]]++] = i;
        }
    }
    return { count, symbol };
}

function decodeSym(br: BitReader, h: Huff): number {
    let code = 0;
    let first = 0;
    let index = 0;
    for (let len = 1; len < 16; len++) {
        code |= br.bits(1);
        const c = h.count[len];
        if (code - c < first) {
            return h.symbol[index + (code - first)];
        }
        index += c;
        first += c;
        first <<= 1;
        code <<= 1;
    }
    throw new Error("inflate: bad code");
}

const LBASE = [3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115,
    131, 163, 195, 227, 258];
const LEXT = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0];
const DBASE = [1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537,
    2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577];
const DEXT = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13];
const ORDER = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];

let fixed: [Huff, Huff] | null = null;

function fixedTables(): [Huff, Huff] {
    if (fixed === null) {
        const l = new Array<number>(288);
        for (let i = 0; i < 288; i++) {
            l[i] = i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8;
        }
        fixed = [huff(l, 288), huff(new Array<number>(30).fill(5), 30)];
    }
    return fixed;
}

// raw DEFLATE stream at d[pos..]
export function inflateRaw(d: Uint8Array, pos = 0): Uint8Array {
    const br = new BitReader(d, pos);
    let out = new Uint8Array(Math.max(1024, d.length * 4));
    let n = 0;
    const need = (k: number): void => {
        if (n + k > out.length) {
            let size = out.length * 2;
            while (size < n + k) {
                size *= 2;
            }
            const o = new Uint8Array(size);
            o.set(out.subarray(0, n));
            out = o;
        }
    };
    let last = 0;
    while (!last) {
        last = br.bits(1);
        const type = br.bits(2);
        if (type === 0) {
            br.align();
            const p = br.pos;
            const len = d[p] | (d[p + 1] << 8);
            need(len);
            out.set(d.subarray(p + 4, p + 4 + len), n);
            n += len;
            br.pos = p + 4 + len;
            continue;
        }
        let lit: Huff;
        let dist: Huff;
        if (type === 1) {
            [lit, dist] = fixedTables();
        } else if (type === 2) {
            const nlen = br.bits(5) + 257;
            const ndist = br.bits(5) + 1;
            const ncode = br.bits(4) + 4;
            const lengths = new Array<number>(320).fill(0);
            for (let i = 0; i < ncode; i++) {
                lengths[ORDER[i]] = br.bits(3);
            }
            const lencode = huff(lengths, 19);
            let i = 0;
            const ll = new Array<number>(nlen + ndist).fill(0);
            while (i < nlen + ndist) {
                const sym = decodeSym(br, lencode);
                if (sym < 16) {
                    ll[i++] = sym;
                } else {
                    let rep = 0;
                    let val = 0;
                    if (sym === 16) {
                        val = ll[i - 1];
                        rep = 3 + br.bits(2);
                    } else if (sym === 17) {
                        rep = 3 + br.bits(3);
                    } else {
                        rep = 11 + br.bits(7);
                    }
                    while (rep--) {
                        ll[i++] = val;
                    }
                }
            }
            lit = huff(ll.slice(0, nlen), nlen);
            dist = huff(ll.slice(nlen), ndist);
        } else {
            throw new Error("inflate: bad block type");
        }
        for (;;) {
            const sym = decodeSym(br, lit);
            if (sym < 256) {
                need(1);
                out[n++] = sym;
            } else if (sym === 256) {
                break;
            } else {
                const s = sym - 257;
                const len = LBASE[s] + br.bits(LEXT[s]);
                const ds = decodeSym(br, dist);
                const back = DBASE[ds] + br.bits(DEXT[ds]);
                need(len);
                for (let k = 0; k < len; k++) {
                    out[n] = out[n - back];
                    n++;
                }
            }
        }
    }
    return out.slice(0, n);
}

// zlib stream (2-byte header, no preset dictionary)
export function inflateZlib(d: Uint8Array): Uint8Array {
    if ((d[0] & 15) !== 8 || ((d[0] << 8) | d[1]) % 31 !== 0 || (d[1] & 0x20) !== 0) {
        throw new Error("inflate: not a zlib stream");
    }
    return inflateRaw(d, 2);
}
