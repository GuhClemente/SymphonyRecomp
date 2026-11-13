// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_voice.ts PSP.iso
// the first dub line of build/us/psp_dub/dub.json, encoded from disks/ and
// from the PSP disc image, in fresh and reused modules
import * as fs from "node:fs";
import * as path from "node:path";
import { IsoFs } from "../src/disc.ts";
import { encodeVoice } from "../src/audio.ts";
import { md5 } from "../src/md5.ts";
import { audioModule } from "./audio_cli.ts";
import { ROOT, fileSource } from "./util.ts";

const dub = JSON.parse(fs.readFileSync(path.join(ROOT, "build/us/psp_dub/dub.json"), "utf8"));
const first = Object.values(dub.replace)[0] as { xa: string; name: string };
const meta = JSON.parse(fs.readFileSync(first.xa.replace(/\.xa$/, ".json"), "utf8"));
console.log("line", meta.name, meta.src, meta.gain);
const fromDisk = new Uint8Array(fs.readFileSync(path.join(ROOT, meta.src)));
const iso = await IsoFs.open(fileSource(process.argv[2]));
const fromIso = await iso.readFile("PSP_GAME/USRDIR/res/ps/" + meta.src.slice("disks/psp/".length));
console.log("bytes: disk %d %s, iso %d %s", fromDisk.length, md5(fromDisk), fromIso.length, md5(fromIso));
const a = await audioModule();
console.log("disk, fresh module:", md5(encodeVoice(a, fromDisk, Number(meta.gain)).xa));
console.log("iso, same module:", md5(encodeVoice(a, fromIso, Number(meta.gain)).xa));
const b = await audioModule();
console.log("iso, second module:", md5(encodeVoice(b, fromIso, Number(meta.gain)).xa));
console.log("build's:", md5(new Uint8Array(fs.readFileSync(first.xa))));
