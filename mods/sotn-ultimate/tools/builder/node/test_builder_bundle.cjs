// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_builder_bundle.cjs PATCHDIR PS1.bin SATURN.bin PSP.iso
// dist/builder_core.js run as the builder page runs it (a classic script):
// identify the discs, build, print the result's md5.
const vm = require("vm");
const fs = require("fs");
const path = require("path");
const MOD_WORK = process.env.MOD_WORK || path.join(__dirname, "../../../../../build/mod-work");

// what a browser page has (the bundle uses no other globals)
const ctx = { TextEncoder, TextDecoder, console, setTimeout };
vm.createContext(ctx);
vm.runInContext(fs.readFileSync(path.join(__dirname, "../dist/builder_core.js"), "utf8") +
                ";this.S = SotnBuilder;", ctx);
const S = ctx.S;
const src = (p) => {
    const fd = fs.openSync(p, "r");
    return { size: fs.fstatSync(fd).size,
             read: async (o, n) => { const b = new Uint8Array(n); fs.readSync(fd, b, 0, n, o); return b; } };
};

(async () => {
    const [dir, ...discs] = process.argv.slice(2);
    const m = JSON.parse(fs.readFileSync(path.join(dir, "manifest.json"), "utf8"));
    const found = {};
    for (const p of discs) {
        const d = await S.identify.identify(src(p));
        d.source = src(p);
        found[d.kind] = d;
        console.log(d.ok ? "OK " : "BAD", d.kind, d.serial);
    }
    const patch = new Uint8Array(fs.readFileSync(path.join(dir, m.name + ".builder.xdelta")));
    let last = "";
    const img = await S.build.buildDisc({ ps1: found.ps1.source, ps1Fs: found.ps1.fs, saturn: found.saturn.fs,
                                          psp: found.psp.fs }, patch, m, { audio: () => require(path.join(MOD_WORK, "builder/audio/audio_core.js"))({ print() {}, printErr() {} }),
                                          satconv: () => require(path.join(MOD_WORK, "builder/satconv/saturn_assets.js"))({ print() {}, printErr() {}, preRun: [(mod) => { mod.ENV.SOTN_MOD_UNCAPPED = "1"; }] }) }, (s) => { if (s !== last) console.log(" ", s); last = s; });
    console.log("result md5", S.md5.md5(img), "expected", m.target.md5);
})().catch((e) => { console.error("FAILED:", e.message); process.exit(1); });
