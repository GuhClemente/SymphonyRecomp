// SPDX-License-Identifier: AGPL-3.0-or-later
// VCDIFF decoder (RFC 3284) for xdelta3 patches made with -S none: no
// secondary compression, the default code table, xdelta3's per-window
// Adler-32 and application header extensions. The source (the player's PS1
// Track 1) is read one window segment at a time, so it is never held whole.
import type { ByteSource } from "./disc.ts";

const NOOP = 0;
const ADD = 1;
const RUN = 2;
const COPY = 3;
const S_NEAR = 4;
const S_SAME = 3;

interface Inst {
    type1: number;
    size1: number;
    mode1: number;
    type2: number;
    size2: number;
    mode2: number;
}

// RFC 3284 5.6, the default instruction code table
function defaultTable(): Inst[] {
    const t: Inst[] = [];
    const e = (type1: number, size1: number, mode1: number, type2 = NOOP, size2 = 0, mode2 = 0): void => {
        t.push({ type1, size1, mode1, type2, size2, mode2 });
    };
    e(RUN, 0, 0);
    for (let s = 0; s <= 17; s++) {
        e(ADD, s, 0);
    }
    for (let m = 0; m < 9; m++) {
        e(COPY, 0, m);
        for (let s = 4; s <= 18; s++) {
            e(COPY, s, m);
        }
    }
    for (let m = 0; m < 6; m++) {
        for (let a = 1; a <= 4; a++) {
            for (let c = 4; c <= 6; c++) {
                e(ADD, a, 0, COPY, c, m);
            }
        }
    }
    for (let m = 6; m < 9; m++) {
        for (let a = 1; a <= 4; a++) {
            e(ADD, a, 0, COPY, 4, m);
        }
    }
    for (let m = 0; m < 9; m++) {
        e(COPY, 4, m, ADD, 1, 0);
    }
    if (t.length !== 256) {
        throw new Error("vcdiff: code table has " + t.length + " entries");
    }
    return t;
}

const TABLE = defaultTable();

class Reader {
    buf: Uint8Array;
    pos: number;
    end: number;

    constructor(buf: Uint8Array, pos = 0, end = buf.length) {
        this.buf = buf;
        this.pos = pos;
        this.end = end;
    }

    byte(): number {
        if (this.pos >= this.end) {
            throw new Error("vcdiff: truncated");
        }
        return this.buf[this.pos++];
    }

    varint(): number {
        let v = 0;
        for (;;) {
            const b = this.byte();
            v = v * 128 + (b & 0x7f); // past 2^31: no bit operators
            if (!(b & 0x80)) {
                return v;
            }
        }
    }

    bytes(n: number): Uint8Array {
        if (this.pos + n > this.end) {
            throw new Error("vcdiff: truncated");
        }
        const out = this.buf.subarray(this.pos, this.pos + n);
        this.pos += n;
        return out;
    }
}

function adler32(b: Uint8Array): number {
    let a = 1;
    let s = 0;
    for (let i = 0; i < b.length; ) {
        const n = Math.min(b.length, i + 3800); // no overflow before the modulo
        for (; i < n; i++) {
            a += b[i];
            s += a;
        }
        a %= 65521;
        s %= 65521;
    }
    return ((s << 16) | a) >>> 0;
}

export interface VcdiffProgress {
    (done: number, total: number): void;
}

