// SPDX-License-Identifier: AGPL-3.0-or-later
// Python's json.dumps(value, indent=N) (ensure_ascii, the default separators
// "," / ": " with an indent), for byte-exact JSON assets. Values: null,
// booleans, integers, strings, arrays and plain objects (keys in insertion
// order: avoid integer-like keys, which JavaScript reorders). Floats are
// written as Python's repr only for integral values (not needed here).

export type PyJson = null | boolean | number | string | PyJson[] | { [k: string]: PyJson };

function str(s: string): string {
    let out = "\"";
    for (let i = 0; i < s.length; i++) {
        const c = s.charCodeAt(i);
        if (c === 0x22) {
            out += "\\\"";
        } else if (c === 0x5C) {
            out += "\\\\";
        } else if (c === 0x0A) {
            out += "\\n";
        } else if (c === 0x0D) {
            out += "\\r";
        } else if (c === 0x09) {
            out += "\\t";
        } else if (c === 0x08) {
            out += "\\b";
        } else if (c === 0x0C) {
            out += "\\f";
        } else if (c < 0x20 || c > 0x7E) {
            out += "\\u" + c.toString(16).padStart(4, "0");
        } else {
            out += s[i];
        }
    }
    return out + "\"";
}

function num(v: number): string {
    if (!Number.isFinite(v)) {
        throw new Error("pyjson: non-finite number");
    }
    if (Number.isInteger(v)) {
        return Object.is(v, -0) ? "0" : v.toFixed(0);
    }
    throw new Error("pyjson: floats are not supported");
}

function dump(v: PyJson | undefined, indent: number, level: number): string {
    if (v === null) {
        return "null";
    }
    if (v === true) {
        return "true";
    }
    if (v === false) {
        return "false";
    }
    if (typeof v === "number") {
        return num(v);
    }
    if (typeof v === "string") {
        return str(v);
    }
    if (v === undefined) {
        throw new Error("pyjson: undefined value");
    }
    const pad = "\n" + " ".repeat(indent * (level + 1));
    const end = "\n" + " ".repeat(indent * level);
    if (Array.isArray(v)) {
        if (v.length === 0) {
            return "[]";
        }
        return "[" + v.map((e) => pad + dump(e, indent, level + 1)).join(",") + end + "]";
    }
    const keys = Object.keys(v);
    if (keys.length === 0) {
        return "{}";
    }
    return "{" + keys.map((k) => pad + str(k) + ": " + dump(v[k], indent, level + 1)).join(",") + end + "}";
}

export function pyJsonDumps(v: PyJson, indent = 2): string {
    return dump(v, indent, 0);
}
