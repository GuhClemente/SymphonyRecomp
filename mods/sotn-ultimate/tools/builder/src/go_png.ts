// SPDX-License-Identifier: AGPL-3.0-or-later
// The decomp asset extractor's PNG files, byte for byte: a port of
// tools/sotn-assets/util/png/pngencode.go (indexed 8-bit PNG with PLTE and
// tRNS) and of the Go standard library's zlib writer at zlib.BestSpeed
// (compress/flate: deflatefast.go's encoder, huffman_bit_writer.go and
// huffman_code.go), which it compresses the image with. Only what BestSpeed
// takes for one Write + Close of less than 64 KiB (one block) is ported.

const MAX_STORE = 65535;
const MAX_MATCH_OFFSET = 1 << 15;
const MATCH_TYPE = 1 << 30;
const END_BLOCK = 256;
const MAX_NUM_LIT = 286;
const OFFSET_CODE_COUNT = 30;
const CODEGEN_ORDER = [16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15];
const LENGTH_EXTRA = [0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0];
const LENGTH_BASE = [0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112,
    128, 160, 192, 224, 255];
const OFFSET_EXTRA = [0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13];
const OFFSET_BASE = [0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536,
    2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576];
const MAX_INT32 = 0x7FFFFFFF;

function lengthCode(xlen: number): number {
    if (xlen === 255) {
        return 28;
    }
    let c = 0;
    while (c < 27 && LENGTH_BASE[c + 1] <= xlen) {
        c++;
    }
    return c;
}

function offsetCode(off: number): number {
    let c = 0;
    while (c < 29 && OFFSET_BASE[c + 1] <= off) {
        c++;
    }
    return c;
}

function reverseBits(code: number, n: number): number {
    let r = 0;
    for (let i = 0; i < n; i++) {
        r = (r << 1) | ((code >> i) & 1);
    }
    return r;
}

// huffman_code.go: huffmanEncoder.generate
class HuffEncoder {
    code: Int32Array;
    len: Int32Array;

    constructor(n: number) {
        this.code = new Int32Array(n);
        this.len = new Int32Array(n);
    }

    bitLength(freq: ArrayLike<number>): number {
        let total = 0;
        for (let i = 0; i < freq.length; i++) {
            if (freq[i] !== 0) {
                total += freq[i] * this.len[i];
            }
        }
        return total;
    }

    generate(freq: ArrayLike<number>, maxBits: number): void {
        const list: [number, number][] = []; // (literal, freq)
        for (let i = 0; i < freq.length; i++) {
            if (freq[i] !== 0) {
                list.push([i, freq[i]]);
            } else {
                this.len[i] = 0;
            }
        }
        if (list.length <= 2) {
            list.forEach(([lit], i) => {
                this.code[lit] = i;
                this.len[lit] = 1;
            });
            return;
        }
        list.sort((a, b) => a[1] - b[1] || a[0] - b[0]);
        const bitCount = bitCounts(list, maxBits);
        // assignEncodingAndSize
        let code = 0;
        let rest = list;
        for (let n = 0; n < bitCount.length; n++) {
            const bits = bitCount[n];
            code = (code << 1) & 0xFFFF;
            if (n === 0 || bits === 0) {
                continue;
            }
            const chunk = rest.slice(rest.length - bits).sort((a, b) => a[0] - b[0]);
            for (const [lit] of chunk) {
                this.code[lit] = reverseBits(code, n);
                this.len[lit] = n;
                code = (code + 1) & 0xFFFF;
            }
            rest = rest.slice(0, rest.length - bits);
        }
    }
}

