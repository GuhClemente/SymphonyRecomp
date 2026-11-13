// SPDX-License-Identifier: AGPL-3.0-or-later
// Saturn port mod: the Librarian's bestiary pictures from the Saturn
// (TypeScript port of mods/sotn-ultimate/tools/build_monster2.py, the normal build: no
// Ghost Skull record).
//
// The Saturn keeps its 164 pictures in OYAJI.CHR: a table at file offset
// 0x40000 of 164 {be32 offset from the table, be32 size}, each entry LZSS ->
// 0x2C00 bytes = 256-colour palette (BE RGB555) + 96x112 8bpp pixels, indexed
// by the Saturn list position (LIBRARY.PRG 0x1825C, 164 be16 enemy ids).
//
// The PS1 picture (BIN/MONSTER.BIN, a 0x5800-byte record per PS1 list
// position) is the same image as 96x112 LE 16-bit pixels with STP (0x8000)
// set, then 1024 bytes of retail "DRACULAX" filler. The conversion is lossless.
//
// MONSTER2.BIN: the 18 Saturn-only pictures in bestiary-bit order 146..
// (g_ModLibEnemy), then record 18: the PSP port's own Maria (hdbin/mo_139.bin,
// 96x112 LE 16-bit without STP / filler). MONSTER.BIN: the retail file with
// the 3 pictures the Saturn touched up replaced in place.
import { decompressLzss } from "./saturn_lzss.ts";

const REC = 0x5800;
const PIX = 96 * 112;
const RETAIL = 146;
const TOUCHED_UP = [0x043, 0x071, 0x182];
const TABLE = 0x40000;

export interface Monster2Inputs {
    oyaji: Uint8Array; // Saturn OYAJI.CHR
    library: Uint8Array; // Saturn LIBRARY.PRG
    monster: Uint8Array; // PS1 BIN/MONSTER.BIN
    pspMaria: Uint8Array; // PSP PSP_GAME/USRDIR/res/ps/hdbin/mo_139.bin
    bits: number[]; // g_ModLibEnemy: enemy id per bestiary bit (164)
}

export interface Monster2Outputs {
    monster2: Uint8Array; // BIN/MONSTER2.BIN
    monster: Uint8Array; // BIN/MONSTER.BIN
}

function check(ok: boolean, what: string): void {
    if (!ok) {
        throw new Error("monster2: " + what);
    }
}

export function buildMonster2(inp: Monster2Inputs): Monster2Outputs {
    const { oyaji, library, monster, pspMaria, bits } = inp;
    check(monster.length === 147 * REC, "MONSTER.BIN size " + monster.length);
    check(bits.length === 164, "bestiary bits " + bits.length);
    const lib = new DataView(library.buffer, library.byteOffset, library.byteLength);
    const oy = new DataView(oyaji.buffer, oyaji.byteOffset, oyaji.byteLength);
    const satIds: number[] = [];
    for (let i = 0; i < 164; i++) {
        satIds.push(lib.getUint16(0x1825c + 2 * i, false));
    }
    const filler = monster.subarray(PIX * 2, REC);

    const record = (eid: number): Uint8Array => {
        const pos = satIds.indexOf(eid);
        check(pos >= 0, "enemy 0x" + eid.toString(16) + " not in the Saturn list");
        const o = oy.getUint32(TABLE + 8 * pos, false);
        const s = oy.getUint32(TABLE + 8 * pos + 4, false);
        const raw = decompressLzss(oyaji.subarray(TABLE + o, TABLE + o + s));
        check(raw.length === 0x2c00, "picture 0x" + eid.toString(16) + " size " + raw.length);
        const pal = new DataView(raw.buffer, raw.byteOffset, 0x200);
        const out = new Uint8Array(REC);
        const px = new DataView(out.buffer);
        for (let i = 0; i < PIX; i++) {
            const c = (pal.getUint16(2 * raw[0x200 + i], false) & 0x7fff) | 0x8000;
            px.setUint16(2 * i, c, true);
        }
        out.set(filler, PIX * 2);
        return out;
    };

    const monster2 = new Uint8Array(19 * REC);
    for (let k = 0; k < 18; k++) {
        monster2.set(record(bits[RETAIL + k]), k * REC);
    }
    // the PSP's Maria: same size, set STP on every pixel (black = 0x8000)
    check(pspMaria.length === PIX * 2, "mo_139.bin size " + pspMaria.length);
    const maria = new DataView(monster2.buffer, 18 * REC, REC);
    const psp = new DataView(pspMaria.buffer, pspMaria.byteOffset, pspMaria.byteLength);
    for (let i = 0; i < PIX; i++) {
        maria.setUint16(2 * i, psp.getUint16(2 * i, true) | 0x8000, true);
    }
    monster2.set(filler, 18 * REC + PIX * 2);

    const mon = monster.slice();
    for (const eid of TOUCHED_UP) {
        const n = bits.indexOf(eid);
        check(n >= 0 && n < RETAIL, "touched-up enemy 0x" + eid.toString(16));
        const fresh = record(eid);
        let diff = 0;
        for (let i = 0; i < PIX; i++) {
            const a = n * REC + 2 * i;
            if (mon[a] !== fresh[2 * i] || mon[a + 1] !== fresh[2 * i + 1]) {
                diff++;
            }
        }
        check(diff > 0 && diff < 1000, "touch-up 0x" + eid.toString(16) + " changes " + diff + " pixels");
        mon.set(fresh, n * REC);
    }
    return { monster2, monster: mon };
}
