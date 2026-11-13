#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
# Saturn port mod: generate entity layout copies with the mod's extra
# entities for each stage that gets one.
#
# Stage cutscene scripts reference their overlay's data section by absolute
# address, so a stage's data must stay byte-identical to retail (see
# mods/sotn-ultimate/tools/check_stage_data.py). Instead of editing the layout data, this
# writes <dir>/mod_layout.h with copies of one room's horizontal and vertical
# layout lists plus the new entities. They are const, included from the
# stage's e_red_door.c (whose rodata is linked after the data section), and
# InitRoomEntities (src/st/create_entity.h) switches to them for
# MOD_LAYOUT_ID, which the stage header defines.
#
# The lists are read from the retail overlay binaries. Do not use
# assets/st/*/entity_layouts.json for this: its "entities" are ordered by
# address, which only sometimes matches the layout ids rooms use (it does in
# TOP, not in NO4 or NZ1). Table addresses are entityLayoutHorizontal and
# entityLayoutVertical from the overlay's map; they sit in the data section,
# so retail and modded builds agree. MOD_LAYOUT_ID is a room's objLayoutId
# (byte 7 of its RoomHeader).
import struct
import os

ROOT = os.path.join(os.path.dirname(__file__), "../../..")
BASE = 0x80180000

STAGES = {
    # God Speed Shoes relic orb, Castle Keep. On the Saturn (STAGE_07.PRG) it
    # sits in room (32,8)-(36,8) at (160,192), with map cells 320px wide: map
    # position 32.5 x 8.75, which on PSX is the main room (30,5)-(37,12),
    # layout 1, at (640,960), open air with floor right below. params is the
    # relic id: RELIC_GODSPEED_SHOES = 30 (as on the Saturn). The Saturn's slot 45
    # belongs to a PSX breakable in the same room, so it uses slot 11.
    "top": {
        "bin": "disks/us/ST/TOP/TOP.BIN",
        "tables": (0x80180740, 0x80180814),
        "layout": 1,
        "id_check": ("E_RELIC_ORB", 0x0B),
        "add": [
            {"x": 640, "y": 960, "id": "E_RELIC_ORB", "slot": 11, "params": 30},
        ],
    },
    # Glaring Cloth, next to the Crystal cloak. That room, (40,38), exists
    # twice with the same layout id 25: in NO4 and in Scylla's overlay BO3
    # (what you enter right after the fight). Both put it on the rock ledge
    # right of the pedestal (ledge surface y=160 over x=176-255).
    # E_PERSISTENT_ITEM_DROP params index PrizeDrops[]; both overlays map the
    # extra index 40 to Glaring Cloth (MOD_EXTRA_PRIZE_INDEX in no4.h /
    # bo3.h). They share HEART_DROP_CASTLE_FLAG 0x40, so the collected flag
    # (bit 0x68, byte 0x10D of g_CastleFlags, unused by every stage) is the
    # same in both.
    "no4": {
        "bin": "disks/us/ST/NO4/NO4.BIN",
        "tables": (0x80180864, 0x80180938),
        "layout": 25,
        "id_check": ("E_PERSISTENT_ITEM_DROP", 0x0C),
        "add": [
            {"x": 216, "y": 152, "id": "E_PERSISTENT_ITEM_DROP", "slot": 17,
             "params": 40},
        ],
    },
    "bo3": {
        "dir": "src/boss/bo3",
        "bin": "disks/us/BOSS/BO3/BO3.BIN",
        "tables": (0x80180280, 0x80180354),
        "layout": 25,
        "id_check": ("E_PERSISTENT_ITEM_DROP", 0x0C),
        "add": [
            {"x": 216, "y": 152, "id": "E_PERSISTENT_ITEM_DROP", "slot": 17,
             "params": 40},
        ],
    },
    # The cut Outer Wall "Ghost" (flaming skull, enemy 0xB1, E_ID_51,
    # src/st/no1/unk_54400.c; include/game.h MOD_OUTER_WALL_GHOST), in the
    # tower, room 13 (layout 14), the only NO1 room that loads its art (gfx
    # bank 3). It hides
    # (scale 0) until the player is inside a square of half-size params & 0xF8
    # px, then fades in and chases through walls. params bits 0-2 (unused in
    # retail) = the weathers it appears in, read by the mod's check in its
    # step 0 (1 rain, 2 clear, 4 fog: g_CastleFlags[NO1_WEATHER]); each spot
    # skips one weather, so every weather has two ghosts, a different pair:
    #   A (512,776) the night sky beside the Soul of Wolf nook (top elevator
    #     stop): trips when the player steps into the nook; clear + fog
    #   B (480,2072) the sky right of the bottom elevator stop: trips on the
    #     walk to the elevator; rain + clear
    #   C (480,1600) the sky beside the middle stop / red-door hub; rain + fog
    # Rain keeps them low (B, C), fog brings them up into the open sky by the
    # relic (A, C), a clear night has the first contact and the relic (A, B).
    # Slots 36/38/40 are free in room 13; spawn ids 8-10 (unused there: its
    # enemies have 1-7) keep a killed ghost away until the room is re-entered,
    # like the tower's other enemies. Gated by MOD_LAYOUT_ID in no1.h.
    "no1": {
        "bin": "disks/us/ST/NO1/NO1.BIN",
        "tables": (0x80180618, 0x801806EC),
        "layout": 14,
        "id_check": ("E_ID_51", 0x51),
        "add": [
            {"x": 512, "y": 776, "id": "E_ID_51", "slot": 0x824, "params": 0x100 | 0x80 | 2 | 4},
            {"x": 480, "y": 2072, "id": "E_ID_51", "slot": 0x926, "params": 0x90 | 1 | 2},
            {"x": 480, "y": 1600, "id": "E_ID_51", "slot": 0xA28, "params": 0x80 | 1 | 4},
        ],
    },
    # Rainbow Mantle, Clock Tower. The Saturn (STAGE_08.PRG) adds it as
    # PrizeDrops[12] and places it in the room with the two vessels, between
    # them at (480,297) with slot 3. On PSX that room is (49,8)-(53,9),
    # layout 5, whose centre is x=640 (Saturn x maps around the centre by
    # 256/320; the vessels' y is unchanged), on the small platform at y=304.
    # PSX NZ1 has exactly 12 PrizeDrops, so index 12 is free here too; its
    # collected flag is bit 0x108 + 12 = 0x114, below DRE's 0x118. Only NZ1
    # has this room (no boss overlay carries NZ1's prize table).
    # Song Lyrics Card, Catacombs. The Saturn (STAGE_13.PRG) adds it as
    # PrizeDrops[0x15] in its layout list 60, at (80,185) next to a breakable
    # at (89,101). On PSX that room is (20,51), layout 6, whose breakable sits
    # at (71,101): Saturn x maps around the screen centre by 256/320, so the
    # card goes to (64,185), on the floor at y=192. Slot 0x10 like CAT's other
    # persistent drops.
    "cat": {
        "bin": "disks/us/ST/CAT/CAT.BIN",
        "tables": (0x80180E24, 0x80180EF8),
        "layout": 6,
        "id_check": ("E_PERSISTENT_ITEM_DROP", 0x0C),
        "add": [
            {"x": 64, "y": 185, "id": "E_PERSISTENT_ITEM_DROP", "slot": 0x10,
             "params": 0x15},
        ],
    },
    "nz1": {
        "bin": "disks/us/ST/NZ1/NZ1.BIN",
        "tables": (0x80180820, 0x801808F4),
        "layout": 5,
        "id_check": ("E_PERSISTENT_ITEM_DROP", 0x0C),
        "add": [
            {"x": 640, "y": 297, "id": "E_PERSISTENT_ITEM_DROP", "slot": 3,
             "params": 12},
        ],
    },
    # Castle Entrance shaft (2,41)-(2,44), extended as on the Saturn
    # (mods/sotn-ultimate/tools/gen_mod_rooms.py). "replace" drops the retail list, here the
    # PSX shaft-bottom pool (E_ID_18 transparent water, E_ID_2C water effect),
    # which the Saturn's longer shaft does not have. The entities are the
    # Saturn's (STAGE_01 room 17, layout list 18), x scaled by 4/5 like the
    # tiles: three candles and the red door to the loading room (3,44), placed
    # like NP3's other right-side red doors (x=248, params 0x100). The Saturn
    # candles carry breakable type 3 (params 0x3000), which PSX NP3 lacks (it
    # has types 0 and 1): they use the standard type 0 with the Saturn's drop
    # index. The Saturn puts them on the floor line (y 480/687/991 over floors
    # at 480/688/992): they are its tall candelabras (type 3), which stand on
    # the floor; on the PS1 they are NP3's type 2 (mods/sotn-ultimate/tools/gen_np3_sconce.py).
    # The Saturn's sixth warp room (2,45) (mods/sotn-ultimate/tools/gen_mod_rooms.py) uses
    # layout 6, which no WRP room uses: the warp machine with params 5 (its
    # slot in the warp cycle, src/st/wrp/warp.c) and the red door on the
    # right, as in the Entrance warp room (15,38) it copies (layout 4).
    "wrp": {
        "bin": "disks/us/ST/WRP/WRP.BIN",
        "tables": (0x8018023C, 0x80180310),
        "layout": 6,
        "replace": True,
        "id_check": [("E_WARP", 0x16), ("E_RED_DOOR", 0x05)],
        "add": [
            {"x": 128, "y": 144, "id": "E_WARP", "slot": 0x10, "params": 5},
            {"x": 248, "y": 97, "id": "E_RED_DOOR", "slot": 0x90, "params": 0x100},
        ],
    },
    # The Cursed Prison's red doors (mods/sotn-ultimate/tools/gen_prs_links.py): in the
    # Underground Caverns' room 5 (layout 6), on its bottom corridor's left
    # end, and in the Marble Gallery's room 0 (layout 1), bottom right
    # (opening right), as on the Saturn (STAGE_09 list 58, STAGE_03 list 7;
    # x 4/5). NO4 already has a layout copy, so this one is its second
    # (suffix "2": mod_layout2.h, MOD_LAYOUT_ID2).
    "no4#2": {
        "bin": "disks/us/ST/NO4/NO4.BIN",
        "tables": (0x80180864, 0x80180938),
        "layout": 6,
        "suffix": "2",
        "id_check": ("E_RED_DOOR", 0x05),
        "add": [{"x": 8, "y": 353, "id": "E_RED_DOOR", "slot": 0x90, "params": 0}],
    },
    "no0": {
        "bin": "disks/us/ST/NO0/NO0.BIN",
        "tables": (0x80180778, 0x8018084C),
        "layout": 1,
        "id_check": ("E_RED_DOOR", 0x05),
        "add": [{"x": 1528, "y": 609, "id": "E_RED_DOOR", "slot": 0x93, "params": 0x100}],
    },
    # The Entrance's first room (layout 1 of NO3 and NP3): the Power of Wolf
    # orb (top, (0x110,0xC0)) shares slot 0x10 with the trapdoor near the
    # castle door. Retail's trapdoor destroys itself while the player is
    # above it, freeing the slot; the Saturn rule (src/st/e_trapdoor.h) keeps
    # it alive (it draws the door, open or shut), so once it spawned the
    # orb's 0x0000 entry never could (create_entity.h: empty slots only).
    # "move" gives the orb slot 0x18, free in that room in both stages.
    "no3": {
        "bin": "disks/us/ST/NO3/NO3.BIN",
        "tables": (0x8018077C, 0x80180850),
        "layout": 1,
        "id_check": [],
        "add": [],
        "move": [{"x": 0x110, "y": 0xC0, "eid": 0x000B, "params": 5, "slot": 0x18}],
    },
    "np3#2": {
        "bin": "disks/us/ST/NP3/NP3.BIN",
        "tables": (0x80180728, 0x801807FC),
        "layout": 1,
        "suffix": "2",
        "id_check": [],
        "add": [],
        "move": [{"x": 0x110, "y": 0xC0, "eid": 0x000B, "params": 5, "slot": 0x18}],
    },
    # the first visit's Entrance (NO3) gets the same shaft (gen_mod_rooms.py
    # np3("no3")) and the same list (layout 2 of NO3 too; its first room's
    # copy above is mod_layout.h, so this one is mod_layout2.h). Filled in
    # below from "np3"'s entities.
    "no3#2": {
        "bin": "disks/us/ST/NO3/NO3.BIN",
        "tables": (0x8018077C, 0x80180850),
        "layout": 2,
        "suffix": "2",
        "replace": True,
    },
    "np3": {
        "bin": "disks/us/ST/NP3/NP3.BIN",
        "tables": (0x80180728, 0x801807FC),
        "layout": 2,
        "replace": True,
        "id_check": [("E_BREAKABLE", 0x01), ("E_RED_DOOR", 0x05)],
        "add": [
            # the candles are the Saturn's tall candelabras (type 2, mods/sotn-ultimate/tools/gen_np3_sconce.py)
            {"x": 70, "y": 480, "id": "E_BREAKABLE", "flags": 0xA000, "slot": 0x101, "params": 0x2000},
            {"x": 204, "y": 687, "id": "E_BREAKABLE", "flags": 0xA000, "slot": 0x202, "params": 0x200E},
            {"x": 248, "y": 865, "id": "E_RED_DOOR", "slot": 0x90, "params": 0x100},
            {"x": 70, "y": 991, "id": "E_BREAKABLE", "flags": 0xA000, "slot": 0x303, "params": 0x2000},
        ],
    },
    # The inverted castle's neighbours of the Hell Garden and the Soul
    # Prison, the mirrors of the above (Saturn RSTAGE01 list 18, RWARP list 6,
    # RSTAGE09 list 58, RSTAGE03 list 7; x 4/5).
    # Reverse Entrance shaft (61,19)-(61,22) (gen_mod_rooms.py rno3()): the
    # Saturn's list replaces RNO3's (its red door to the save room, which
    # moved behind the alcove): the red door to the loading room (60,19) at
    # the top left and three candles, the Saturn's hanging candelabras (type 2
    # of RNO3, gen_np3_sconce.py), at the ceiling lines (y 544/337/33).
    "rno3": {
        "bin": "disks/us/ST/RNO3/RNO3.BIN",
        "tables": (0x801805EC, 0x801806C0),
        "layout": 2,
        "replace": True,
        "id_check": [("E_BREAKABLE", 0x01), ("E_RED_DOOR", 0x05)],
        "add": [
            {"x": 8, "y": 97, "id": "E_RED_DOOR", "slot": 0x90, "params": 0},
            {"x": 52, "y": 337, "id": "E_BREAKABLE", "flags": 0xA000, "slot": 0x202, "params": 0x200E},
            {"x": 186, "y": 544, "id": "E_BREAKABLE", "flags": 0xA000, "slot": 0x101, "params": 0x2000},
            {"x": 186, "y": 33, "id": "E_BREAKABLE", "flags": 0xA000, "slot": 0x303, "params": 0x2000},
        ],
    },
    # the sixth inverted warp room (61,18): warp machine params 5, red door on
    # the left (as the inverted Entrance warp room (48,25), layout 4)
    "rwrp": {
        "bin": "disks/us/ST/RWRP/RWRP.BIN",
        "tables": (0x8018023C, 0x80180310),
        "layout": 6,
        "replace": True,
        "id_check": [("E_WARP", 0x16), ("E_RED_DOOR", 0x05)],
        "add": [
            {"x": 8, "y": 97, "id": "E_RED_DOOR", "slot": 0x90, "params": 0},
            {"x": 128, "y": 144, "id": "E_WARP", "slot": 0x10, "params": 5},
        ],
    },
    # the Soul Prison's red doors (gen_prs_links.py): RNO4 room 5 (layout 6)
    # at its top corridor's right end (the Saturn's (310,97)); RNO0 room 0
    # (layout 1) top left
    "rno4": {
        "bin": "disks/us/ST/RNO4/RNO4.BIN",
        "tables": (0x801807B8, 0x8018088C),
        "layout": 6,
        "id_check": ("E_RED_DOOR", 0x05),
        "add": [{"x": 248, "y": 97, "id": "E_RED_DOOR", "slot": 0x90, "params": 0x100}],
    },
    "rno0": {
        "bin": "disks/us/ST/RNO0/RNO0.BIN",
        "tables": (0x80180754, 0x80180828),
        "layout": 1,
        "id_check": ("E_RED_DOOR", 0x05),
        "add": [{"x": 8, "y": 97, "id": "E_RED_DOOR", "slot": 0x90, "params": 0}],
    },
}