function bitCounts(sorted: [number, number][], maxBitsIn: number): number[] {
    const n = sorted.length;
    const list = sorted.concat([[0xFFFF, MAX_INT32]]);
    let maxBits = maxBitsIn;
    if (maxBits > n - 1) {
        maxBits = n - 1;
    }
    const levels = Array.from({ length: 17 }, () => ({ level: 0, lastFreq: 0, nextCharFreq: 0, nextPairFreq: 0, needed: 0 }));
    const leafCounts = Array.from({ length: 17 }, () => new Array<number>(17).fill(0));
    for (let level = 1; level <= maxBits; level++) {
        levels[level] = { level, lastFreq: list[1][1], nextCharFreq: list[2][1],
            nextPairFreq: list[0][1] + list[1][1], needed: 0 };
        leafCounts[level][level] = 2;
        if (level === 1) {
            levels[level].nextPairFreq = MAX_INT32;
        }
    }
    levels[maxBits].needed = 2 * n - 4;
    let level = maxBits;
    for (;;) {
        const l = levels[level];
        if (l.nextPairFreq === MAX_INT32 && l.nextCharFreq === MAX_INT32) {
            l.needed = 0;
            levels[level + 1].nextPairFreq = MAX_INT32;
            level++;
            continue;
        }
        const prevFreq = l.lastFreq;
        if (l.nextCharFreq < l.nextPairFreq) {
            const k = leafCounts[level][level] + 1;
            l.lastFreq = l.nextCharFreq;
            leafCounts[level][level] = k;
            l.nextCharFreq = list[k][1];
        } else {
            l.lastFreq = l.nextPairFreq;
            for (let i = 0; i < level; i++) {
                leafCounts[level][i] = leafCounts[level - 1][i];
            }
            levels[l.level - 1].needed = 2;
        }
        if (--l.needed === 0) {
            if (l.level === maxBits) {
                break;
            }
            levels[l.level + 1].nextPairFreq = prevFreq + l.lastFreq;
            level++;
        } else {
            while (levels[level - 1].needed > 0) {
                level--;
            }
        }
    }
    if (leafCounts[maxBits][maxBits] !== n) {
        throw new Error("go flate: leafCounts[maxBits][maxBits] != n");
    }
    const bitCount = new Array<number>(maxBits + 1).fill(0);
    let bits = 1;
    const counts = leafCounts[maxBits];
    for (let lv = maxBits; lv > 0; lv--) {
        bitCount[bits] = counts[lv] - counts[lv - 1];
        bits++;
    }
    return bitCount;
}

// the fixed offset encoder writeBlockHuff uses (huffOffset)
const HUFF_OFFSET = ((): HuffEncoder => {
    const h = new HuffEncoder(OFFSET_CODE_COUNT);
    const f = new Array<number>(OFFSET_CODE_COUNT).fill(0);
    f[0] = 1;
    h.generate(f, 15);
    return h;
})();

class BitWriter {
    out: number[] = [];
    bits = 0;
    nbits = 0;

    writeBits(v: number, n: number): void {
        for (let i = 0; i < n; i++) {
            this.bits |= ((v >>> i) & 1) << this.nbits;
            if (++this.nbits === 8) {
                this.out.push(this.bits);
                this.bits = 0;
                this.nbits = 0;
            }
        }
    }

    flush(): void {
        if (this.nbits > 0) {
            this.out.push(this.bits);
            this.bits = 0;
            this.nbits = 0;
        }
    }
}

// huffman_bit_writer.go
class HuffmanBitWriter {
    w = new BitWriter();
    literalFreq = new Int32Array(MAX_NUM_LIT);
    offsetFreq = new Int32Array(OFFSET_CODE_COUNT);
    codegen = new Uint8Array(MAX_NUM_LIT + OFFSET_CODE_COUNT + 1);
    codegenFreq = new Int32Array(19);
    literalEncoding = new HuffEncoder(MAX_NUM_LIT);
    offsetEncoding = new HuffEncoder(OFFSET_CODE_COUNT);
    codegenEncoding = new HuffEncoder(19);

    writeCode(enc: HuffEncoder, sym: number): void {
        this.w.writeBits(enc.code[sym], enc.len[sym]);
    }