// patch: the whole .xdelta; source: the original; out: the target buffer
// (its size known from the manifest). Returns the bytes written.
export async function vcdiffDecode(patch: Uint8Array, source: ByteSource, out: Uint8Array,
                                   progress?: VcdiffProgress): Promise<number> {
    const r = new Reader(patch);
    if (r.byte() !== 0xd6 || r.byte() !== 0xc3 || r.byte() !== 0xc4 || r.byte() !== 0) {
        throw new Error("not an xdelta3 / VCDIFF patch");
    }
    const hdr = r.byte();
    if (hdr & 0x01) {
        throw new Error("vcdiff: secondary compression is not supported (make the patch with -S none)");
    }
    if (hdr & 0x02) {
        throw new Error("vcdiff: custom code tables are not supported");
    }
    if (hdr & 0x04) {
        r.bytes(r.varint()); // xdelta3's application header (file names)
    }
    let tpos = 0;
    while (r.pos < r.end) {
        const win = r.byte();
        let seg: Uint8Array = new Uint8Array(0);
        if (win & 0x03) {
            const len = r.varint();
            const at = r.varint();
            if (win & 0x01) {
                seg = await source.read(at, len);
                if (seg.length !== len) {
                    throw new Error("vcdiff: the source is too short");
                }
            } else {
                seg = out.slice(at, at + len); // VCD_TARGET: earlier output
            }
        }
        r.varint(); // length of the delta encoding
        const tlen = r.varint();
        if (r.byte() !== 0) {
            throw new Error("vcdiff: compressed sections are not supported");
        }
        const dataLen = r.varint();
        const instLen = r.varint();
        const addrLen = r.varint();
        let check = -1;
        if (win & 0x04) {
            const c = r.bytes(4);
            check = ((c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3]) >>> 0;
        }
        const data = new Reader(patch, r.pos, r.pos + dataLen);
        const inst = new Reader(patch, data.end, data.end + instLen);
        const addr = new Reader(patch, inst.end, inst.end + addrLen);
        r.pos = addr.end;
        if (tpos + tlen > out.length) {
            throw new Error("vcdiff: the target is larger than expected");
        }
        const S = seg.length;
        const near = new Array<number>(S_NEAR).fill(0);
        const same = new Array<number>(S_SAME * 256).fill(0);
        let nextNear = 0;
        let t = 0; // position in this target window
        const doInst = (type: number, size: number, mode: number): void => {
            if (type === NOOP) {
                return;
            }
            if (size === 0) {
                size = inst.varint();
            }
            if (type === ADD) {
                out.set(data.bytes(size), tpos + t);
            } else if (type === RUN) {
                out.fill(data.byte(), tpos + t, tpos + t + size);
            } else {
                const here = S + t;
                let a: number;
                if (mode === 0) {
                    a = addr.varint();
                } else if (mode === 1) {
                    a = here - addr.varint();
                } else if (mode < 2 + S_NEAR) {
                    a = near[mode - 2] + addr.varint();
                } else {
                    a = same[(mode - 2 - S_NEAR) * 256 + addr.byte()];
                }
                near[nextNear] = a;
                nextNear = (nextNear + 1) % S_NEAR;
                same[a % (S_SAME * 256)] = a;
                if (a >= here) {
                    throw new Error("vcdiff: bad copy address");
                }
                let n = size;
                let p = a;
                if (p < S) { // from the source segment
                    const k = Math.min(n, S - p);
                    out.set(seg.subarray(p, p + k), tpos + t);
                    t += k;
                    n -= k;
                    p = S;
                }
                // from this window's own output (may overlap: byte by byte)
                for (let q = tpos + p - S, d = tpos + t, e = d + n; d < e; ) {
                    out[d++] = out[q++];
                }
                t += n;
                return;
            }
            t += size;
        };
        while (inst.pos < inst.end) {
            const c = TABLE[inst.byte()];
            doInst(c.type1, c.size1, c.mode1);
            doInst(c.type2, c.size2, c.mode2);
        }
        if (t !== tlen) {
            throw new Error("vcdiff: window decoded to " + t + " bytes, expected " + tlen);
        }
        if (check >= 0 && adler32(out.subarray(tpos, tpos + tlen)) !== check) {
            throw new Error("vcdiff: checksum mismatch at " + tpos + " (wrong source disc?)");
        }
        tpos += tlen;
        progress?.(tpos, out.length);
        await new Promise((res) => setTimeout(res, 0)); // let the page repaint
    }
    return tpos;
}
