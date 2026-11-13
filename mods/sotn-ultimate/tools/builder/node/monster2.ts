// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/monster2.ts
//     the build step (replaces mods/sotn-ultimate/tools/build_monster2.py): reads disks/,
//     writes build/us/MONSTER2.BIN + MONSTER.BIN
// node mods/sotn-ultimate/tools/builder/node/monster2.ts --discs PS1.bin SATURN.bin PSP.iso OUTDIR
//     the same, reading the player's disc images (what the builder page does)
import * as fs from "node:fs";
import * as path from "node:path";
import { IsoFs } from "../src/disc.ts";
import { buildMonster2 } from "../src/monster2.ts";
import { ROOT, bestiaryBits, fileSource } from "./util.ts";

const disk = (p: string): Uint8Array => fs.readFileSync(path.join(ROOT, "disks", p));

async function main(): Promise<void> {
    const args = process.argv.slice(2);
    const bits = bestiaryBits();
    let out: string;
    let inputs;
    if (args[0] === "--discs") {
        const [ps1, sat, psp] = await Promise.all(args.slice(1, 4).map((p) => IsoFs.open(fileSource(p))));
        out = args[4];
        inputs = {
            oyaji: await sat.readFile("OYAJI.CHR"),
            library: await sat.readFile("LIBRARY.PRG"),
            monster: await ps1.readFile("BIN/MONSTER.BIN"),
            pspMaria: await psp.readFile("PSP_GAME/USRDIR/res/ps/hdbin/mo_139.bin"),
            bits,
        };
    } else {
        out = path.join(ROOT, "build/us");
        inputs = {
            oyaji: disk("saturn/OYAJI.CHR"),
            library: disk("saturn/LIBRARY.PRG"),
            monster: disk("us/BIN/MONSTER.BIN"),
            pspMaria: disk("psp/hdbin/mo_139.bin"),
            bits,
        };
    }
    const r = buildMonster2(inputs);
    fs.mkdirSync(out, { recursive: true });
    fs.writeFileSync(path.join(out, "MONSTER2.BIN"), r.monster2);
    fs.writeFileSync(path.join(out, "MONSTER.BIN"), r.monster);
    console.log("bestiary pictures: MONSTER2.BIN %d records, MONSTER.BIN 3 replaced",
                r.monster2.length / 0x5800);
}

await main();
