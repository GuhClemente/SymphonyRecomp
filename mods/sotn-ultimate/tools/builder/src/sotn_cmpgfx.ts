// SPDX-License-Identifier: AGPL-3.0-or-later
// Byte-exact port of the decomp's `cmpgfx` asset build (tools/sotn-assets:
// assets/cmpgfx/handler.go Build -> util/png Decode -> sotn/deflate.go
// Deflate). The bytes returned here are exactly the ones the Go tool writes
// as hex into src/st/<stage>/gen/<name>.h (after a "// clang-format off"
// line), i.e. what ends up in the stage overlay.

// sotn/deflate.go buildDictionary: the 8 most used "short" opcodes
// (0x1v = one literal nibble v, 0x2v = two nibbles v, 0x6c = c+3 zeros),
// count descending, then dictionary byte ascending; unused slots stay 0.
function buildDictionary(nibbles: Uint8Array): Uint8Array {
    const w1Counts = new Array<number>(16).fill(0);
    const w2Counts = new Array<number>(16).fill(0);
    const zCounts = new Array<number>(16).fill(0);
    const n = nibbles.length;

    let i = 0;
    while (i < n) {
        let runLen = 1;
        while (i + runLen < n && nibbles[i + runLen] === nibbles[i]) {
            runLen++;
        }
        const val = nibbles[i];
        let remaining = runLen;
        if (val === 0) {
            if (remaining >= 19) {
                remaining %= 274;
                if (remaining >= 19) {
                    remaining = 0;
                }
            }
            while (remaining >= 3) {
                const consume = Math.min(remaining, 18);
                zCounts[consume - 3]++;
                remaining -= consume;
            }
        } else {
            while (remaining >= 3) {
                remaining -= Math.min(remaining, 18);
            }
        }
        if (remaining === 2) {
            w2Counts[val]++;
            remaining = 0;
        }
        if (remaining === 1) {
            i += runLen - 1;
        } else {
            i += runLen;
            continue;
        }

        // literal stretch up to the next run of 2+; grouped as 3,3,...,(2|1)
        const litStart = i;
        while (i < n) {
            let rl = 1;
            while (i + rl < n && nibbles[i + rl] === nibbles[i]) {
                rl++;
            }
            if (rl >= 2) {
                break;
            }
            i++;
        }
        const litLen = i - litStart;
        if (litLen % 3 === 1) {
            w1Counts[nibbles[litStart + litLen - 1]]++;
        }
    }

    const cands: { dictByte: number; count: number }[] = [];
    for (let v = 0; v < 16; v++) {
        if (w1Counts[v] > 0) {
            cands.push({ dictByte: 0x10 | v, count: w1Counts[v] });
        }
    }
    for (let v = 0; v < 16; v++) {
        if (w2Counts[v] > 0) {
            cands.push({ dictByte: 0x20 | v, count: w2Counts[v] });
        }
    }
    for (let c = 0; c < 16; c++) {
        if (zCounts[c] > 0) {
            cands.push({ dictByte: 0x60 | c, count: zCounts[c] });
        }
    }
    // same swap sort as Go (keys are unique, so any correct sort agrees)
    for (let a = 0; a < cands.length; a++) {
        for (let b = a + 1; b < cands.length; b++) {
            if (cands[b].count > cands[a].count ||
                (cands[b].count === cands[a].count && cands[b].dictByte < cands[a].dictByte)) {
                const t = cands[a];
                cands[a] = cands[b];
                cands[b] = t;
            }
        }
    }
    const dict = new Uint8Array(8);
    for (let k = 0; k < 8 && k < cands.length; k++) {
        dict[k] = cands[k].dictByte;
    }
    return dict;
}

