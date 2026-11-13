// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_bundle.cjs PS1.bin SATURN.bin PSP.iso
// dist/builder_core.js run as the page runs it (a classic script, no Node
// modules): identify the 3 discs, build the bestiary pictures, print md5s.
const vm = require("vm");
const fs = require("fs");
const path = require("path");

const ctx = { TextEncoder, TextDecoder, console };
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
    const found = {};
    for (const p of process.argv.slice(2)) {
        const d = await S.identify.identify(src(p));
        console.log(d.ok ? "OK " : "BAD", d.kind, d.serial, "-", d.note);
        found[d.kind] = d;
    }
    const hdr = fs.readFileSync(path.join(__dirname, "../../../../../src/st/lib/mod_bestiary.h"), "latin1");
    let body = hdr.slice(hdr.indexOf("g_ModLibEnemy[]"));
    body = body.slice(0, body.indexOf("};"));
    if (body.includes("#ifdef")) body = body.slice(0, body.indexOf("#ifdef"));
    const r = S.monster2.buildMonster2({
        oyaji: await found.saturn.fs.readFile("OYAJI.CHR"),
        library: await found.saturn.fs.readFile("LIBRARY.PRG"),
        monster: await found.ps1.fs.readFile("BIN/MONSTER.BIN"),
        pspMaria: await found.psp.fs.readFile("PSP_GAME/USRDIR/res/ps/hdbin/mo_139.bin"),
        bits: [...body.matchAll(/0x([0-9A-Fa-f]+)/g)].map((m) => parseInt(m[1], 16)),
    });
    console.log("MONSTER2.BIN", S.md5.md5(r.monster2));
    console.log("MONSTER.BIN ", S.md5.md5(r.monster));
})();
