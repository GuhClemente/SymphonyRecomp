// SPDX-License-Identifier: AGPL-3.0-or-later
// The Saturn map converter (tools/saturn/assets) as WebAssembly, for Node:
// built with cargo --target wasm32-unknown-emscripten (MODULARIZE,
// EXPORT_NAME=SaturnAssets, INVOKE_RUN=0, SINGLE_FILE; see the builder notes)
// and kept at SATCONV_JS; the page embeds the same file
import * as path from "node:path";
import { createRequire } from "node:module";
import type { SatConvModule } from "../src/stage_build.ts";
import { MOD_WORK } from "./util.ts";

export const SATCONV_JS = path.join(MOD_WORK, "builder/satconv/saturn_assets.js");

// a fresh module; SOTN_MOD_UNCAPPED as mods/sotn-ultimate/tools/gdn_assets.py runs it
export function satconvModule(): Promise<SatConvModule> {
    return createRequire(import.meta.url)(SATCONV_JS)({
        print: () => {}, printErr: () => {},
        preRun: [(mod: { ENV: Record<string, string> }) => { mod.ENV.SOTN_MOD_UNCAPPED = "1"; }],
    });
}
