// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_cmpgfx.ts [--no-go]
// 1. every cmpgfx asset of build/us/{gdn,prs,rgdn,rprs}_assets.us.yaml:
//    PNG -> buildCmpGfx must equal the bytes (and exact text) of the
//    generated src/st/<stage>/gen/<name>.h
// 2. (needs go 1.24 + the module cache, offline) a throwaway Go harness in
//    a temp folder runs the real sotn.Deflate / png.Decode on the same
//    PNGs and on a few hundred synthetic inputs, compared with sotnDeflate
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import * as zlib from "node:zlib";
import { execFileSync } from "node:child_process";
import { buildCmpGfx, sotnDeflate } from "../src/sotn_cmpgfx.ts";

const repo = path.resolve(import.meta.dirname, "../../../../..");
const useGo = !process.argv.includes("--no-go");

// minimal PNG reader for non-interlaced greyscale / indexed images;
// returns one palette index (or grey level) per pixel plus the PLTE size
function readPng(file: string): { width: number; height: number; pixels: Uint8Array; paletteColors: number } {
    const d = fs.readFileSync(file);
    let o = 8;
    let width = 0, height = 0, depth = 0, colorType = 0, interlace = 0, paletteColors = 0;
    const idat: Buffer[] = [];
    while (o < d.length) {
        const len = d.readUInt32BE(o);
        const type = d.toString("latin1", o + 4, o + 8);
        const c = d.subarray(o + 8, o + 8 + len);
        if (type === "IHDR") {
            width = c.readUInt32BE(0);
            height = c.readUInt32BE(4);
            depth = c[8];
            colorType = c[9];
            interlace = c[12];
        } else if (type === "PLTE") {
            paletteColors = len / 3;
        } else if (type === "IDAT") {
            idat.push(c);
        }
        o += 12 + len;
    }
    if (interlace || (colorType !== 0 && colorType !== 3)) {
        throw new Error(`${file}: unsupported PNG (type ${colorType}, interlace ${interlace})`);
    }
    const raw = zlib.inflateSync(Buffer.concat(idat));
    const stride = Math.ceil(width * depth / 8);
    const bpp = Math.max(1, depth >> 3);
    const prev = new Uint8Array(stride);
    const row = new Uint8Array(stride);
    const pixels = new Uint8Array(width * height);
    for (let y = 0; y < height; y++) {
        const f = raw[y * (stride + 1)];
        row.set(raw.subarray(y * (stride + 1) + 1, (y + 1) * (stride + 1)));
        for (let x = 0; x < stride; x++) {
            const a = x >= bpp ? row[x - bpp] : 0;
            const b = prev[x];
            const c = x >= bpp ? prev[x - bpp] : 0;
            let p = 0;
            if (f === 1) {
                p = a;
            } else if (f === 2) {
                p = b;
            } else if (f === 3) {
                p = (a + b) >> 1;
            } else if (f === 4) {
                const q = a + b - c;
                const pa = Math.abs(q - a), pb = Math.abs(q - b), pc = Math.abs(q - c);
                p = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
            }
            row[x] = (row[x] + p) & 0xff;
        }
        prev.set(row);
        const per = 8 / depth;
        for (let x = 0; x < width; x++) {
            const byte = row[Math.floor(x / per)];
            const shift = 8 - depth * (x % per + 1);
            pixels[y * width + x] = (byte >> shift) & ((1 << depth) - 1);
        }
    }
    return { width, height, pixels, paletteColors };
}

// util.WriteBytesAsHex after the handler's "// clang-format off" line
function headerText(bytes: Uint8Array): string {
    const hex = "0123456789ABCDEF";
    let s = "// clang-format off\n";
    for (let k = 0; k < bytes.length; k++) {
        s += "0x" + hex[bytes[k] >> 4] + hex[bytes[k] & 15] + ",";
        if ((k & 15) === 15) {
            s += "\n";
        }
    }
    return s + "\n";
}

