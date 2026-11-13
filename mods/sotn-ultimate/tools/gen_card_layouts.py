#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: put the familiar cards where the Saturn (= JP PS1) has them.
#
# The US release dropped the JP-only Sprite (Yousei) and Nose Devil cards and
# moved things around (compared against the Saturn's STAGE_10 / STAGE_11
# layouts):
#   NO2 (Olrox's Quarters) room (19,18)-(20,18): US Sword Card, Saturn Sprite
#     Card (RELIC_JP_0) at the same spot (Saturn x 460 = 368 * 1.25).
#   NO2 room (33,18): US Garnet (E_PERSISTENT_ITEM_DROP 12), Saturn Sword Card
#     at (160,148) = PS1 (128,148). (The PSP also has the Sword Card there.)
#   ARE (Colosseum) room (19,20)-(20,20): US Holy Sword (E_PERSISTENT_ITEM_DROP
#     7), Saturn Nose Devil Card (RELIC_JP_1) at (60,153) = PS1 (48,153).
# Each is a one-entity room, so the entity is replaced in place in
# assets/st/<stage>/entity_layouts.json (same size: the data section keeps
# its addresses). The edited layout arrays are added to
# build/us/mod_data_edits.json for mods/sotn-ultimate/tools/check_stage_data.py; run this
# after mods/sotn-ultimate/tools/gen_prs_links.py, which rewrites that file.
import json
import os
import re
import shutil
from mod_work import work_dir  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
PRISTINE = work_dir("retail-assets")

# stage -> [(match, changes)]; match = (x, y, id, params)
EDITS = {
    "no2": [
        ((368, 148, "E_RELIC_ORB", 22), {"params": 23}),
        ((128, 153, "E_PERSISTENT_ITEM_DROP", 12),
         {"id": "E_RELIC_ORB", "y": 148, "params": 22}),
    ],
    "are": [
        ((48, 161, "E_PERSISTENT_ITEM_DROP", 7),
         {"id": "E_RELIC_ORB", "y": 153, "params": 24}),
    ],
}

edits_path = os.path.join(ROOT, "build/us/mod_data_edits.json")
edited = json.load(open(edits_path)) if os.path.exists(edits_path) else {}

for stage, changes in EDITS.items():
    path = os.path.join(ROOT, f"assets/st/{stage}/entity_layouts.json")
    pristine = os.path.join(PRISTINE, stage, "entity_layouts.json")
    if not os.path.exists(pristine):
        os.makedirs(os.path.dirname(pristine), exist_ok=True)
        shutil.copy(path, pristine)
    raw = open(pristine).read()
    data = json.loads(raw)
    count = 0
    for layout in data["entities"]:
        count += len(layout)
        for e in layout:
            for (x, y, eid, params), new in changes:
                if (e["x"], e["y"], e["id"], e["params"]) == (x, y, eid, params):
                    assert len(layout) == 3, (stage, e)  # one entity + sentinels
                    e.update(new)
                    e["done"] = True
    for layout in data["entities"]:
        for e in layout:
            e.pop("done", None)
    hits = sum(1 for layout in json.loads(raw)["entities"] for e in layout
               for (x, y, eid, params), _ in changes
               if (e["x"], e["y"], e["id"], e["params"]) == (x, y, eid, params))
    assert hits == len(changes), (stage, hits)
    out = json.dumps(data, indent=2) + "\n"
    if open(path).read() != out:
        open(path, "w").write(out)
    # the generated arrays (names are stable: a hash of the output dir)
    gen = open(os.path.join(ROOT, f"src/st/{stage}/gen/e_layout.c")).read()
    syms = re.findall(r"u16 (entity_layout_\w+_[xy])\[\]", gen)
    assert len(syms) == 2, syms
    lst = edited.setdefault("st" + stage, [])
    for sym in syms:
        if [sym, count * 10] not in lst:
            lst.append([sym, count * 10])
    print(f"{stage}: {len(changes)} card placement(s) restored")

os.makedirs(os.path.dirname(edits_path), exist_ok=True)
json.dump(edited, open(edits_path, "w"), indent=2)
