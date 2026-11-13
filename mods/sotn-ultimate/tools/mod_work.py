# SPDX-License-Identifier: AGPL-3.0-or-later
"""The build's work folder: renders, caches, pristine retail copies and the
builder's compiled modules (mods/sotn-ultimate/tools/builder/native/build.sh). $MOD_WORK,
else build/mod-work in the repo (build/ is not tracked)."""
import os
import re

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
MOD_WORK = os.environ.get("MOD_WORK") or os.path.join(ROOT, "build", "mod-work")


def work_dir(*parts):
    """a path in the work folder"""
    return os.path.join(MOD_WORK, *parts)


def read_src(path):
    """a repo source file (path from the repo root) with the mod headers it
    includes (mods/sotn-ultimate/...) put back in place: the text the tools
    parsed before those blocks moved out of the upstream files"""
    full = os.path.join(ROOT, path)
    out = []
    for line in open(full, encoding="utf-8").read().split("\n"):
        m = re.match(r"^\s*#\s*include\s+\"([^\"]+)\"", line)
        if m:
            tgt = os.path.normpath(os.path.join(os.path.dirname(full), m.group(1)))
            if tgt.startswith(os.path.join(ROOT, "mods", "sotn-ultimate") + os.sep):
                out.append(read_src(os.path.relpath(tgt, ROOT)))
                continue
        out.append(line)
    return "\n".join(out)