function parseHeader(text: string): Uint8Array {
    return Uint8Array.from((text.match(/0x[0-9A-Fa-f]{2}/g) ?? []).map((h) => parseInt(h, 16)));
}

function same(a: Uint8Array, b: Uint8Array): boolean {
    return a.length === b.length && a.every((v, k) => v === b[k]);
}

// collect the cmpgfx entries
type Asset = { stage: string; name: string; png: string; header: string; width: number; height: number; bpp: number };
const assets: Asset[] = [];
for (const stage of ["gdn", "prs", "rgdn", "rprs"]) {
    const yaml = path.join(repo, `build/us/${stage}_assets.us.yaml`);
    if (!fs.existsSync(yaml)) {
        continue;
    }
    const text = fs.readFileSync(yaml, "utf8");
    const assetDir = /asset_path:\s*(\S+)/.exec(text)![1];
    const srcDir = /src_path:\s*(\S+)/.exec(text)![1];
    for (const m of text.matchAll(/\[\s*0x[0-9A-Fa-f]+\s*,\s*cmpgfx\s*,\s*(\w+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)/g)) {
        assets.push({
            stage, name: m[1], width: +m[2], height: +m[3], bpp: +m[4],
            png: path.join(repo, assetDir, m[1] + ".png"),
            header: path.join(repo, srcDir, "gen", m[1] + ".h"),
        });
    }
}

// 1. against the generated headers
const tsOut: Uint8Array[] = [];
let headerOk = 0, textOk = 0;
for (const a of assets) {
    const img = readPng(a.png);
    // util/png Decode packs by palette size, not by the yaml bpp
    const bpp = img.paletteColors <= 2 ? 1 : img.paletteColors <= 16 ? 4 : 8;
    if (bpp !== a.bpp || img.width !== a.width || img.height !== a.height) {
        console.log(`note ${a.stage}/${a.name}: png ${img.width}x${img.height} ${img.paletteColors} colours vs yaml ${a.width}x${a.height} ${a.bpp}bpp`);
    }
    const mine = buildCmpGfx(img.pixels, img.width, img.height, bpp);
    tsOut.push(mine);
    const htext = fs.readFileSync(a.header, "utf8");
    if (same(mine, parseHeader(htext))) {
        headerOk++;
    } else {
        console.log(`MISMATCH vs header ${a.stage}/${a.name}`);
    }
    if (htext === headerText(mine)) {
        textOk++;
    }
}
console.log(`headers: ${headerOk} of ${assets.length} identical bytes, ${textOk} of ${assets.length} identical text`);

// 2. against the Go code itself
if (useGo) {
    // seeded xorshift so the synthetic inputs are reproducible
    let seed = 0x1234567;
    const rnd = (n: number): number => {
        seed ^= seed << 13;
        seed ^= seed >>> 17;
        seed ^= seed << 5;
        return (seed >>> 0) % n;
    };
    const inputs: Uint8Array[] = [new Uint8Array(0), new Uint8Array(1), Uint8Array.of(0x10), Uint8Array.of(1)];
    const lengths = [2, 3, 7, 10, 11, 64, 137, 138, 300, 1000, 4096, 0x1fff, 0x2000, 0x2001, 9000];
    for (let k = 0; k < 400; k++) {
        const len = k < lengths.length * 4 ? lengths[k % lengths.length] : rnd(k % 5 === 0 ? 0x2400 : 600);
        const a = new Uint8Array(len);
        const style = k % 6;
        let p = 0;
        while (p < len) {
            // mix of zero runs, value runs and noise, with varied run lengths
            const kind = style === 0 ? 2 : rnd(style + 2);
            const run = Math.min(len - p, 1 + rnd(kind === 0 ? 300 : 24));
            const v = rnd(256);
            for (let q = 0; q < run; q++, p++) {
                a[p] = kind === 0 ? 0 : kind === 1 ? v : kind === 2 ? rnd(256) : kind === 3 ? rnd(4) * 0x11 : rnd(16) << (rnd(2) * 4);
            }
        }
        if (k % 4 === 1 && len > 0) {
            // trailing zero runs exercise the < 0x2000 end padding
            a.fill(0, len - Math.min(len, rnd(400)));
        }
        inputs.push(a);
    }
    for (const t of tsOut) {
        // real packed graphics, cut at odd places
        const packed = t; // compressed bytes are as good noise as any
        inputs.push(packed.subarray(0, rnd(packed.length + 1)));
    }

    const dir = fs.mkdtempSync(path.join(os.tmpdir(), "sotn_cmpgfx_go_"));
    fs.mkdirSync(path.join(dir, "in"));
    const mod = path.join(repo, "tools/sotn-assets");
    fs.writeFileSync(path.join(dir, "go.mod"),
        "module sotncmpgfxharness\n\ngo 1.24.0\n\n" +
        "require github.com/xeeynamo/sotn-decomp/tools/sotn-assets v0.0.0\n\n" +
        `replace github.com/xeeynamo/sotn-decomp/tools/sotn-assets => ${mod}\n`);
    fs.copyFileSync(path.join(mod, "go.sum"), path.join(dir, "go.sum"));
    fs.writeFileSync(path.join(dir, "main.go"), `package main

import (
	"os"
	"path/filepath"
	"strings"

	"github.com/xeeynamo/sotn-decomp/tools/sotn-assets/sotn"
	"github.com/xeeynamo/sotn-decomp/tools/sotn-assets/util/png"
)

// in/N.bin -> in/N.out = Deflate(bytes); in/N.pngpath -> in/N.out = Deflate(png.Decode(file))
func main() {
	dir := os.Args[1]
	files, _ := filepath.Glob(filepath.Join(dir, "*"))
	for _, f := range files {
		var src []byte
		if strings.HasSuffix(f, ".bin") {
			src, _ = os.ReadFile(f)
		} else if strings.HasSuffix(f, ".pngpath") {
			p, _ := os.ReadFile(f)
			fh, err := os.Open(string(p))
			if err != nil {
				panic(err)
			}
			src, _, err = png.Decode(fh)
			fh.Close()
			if err != nil {
				panic(err)
			}
		} else {
			continue
		}
		out := strings.TrimSuffix(strings.TrimSuffix(f, ".bin"), ".pngpath") + ".out"
		if err := os.WriteFile(out, sotn.Deflate(src), 0o644); err != nil {
			panic(err)
		}
	}
}
`);
    inputs.forEach((a, k) => fs.writeFileSync(path.join(dir, "in", `d${k}.bin`), a));
    assets.forEach((a, k) => fs.writeFileSync(path.join(dir, "in", `p${k}.pngpath`), a.png));
    const env = { ...process.env, GOFLAGS: "-mod=mod", GOPROXY: "off", GOTOOLCHAIN: "local", GOWORK: "off" };
    execFileSync("bash", ["-lc", `cd ${dir} && go build -o harness . && ./harness in`], { env, stdio: "inherit" });

    let dOk = 0, pOk = 0;
    inputs.forEach((a, k) => {
        if (same(sotnDeflate(a), fs.readFileSync(path.join(dir, "in", `d${k}.out`)))) {
            dOk++;
        } else if (dOk + 5 > k) {
            console.log(`MISMATCH deflate input ${k} (${a.length} bytes)`);
        }
    });
    assets.forEach((a, k) => {
        if (same(tsOut[k], fs.readFileSync(path.join(dir, "in", `p${k}.out`)))) {
            pOk++;
        } else {
            console.log(`MISMATCH vs Go ${a.stage}/${a.name}`);
        }
    });
    console.log(`go Deflate: ${dOk} of ${inputs.length} identical; go png.Decode+Deflate: ${pOk} of ${assets.length} identical`);
    if (dOk !== inputs.length || pOk !== assets.length) {
        process.exitCode = 1;
    }
}
if (headerOk !== assets.length) {
    process.exitCode = 1;
}
