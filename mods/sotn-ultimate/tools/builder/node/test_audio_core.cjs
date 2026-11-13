// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_audio_core.cjs
// audio_core.js ($MOD_WORK/builder/audio, mods/sotn-ultimate/tools/builder/native/build.sh)
// against the proof's reference (011a.at3 decode + encmono), satxa enc
// (SD25.PCM) against the native bin/satxa's output (made here, in a temp
// folder), and a stereo decode.
const fs = require("fs");
const os = require("os");
const path = require("path");
const crypto = require("crypto");
const { execFileSync } = require("child_process");
const md5 = (b) => crypto.createHash("md5").update(b).digest("hex");
const ROOT = path.resolve(__dirname, "../../../../..");
const MOD_WORK = process.env.MOD_WORK || path.join(ROOT, "build/mod-work");
const CHAIN = "highpass=f=50,volume=0.5,aresample=37800,aformat=sample_fmts=s16:channel_layouts=mono";

require(path.join(MOD_WORK, "builder/audio/audio_core.js"))({ print() {}, printErr() {} }).then((m) => {
    m.FS.writeFile("/in.at3", fs.readFileSync(path.join(ROOT, "disks/psp/snd_e/XA/011a.at3")));
    const n = m.ccall("at3_decode", "number", ["string", "string", "string"], ["/in.at3", "/out.wav", CHAIN]);
    m.callMain(["encmono", "/out.wav", "/out.xa", "1", "0"]);
    console.log("011a: %d samples, wav %s (ref 35b93d19...), xa %s (ref 188cdd82...)", n,
                md5(m.FS.readFile("/out.wav")), md5(m.FS.readFile("/out.xa")));
    const pcm = path.join(ROOT, "disks/saturn/SD/SD25.PCM");
    m.FS.writeFile("/sd25.pcm", fs.readFileSync(pcm));
    m.callMain(["enc", "/sd25.pcm", "/sd25.xa", "100", "0", "0.64", "270"]);
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "sotn_audio_"));
    const nat = path.join(tmp, "SD25.nat.xa");
    execFileSync(path.join(ROOT, "bin/satxa"), ["enc", pcm, nat, "100", "0", "0.64", "270"]);
    const a = md5(m.FS.readFile("/sd25.xa")), b = md5(fs.readFileSync(nat));
    fs.rmSync(tmp, { recursive: true });
    console.log("SD25 enc: %s native %s %s", a, b, a === b ? "SAME" : "DIFF");
    const st = "volume=0.44,aresample=37800,aformat=sample_fmts=s16:channel_layouts=stereo";
    m.FS.writeFile("/h.at3", fs.readFileSync(path.join(ROOT, "disks/psp/snd_e/PS_BGM/PS_HEKIRU.at3")));
    const k = m.ccall("at3_decode", "number", ["string", "string", "string"], ["/h.at3", "/h.wav", st]);
    const w = m.FS.readFile("/h.wav");
    console.log("Hekiru stereo: %d frames, channels %d, rate %d, %d bytes", k, w[22],
                w[24] | (w[25] << 8) | (w[26] << 16), w.length);
});
