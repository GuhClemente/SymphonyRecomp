// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_saturn_sprites.ts [STAGE...] [tilematch]
// (no arguments: gdn prs rgdn rprs tilematch; the tilematch part takes ~2
// minutes, mostly the Python)
// src/saturn_sprites.ts against mods/sotn-ultimate/tools/saturn_sprites.py on the calls the
// stages make: for each stage (gdn, prs, rgdn, rprs by default) a Python
// script runs its sprites() (gdn_assets.py / prs_assets.py, configured by
// rgdn_assets.py / rprs_assets.py) with every PsxSpriteSheet call traced,
// and stops at its first json.load, right after the last fill and before any
// file is written. The trace (each call's arguments and result, the sheets'
// final placements, quadrant pixels (md5), cluts and the shared BLEED /
// NW_MAX / TRIM tables) is replayed here and compared exactly.
// tilematch: src/saturn_tilematch.ts against mods/sotn-ultimate/tools/saturn_tilematch.py on
// its call sites in gen_mod_rooms.py and gen_prs_links.py (TILEMATCH_DUMP):
// every output file byte for byte, and the candidate set's order.
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { execFileSync } from "node:child_process";
import { md5 } from "../src/md5.ts";
import { PsxSpriteSheet, SaturnStage, SpriteState } from "../src/saturn_sprites.ts";
import type { PlaceKey } from "../src/saturn_sprites.ts";
import { layersTilemaps, tilematch, usedTiles } from "../src/saturn_tilematch.ts";
import { ROOT } from "./util.ts";

const DUMP = `
import hashlib, json, os, sys
sys.path.insert(0, os.path.join(sys.argv[3], "mods/sotn-ultimate/tools"))
stage, out = sys.argv[1], sys.argv[2]
import saturn_sprites as ss

trace, sheets, cluts_ids = [], [], []


class Stop(Exception):
    pass


def sid(sheet):
    return next(i for i, s in enumerate(sheets) if s is sheet)


orig = {n: getattr(ss.PsxSpriteSheet, n) for n in
        ("__init__", "palette_block", "frame", "image_part", "prepack", "used_quadrants")}


def w_init(self, quadrants, clut_base, cluts=None):
    orig["__init__"](self, quadrants, clut_base, cluts)
    sheets.append(self)
    if not any(c is self.cluts for c in cluts_ids):
        cluts_ids.append(self.cluts)
    trace.append({"op": "new", "sheet": len(sheets) - 1, "quadrants": [list(q) for q in self.quadrants],
                  "clut_base": clut_base,
                  "cluts": next(i for i, c in enumerate(cluts_ids) if c is self.cluts),
                  "state": {"bleed_banks": sorted(ss.BLEED_BANKS), "stp_banks": sorted(ss.STP_BANKS),
                            "trim_banks": [[b, sorted(v)] for b, v in ss.TRIM_BANKS.items()],
                            "bleed_rows": ss.BLEED_ROWS}})


def w_palette_block(self, st, bank):
    r = orig["palette_block"](self, st, bank)
    trace.append({"op": "palette_block", "sheet": sid(self), "bank": bank, "ret": r})
    return r


def w_frame(self, st, bank, frames, index, pal_shift=0):
    r = orig["frame"](self, st, bank, frames, index, pal_shift)
    trace.append({"op": "frame", "sheet": sid(self), "bank": bank, "frames": frames, "index": index,
                  "pal_shift": pal_shift, "ret": r})
    return r


def w_image_part(self, st, bank, img, dx, dy, colours, flip=0):
    r = orig["image_part"](self, st, bank, img, dx, dy, colours, flip)
    trace.append({"op": "image_part", "sheet": sid(self), "bank": bank, "img": img, "dx": dx, "dy": dy,
                  "colours": list(colours), "flip": flip, "ret": r})
    return r


def w_prepack(self, st, keys):
    ev = {"op": "prepack", "sheet": sid(self), "keys": [list(k) for k in keys], "full": False}
    trace.append(ev)
    try:
        orig["prepack"](self, st, keys)
    except AssertionError:
        ev["full"] = True
        raise


def w_used_quadrants(self):
    r = orig["used_quadrants"](self)
    trace.append({"op": "used_quadrants", "sheet": sid(self), "ret": r})
    return r


for n, f in (("__init__", w_init), ("palette_block", w_palette_block), ("frame", w_frame),
             ("image_part", w_image_part), ("prepack", w_prepack), ("used_quadrants", w_used_quadrants)):
    setattr(ss.PsxSpriteSheet, n, f)


class StopJson:
    @staticmethod
    def load(*a, **k):
        raise Stop()


if stage == "gdn":
    import gdn_assets as mod
elif stage == "rgdn":
    import rgdn_assets
    import gdn_assets as mod
elif stage == "prs":
    import prs_assets as mod
    # prs_assets.py's __main__
    ss.STP_BANKS.update((mod.SPECTER[0], mod.WISP[0]))
elif stage == "rprs":
    import rprs_assets
    import prs_assets as mod
mod.json = StopJson
try:
    mod.sprites()
    raise SystemExit("sprites() never reached json.load")
except Stop:
    pass
json.dump({
    "prg": os.path.basename(mod.PRG), "chr": os.path.basename(mod.CHR), "trace": trace,
    "sheets": [{
        "placed": [[list(k), list(v)] for k, v in s.placed.items()],
        "requests": [list(k) for k in s.requests],
        "blocks": [[b, v] for b, v in s.blocks.items()],
        "pages_md5": [hashlib.md5(s.quadrant_pixels(q)).hexdigest() for q in range(len(s.pages))],
        "skylines": [[q, v] for q, v in s.skylines.items()],
        "used": s.used_quadrants(),
    } for s in sheets],
    "cluts": [[list(p) for p in c] for c in cluts_ids],
    "bleed": [[list(k), v] for k, v in ss.BLEED.items()],
    "nw_max": [[list(k), v] for k, v in ss.NW_MAX.items()],
    "trim": [[list(k), list(v)] for k, v in ss.TRIM.items()],
}, open(out, "w"))
`;

