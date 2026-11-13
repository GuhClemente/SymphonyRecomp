// SPDX-License-Identifier: AGPL-3.0-or-later
// Read-only ISO 9660 access to the player's disc images: raw 2352-byte
// sector images (PS1 MODE2 Track 1, Saturn MODE1 Track 1) and 2048-byte
// ISOs (the PSP UMD). Files are read through a ByteSource, so a 1 GB image
// is never loaded whole (browser: File.slice, Node: fs reads).

export interface ByteSource {
    size: number;
    read(offset: number, length: number): Promise<Uint8Array>;
}

export interface SectorFormat {
    size: number; // bytes per sector in the image
    data: number; // offset of the 2048 user bytes in a sector
}

export const MODE1_RAW: SectorFormat = { size: 2352, data: 16 };
export const MODE2_RAW: SectorFormat = { size: 2352, data: 24 };
export const ISO_2048: SectorFormat = { size: 2048, data: 0 };

const SYNC = [0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00];

export async function detectFormat(src: ByteSource): Promise<SectorFormat> {
    const head = await src.read(0, 16);
    if (SYNC.every((b, i) => head[i] === b)) {
        return head[15] === 2 ? MODE2_RAW : MODE1_RAW;
    }
    return ISO_2048;
}

export interface FileEntry {
    lba: number;
    size: number;
    dir: boolean;
}

const u32 = (b: Uint8Array, o: number): number =>
    (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;

export class IsoFs {
    readonly src: ByteSource;
    readonly fmt: SectorFormat;
    private root: FileEntry;

    private constructor(src: ByteSource, fmt: SectorFormat, root: FileEntry) {
        this.src = src;
        this.fmt = fmt;
        this.root = root;
    }

    static async open(src: ByteSource): Promise<IsoFs> {
        const fmt = await detectFormat(src);
        const fs = new IsoFs(src, fmt, { lba: 0, size: 0, dir: true });
        const pvd = await fs.readSectors(16, 1);
        const id = String.fromCharCode(...pvd.subarray(1, 6));
        if (pvd[0] !== 1 || id !== "CD001") {
            throw new Error("not an ISO 9660 disc image");
        }
        fs.root = { lba: u32(pvd, 156 + 2), size: u32(pvd, 156 + 10), dir: true };
        return fs;
    }

    // the user data of `count` sectors from `lba`
    async readSectors(lba: number, count: number): Promise<Uint8Array> {
        const { size, data } = this.fmt;
        const raw = await this.src.read(lba * size, count * size);
        if (size === 2048) {
            return raw;
        }
        const out = new Uint8Array(count * 2048);
        for (let k = 0; k < count; k++) {
            out.set(raw.subarray(k * size + data, k * size + data + 2048), k * 2048);
        }
        return out;
    }

    // raw sectors as stored in the image (for XA / form 2 data)
    async readRaw(lba: number, count: number): Promise<Uint8Array> {
        return this.src.read(lba * this.fmt.size, count * this.fmt.size);
    }

    private async list(dir: FileEntry): Promise<Map<string, FileEntry>> {
        const data = await this.readSectors(dir.lba, Math.ceil(dir.size / 2048));
        const out = new Map<string, FileEntry>();
        let i = 0;
        while (i < dir.size) {
            const n = data[i];
            if (n === 0) {
                i = (Math.floor(i / 2048) + 1) * 2048; // records never cross sectors
                continue;
            }
            const nl = data[i + 32];
            const name = String.fromCharCode(...data.subarray(i + 33, i + 33 + nl));
            if (nl !== 1 || (data[i + 33] !== 0 && data[i + 33] !== 1)) {
                out.set(name.split(";")[0].toUpperCase(), {
                    lba: u32(data, i + 2),
                    size: u32(data, i + 10),
                    dir: (data[i + 25] & 2) !== 0,
                });
            }
            i += n;
        }
        return out;
    }

    // path like "BIN/MONSTER.BIN" (case-insensitive, no ";1")
    async lookup(path: string): Promise<FileEntry> {
        let cur = this.root;
        for (const part of path.split("/").filter((p) => p)) {
            const next = (await this.list(cur)).get(part.toUpperCase());
            if (!next) {
                throw new Error("not on the disc: " + path);
            }
            cur = next;
        }
        return cur;
    }

    // a directory's entries ("" = the root): name -> entry
    async listDir(path: string): Promise<Map<string, FileEntry>> {
        return this.list(path ? await this.lookup(path) : this.root);
    }

    async readFile(path: string): Promise<Uint8Array> {
        const e = await this.lookup(path);
        const data = await this.readSectors(e.lba, Math.ceil(e.size / 2048));
        return data.subarray(0, e.size);
    }
}
