// SPDX-License-Identifier: AGPL-3.0-or-later
// Signal arithmetic for the sound-bank converters, ported from the mod's
// numpy code (mods/sotn-ultimate/tools/gen_gdn_sounds.py, gen_psp_grunts.py, ...).
// - sums and means follow numpy's pairwise summation exactly, so the ports
//   can be checked byte for byte against the Python they replace;
// - the transcendental functions come from the audio module's C library
//   (native/dsp.c), never from JavaScript's Math: Math.sin / Math.pow /
//   Math.log2 may differ in the last bit between Chrome and Firefox.

export interface MathModule {
    _dsp_sin(x: number): number;
    _dsp_cos(x: number): number;
    _dsp_pow(x: number, y: number): number;
    _dsp_log2(x: number): number;
    _dsp_log10(x: number): number;
    _dsp_exp(x: number): number;
}

export class Dsp {
    readonly m: MathModule;

    constructor(m: MathModule) {
        this.m = m;
    }

    sin(x: number): number {
        return this.m._dsp_sin(x);
    }

    cos(x: number): number {
        return this.m._dsp_cos(x);
    }

    pow(x: number, y: number): number {
        return this.m._dsp_pow(x, y);
    }

    log2(x: number): number {
        return this.m._dsp_log2(x);
    }

    log10(x: number): number {
        return this.m._dsp_log10(x);
    }

    exp(x: number): number {
        return this.m._dsp_exp(x);
    }

    // np.sinc
    sinc(x: number): number {
        if (x === 0) {
            return 1;
        }
        const y = Math.PI * x;
        return this.sin(y) / y;
    }

    // np.hanning(n)
    hanning(n: number): Float64Array {
        const out = new Float64Array(n);
        if (n === 1) {
            out[0] = 1;
            return out;
        }
        for (let i = 0; i < n; i++) {
            const k = 1 - n + 2 * i; // np.arange(1 - n, n, 2)
            out[i] = 0.5 + 0.5 * this.cos(Math.PI * k / (n - 1));
        }
        return out;
    }
}

// numpy's pairwise_sum (umath loops: < 8 sequential, <= 128 eight partial
// sums, above: halves on a multiple of 8)
function pairwise(a: ArrayLike<number>, lo: number, n: number): number {
    if (n < 8) {
        let r = -0.0;
        for (let i = 0; i < n; i++) {
            r += a[lo + i];
        }
        return r;
    }
    if (n <= 128) {
        const r = [a[lo], a[lo + 1], a[lo + 2], a[lo + 3], a[lo + 4], a[lo + 5], a[lo + 6], a[lo + 7]];
        let i = 8;
        for (; i < n - (n % 8); i += 8) {
            for (let j = 0; j < 8; j++) {
                r[j] += a[lo + i + j];
            }
        }
        let res = ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
        for (; i < n; i++) {
            res += a[lo + i];
        }
        return res;
    }
    let n2 = Math.floor(n / 2);
    n2 -= n2 % 8;
    return pairwise(a, lo, n2) + pairwise(a, lo + n2, n - n2);
}

// np.sum of a contiguous float64 array: the reduction starts from the first
// element and adds the pairwise sum of the rest
export function npSum(a: ArrayLike<number>): number {
    if (a.length === 0) {
        return 0;
    }
    return a[0] + pairwise(a, 1, a.length - 1);
}

export function npMean(a: ArrayLike<number>): number {
    return npSum(a) / a.length;
}

// np.median (the mean of the two middle values when the count is even)
export function npMedian(a: ArrayLike<number>): number {
    const s = Float64Array.from(a).sort();
    const n = s.length;
    if (n % 2) {
        return s[(n - 1) / 2];
    }
    return (s[n / 2 - 1] + s[n / 2]) / 2;
}

// np.linspace(start, stop, num) (endpoint included)
export function npLinspace(start: number, stop: number, num: number): Float64Array {
    const out = new Float64Array(num);
    if (num === 0) {
        return out;
    }
    const div = num - 1;
    const delta = stop - start;
    const step = div > 0 ? delta / div : 0;
    for (let i = 0; i < num; i++) {
        out[i] = div > 0 && step !== 0 ? i * step + start : div > 0 ? (i / div) * delta + start : start;
    }
    if (num > 1) {
        out[num - 1] = stop;
    }
    return out;
}

// np.interp(x, xp, fp) for increasing xp, outside: the end values
export function npInterp(x: ArrayLike<number>, xp: ArrayLike<number>, fp: ArrayLike<number>): Float64Array {
    const out = new Float64Array(x.length);
    const n = xp.length;
    let j = 0;
    for (let i = 0; i < x.length; i++) {
        const v = x[i];
        if (v <= xp[0]) {
            out[i] = v === xp[0] ? fp[0] : fp[0];
            continue;
        }
        if (v >= xp[n - 1]) {
            out[i] = fp[n - 1];
            continue;
        }
        if (v < xp[j]) {
            j = 0;
        }
        while (j + 1 < n && xp[j + 1] <= v) {
            j++;
        }
        const slope = (fp[j + 1] - fp[j]) / (xp[j + 1] - xp[j]);
        out[i] = slope * (v - xp[j]) + fp[j];
        if (Number.isNaN(out[i])) {
            out[i] = slope * (v - xp[j + 1]) + fp[j + 1];
            if (Number.isNaN(out[i]) && fp[j] === fp[j + 1]) {
                out[i] = fp[j];
            }
        }
    }
    return out;
}

// np.convolve(x, h, "same"): output aligned to x, length max(len(x), len(h)).
// Each output sums x in increasing order (numpy correlates x with h
// reversed); numpy may use a BLAS dot there, so this is the reference, not
// a bit-exact copy
export function npConvolveSame(x: ArrayLike<number>, h: ArrayLike<number>): Float64Array {
    const n = x.length;
    const m = h.length;
    const full = n + m - 1;
    const start = Math.floor((full - Math.max(n, m)) / 2);
    const out = new Float64Array(Math.max(n, m));
    for (let k = 0; k < out.length; k++) {
        const i = k + start; // index in the full convolution
        let s = 0;
        for (let j = Math.min(i, m - 1); j >= Math.max(0, i - n + 1); j--) {
            s += x[i - j] * h[j];
        }
        out[k] = s;
    }
    return out;
}

// Python's round() / numpy's round(): half to even
export function roundHalfEven(x: number): number {
    const r = Math.round(x);
    return Math.abs(x % 1) === 0.5 && r % 2 !== 0 ? r - 1 : r;
}

export function clamp16(x: number): number {
    return x < -32768 ? -32768 : x > 32767 ? 32767 : x;
}