// saturn_tilematch.py's call sites (gen_mod_rooms.py np3(), rno3(), wrp() for
// wrp / rwrp; gen_prs_links.py saturn_graft() for each LINKS entry), their
// arguments derived as those scripts do; each is run into OUT/tm<n>.bin
// (+ .err, .full, .px, .coll), with the script's `used` set's iteration order
const TILEMATCH_DUMP = `
import json, os, subprocess, sys
out, ROOT = sys.argv[1], sys.argv[2]
sys.path.insert(0, os.path.join(ROOT, "mods/sotn-ultimate/tools"))
import gen_mod_rooms as gm
import gen_prs_links as gp

calls = []


def add(prg, mapf, room, layer, stage, adir, tdef, art_only=False, pool=(), dset=0):
    up = stage.upper()
    calls.append({"prg": "disks/saturn/%s.PRG" % prg, "map": "disks/saturn/%s.MAP" % mapf, "room": room,
                  "layer": layer, "fbin": "disks/us/ST/%s/F_%s.BIN" % (up, up),
                  "adir": os.path.relpath(adir, ROOT), "tdef": tdef, "art_only": art_only,
                  "pool": [os.path.relpath(p, ROOT) for p in pool], "set": dset})


for stage, prg, shaft in (("np3", "STAGE_01", 1), ("rno3", "RSTAGE01", gm.RNO3_SHAFT)):
    _, layers, roomdefs = gm.parse_layers(stage)
    rooms = gm.parse_rooms(stage)
    fg = layers[int(roomdefs[rooms[shaft][4]][1])]
    tdef = "%s_tiledef_%s" % (stage, stage)
    adir = gm.asset_dir(stage)
    add(prg, prg, 17, 0, stage, adir, tdef, True, [os.path.join(adir, fg[0] + ".bin")])
    add(prg, prg, 17, 2, stage, adir, tdef)
for stage in ("wrp", "rwrp"):
    spec = gm.WARP6[stage]
    _, layers, roomdefs = gm.parse_layers(stage)
    rooms = gm.parse_rooms(stage)
    src = next(i for i, r in enumerate(rooms) if r[:4] == [*spec["src"], *spec["src"]])
    tdef = "%s_tiledef_%s" % (stage, stage)
    src_fg = layers[int(roomdefs[rooms[src][4]][1])]
    adir = gm.asset_dir(stage)
    for room in (0, 5):
        for layer in (0, 2):
            add(spec["sat"], spec["sat"], room, layer, stage, adir, tdef, True,
                [os.path.join(adir, src_fg[0] + ".bin")])
for stage, spec in gp.LINKS.items():
    _, layers, roomdefs = gp.parse_layers(stage)
    rooms = gp.parse_rooms(stage)
    room = next(i for i, r in enumerate(rooms) if tuple(r[:4]) == spec["room"])
    fg = layers[int(roomdefs[rooms[room][4]][1])]
    prg, mapf, dset, sroom = spec["saturn"][:4]
    for layer in (0, 2):
        add(prg, mapf, sroom, layer, stage, os.path.join(ROOT, "assets/st", stage), fg[1], dset=dset)

for n, c in enumerate(calls):
    j = lambda p: os.path.join(ROOT, p)
    argv = [sys.executable, j("mods/sotn-ultimate/tools/saturn_tilematch.py"), j(c["prg"]), j(c["map"]), str(c["room"]),
            str(c["layer"]), j(c["fbin"]), j(c["adir"]), c["tdef"], os.path.join(out, "tm%d.bin" % n)]
    if c["art_only"]:
        argv.append("--art-only")
    if c["pool"]:
        argv.append("--pool=" + ",".join(j(p) for p in c["pool"]))
    if c["set"]:
        argv.append("--set=%d" % c["set"])
    subprocess.run(argv, check=True, stdout=subprocess.DEVNULL)
    # the script's candidate set, as it builds it
    import struct
    used = set()
    for L in json.load(open(os.path.join(j(c["adir"]), "layers.json"))):
        for part in L.values() if isinstance(L, dict) else ():
            if isinstance(part, dict) and part.get("tiledef") == c["tdef"] + ".json":
                d = open(os.path.join(j(c["adir"]), part["data"]), "rb").read()
                used.update(struct.unpack("<%dH" % (len(d) // 2), d))
    if c["pool"]:
        used = set()
        for f in c["pool"]:
            d = open(j(f), "rb").read()
            used.update(struct.unpack("<%dH" % (len(d) // 2), d))
    used.discard(0)
    c["used"] = list(used)
json.dump(calls, open(os.path.join(out, "tilematch.json"), "w"))
`;