// sotn/deflate.go Deflate: SOTN nibble RLE (the inverse of the game's
// DecompressData / sotn.Inflate). Output: 8 dictionary bytes, the opcode
// nibble stream (high nibble first) ending with 0xF, zero padded to a
// multiple of 4 bytes.
export function sotnDeflate(src: Uint8Array): Uint8Array {
    // short inputs ending in a long zero run get extra zero nibbles so that
    // run is a multiple of 274 (one long-zero opcode per 274)
    let extraPadding = 0;
    if (src.length < 0x2000) {
        let run = 0;
        for (let k = src.length - 1; k >= 0 && src[k] === 0; k--) {
            run += 2;
        }
        if (src.length > run / 2 && (src[src.length - run / 2 - 1] & 0xf0) === 0) {
            run++;
        }
        if (run > 20) {
            extraPadding = (274 - run % 274) % 274;
        }
    }
    const nibbles = new Uint8Array(src.length * 2 + extraPadding);
    for (let k = 0; k < src.length; k++) {
        nibbles[k * 2] = src[k] & 0xf;
        nibbles[k * 2 + 1] = (src[k] >> 4) & 0xf;
    }
    const n = nibbles.length;
    const dict = buildDictionary(nibbles);

    // worst case is ~1.34 opcode nibbles per input nibble; grow if needed
    let out = new Uint8Array(Math.max(64, 8 + n + 8));
    out.set(dict);
    let len = 8;
    let half = false;
    const put = (ch: number): void => {
        if (!half) {
            if (len === out.length) {
                const bigger = new Uint8Array(out.length * 2);
                bigger.set(out);
                out = bigger;
            }
            out[len++] = (ch << 4) & 0xff;
        } else {
            out[len - 1] |= ch & 0xf;
        }
        half = !half;
    };

    // dictionary entry k is opcode k+7
    const w1Dict = new Array<number>(16).fill(0);
    const w2Dict = new Array<number>(16).fill(0);
    const zDict = new Array<number>(19).fill(0);
    for (let k = 0; k < 8; k++) {
        const kind = dict[k] & 0xf0;
        const val = dict[k] & 0x0f;
        if (kind === 0x10) {
            w1Dict[val] = k + 7;
        } else if (kind === 0x20) {
            w2Dict[val] = k + 7;
        } else if (kind === 0x60) {
            zDict[val + 3] = k + 7;
        }
    }

    let i = 0;
    while (i < n) {
        let runLen = 1;
        while (i + runLen < n && nibbles[i + runLen] === nibbles[i]) {
            runLen++;
        }
        const val = nibbles[i];
        if (val === 0) {
            while (runLen >= 2) {
                if (runLen >= 19) {
                    // op 0: 19..274 zeros
                    const consume = Math.min(runLen, 274);
                    const amount = consume - 0x13;
                    put(0);
                    put(amount >> 4);
                    put(amount & 0xf);
                    i += consume;
                    runLen -= consume;
                } else if (runLen >= 3) {
                    // op 6: 3..18 zeros (or its dictionary opcode)
                    const consume = Math.min(runLen, 18);
                    if (zDict[consume] > 0) {
                        put(zDict[consume]);
                    } else {
                        put(6);
                        put(consume - 3);
                    }
                    i += consume;
                    runLen -= consume;
                } else {
                    if (w2Dict[0] > 0) {
                        put(w2Dict[0]);
                    } else {
                        put(2);
                        put(0);
                    }
                    i += 2;
                    runLen -= 2;
                }
            }
        } else {
            while (runLen >= 2) {
                if (runLen >= 3) {
                    // op 5: 3..18 copies of v
                    const consume = Math.min(runLen, 18);
                    put(5);
                    put(val);
                    put(consume - 3);
                    i += consume;
                    runLen -= consume;
                } else {
                    if (w2Dict[val] > 0) {
                        put(w2Dict[val]);
                    } else {
                        put(2);
                        put(val);
                    }
                    i += 2;
                    runLen -= 2;
                }
            }
        }
        if (runLen === 0) {
            continue;
        }

        // literal stretch up to the next run of 2+: op 4 (3 nibbles),
        // then op 3 (2) or op 1 / dictionary (1) for the tail
        const litStart = i;
        while (i < n) {
            let rl = 1;
            while (i + rl < n && nibbles[i + rl] === nibbles[i]) {
                rl++;
            }
            if (rl >= 2) {
                break;
            }
            i++;
        }
        let j = litStart;
        while (i - j >= 3) {
            put(4);
            put(nibbles[j]);
            put(nibbles[j + 1]);
            put(nibbles[j + 2]);
            j += 3;
        }
        if (i - j === 2) {
            put(3);
            put(nibbles[j]);
            put(nibbles[j + 1]);
        } else if (i - j === 1) {
            if (w1Dict[nibbles[j]] > 0) {
                put(w1Dict[nibbles[j]]);
            } else {
                put(1);
                put(nibbles[j]);
            }
        }
    }
    put(15);
    const padded = (len + 3) & ~3;
    const result = new Uint8Array(padded);
    result.set(out.subarray(0, len));
    return result;
}

// Packs row-major palette indices the way util/png Decode does for an
// 8-bit indexed PNG: bpp 1 (PNG palette <= 2 colours) = 8 pixels per byte,
// first pixel in bit 7, index & 1; bpp 4 (3..16 colours) = 2 pixels per
// byte, first pixel in the LOW nibble, index & 0xF; bpp 8 = one byte each.
// No row padding (rows are simply concatenated).
export function packCmpGfxPixels(pixels: Uint8Array, width: number, height: number, bpp: number): Uint8Array {
    const count = width * height;
    if (pixels.length < count) {
        throw new Error(`cmpgfx: ${pixels.length} pixels < ${width}x${height}`);
    }
    if (bpp === 8) {
        return pixels.slice(0, count);
    }
    if (bpp === 4) {
        if (count % 2) {
            throw new Error("cmpgfx: 4bpp needs an even pixel count");
        }
        const out = new Uint8Array(count / 2);
        for (let k = 0; k < count; k += 2) {
            out[k >> 1] = (pixels[k] & 0xf) | ((pixels[k + 1] & 0xf) << 4);
        }
        return out;
    }
    if (bpp === 1) {
        if (count % 8) {
            throw new Error("cmpgfx: 1bpp needs a pixel count multiple of 8");
        }
        const out = new Uint8Array(count / 8);
        for (let k = 0; k < count; k++) {
            out[k >> 3] |= (pixels[k] & 1) << (7 - (k & 7));
        }
        return out;
    }
    throw new Error(`cmpgfx: unsupported bpp ${bpp}`);
}

// The full cmpgfx asset: the bytes the Go build emits into
// src/st/<stage>/gen/<name>.h for `[offset, cmpgfx, name, w, h, bpp]`.
// Note the Go tool picks the packing from the PNG's palette size, not from
// the yaml bpp; pass the bpp matching the palette (all mod assets: 16
// colours -> 4).
export function buildCmpGfx(pixels: Uint8Array, width: number, height: number, bpp: number): Uint8Array {
    return sotnDeflate(packCmpGfxPixels(pixels, width, height, bpp));
}
