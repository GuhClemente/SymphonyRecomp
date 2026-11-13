// SPDX-License-Identifier: AGPL-3.0-or-later
// CD-ROM sector error codes (EDC, ECC P/Q) for the raw 2352-byte Mode 2
// sectors of the PS1 image, and writing a file's bytes into its sectors.
// The builder rewrites sectors the hollow disc left blank, so every rewritten
// sector needs its codes recomputed (the console checks them).

const EDC = new Uint32Array(256);
const ECC_F = new Uint8Array(256);
const ECC_B = new Uint8Array(256);
for (let i = 0; i < 256; i++) {
    let e = i;
    for (let k = 0; k < 8; k++) {
        e = (e >>> 1) ^ (e & 1 ? 0xd8018001 : 0);
    }
    EDC[i] = e >>> 0;
    const f = (i << 1) ^ (i & 0x80 ? 0x11d : 0);
    ECC_F[i] = f;
    ECC_B[i ^ f] = i;
}

function edc(b: Uint8Array, from: number, len: number): number {
    let e = 0;
    for (let i = from; i < from + len; i++) {
        e = (e >>> 8) ^ EDC[(e ^ b[i]) & 0xff];
    }
    return e >>> 0;
}

function eccBlock(s: Uint8Array, base: number, majorCount: number, minorCount: number,
                  majorMult: number, minorInc: number, dest: number): void {
    const size = majorCount * minorCount;
    for (let major = 0; major < majorCount; major++) {
        let index = (major >> 1) * majorMult + (major & 1);
        let a = 0;
        let b = 0;
        for (let minor = 0; minor < minorCount; minor++) {
            const v = s[base + index];
            index += minorInc;
            if (index >= size) {
                index -= size;
            }
            a ^= v;
            b ^= v;
            a = ECC_F[a];
        }
        a = ECC_B[ECC_F[a] ^ b];
        s[dest + major] = a;
        s[dest + major + majorCount] = a ^ b;
    }
}

// sector: one 2352-byte Mode 2 sector (a view); its subheader decides the form.
// Form 2 EDC is optional: the disc's XA audio sectors leave it 0 (240,898 of
// v1.0's 240,902 Form 2 sectors), so it is written only when asked
export function fixSectorCodes(s: Uint8Array, form2Edc = false): void {
    const form2 = (s[18] & 0x20) !== 0;
    if (form2) {
        if (!form2Edc) {
            return;
        }
        const e = edc(s, 16, 2332);
        new DataView(s.buffer, s.byteOffset).setUint32(2348, e, true);
        return;
    }
    const e = edc(s, 16, 2056);
    new DataView(s.buffer, s.byteOffset).setUint32(2072, e, true);
    // Mode 2: the ECC is computed with the 4 header bytes taken as zero
    const head = s.slice(12, 16);
    s.fill(0, 12, 16);
    eccBlock(s, 12, 86, 24, 2, 86, 2076); // P
    eccBlock(s, 12, 52, 43, 86, 88, 2248); // Q
    s.set(head, 12);
}

// write `data` into the Form 1 sectors from `lba` (2048 bytes each, the last
// one zero-padded), keeping each sector's header and subheader
export function writeFile(image: Uint8Array, lba: number, data: Uint8Array): void {
    const n = Math.ceil(data.length / 2048);
    for (let k = 0; k < n; k++) {
        const s = image.subarray((lba + k) * 2352, (lba + k + 1) * 2352);
        if (s[18] & 0x20) {
            throw new Error("writeFile: sector " + (lba + k) + " is Form 2");
        }
        s.fill(0, 24, 24 + 2048);
        s.set(data.subarray(k * 2048, Math.min(data.length, (k + 1) * 2048)), 24);
        fixSectorCodes(s);
    }
}
