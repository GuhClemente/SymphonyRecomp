// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn equipment/relic icons and icon palettes -> DRA: a byte-exact port of
// mods/sotn-ultimate/tools/port_saturn_icons.py. The Python rewrites the extracted assets
// assets/dra/g_GfxEquipIcon.png (rawgfx 16x5120 4bpp, 128 bytes per icon,
// left pixel in the low nibble) and assets/dra/g_PalEquipIcon.bin (16
// little-endian u16 per palette), which are linked into DRA.BIN unchanged in
// size: g_GfxEquipIcon at 0x800C5324 (DRA.BIN offset 0x25324), g_PalEquipIcon
// at 0x800D88D4 (offset 0x388D4), the retail offsets. Only the listed slots
// change. Reads the Saturn's ALUCARD.CHR only.
export type SaturnRead = (path: string) => Promise<Uint8Array>;

export const DRA_GFX_EQUIP_ICON = 0x25324; // DRA.BIN file offsets
export const DRA_PAL_EQUIP_ICON = 0x388d4;
export const ICON_BYTES = 128;
export const PAL_BYTES = 32;

// icon slots (PSX = Saturn), in the Python's dict order
export const ICONS = [275, 231, 236, 2, 3, 156, 210, 233, 212, 214, 138];
// PSX palette slot -> Saturn palette slot
export const PALETTES: [number, number][] = [
    [281, 281], [231, 219], [236, 236], [2, 62], [3, 54], [141, 141],
    [138, 138], [211, 211], [140, 140], [136, 136], [139, 74],
];

export interface SaturnIcons {
    icons: { slot: number; bytes: Uint8Array }[]; // at DRA_GFX_EQUIP_ICON + slot * 128
    palettes: { slot: number; bytes: Uint8Array }[]; // at DRA_PAL_EQUIP_ICON + slot * 32
}

export async function buildSaturnIcons(saturn: SaturnRead): Promise<SaturnIcons> {
    const chr = await saturn("ALUCARD.CHR");
    if (chr.length < 0x2a000 + 282 * 32) {
        throw new Error("saturn icons: ALUCARD.CHR too short");
    }
    const icons = ICONS.map((slot) => {
        const src = chr.subarray(0x20000 + slot * 128, 0x20000 + (slot + 1) * 128);
        const bytes = new Uint8Array(ICON_BYTES);
        for (let i = 0; i < 128; i++) { // Saturn high nibble first -> PSX low nibble first
            bytes[i] = (src[i] >> 4) | ((src[i] & 15) << 4);
        }
        return { slot, bytes };
    });
    const palettes = PALETTES.map(([slot, sat]) => {
        const bytes = new Uint8Array(PAL_BYTES);
        for (let i = 1; i < 16; i++) { // colour 0 stays 0x0000
            const o = 0x2a000 + sat * 32 + i * 2;
            bytes[i * 2] = chr[o + 1];
            bytes[i * 2 + 1] = chr[o];
        }
        return { slot, bytes };
    });
    return { icons, palettes };
}

// writes the icons and palettes into a DRA.BIN image in place
export function applySaturnIcons(dra: Uint8Array, r: SaturnIcons): void {
    for (const { slot, bytes } of r.icons) {
        dra.set(bytes, DRA_GFX_EQUIP_ICON + slot * ICON_BYTES);
    }
    for (const { slot, bytes } of r.palettes) {
        dra.set(bytes, DRA_PAL_EQUIP_ICON + slot * PAL_BYTES);
    }
}