def read_lists(spec):
    data = open(os.path.join(ROOT, spec["bin"]), "rb").read()

    def u32(addr):
        return struct.unpack("<I", data[addr - BASE : addr - BASE + 4])[0]

    def read(table):
        p = u32(table + 4 * spec["layout"])
        out = []
        while True:
            x, y, eid, slot, params = struct.unpack(
                "<HHHHH", data[p - BASE : p - BASE + 10])
            out.append((x, y, eid, 0, slot, params))
            p += 10
            if x == 0xFFFF:
                return out

    return read(spec["tables"][0]), read(spec["tables"][1])


def raw(e):
    # (x, y, entityId, flags, slot|spawnId<<8, params) as written in the lists
    return (e["x"], e["y"], e["id"], e.get("flags", 0), e["slot"], e["params"])


def fmt(rec):
    x, y, eid, flags, slot, params = rec
    ident = eid if isinstance(eid, str) else "0x%04X" % eid
    return "    {0x%04X, 0x%04X, %s | 0x%04X, 0x%04X, 0x%04X},\n" % (
        x, y, ident, flags, slot, params)


STAGES["no3#2"]["id_check"] = STAGES["np3"]["id_check"]
STAGES["no3#2"]["add"] = STAGES["np3"]["add"]


def write(stage, spec, xs, ys):
    folder = spec.get("dir", f"src/st/{stage}")
    sfx = spec.get("suffix", "")
    dst = os.path.join(ROOT, folder, "mod_layout%s.h" % sfx)
    with open(dst, "w") as f:
        f.write("// Generated by mods/sotn-ultimate/tools/gen_mod_layouts.py, do not edit\n")
        f.write(f"// {stage.upper()} entity layout {spec['layout']} plus the mod's entities\n")
        f.write("// clang-format off\n")
        # define checks in the stage header can silently drop out (e.g.
        # tested before stage.h defines MOD_SATURN), so insist on them
        f.write("#if !defined(MOD_LAYOUT_ID%s)\n#error the stage header must define MOD_LAYOUT_ID%s\n#endif\n"
                % (sfx, sfx))
        drops = [e for e in spec["add"] if e["id"] == "E_PERSISTENT_ITEM_DROP"]
        for e in drops:
            f.write("#if !defined(MOD_EXTRA_PRIZE_INDEX) || MOD_EXTRA_PRIZE_INDEX != %d\n"
                    "#error the stage header must define MOD_EXTRA_PRIZE_INDEX %d\n#endif\n"
                    % (e["params"], e["params"]))
        f.write("typedef char ModLayoutIdCheck%s[MOD_LAYOUT_ID%s == %d ? 1 : -1];\n"
                % (sfx, sfx, spec["layout"]))
        for k, (name, eid) in enumerate(spec["id_check"]):
            f.write("typedef char ModEntityIdCheck%s%d[%s == 0x%02X ? 1 : -1];\n" % (sfx, k, name, eid))
        f.write("const LayoutEntity g_ModLayout%sX[] = {\n" % sfx)
        f.writelines(fmt(r) for r in xs)
        f.write("};\n")
        f.write("const LayoutEntity g_ModLayout%sY[] = {\n" % sfx)
        f.writelines(fmt(r) for r in ys)
        f.write("};\n")
        f.write("// clang-format on\n")
    kept = len(xs) - 2 - len(spec["add"])
    print(f"wrote {folder}/mod_layout{sfx}.h (layout {spec['layout']}, "
          f"{kept} retail + {len(spec['add'])} added, {len(spec.get('move', []))} moved)")


