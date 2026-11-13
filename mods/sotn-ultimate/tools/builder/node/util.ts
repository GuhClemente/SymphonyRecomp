// SPDX-License-Identifier: AGPL-3.0-or-later
// Node helpers shared by the builder's scripts
import * as fs from "node:fs";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import type { ByteSource } from "../src/disc.ts";

export const ROOT = path.resolve(import.meta.dirname, "../../../../..");
// the build's work folder (mods/sotn-ultimate/tools/mod_work.py): renders, caches and the
// compiled modules of mods/sotn-ultimate/tools/builder/native/build.sh
export const MOD_WORK = process.env.MOD_WORK || path.join(ROOT, "build/mod-work");

// g_ModLibEnemy in src/st/lib/mod_bestiary.h (written by gen_saturn_bestiary.py):
// mod data (enemy ids), shipped to the builder in its manifest
export function bestiaryBits(): number[] {
    const hdr = fs.readFileSync(path.join(ROOT, "src/st/lib/mod_bestiary.h"), "latin1");
    let body = hdr.slice(hdr.indexOf("g_ModLibEnemy[]"));
    body = body.slice(0, body.indexOf("};"));
    if (body.includes("#ifdef")) {
        body = body.slice(0, body.indexOf("#ifdef")); // the Ghost Skull toggle
    }
    return [...body.matchAll(/0x([0-9A-Fa-f]+)/g)].map((m) => parseInt(m[1], 16));
}

export function fileSource(p: string): ByteSource {
    const fd = fs.openSync(p, "r");
    return {
        size: fs.fstatSync(fd).size,
        read: async (offset: number, length: number): Promise<Uint8Array> => {
            const buf = new Uint8Array(length);
            fs.readSync(fd, buf, 0, length, offset);
            return buf;
        },
    };
}

// sfx_config.yaml entries by PS1 sound id (hex, upper case): mod data the
// sound-bank converters need, shipped in the manifest
export function sfxConfig(ids: number[]): Record<string, { vabid: number; prog: number; tone: number; note: number; volume: number }> {
    const all = JSON.parse(execFileSync(path.join(ROOT, ".venv/bin/python"), ["-c",
        "import yaml, json; print(json.dumps(yaml.safe_load(open(\x27assets/dra/us/sfx_config.yaml\x27))))"],
        { cwd: ROOT, encoding: "utf8", maxBuffer: 1 << 26 }));
    const out: Record<string, { vabid: number; prog: number; tone: number; note: number; volume: number }> = {};
    for (const id of ids) {
        const e = all[id - 0x600];
        out[id.toString(16).toUpperCase()] = { vabid: e.vabid, prog: e.prog, tone: e.tone, note: e.note, volume: e.volume };
    }
    return out;
}