    generateCodegen(numLiterals: number, numOffsets: number, lit: HuffEncoder, off: HuffEncoder): void {
        this.codegenFreq.fill(0);
        const cg = this.codegen;
        for (let i = 0; i < numLiterals; i++) {
            cg[i] = lit.len[i];
        }
        for (let i = 0; i < numOffsets; i++) {
            cg[numLiterals + i] = off.len[i];
        }
        cg[numLiterals + numOffsets] = 255;
        let size = cg[0];
        let count = 1;
        let outIndex = 0;
        for (let inIndex = 1; size !== 255; inIndex++) {
            const nextSize = cg[inIndex];
            if (nextSize === size) {
                count++;
                continue;
            }
            if (size !== 0) {
                cg[outIndex++] = size;
                this.codegenFreq[size]++;
                count--;
                while (count >= 3) {
                    const n = Math.min(6, count);
                    cg[outIndex++] = 16;
                    cg[outIndex++] = n - 3;
                    this.codegenFreq[16]++;
                    count -= n;
                }
            } else {
                while (count >= 11) {
                    const n = Math.min(138, count);
                    cg[outIndex++] = 18;
                    cg[outIndex++] = n - 11;
                    this.codegenFreq[18]++;
                    count -= n;
                }
                if (count >= 3) {
                    cg[outIndex++] = 17;
                    cg[outIndex++] = count - 3;
                    this.codegenFreq[17]++;
                    count = 0;
                }
            }
            count--;
            for (; count >= 0; count--) {
                cg[outIndex++] = size;
                this.codegenFreq[size]++;
            }
            size = nextSize;
            count = 1;
        }
        cg[outIndex] = 255;
    }

    dynamicSize(lit: HuffEncoder, off: HuffEncoder): [number, number] {
        let numCodegens = 19;
        while (numCodegens > 4 && this.codegenFreq[CODEGEN_ORDER[numCodegens - 1]] === 0) {
            numCodegens--;
        }
        const header = 3 + 5 + 5 + 4 + 3 * numCodegens + this.codegenEncoding.bitLength(this.codegenFreq) +
            this.codegenFreq[16] * 2 + this.codegenFreq[17] * 3 + this.codegenFreq[18] * 7;
        return [header + lit.bitLength(this.literalFreq) + off.bitLength(this.offsetFreq), numCodegens];
    }

    writeStoredHeader(length: number, eof: boolean): void {
        this.w.writeBits(eof ? 1 : 0, 3);
        this.w.flush();
        this.w.writeBits(length, 16);
        this.w.writeBits(~length & 0xFFFF, 16);
    }

    writeBytes(b: Uint8Array): void {
        for (const v of b) {
            this.w.out.push(v);
        }
    }

    writeDynamicHeader(numLiterals: number, numOffsets: number, numCodegens: number, eof: boolean): void {
        this.w.writeBits(eof ? 5 : 4, 3);
        this.w.writeBits(numLiterals - 257, 5);
        this.w.writeBits(numOffsets - 1, 5);
        this.w.writeBits(numCodegens - 4, 4);
        for (let i = 0; i < numCodegens; i++) {
            this.w.writeBits(this.codegenEncoding.len[CODEGEN_ORDER[i]], 3);
        }
        let i = 0;
        for (;;) {
            const cw = this.codegen[i++];
            if (cw === 255) {
                break;
            }
            this.writeCode(this.codegenEncoding, cw);
            if (cw === 16) {
                this.w.writeBits(this.codegen[i++], 2);
            } else if (cw === 17) {
                this.w.writeBits(this.codegen[i++], 3);
            } else if (cw === 18) {
                this.w.writeBits(this.codegen[i++], 7);
            }
        }
    }

    storedFits(input: Uint8Array, size: number): boolean {
        return input.length <= MAX_STORE && (input.length + 5) * 8 < size + (size >> 4);
    }

