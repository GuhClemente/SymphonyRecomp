// SPDX-License-Identifier: AGPL-3.0-or-later
// MD5 (RFC 1321), for the builder's checks: the browser's crypto.subtle has
// no MD5 and the published release hashes are MD5.
const S = [7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21];
const K = new Int32Array(64);
for (let i = 0; i < 64; i++) {
    K[i] = Math.floor(Math.abs(Math.sin(i + 1)) * 4294967296) | 0;
}
// the table above uses Math.sin: check it against the RFC's constants once
if (K[0] !== (0xd76aa478 | 0) || K[63] !== (0xeb86d391 | 0)) {
    throw new Error("md5: bad constants");
}

export class Md5 {
    private h = new Int32Array([0x67452301, 0xefcdab89 | 0, 0x98badcfe | 0, 0x10325476]);
    private buf = new Uint8Array(64);
    private used = 0;
    private total = 0;
    private w = new Int32Array(16);

    private block(b: Uint8Array, o: number): void {
        const w = this.w;
        for (let i = 0; i < 16; i++) {
            w[i] = b[o + 4 * i] | (b[o + 4 * i + 1] << 8) | (b[o + 4 * i + 2] << 16) | (b[o + 4 * i + 3] << 24);
        }
        let [a, bb, c, d] = this.h;
        for (let i = 0; i < 64; i++) {
            let f: number;
            let g: number;
            if (i < 16) {
                f = (bb & c) | (~bb & d);
                g = i;
            } else if (i < 32) {
                f = (d & bb) | (~d & c);
                g = (5 * i + 1) % 16;
            } else if (i < 48) {
                f = bb ^ c ^ d;
                g = (3 * i + 5) % 16;
            } else {
                f = c ^ (bb | ~d);
                g = (7 * i) % 16;
            }
            const s = S[(i >> 4) * 4 + (i % 4)];
            const t = d;
            d = c;
            c = bb;
            const x = (a + f + K[i] + w[g]) | 0;
            bb = (bb + ((x << s) | (x >>> (32 - s)))) | 0;
            a = t;
        }
        this.h[0] += a;
        this.h[1] += bb;
        this.h[2] += c;
        this.h[3] += d;
    }

    update(data: Uint8Array): this {
        let i = 0;
        this.total += data.length;
        if (this.used) {
            const n = Math.min(64 - this.used, data.length);
            this.buf.set(data.subarray(0, n), this.used);
            this.used += n;
            i = n;
            if (this.used < 64) {
                return this;
            }
            this.block(this.buf, 0);
            this.used = 0;
        }
        for (; i + 64 <= data.length; i += 64) {
            this.block(data, i);
        }
        this.buf.set(data.subarray(i), 0);
        this.used = data.length - i;
        return this;
    }

    hex(): string {
        const bits = this.total * 8;
        const pad = new Uint8Array(((this.used + 8) >> 6) * 64 + 64 - this.used);
        pad[0] = 0x80;
        const n = pad.length;
        for (let i = 0; i < 4; i++) {
            pad[n - 8 + i] = (bits >>> (8 * i)) & 255;
            pad[n - 4 + i] = (Math.floor(bits / 4294967296) >>> (8 * i)) & 255;
        }
        this.update(pad);
        let s = "";
        for (const v of this.h) {
            for (let i = 0; i < 4; i++) {
                s += ((v >>> (8 * i)) & 255).toString(16).padStart(2, "0");
            }
        }
        return s;
    }
}

export const md5 = (data: Uint8Array): string => new Md5().update(data).hex();
