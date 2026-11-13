// SPDX-License-Identifier: AGPL-3.0-or-later
// One exact-count Saturn LZSS stream (tools/saturn/character_viewer.py
// decompress_lzss; runtime: func_0600F96C). No end marker: pass one
// directory entry, not a whole padded CHR file.
export function decompressLzss(data: Uint8Array): Uint8Array {
    const dict = new Uint8Array(0x400);
    let cursor = 0x3de;
    let out = new Uint8Array(Math.max(1024, data.length * 9));
    let n = 0;
    let src = 0;
    let flags = 0;
    const put = (v: number): void => {
        if (n === out.length) {
            const bigger = new Uint8Array(out.length * 2);
            bigger.set(out);
            out = bigger;
        }
        out[n++] = v;
        dict[cursor] = v;
        cursor = (cursor + 1) & 0x3ff;
    };
    while (src < data.length) {
        flags >>= 1;
        if (!(flags & 0x100)) {
            flags = data[src++] | 0xff00;
        }
        if (flags & 1) {
            if (src >= data.length) {
                break;
            }
            put(data[src++]);
        } else {
            if (src + 1 >= data.length) {
                break;
            }
            const first = data[src];
            const second = data[src + 1];
            src += 2;
            const index = first | ((second & 0xe0) << 3);
            const length = (second & 0x1f) + 3;
            for (let i = 0; i < length; i++) {
                put(dict[(index + i) & 0x3ff]);
            }
        }
    }
    return out.slice(0, n);
}