const args = process.argv.slice(2);
const withTilematch = args.length === 0 || args.includes("tilematch");
const named = args.filter((a) => a !== "tilematch");
const stages = args.length === 0 ? ["gdn", "prs", "rgdn", "rprs"] : named;
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "satspr-"));
fs.writeFileSync(path.join(tmp, "dump.py"), DUMP);
const sat = (name: string): Uint8Array => new Uint8Array(fs.readFileSync(path.join(ROOT, "disks/saturn", name)));
// Python keys (lists, nw None -> null) as the sheets' Map keys
const keyStr = (k: unknown[]): string => k.map((v) => (v === null ? "null" : String(v))).join(",");
const same = (a: unknown, b: unknown): boolean => JSON.stringify(a) === JSON.stringify(b);

let failures = 0;
for (const name of stages) {
    const out = path.join(tmp, name + ".json");
    execFileSync(path.join(ROOT, ".venv/bin/python"), [path.join(tmp, "dump.py"), name, out, ROOT],
                 { cwd: ROOT, stdio: ["ignore", "ignore", "inherit"] });
    const py = JSON.parse(fs.readFileSync(out, "utf8"));
    const stage = new SaturnStage(sat(py.prg), sat(py.chr), sat("0.BIN"));
    const state = new SpriteState();
    const sheets: PsxSpriteSheet[] = [];
    const cluts: number[][][] = [];
    let bad = 0;
    const counts: Record<string, number> = {};
    const fail = (what: string, mine: unknown, ref: unknown): void => {
        if (bad++ < 10) {
            console.log("%s: %s differs\n  ts: %s\n  py: %s", name, what, JSON.stringify(mine).slice(0, 400),
                        JSON.stringify(ref).slice(0, 400));
        }
    };
    for (const ev of py.trace) {
        counts[ev.op] = (counts[ev.op] ?? 0) + 1;
        const sh = sheets[ev.sheet];
        if (ev.op === "new") {
            // the module globals as the stage script had set them by then
            state.bleedBanks = new Set(ev.state.bleed_banks);
            state.stpBanks = new Set(ev.state.stp_banks);
            state.trimBanks = new Map(ev.state.trim_banks.map((e: [number, number[]]) => [e[0], new Set(e[1])]));
            state.bleedRows = ev.state.bleed_rows;
            if (cluts[ev.cluts] === undefined) {
                cluts[ev.cluts] = [];
            }
            sheets.push(new PsxSpriteSheet(ev.quadrants, ev.clut_base, cluts[ev.cluts], state));
        } else if (ev.op === "palette_block") {
            const r = sh.paletteBlock(stage, ev.bank);
            if (r !== ev.ret) {
                fail("palette_block " + ev.bank.toString(16), r, ev.ret);
            }
        } else if (ev.op === "frame") {
            const r = sh.frame(stage, ev.bank, ev.frames, ev.index, ev.pal_shift);
            if (!same(r, ev.ret)) {
                fail("frame " + ev.bank.toString(16) + " " + ev.index, r, ev.ret);
            }
        } else if (ev.op === "image_part") {
            const r = sh.imagePart(stage, ev.bank, ev.img, ev.dx, ev.dy, ev.colours, ev.flip);
            if (!same(r, ev.ret)) {
                fail("image_part " + ev.bank.toString(16) + " " + ev.img, r, ev.ret);
            }
        } else if (ev.op === "prepack") {
            let full = false;
            try {
                sh.prepack(stage, ev.keys as PlaceKey[]);
            } catch (e) {
                if (!(e instanceof Error) || e.message !== "sprite quadrants full") {
                    throw e;
                }
                full = true;
            }
            if (full !== ev.full) {
                fail("prepack (sheet " + ev.sheet + ") full", full, ev.full);
            }
        } else if (ev.op === "used_quadrants") {
            if (sh.usedQuadrants() !== ev.ret) {
                fail("used_quadrants (sheet " + ev.sheet + ")", sh.usedQuadrants(), ev.ret);
            }
        }
    }
    let pages = 0;
    py.sheets.forEach((ref: any, i: number) => {
        const sh = sheets[i];
        const placed = [...sh.placed.entries()];
        const refPlaced = ref.placed.map((e: [unknown[], number[]]) => [keyStr(e[0]), e[1]]);
        if (!same(placed, refPlaced)) {
            fail("sheet " + i + " placed", placed, refPlaced);
        }
        const req = sh.requests.map((k) => keyStr(k));
        if (!same(req, ref.requests.map(keyStr))) {
            fail("sheet " + i + " requests", req, ref.requests.map(keyStr));
        }
        if (!same([...sh.blocks.entries()], ref.blocks)) {
            fail("sheet " + i + " blocks", [...sh.blocks.entries()], ref.blocks);
        }
        if (!same([...sh.skylines.entries()], ref.skylines)) {
            fail("sheet " + i + " skylines", [...sh.skylines.entries()], ref.skylines);
        }
        if (sh.usedQuadrants() !== ref.used) {
            fail("sheet " + i + " used quadrants", sh.usedQuadrants(), ref.used);
        }
        const sums = sh.pages.map((_, q) => md5(sh.quadrantPixels(q)));
        pages += sums.length;
        if (!same(sums, ref.pages_md5)) {
            fail("sheet " + i + " quadrant pixels", sums, ref.pages_md5);
        }
    });
    if (!same(cluts, py.cluts)) {
        fail("cluts", cluts, py.cluts);
    }
    const tables: [string, Map<string, unknown>, [unknown[], unknown][]][] = [
        ["BLEED", state.bleed, py.bleed], ["NW_MAX", state.nwMax, py.nw_max], ["TRIM", state.trim, py.trim]];
    for (const [what, mine, ref] of tables) {
        const r = ref.map((e) => [keyStr(e[0]), e[1]]);
        if (!same([...mine.entries()], r)) {
            fail(what, [...mine.entries()], r);
        }
    }
    console.log("%s: %s; %d sheets, %d quadrants, %d cluts: %s", name,
                Object.entries(counts).map(([k, v]) => v + " " + k).join(", "), sheets.length, pages,
                cluts.reduce((s, c) => s + c.length, 0), bad ? bad + " DIFFERENCES" : "all identical");
    failures += bad;
}

