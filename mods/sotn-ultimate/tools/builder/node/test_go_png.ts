// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_go_png.ts [go binary]
// src/go_png.ts's zlib BestSpeed against Go's compress/zlib on varied inputs
// (needs a Go toolchain, default go on PATH: runs the small stdlib-only
// harness GO_MAIN from a temp folder)
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import { goZlibBestSpeed } from "../src/go_png.ts";

// each argument: a file, compressed to <file>.z with zlib.BestSpeed
const GO_MAIN = `package main

import (
	"bytes"
	"compress/zlib"
	"os"
)

func main() {
	for _, p := range os.Args[1:] {
		d, err := os.ReadFile(p)
		if err != nil {
			panic(err)
		}
		var b bytes.Buffer
		w, _ := zlib.NewWriterLevel(&b, zlib.BestSpeed)
		w.Write(d)
		w.Close()
		if err := os.WriteFile(p+".z", b.Bytes(), 0o644); err != nil {
			panic(err)
		}
	}
}
`;
const go = process.argv[2] ?? "go";
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "sotn_gozlib_"));
const dir = path.join(tmp, "cases");
fs.mkdirSync(dir);
fs.writeFileSync(path.join(tmp, "main.go"), GO_MAIN);
let seed = 12345;
const rnd = (): number => {
    seed = (Math.imul(seed, 1103515245) + 12345) >>> 0;
    return seed >>> 16;
};
const cases: Uint8Array[] = [];
for (const n of [0, 1, 5, 16, 17, 40, 127, 128, 129, 300, 1000, 4097, 16512, 30000, 65000]) {
    for (const kind of [0, 1, 2, 3, 4]) {
        const d = new Uint8Array(n);
        for (let i = 0; i < n; i++) {
            d[i] = kind === 0 ? rnd() & 255 : kind === 1 ? rnd() & 15 : kind === 2 ? (i % 129 === 0 ? 0 : (rnd() % 7 === 0 ? rnd() & 15 : 3))
                : kind === 3 ? (Math.floor(i / 37) * 7) & 15 : (rnd() % 50 === 0 ? rnd() & 255 : 0);
        }
        cases.push(d);
    }
}
const names = cases.map((d, i) => {
    const p = `${dir}/c${i}.bin`;
    fs.writeFileSync(p, d);
    return p;
});
execFileSync(go, ["run", path.join(tmp, "main.go"), ...names], { env: { ...process.env, GOFLAGS: "-mod=mod", GO111MODULE: "off" } });
let bad = 0;
cases.forEach((d, i) => {
    const want = new Uint8Array(fs.readFileSync(names[i] + ".z"));
    const mine = goZlibBestSpeed(d);
    const same = want.length === mine.length && want.every((v, k) => v === mine[k]);
    if (!same) {
        bad++;
        let k = 0;
        while (k < Math.min(want.length, mine.length) && want[k] === mine[k]) {
            k++;
        }
        console.log(`case ${i} (${d.length} bytes): DIFFERS at ${k} (ts ${mine.length}, go ${want.length} bytes)`);
    }
});
console.log(`${cases.length - bad}/${cases.length} identical to Go's zlib BestSpeed`);
fs.rmSync(tmp, { recursive: true });
process.exit(bad ? 1 : 0);