for key, spec in STAGES.items():
    stage = key.split("#")[0]
    if isinstance(spec["id_check"], tuple):
        spec["id_check"] = [spec["id_check"]]
    xs, ys = read_lists(spec)
    if spec.get("replace"):
        # keep only the (-2,-2) head and the (-1,-1) tail
        xs = [r for r in xs if r[0] in (0xFFFE, 0xFFFF)]
        ys = [r for r in ys if r[1] in (0xFFFE, 0xFFFF)]
    # "move": a retail entry to another slot (x, y, entity id and params
    # must match exactly one record of each list)
    for mv in spec.get("move", []):
        for lst in (xs, ys):
            hits = [i for i, r in enumerate(lst) if (r[0], r[1], r[2], r[5]) ==
                    (mv["x"], mv["y"], mv["eid"], mv["params"])]
            assert len(hits) == 1, (stage, mv, hits)
            r = lst[hits[0]]
            assert r[4] != mv["slot"], (stage, mv)
            lst[hits[0]] = (r[0], r[1], r[2], r[3], mv["slot"], r[5])
    for new in spec["add"]:
        # the generated header asserts that the enum matches the retail id
        assert new["id"] in [n for n, _ in spec["id_check"]], (stage, new)
        assert not any(r[0] == new["x"] and r[1] == new["y"] and r[5] == new["params"]
                       for r in xs), f"{stage} already has {new}"
        rec = raw(new)
        # Both lists run from a (-2,-2) head to a (-1,-1) tail, sorted by x
        # and by y respectively: insert before the first record further
        # right/down, or before the tail.
        i = next(i for i, r in enumerate(xs)
                 if r[0] != 0xFFFE and (r[0] == 0xFFFF or r[0] > rec[0]))
        xs.insert(i, rec)
        i = next(i for i, r in enumerate(ys)
                 if r[1] != 0xFFFE and (r[1] == 0xFFFF or r[1] > rec[1]))
        ys.insert(i, rec)
    write(stage, spec, xs, ys)