if (withTilematch) {
    execFileSync(path.join(ROOT, ".venv/bin/python"), ["-c", TILEMATCH_DUMP, tmp, ROOT],
                 { cwd: ROOT, stdio: ["ignore", "ignore", "inherit"] });
    const calls = JSON.parse(fs.readFileSync(path.join(tmp, "tilematch.json"), "utf8"));
    const rd = (p: string): Uint8Array => new Uint8Array(fs.readFileSync(path.join(ROOT, p)));
    const u16 = (a: number[]): Uint8Array => {
        const b = new Uint8Array(a.length * 2);
        a.forEach((v, i) => {
            b[2 * i] = v & 255;
            b[2 * i + 1] = v >> 8;
        });
        return b;
    };
    const u32 = (a: number[]): Uint8Array => new Uint8Array(new Uint32Array(a).buffer);
    const eq = (a: Uint8Array, b: Uint8Array): boolean => a.length === b.length && a.every((v, i) => v === b[i]);
    let bad = 0;
    calls.forEach((c: any, n: number) => {
        const adir = c.adir;
        const maps = c.pool.length ? c.pool
            : layersTilemaps(JSON.parse(fs.readFileSync(path.join(ROOT, adir, "layers.json"), "utf8")), c.tdef)
                .map((f) => path.join(adir, f));
        const used = usedTiles(maps.map(rd));
        const defs = { tiles: rd(path.join(adir, c.tdef + "_tiles.bin")), pages: rd(path.join(adir, c.tdef + "_pages.bin")),
                       cluts: rd(path.join(adir, c.tdef + "_cluts.bin")), cols: rd(path.join(adir, c.tdef + "_cols.bin")) };
        const r = tilematch(rd(c.prg), rd(c.map), c.room, c.layer, rd(c.fbin), defs, used, c.art_only, c.set);
        const out = path.join(tmp, "tm" + n + ".bin");
        const full = new Uint8Array(4 + r.full.length);
        full.set([r.width & 255, r.width >> 8, r.height & 255, r.height >> 8]);
        full.set(r.full, 4);
        const diffs: string[] = [];
        if (!same(used, c.used)) {
            diffs.push("used order");
        }
        const files: [string, Uint8Array | null][] = [["", u16(r.tiles)], [".err", u32(r.errs)], [".full", full],
                                                      [".px", r.cells], [".coll", r.coll]];
        for (const [ext, mine] of files) {
            const ref = fs.existsSync(out + ext) ? new Uint8Array(fs.readFileSync(out + ext)) : null;
            if (ref === null || mine === null ? ref !== mine : !eq(mine, ref)) {
                diffs.push("OUT" + ext);
            }
        }
        const what = path.basename(c.prg) + " room " + c.room + " layer " + c.layer + " for " + c.tdef +
            (c.art_only ? " --art-only" : "") + (c.pool.length ? " --pool" : "") + (c.set ? " --set=" + c.set : "");
        console.log("tilematch %s: %dx%d cells, %d candidates: %s", what, r.tw, r.th, used.length,
                    diffs.length ? "DIFFERS (" + diffs.join(", ") + ")" : "identical");
        bad += diffs.length ? 1 : 0;
    });
    // the stages' sets come out sorted (their tables outgrow the largest
    // tile); the set's order where they don't: random u16s, in tilemap order
    let seed = 1;
    const seqs: number[][] = [];
    for (const n of [5, 40, 300, 3000, 20000]) {
        const s: number[] = [];
        for (let i = 0; i < n; i++) {
            seed = (seed * 1103515245 + 12345) % 2147483648;
            s.push(i % 7 === 0 ? 0 : seed % 65536);
        }
        seqs.push(s);
    }
    const pyOrders = JSON.parse(execFileSync(path.join(ROOT, ".venv/bin/python"), ["-c",
        "import json, sys\nout = []\nfor s in json.load(sys.stdin):\n    u = set()\n    u.update(tuple(s))\n" +
        "    u.discard(0)\n    out.append(list(u))\nprint(json.dumps(out))"],
        { input: JSON.stringify(seqs), encoding: "utf8", maxBuffer: 1 << 26 }));
    seqs.forEach((s, i) => {
        if (!same(usedTiles([u16(s)]), pyOrders[i])) {
            console.log("tilematch: set order of %d random u16s DIFFERS", s.length);
            bad++;
        }
    });
    console.log("tilematch: %d calls, %s", calls.length, bad ? bad + " DIFFER" : "all identical");
    failures += bad;
}
fs.rmSync(tmp, { recursive: true });
process.exit(failures ? 1 : 0);