    writeBlockHuff(eof: boolean, input: Uint8Array): void {
        this.literalFreq.fill(0);
        for (const v of input) {
            this.literalFreq[v]++;
        }
        this.literalFreq[END_BLOCK] = 1;
        this.offsetFreq[0] = 1;
        this.literalEncoding.generate(this.literalFreq, 15);
        this.generateCodegen(257, 1, this.literalEncoding, HUFF_OFFSET);
        this.codegenEncoding.generate(this.codegenFreq, 7);
        const [size, numCodegens] = this.dynamicSize(this.literalEncoding, HUFF_OFFSET);
        if (this.storedFits(input, size)) {
            this.writeStoredHeader(input.length, eof);
            this.writeBytes(input);
            return;
        }
        this.writeDynamicHeader(257, 1, numCodegens, eof);
        for (const v of input) {
            this.writeCode(this.literalEncoding, v);
        }
        this.writeCode(this.literalEncoding, END_BLOCK);
    }

    writeBlockDynamic(tokens: number[], eof: boolean, input: Uint8Array): void {
        tokens = tokens.concat([END_BLOCK]);
        // indexTokens
        this.literalFreq.fill(0);
        this.offsetFreq.fill(0);
        for (const t of tokens) {
            if (t < MATCH_TYPE) {
                this.literalFreq[t]++;
                continue;
            }
            this.literalFreq[257 + lengthCode((t >>> 22) & 0xFF)]++;
            this.offsetFreq[offsetCode(t & 0x3FFFFF)]++;
        }
        let numLiterals = MAX_NUM_LIT;
        while (this.literalFreq[numLiterals - 1] === 0) {
            numLiterals--;
        }
        let numOffsets = OFFSET_CODE_COUNT;
        while (numOffsets > 0 && this.offsetFreq[numOffsets - 1] === 0) {
            numOffsets--;
        }
        if (numOffsets === 0) {
            this.offsetFreq[0] = 1;
            numOffsets = 1;
        }
        this.literalEncoding.generate(this.literalFreq, 15);
        this.offsetEncoding.generate(this.offsetFreq, 15);
        this.generateCodegen(numLiterals, numOffsets, this.literalEncoding, this.offsetEncoding);
        this.codegenEncoding.generate(this.codegenFreq, 7);
        const [size, numCodegens] = this.dynamicSize(this.literalEncoding, this.offsetEncoding);
        if (this.storedFits(input, size)) {
            this.writeStoredHeader(input.length, eof);
            this.writeBytes(input);
            return;
        }
        this.writeDynamicHeader(numLiterals, numOffsets, numCodegens, eof);
        for (const t of tokens) {
            if (t < MATCH_TYPE) {
                this.writeCode(this.literalEncoding, t);
                continue;
            }
            const xlen = (t >>> 22) & 0xFF;
            const lc = lengthCode(xlen);
            this.writeCode(this.literalEncoding, 257 + lc);
            if (LENGTH_EXTRA[lc] > 0) {
                this.w.writeBits(xlen - LENGTH_BASE[lc], LENGTH_EXTRA[lc]);
            }
            const off = t & 0x3FFFFF;
            const oc = offsetCode(off);
            this.writeCode(this.offsetEncoding, oc);
            if (OFFSET_EXTRA[oc] > 0) {
                this.w.writeBits(off - OFFSET_BASE[oc], OFFSET_EXTRA[oc]);
            }
        }
    }
}

const load32 = (b: Uint8Array, i: number): number => (b[i] | (b[i + 1] << 8) | (b[i + 2] << 16) | (b[i + 3] << 24)) >>> 0;
const hash14 = (u: number): number => Math.imul(u, 0x1e35a7bd) >>> 18;

