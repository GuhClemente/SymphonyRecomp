// SPDX-License-Identifier: AGPL-3.0-or-later
// node mods/sotn-ultimate/tools/builder/node/test_cen_maria.ts
// src/cen_maria.ts against the generated mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h (gen_cen_maria.py)
// (gen_cen_maria.py's output), Saturn files from disks/saturn. The header's
// arrays carry MOD_RO (an __attribute__ macro) and some are 2D ([][4]),
// which parseCArrays doesn't take: a copy without MOD_RO, the 2D arrays
// flattened, is what it parses here.
import * as fs from "node:fs";
import * as os from "node:os";
import * as path from "node:path";
import { cenMaria } from "../src/cen_maria.ts";
import { parseCArrays } from "./c_arrays.ts";
import { ROOT } from "./util.ts";

const hdr = path.join(ROOT, "mods/sotn-ultimate/src/maria_saturn/mod_maria_boss_data.h");
const text = fs.readFileSync(hdr, "utf8");
const direct = parseCArrays(hdr).length;
const flat = text.replace(/ MOD_RO =/g, " =").replace(
    /(\w+)\[\]\[\d+\] = \{([^;]*)\};/g, (_m, name: string, body: string) => name + "[] = {" + body.replace(/[{}]/g, "") + "};");
const tmp = path.join(fs.mkdtempSync(path.join(os.tmpdir(), "cen_maria_")), "mod_maria_boss_data.h");
fs.writeFileSync(tmp, flat);
const ref = parseCArrays(tmp);

const rd = async (p: string): Promise<Uint8Array> => new Uint8Array(fs.readFileSync(path.join(ROOT, "disks/saturn", p)));
const t0 = performance.now();
const mine = await cenMaria(rd);
console.log("cenMaria: %d arrays in %s s (parseCArrays on the header as is: %d arrays)",
            mine.size, ((performance.now() - t0) / 1000).toFixed(1), direct);
let same = 0;
let first = "";
const bad: string[] = [];
for (const r of ref) {
    const m = mine.get(r.name);
    let why = "";
    if (m === undefined) {
        why = "missing";
    } else if (m.size !== r.size) {
        why = "size " + m.size + " vs " + r.size;
    } else if (m.values.length !== r.values.length) {
        why = "length " + m.values.length + " vs " + r.values.length;
    } else {
        const i = m.values.findIndex((v, k) => v !== r.values[k]);
        if (i >= 0) {
            why = "value " + i + ": " + m.values[i] + " vs " + r.values[i];
        }
    }
    if (why) {
        bad.push(r.name);
        first ||= r.name + ": " + why;
    } else {
        same++;
    }
}
const extra = [...mine.keys()].filter((n) => !ref.some((r) => r.name === n));
console.log("%d / %d arrays identical, %d differ%s%s", same, ref.length, bad.length,
            bad.length ? " (" + bad.join(" ") + "); first: " + first : "",
            extra.length ? "; not in the header: " + extra.join(" ") : "");
process.exitCode = bad.length || extra.length ? 1 : 0;
