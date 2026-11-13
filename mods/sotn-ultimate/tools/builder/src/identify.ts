// SPDX-License-Identifier: AGPL-3.0-or-later
// Which disc did the player give us? By the serial each disc carries.
import { IsoFs, type ByteSource } from "./disc.ts";

export type DiscKind = "ps1" | "saturn" | "psp";

export interface DiscInfo {
    kind: DiscKind | null;
    serial: string; // what the disc says, e.g. "SLUS_000.67"
    ok: boolean; // the exact disc the builder needs
    note: string; // for the player
    fs: IsoFs | null;
}

export const NEEDED: Record<DiscKind, { serial: string; name: string }> = {
    ps1: { serial: "SLUS_000.67", name: "Castlevania: Symphony of the Night (USA), Track 1" },
    saturn: { serial: "T-9527G", name: "Akumajou Dracula X: Gekka no Yasoukyoku (Japan, Saturn), Track 1" },
    psp: { serial: "ULUS-10277", name: "Castlevania: The Dracula X Chronicles (USA, PSP)" },
};

// The Redump dumps' sector counts (Saturn Track 1, the UMD), in any sector
// format the reader takes. A patched image keeps the disc's serial and
// version (the Saturn English fan translation does), so the serial alone
// lets it through, to fail mid-build on the files it changed; its sector
// count gives it away first. (The PS1 Track 1 is checked by size when the
// build starts.)
const SECTORS: Partial<Record<DiscKind, { count: number; md5: string }>> = {
    saturn: { count: 204813, md5: "32f037579707e192320db6e8f24ef571" },
    psp: { count: 542656, md5: "9734bc89f4956324c28df897e25f1a71" },
};

function notOriginal(kind: DiscKind, fs: IsoFs): string | null {
    const want = SECTORS[kind]!;
    const sectors = fs.src.size / fs.fmt.size;
    return sectors === want.count ? null
        : "not the original disc image (" + Math.round(sectors) + " sectors, the Redump dump has " +
          want.count + "): a translated or patched copy? This builder needs the unmodified " +
          NEEDED[kind].name + ", MD5 " + want.md5;
}

const text = (b: Uint8Array): string => String.fromCharCode(...b);

export async function identify(src: ByteSource): Promise<DiscInfo> {
    let fs: IsoFs;
    try {
        fs = await IsoFs.open(src);
    } catch (err) {
        // a file the browser cannot read (a phone's cloud file, say) is not a
        // wrong disc: the caller reports it as such
        if (!(err instanceof Error && err.message === "not an ISO 9660 disc image")) {
            throw err;
        }
        return { kind: null, serial: "", ok: false, note: "not a disc image this builder can read", fs: null };
    }
    const sys = text((await fs.readSectors(0, 1)).subarray(0, 0x30));
    if (sys.startsWith("SEGA SEGASATURN")) {
        const serial = sys.slice(0x20, 0x2a).trim();
        const bad = serial === NEEDED.saturn.serial ? notOriginal("saturn", fs) : null;
        const ok = serial === NEEDED.saturn.serial && !bad;
        return { kind: "saturn", serial, ok, fs,
                 note: ok ? NEEDED.saturn.name : bad ?? "a Saturn disc, but not " + NEEDED.saturn.name };
    }
    try {
        const umd = text((await fs.readFile("UMD_DATA.BIN")).subarray(0, 10));
        const bad = umd === NEEDED.psp.serial ? notOriginal("psp", fs) : null;
        const ok = umd === NEEDED.psp.serial && !bad;
        return { kind: "psp", serial: umd, ok, fs,
                 note: ok ? NEEDED.psp.name
                     : bad ?? (umd === "ULES-00841" ? "the European PSP disc: this builder needs the USA one (ULUS-10277)"
                         : "a PSP disc, but not " + NEEDED.psp.name) };
    } catch {
        // not a PSP disc
    }
    try {
        const cnf = text(await fs.readFile("SYSTEM.CNF"));
        const m = /cdrom:\\?([A-Z]{4}_\d{3}\.\d{2})/i.exec(cnf);
        const serial = m ? m[1].toUpperCase() : "";
        const ok = serial === NEEDED.ps1.serial;
        return { kind: "ps1", serial, ok, fs,
                 note: ok ? NEEDED.ps1.name : "a PlayStation disc, but not " + NEEDED.ps1.name };
    } catch {
        return { kind: null, serial: "", ok: false, note: "not one of the three discs", fs };
    }
}