// deflatefast.go: encode() of a fresh encoder (no history: every table entry
// fails the distance check)
function encodeFast(src: Uint8Array): number[] {
    const dst: number[] = [];
    const emitLiteral = (a: number, b: number): void => {
        for (let i = a; i < b; i++) {
            dst.push(src[i]);
        }
    };
    if (src.length < 17) {
        emitLiteral(0, src.length);
        return dst;
    }
    const cur = MAX_STORE;
    const tOff = new Int32Array(1 << 14);
    const tVal = new Uint32Array(1 << 14);
    const sLimit = src.length - 15;
    let nextEmit = 0;
    let s = 0;
    let cv = load32(src, s);
    let nextHash = hash14(cv);
    outer:
    for (;;) {
        let skip = 32;
        let nextS = s;
        let cand = 0;
        let candVal = 0;
        for (;;) {
            s = nextS;
            const between = skip >> 5;
            nextS = s + between;
            skip += between;
            if (nextS > sLimit) {
                break outer;
            }
            cand = tOff[nextHash];
            candVal = tVal[nextHash];
            const now = load32(src, nextS);
            tOff[nextHash] = s + cur;
            tVal[nextHash] = cv;
            nextHash = hash14(now);
            const offset = s - (cand - cur);
            if (offset > MAX_MATCH_OFFSET || cv !== candVal) {
                cv = now;
                continue;
            }
            break;
        }
        emitLiteral(nextEmit, s);
        for (;;) {
            s += 4;
            const t = cand - cur + 4;
            // matchLen
            let s1 = s + 258 - 4;
            if (s1 > src.length) {
                s1 = src.length;
            }
            let l = 0;
            while (s + l < s1 && src[s + l] === src[t + l]) {
                l++;
            }
            dst.push(MATCH_TYPE | ((l + 4 - 3) << 22) | (s - t - 1));
            s += l;
            nextEmit = s;
            if (s >= sLimit) {
                break outer;
            }
            const x0 = load32(src, s - 1);
            tOff[hash14(x0)] = cur + s - 1;
            tVal[hash14(x0)] = x0;
            const x1 = load32(src, s);
            const ch = hash14(x1);
            cand = tOff[ch];
            candVal = tVal[ch];
            tOff[ch] = cur + s;
            tVal[ch] = x1;
            const offset = s - (cand - cur);
            if (offset > MAX_MATCH_OFFSET || x1 !== candVal) {
                cv = load32(src, s + 1);
                nextHash = hash14(cv);
                s++;
                break;
            }
        }
    }
    if (nextEmit < src.length) {
        emitLiteral(nextEmit, src.length);
    }
    return dst;
}

// raw DEFLATE as Go's flate.NewWriter(w, BestSpeed), one Write, Close
export function goDeflateBestSpeed(data: Uint8Array): Uint8Array {
    if (data.length >= MAX_STORE) {
        throw new Error("go flate port: inputs of 64 KiB or more are not supported");
    }
    const hw = new HuffmanBitWriter();
    const n = data.length;
    if (n > 0) {
        if (n < 128) {
            if (n <= 16) {
                hw.writeStoredHeader(n, false);
                hw.writeBytes(data);
            } else {
                hw.writeBlockHuff(false, data);
            }
        } else {
            const tokens = encodeFast(data);
            if (tokens.length > n - (n >> 4)) {
                hw.writeBlockHuff(false, data);
            } else {
                hw.writeBlockDynamic(tokens, false, data);
            }
        }
    }
    hw.writeStoredHeader(0, true);
    hw.w.flush();
    return Uint8Array.from(hw.w.out);
}

function adler32(d: Uint8Array): number {
    let a = 1;
    let b = 0;
    for (const v of d) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    return ((b << 16) | a) >>> 0;
}

// zlib.NewWriterLevel(w, zlib.BestSpeed), one Write, Close
export function goZlibBestSpeed(data: Uint8Array): Uint8Array {
    const body = goDeflateBestSpeed(data);
    const out = new Uint8Array(body.length + 6);
    out[0] = 0x78;
    out[1] = 0x01;
    out.set(body, 2);
    const ad = adler32(data);
    out[out.length - 4] = ad >>> 24;
    out[out.length - 3] = (ad >>> 16) & 255;
    out[out.length - 2] = (ad >>> 8) & 255;
    out[out.length - 1] = ad & 255;
    return out;
}

const CRC_TABLE = ((): Uint32Array => {
    const t = new Uint32Array(256);
    for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) {
            c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
        }
        t[n] = c >>> 0;
    }
    return t;
})();

function crc32(parts: Uint8Array[]): number {
    let c = 0xFFFFFFFF;
    for (const p of parts) {
        for (const v of p) {
            c = CRC_TABLE[(c ^ v) & 255] ^ (c >>> 8);
        }
    }
    return (c ^ 0xFFFFFFFF) >>> 0;
}

export interface Rgba {
    r: number;
    g: number;
    b: number;
    a: number;
}

// util/png/pngencode.go Encode: data is 4bpp packed (low nibble first) for
// palettes of 3-16 colours (except a 16x1 image: one byte per pixel), 8bpp
// otherwise (1bpp palettes are not ported)
export function goPngEncode(dataIn: Uint8Array, width: number, height: number, palette: Rgba[]): Uint8Array {
    let data = dataIn;
    if (palette.length <= 2) {
        throw new Error("go png port: 1bpp images are not supported");
    } else if (palette.length <= 16 && !(width === 16 && height === 1)) {
        data = new Uint8Array(dataIn.length * 2);
        for (let i = 0; i < dataIn.length; i++) {
            data[2 * i] = dataIn[i] & 15;
            data[2 * i + 1] = dataIn[i] >> 4;
        }
    }
    if (data.length < width * height) {
        throw new Error("go png port: not enough pixels");
    }
    const chunks: Uint8Array[] = [Uint8Array.from([137, 80, 78, 71, 13, 10, 26, 10])];
    const chunk = (kind: string, body: Uint8Array): void => {
        const k = Uint8Array.from(kind, (c) => c.charCodeAt(0));
        const head = new Uint8Array(8);
        new DataView(head.buffer).setUint32(0, body.length);
        head.set(k, 4);
        const tail = new Uint8Array(4);
        new DataView(tail.buffer).setUint32(0, crc32([k, body]));
        chunks.push(head, body, tail);
    };
    const ihdr = new Uint8Array(13);
    const dv = new DataView(ihdr.buffer);
    dv.setUint32(0, width);
    dv.setUint32(4, height);
    ihdr[8] = 8;
    ihdr[9] = 3;
    chunk("IHDR", ihdr);
    chunk("PLTE", Uint8Array.from(palette.flatMap((c) => [c.r, c.g, c.b])));
    chunk("tRNS", Uint8Array.from(palette.map((c) => c.a)));
    const raw = new Uint8Array(height * (width + 1));
    for (let y = 0; y < height; y++) {
        raw.set(data.subarray(y * width, (y + 1) * width), y * (width + 1) + 1);
    }
    chunk("IDAT", goZlibBestSpeed(raw));
    chunk("IEND", new Uint8Array(0));
    const out = new Uint8Array(chunks.reduce((a, c) => a + c.length, 0));
    let o = 0;
    for (const c of chunks) {
        out.set(c, o);
        o += c.length;
    }
    return out;
}

// util.MakePaletteFromR5G5B5A1(data, false)
export function paletteFromR5G5B5A1(d: Uint8Array): Rgba[] {
    const out: Rgba[] = [];
    for (let i = 0; i + 1 < d.length; i += 2) {
        const c = d[i] | (d[i + 1] << 8);
        out.push({ r: Math.floor((c & 31) * 255 / 31), g: Math.floor(((c >> 5) & 31) * 255 / 31),
            b: Math.floor(((c >> 10) & 31) * 255 / 31), a: c & 0x8000 ? 127 : 255 });
    }
    return out;
}

// util.MakeGreyPalette(4)
export function greyPalette4(): Rgba[] {
    return Array.from({ length: 16 }, (_, i) => ({ r: i << 4, g: i << 4, b: i << 4, a: 255 }));
}
