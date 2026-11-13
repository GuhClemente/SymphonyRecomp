# SOTN Ultimate (PS1 Version)

A mod of *Castlevania: Symphony of the Night* for the PlayStation (USA,
SLUS-00067) that brings the Sega Saturn version's exclusive content and the
PSP *Dracula X Chronicles* additions to the PS1, on one CD:

- the Underground Garden and the Cursed Prison, and their inverted twins,
  with the Saturn's enemies, boss, items, relics and music;
- the Saturn Maria as a playable character;
- the PSP's English dub and revised script, a Japanese dub, and the PSP
  Maria, playable and as an optional boss;
- a Balance option (Saturn or PS1 values) and many fixes.

**This repository contains no game data.** Players build the game from their
own discs of all three versions with the builder page (`builder.html`, on
the [Releases](https://github.com/renanbianchi/sotn-ultimate/releases)
page): the page applies the mod's patch to the PS1 disc and converts the
Saturn and PSP content from the other two discs on the player's PC. Nothing
is uploaded. The finished disc is checked against the release's MD5, which
the page shows.

The discs it takes:

| Disc | Image | MD5 |
|---|---|---|
| Castlevania: Symphony of the Night, PS1, USA (SLUS-00067) | Redump Track 1 `.bin` | `acbb3a2e4a8f865f363dc06df147afa2` |
| Akumajou Dracula X: Gekka no Yasoukyoku, Saturn, Japan (T-9527G, v1.400) | Redump Track 1 `.bin` | `32f037579707e192320db6e8f24ef571` |
| Castlevania: The Dracula X Chronicles, PSP, USA (ULUS-10277) | `.iso` | `9734bc89f4956324c28df897e25f1a71` |

The release's README.txt covers playing it (emulators, real hardware, the
options and the controls).

## This repository

This is a modified version of [sotn-decomp](https://github.com/Xeeynamo/sotn-decomp)
(the decompilation of Symphony of the Night), forked at commit `698aab2`
(2026-10-14) and changed from October 2026 on for this mod. The mod-only code
is in this folder, one folder per feature (see
[Where the mod lives](#0-where-the-mod-lives)); the changes to the game
itself are in upstream's `src/` and `include/`, behind `MOD_SATURN`
(`include/game.h`; `-DNO_MOD_ULTIMATE` builds the vanilla game). The
[developer notes](#developer-notes) below list every upstream file touched and
why.

### Building from source

The commands below run from the repository's root.

Requirements: Linux or WSL, the sotn-decomp toolchain (see its
[build guide](https://github.com/Xeeynamo/sotn-decomp/wiki/Build)), Python
3 with the repo's `.venv` (`make python-dependencies`, then
`.venv/bin/pip install -r mods/sotn-ultimate/tools/requirements.txt`; build with it active:
`source .venv/bin/activate`), Go, Rust (cargo) and Node 22 or later. The builder's audio module needs emsdk 4.0.10 and the FFmpeg 7.1.1
source (`mods/sotn-ultimate/tools/builder/native/build.sh`).

The extracted discs go in `disks/` (the same three discs the builder takes):
the PS1 disc as sotn-decomp expects it, the Saturn disc's files in
`disks/saturn`, and the US UMD's `PSP_GAME/USRDIR/res/ps` in `disks/psp`
(`mods/sotn-ultimate/tools/extract_psp.sh PSP.iso`). The European UMD is not used.

```sh
# the disc: build/sotn_ultimate.bin (MOD_OUT=... to put it elsewhere)
bash mods/sotn-ultimate/tools/build_disc.sh
# the release: the disc, the patch, builder.html, and the builder run on the
# three discs, whose result must be the disc above
bash mods/sotn-ultimate/tools/make_release.sh PS1_TRACK1.bin SATURN_TRACK1.bin PSP.iso out/
```

Work files (renders, caches, the compiled audio and Saturn map converter
modules) go in `build/mod-work`, or `$MOD_WORK`.

### Tests

`mods/sotn-ultimate/tools/builder/node/test_*.ts` check each part of the builder against
the Python build byte for byte (they read the extracted discs and the
current build); `build_disc.sh` runs the disc's own checks. For example:

```sh
node mods/sotn-ultimate/tools/builder/node/test_psp_sfx.ts build/sotn_ultimate.bin
node mods/sotn-ultimate/tools/builder/node/test_gdn_tile_swap.ts
node mods/sotn-ultimate/tools/builder/node/test_go_png.ts          # needs Go
```

## Developer notes

This part is for sotn-decomp developers who want to know what this fork
changes relative to upstream, and why. The fork is based on upstream commit
`698aab2`. It ports the Sega Saturn version's exclusive content (stages,
enemies, bosses, items, a playable Maria, music) and content from the PSP
*Dracula X Chronicles* (English and Japanese dubs, the revised script, the
PSP Maria) into the PS1 US build (SLUS-00067).

Almost every change carries a comment that starts with `Saturn port` (or
`Saturn port mod` in tools). The reasons given below come from those
comments. Read the comment next to the code for the details.

### 0. Where the mod lives

The mod follows upstream's `mods/<name>/` convention. Everything that is
only the mod's is in `mods/sotn-ultimate`; upstream files carry only what has
to be in them.

| Folder | What |
|---|---|
| `mods/sotn-ultimate/tools` | The generators, `build_disc.sh`, the checks, the builder (`tools/builder`). |
| `mods/sotn-ultimate/config` | The new stages' asset configs (`mod_*assets.us.yaml`), `mod_rel_syms.*`. |
| `mods/sotn-ultimate/src` | Mod-only C, one folder per feature (below). |
| `mods/sotn-ultimate/include` | The blocks moved out of `include/game.h` (`game_mod.h`: the switches, ids, flags and the expanded `PlayerStatus`; `game_mod_status.h`: the retail layout used to convert saves). |

#### The source folders

The mod is one mod, organised by feature. Each folder is one part of it and
has a switch in `include/game.h`, defined with `MOD_SATURN`. The parts are
not separate toggles: they share DRA's layout (section 2), the disc
(section 5) and the save format (section 4), and the builder makes one disc.
A switch names the part a hook belongs to: where a guard in an upstream file
wraps only one part's include, it is that part's switch.

| Folder | What | Switch |
|---|---|---|
| `garden/gdn`, `garden/rgdn` | The Underground Garden and the Hell Garden (stage sources, built by `build_gdn.sh` / `build_rgdn.sh`). | `MOD_SATURN` |
| `prison/prs`, `prison/rprs` | The Cursed Prison and the Soul Prison (`build_prs.sh` / `build_rprs.sh`). | `MOD_SATURN` |
| `enemies` | The Saturn enemies the four stages share, the Cave Troll for the Hell Garden, their collision table entries, the bestiary list. | `MOD_SATURN_ENEMIES` |
| `castle` | Edits to retail stages: the Entrance shaft (`np3`, `no3`, `rno3`), the candles and sconces, the sixth warp room, the Ghost, the horse ride. | `MOD_SATURN_CASTLE` |
| `items` | The Saturn weapons (`weapon/w_060`-`063.c`, built by `build_saturn_weapon.sh`), the capes, the spear's animations, Glaring Cloth, the third hand. | `MOD_SATURN_ITEMS` |
| `maria_saturn` | The Saturn Maria: her deltas to Richter's code, her HUD, her boss fight. | `MOD_SATURN_MARIA` |
| `maria_psp` | The PSP Maria on the PS1 (`mar2.c`, `MARIA2.BIN`, `build_mar2.sh`) and the PSP boss glue (`mod_cenp.c`, `build_cenp.sh`). | `MOD_CENP_BOSS` |
| `richter_saturn` | The kneeling Richter in the coat. | `MOD_SATURN` |
| `menu` | The Ultimate settings popup (`sysopt.c`, `build_sysopt.sh`), the Balance row, the Player Select, the sound mode. | `MOD_ULTIMATE_MENU` |
| `godspeed` | God Speed Shoes' sprint. | `MOD_GODSPEED` |
| `familiars` | The JP familiars' stats. | `MOD_SATURN` |
| `audio` | The Saturn music table, the CD pause. | `MOD_SATURN_AUDIO` |
| `disc` | The new disc files and their loaders. | `MOD_SATURN_DISC` |

The header files keep the upstream file they belong to in their name
(`<file>_mod_<topic>.h`, below).

Includes across the line are relative paths, so no build needs a new `-I`:
an upstream file names a mod header as
`#include "../../mods/sotn-ultimate/src/items/mod_holder.h"`, and a mod source
names an upstream header as `#include "../../../../../src/st/e_misc.h"`.
Two exceptions:

- The new stages are compiled (`build_gdn.sh` etc.) with `-Isrc/st/wrp` after
  their own folder: the shared stage headers (`src/st/*.h`) include
  `"../x.h"`, which resolves from a stage folder. WRP is the stage these are
  compiled like.
- `mod_cenp.c` is compiled in `build_cenp.sh`'s mirror tree
  (`build/us/mod/cenp/st`), where `../cen/cen.h` forwards to CEN's own, so it
  keeps `"../cen/..."` includes (it lives in `maria_psp`).

Generated files follow their sources: `mods/sotn-ultimate/src/.gitignore`
has the asset tool's `gen/` rules (as `src/.gitignore`) and the mod's
generated headers there.

#### Mod blocks in upstream files

A mod block that is whole at file scope (new functions, tables,
declarations; balanced `#if`/`#endif`) is moved to a header in its feature's
folder and included at the same spot, e.g. `src/st/sel/2C048.c` has

```c
#if defined(MOD_ULTIMATE_MENU)
#include "../../../mods/sotn-ultimate/src/menu/2C048_mod_player_select.h"
#endif
```

The header is named `<file>_mod_<topic>.h`. The `#if` stays in the upstream
file, and so does an `#else` branch (the retail definitions, e.g.
`HAND_ITEMS_CAP` in `game.h`). The preprocessed source is unchanged, so are
the binaries. 35 blocks (about 3,200 lines) moved this way.

Edits inside retail functions stay inline, under their switch. Turning them
into calls to mod functions would cost DRA bytes the layout does not have
(`MOD_DRA_PAD` is 4: section 2.2), and the overlays' sizes are as tight. The
`FntPrint` stub in `src/dra/dra.h` (`MOD_DRA_STRIP`) also stays, because it
changes how upstream code compiles.

Tools that parse an upstream file (`include/game.h` switches, `src/dra/cd.c`,
`src/st/collision.h`, `src/st/sel/2C048.c`) read it with
`mod_work.read_src()`, which puts the moved headers back in place.

### 1. Build switches

The main switches are in `include/game.h`, near `NUM_AVAIL_RELICS`.

| Switch | Where | Meaning |
|---|---|---|
| `MOD_SATURN` | `include/game.h` | Every Saturn-only feature. Defined for `VERSION_US` unless `NO_MOD_ULTIMATE` is defined. |
| `MOD_GODSPEED` | `include/game.h` | Always defined together with `MOD_SATURN`. The God Speed Shoes code: relic 30, its sprint, and the 31-relic menu. |
| `NO_MOD_ULTIMATE` | command line | `-DNO_MOD_ULTIMATE` turns the whole mod off: the vanilla game. |
| `MOD_GODSPEED_TEST_START` | `include/game.h` (commented out) | Test builds only: Alucard starts with the shoes. |
| `MOD_ULTIMATE_DEBUG` | `include/game.h` (commented out) | Test builds only: an on-screen readout of the player state. It also keeps `FntPrint` and `FntFlush`. |
| `MOD_SATURN_STAGE_TEST` | `include/game.h` (off) | Test aids: the Library card warps along the dialogue scenes, plus extra teleports `MOD_TP_DLG_*`. |
| `MOD_SATURN_ITEMS_TEST` | `include/game.h` (off) | Test aid: loading a save adds the new items (`src/save_mgr.h` `ModItemsTestGift`). |
| `MOD_OUTER_WALL_GHOST` | `include/game.h` (off) | Puts the cut Outer Wall "Ghost" (NO1 entity 0x51) in NO1 room 13, with a name and a bestiary entry. Off leaves NO1 exactly as before. |
| `MOD_HAS_RGDN` | `include/game.h` | The Hell Garden stage exists on the disc. |
| `MOD_WIGHT_PS1_SPIRIT`, `MOD_WIGHT_PS1_GLOW` | `include/game.h` | PS1-style effects for the Soul Prison Wight (`mods/sotn-ultimate/src/enemies/e_mod_wight.h`). |
| `MOD_GDN_PS1_TILES`, `MOD_PRS_PS1_TILES` | `include/game.h` | Asset tools only. They redraw the Garden and Prison stairs and stonework from PS1 tiles (`mods/sotn-ultimate/tools/gdn_tile_swap.py`, the cells in `mods/sotn-ultimate/tools/gdn_ps1_tiles`, `prs_ps1_tiles`). |
| `MOD_SATURN_MARIA` | `include/game.h` | Shows Maria on the Saturn-style character select and makes her playable. |
| `MOD_SKIP_ANY_CUTSCENE` | `include/game.h` | START skips any scene from a fresh game. DRA starts with every "scene seen" bit set (`MOD_ALL_SCENES_SEEN`). Those bits are read only by the skip checks. |
| `MOD_DRA_STRIP` | `src/dra/dra.h` | US DRA code that can never run is left out (section 2.4). |
| `MOD_DRA_CONFIG` | `src/dra/config_us.c` | Only DRA's copy of `src/config_us.h` gets the modded tables. SEL includes the same header and must stay byte-identical to retail. |
| `MOD_SAVE_DRA` | `src/dra/save_mgr.c` | Marks DRA's copy of `src/save_mgr.h`. DRA writes saves; SEL converts them on load. |
| `MOD_XA_MUSIC_TABLE` | `src/dra/sound.c` | Music configs past the retail XA table (section 6). |
| `MOD_MARIA_SAT` | `mods/sotn-ultimate/tools/build_mar.sh` (`-D`) | Builds `src/ric` a second time as the Saturn Maria. |
| `MOD_CENP`, `MOD_CENP_BOSS` | `mods/sotn-ultimate/tools/build_cenp.sh` (`-D`) | Builds CEN with the PSP Maria boss fight (`BIN/CENP.BIN`). |
| `MOD_MARIA_BOSS` | `src/st/cen/cen.h` | The Saturn Maria boss fight in CEN. |
| Per stage: `MOD_LAYOUT_ID`, `MOD_EXTRA_PRIZE_INDEX`, `MOD_SATURN_CANDLES`, `MOD_NEW_HAND_DROPS`, `MOD_SCONCE_TABLES`, `MOD_HORSE_TABLES`, `MOD_BLOOD_ONLY`, `MOD_PSP_SCRIPT` | stage headers | Turn on the shared helpers in `src/st/*.h` for one overlay. |

#### Runtime choices (not build switches)

The character select's choices are stored in the unused byte
`g_Status.saveName[11]` (`MOD_STATUS_FLAGS`). saveName is 12 bytes, but only
bytes 0-9 are ever written or read, and new-game setup does not clear the
byte. So the choices survive save and load without a change to the save
layout.

| Bit | Macro | Meaning |
|---|---|---|
| 0x01 | `MOD_FLAG_BOSS_RIC_SATURN` | Alucard game: boss Richter in his Saturn look (`BIN/BO6RIC2.BIN`). |
| 0x02 | `MOD_FLAG_MARIA_PSP` | Maria game: the PSP Maria (`BIN/MARIA2.BIN`). |
| 0x04 | `MOD_FLAG_MARIA_BOSS_PSP` | Alucard game: the PSP Maria boss fight (`BIN/CENP.BIN`). |
| 0x08 | `MOD_FLAG_PS1_DUB` | PS1 dub and script. Clear means the PSP dub, so retail saves (0) get the PSP dub. |
| 0x10 | `MOD_FLAG_MARIA_SUB2` | PSP Maria's two-subweapon layout is in use. |
| 0x20 | `MOD_FLAG_JP_DUB` | With 0x08 clear: Japanese voices and the PSP English script. |
| 0x40 | `MOD_FLAG_PS1_BALANCE` | PS1 balance. Clear means the Saturn balance. |

Other stored state:

- `g_Status.saveName[10]` (`MOD_HOLDER_ITEM`): the item in the Saturn's third
  hand ("Holder").
- `g_Status.D_80097C40`: Richter's alternate costume (`MOD_RIC_COSTUME2_MAGIC`)
  and the PSP Maria's front subweapon. Only Alucard (the Muramasa counter)
  and Maria use this field.

### 2. DRA layout invariants

#### 2.1 What must not move

`mods/sotn-ultimate/tools/check_dra_layout.sh` reads `build/us/dra.map` and fails unless
three boundaries match retail:

| Boundary | Retail | Why it is fixed |
|---|---|---|
| `sound_data.c` `.data` | `0x800BD1E0` | Stages use `g_SfxScripts`, inside sound_data's data, at a fixed address. |
| `dra_DATA_END` | `0x800DB3B4` | The end of DRA's data section. |
| `dra_BSS_START` | `0x801362AC` | `main.exe` and the sound banks use fixed addresses from the BSS start on. |

Overlays never call DRA code directly; they go through `g_api`, a data table
(`src/dra/dra.h`). DRA's code may therefore move. Its data and BSS may not.

#### 2.2 The two pads

| Pad | File | Section | Keeps in place |
|---|---|---|---|
| `MOD_DATA_PAD` (`g_ModDataPad`, currently 64 bytes) | value in `src/dra/config_us.c`, array in `src/config_us.h` | DRA data, before sound_data | `sound_data` (so `g_SfxScripts`) |
| `MOD_DRA_PAD` (`g_ModDraPad`, currently 4 bytes) | `src/dra/42398.c` | rodata | `dra_BSS_START` |

Each pad makes up the difference between the bytes the mod adds and the bytes
it removes. When a number is off, `check_dra_layout.sh` prints the value to
use. `mods/sotn-ultimate/tools/build_disc.sh` builds once, sets `MOD_DATA_PAD` first (it
also moves everything after it), rebuilds, then sets `MOD_DRA_PAD`. It
refuses to go on if the checker cannot run.

How the mod frees data bytes (`src/config_us.h`, under `MOD_DRA_CONFIG`):
`g_SpellDefs`, `g_RelicDefs` and `g_ExpNext` become `const`. They are
read-only, so they move to rodata. Their data bytes then hold the Saturn's new
enemy and item records, and `MOD_DATA_PAD` keeps sound_data where it was.

#### 2.3 BSS kept at its retail layout

- `src/dra/42398.c`: the removed video recorder's `g_DebugRecordVideoFid`
  slot holds `g_GodspeedTapTimer` (the sprint double-tap timer).
- `src/dra/47BB8.c`: `g_SimFile` and `D_80136450` stay declared (unused,
  non-static) so the BSS keeps its retail layout. The two VSync line-upload
  buffers `D_80136460` / `D_80136C60` (4 KB) are only started by the removed
  sim-file loader. They become `ModDubState g_ModDub`
  (`include/game.h`): the redirected XA config, the JP dub tables, the dub
  menu text, and the `storeSave` pointer SEL saves through.
- Write-only globals that `MOD_DRA_STRIP` turns into locals (section 2.4) stay
  declared as globals, so their BSS slots stay. The local of the same name
  only saves the address loads.

#### 2.4 Every `MOD_DRA_STRIP` removal

`src/dra/dra.h` defines `MOD_DRA_STRIP` for `MOD_SATURN && VERSION_US`. The
reason it gives: code the US disc build can never run is dropped to make room
for the mod, and each removal states why it is dead. The same block defines
`g_UseDisk` as `1`. `main.c` sets `g_UseDisk = 1` whenever DRA.BIN came from
the CD (0 and 2 are the dev-kit "sim:" PC loader), and DRA never writes it,
so every PC-file branch folds away.

`FntPrint` (in `dra.h`, above the `MOD_DRA_STRIP` define) uses that define's
own condition, `MOD_SATURN && VERSION_US`, and the palette viewer block in
`42398.c` is gated by `MOD_SATURN`. They are listed here too.

**Dev-kit / PC-loader paths (dead because `g_UseDisk` is 1 on a disc boot)**

| File | Removed | Stated reason |
|---|---|---|
| `src/dra/47BB8.c` | `SimFile` tables, `LoadFileSimToMem`, `LoadFileSim` (now a stub returning -1), `func_800E7D08`, the VSync line upload in `VSyncHandler` (`!MOD_SATURN`) | The "sim:" loader only runs when DRA itself was loaded from a PC. |
| `src/dra/47BB8.c` | `LoadStageTileset` | Only the removed sim loader called it. |
| `src/dra/game_handlers.c` | `func_800E6250` | PC familiar loader; only the `g_UseDisk == 0` branches called it. |
| `src/dra/demo.c` | demo key reader from a PC file | The PC (sim:) reader; `g_UseDisk` is 1. |
| `src/dra/demo.c` | `DemoSaveFile` | Only the removed demo recorder saved, and to a PC file. |
| `src/dra/lba_stage.c` | the gfx, overlay and name strings of `g_StagesLba` (`LBA_NAMES`) | Only read by the sim loader and by `FntPrint`, which prints nothing on US. The name field now holds the stage's PSP-dub sound bank sector (section 5). |

**HD / PC debug features (dead on US)**

| File | Removed | Stated reason |
|---|---|---|
| `src/dra/dra.h` | `FntPrint` becomes a no-op (`MOD_SATURN && VERSION_US`, unless `MOD_ULTIMATE_DEBUG`) | US never calls `FntFlush`, so DRA's `FntPrint` lines never reach the screen. |
| `src/dra/42398.c` | `DebugShowWaitInfo`; `DebugInputWait` becomes empty | On US it only shows a message when a compressed graphics bank overflows (a data error that never happens) and in the HD/PC capture tools. No overlay uses `g_api.DebugInputWait`. |
| `src/dra/42398.c` | `DebugCaptureScreen`, `DebugCaptureVideo` | Dev-kit capture tools, only called on HD/PC. |
| `src/dra/42398.c` | palette viewer, colour editor (`D_800A0144` to `DebugEditColorChannel`), `PrintGpuInfo`, `PrintHBlankInfo` (`MOD_SATURN`) | Only called from the HD/PC parts of `DebugUpdate` and `MainGame`. |
| `src/dra/42398.c` | `MainGame`: the `g_GpuMaxUsage` resets and peak updates | Only `PrintGpuInfo` (HD) reads the peaks. |
| `src/dra/42398.c` | `MainGame`: the debug-menu variable resets (`D_801362B4`, `g_DebugCurPal`, hitbox view mode, colour channel, and others) | Only the HD/PC debug menus read them. |
| `src/dra/42398.c`, `src/dra/66590.c` | the `DrawEntitiesHitbox` call and the function | Only HD's `DebugUpdate` sets `g_DebugHitboxViewMode`; it is always 0 on US. |
| `src/dra/4A538.c` | two palette-viewer checks (`g_DebugMode == 10` / `DEBUG_PALETTE_VIEWER`) | The palette viewer is in the HD/PC debug menu only. |
| `src/dra/692E8.c` | the debug fly-mode block | US `MainGame` sets `g_DebugPlayer = 0` for good. |
| `src/dra/71830.c` | `func_80111938`, `func_8011197C`, `func_801119C4` | The debug fly mode; `g_DebugPlayer` is always 0 on US. |
| `src/dra/game_handlers.c` | the debug start under `g_DebugPlayer != 0` | US `MainGame` sets it to 0 and nothing sets it again (only HD and PSP do). |
| `src/dra/demo.c` | `DemoInit` case 1 (start recording), the `Demo_Recording` case of the demo update | Recording needs `DemoInit(1)`, which only the debug start calls. |

**Functions with no caller in the US build**

| File | Removed | Stated reason |
|---|---|---|
| `src/dra/42398.c` | `SetTitleDisplayBuffer` | Static and never referenced. |
| `src/dra/42398.c` | `func_800E38CC` | No caller in the US build. |
| `src/dra/menu.c` | `func_800F9D40` | JP text; only HD calls it. |
| `src/dra/menu.c` | `func_800FD39C` | No caller in the US build. |
| `src/dra/66590.c` | `func_801072FC` | No caller anywhere. |
| `src/dra/5F60C.c` | `func_800FF708` | Only a non-US branch calls it. |
| `src/dra/5087C.c` | `func_800F1424` | Only HD calls it (upstream already notes it is stripped on PSP). |
| `src/dra/cd.c` | `func_801083F0` | No caller in the US build. |
| `src/dra/alu_anim.c` | `func_8010DA2C` | No caller in the US build. |
| `src/dra/sound.c` | `func_80131F38`, `GetCdVolume` | No caller in the US build. |
| `src/save_mgr.h` (DRA copy) | `IsMemcardBlockUsed`, `MemcardReadFile`, `MemcardEraseFile` | DRA never reads or erases a memory card file. SEL does, and SEL does not define `MOD_DRA_STRIP`, so it keeps them. |

**Variables that are written but never read**

| File | Change | Stated reason |
|---|---|---|
| `src/dra/62DEC.c` | the `D_801379C0` reset is dropped | Written, never read. |
| `src/dra/cd.c` | `D_80137F9C` becomes a local | Written, never read. |
| `src/dra/cd.c` | `D_80137FA8` becomes a local | Read only in that function. |
| `src/dra/cd.c` | the `D_80137FAC = 0` write is dropped | Written, never read. |
| `src/dra/sound.c` | `g_CdCommandStatus` becomes a local in `DoCdCommand` | Read only there. |
| `src/dra/sound.c` | the `D_8013B690` writes (three places) are dropped | Written, never read. |
| `src/dra/sound.c` | the `D_80139A74`, `D_8013B69C`, `D_80138FBC`, `D_80139800`, `D_80138F80`, `D_8013B618` resets are dropped | Written there only, never read. |
| `src/dra/sound.c` | the `g_SeqVolume1` / `g_SeqVolume2` writes are dropped; `SsSeqSetVol` gets the value directly | Written, never read elsewhere. |
| `src/dra/sound.c` | `D_8013B65C` is folded into the comparison | Read only there. |

To check a removal, grep for `MOD_DRA_STRIP` in `src/dra` and `src/save_mgr.h`.

#### 2.5 Code placed outside DRA

DRA has no room for some features, so they live elsewhere:

- **The System screen "Ultimate settings" popup.** The code is in
  `mods/sotn-ultimate/src/menu/sysopt.c`, built by `mods/sotn-ultimate/tools/build_sysopt.sh`. It is
  appended after Alucard's sprites as `BIN/ARC_F2.BIN` (a copy of
  `ARC_F.BIN` plus the code), loaded up to `0x80170000`. That RAM is only
  used this way in Alucard games. DRA has three stubs in its *data* section
  (`src/config_us.h`; the data pad had room, the code had none). Each stub
  checks a magic word and then jumps to the popup code or to the retail
  function. maspsx adds nops around branches, so most of the stubs are raw
  `.word`s. `build_sysopt.sh` checks them.
- **The JP dub tables.** They are SEL rodata. SEL copies them into
  `g_ModDub.jp` every frame its menu runs, and every way into a game passes
  through there.

### 3. g_Status layout shift (HAND_ITEMS_CAP)

`include/game.h`: under `MOD_SATURN`, `PlayerStatus` holds the Saturn's 7 new
hand items and 3 new body items.

- `HAND_ITEMS_CAP` is 176 (retail 169). `BODY_ITEMS_CAP` is 93 (retail 90).
  `NUM_HAND_ITEMS` and `NUM_BODY_ITEMS` keep their retail values, because
  stage data encodes body items as `NUM_HAND_ITEMS + id` (pickups, drops,
  shops).
- The four arrays grow in place, so every field from `saveName` on moves by
  `+0x14`.
- To keep `sizeof(PlayerStatus) == 0x334` (`D_80097C98` follows it), the US-unused
  stats of the two JP-only familiars are dropped
  (`FAMILIARS_STORED = FAM_STATS_YOUSEI`). Four bytes are left over as
  `modPadding`. Compile-time checks (`PlayerStatusSizeCheck`,
  `ModLayoutCheck`) pin the size and the field offsets.
- The JP familiars' stats move to castle flag bytes (`MOD_CF_FAMILIARS`,
  `src/dra/5D5BC.c`).
- New hand item ids are 169-175, as on the Saturn. The 48 nameless
  special-attack records move up by `MOD_SPECIAL_SHIFT`, through
  `MOD_SPECIAL()`. `g_EquipDefs` has `MOD_NUM_EQUIP_DEFS` (228) records,
  from `mods/sotn-ultimate/tools/gen_saturn_items.py`.
- Drops and pickups cannot use 169 and up for hand items (those ids are body
  items). New hand items use combined ids from `MOD_DROP_HAND_BASE`.

What follows from the shift:

| Problem | Fix |
|---|---|
| Functions still built from splat asm hard-code `g_Status + 0xNNN`. | `mods/sotn-ultimate/tools/patch_status_asm.py` rewrites those offsets in `asm/us` for the new layout. It works from pristine copies in `$MOD_WORK/retail-asm`, so running it twice never shifts an offset twice. It fails if an offset points into a dropped JP familiar slot. Run it after `make extract`. |
| Retail overlays the mod does not otherwise change must differ only by that re-offset. | `mods/sotn-ultimate/tools/check_status_shift.py`: every built file that differs from retail must keep its size, and each differing word must be the same MIPS instruction with only its 16-bit immediate changed. Files the mod changes on purpose are listed in `INTENDED` (and `MODDED_WEAPONS` for `WEAPON0.BIN`). |
| The decomp only builds `WEAPON0.BIN` (right hand). A retail `WEAPON1.BIN` copy of a weapon that reads moved `g_Status` fields (37, 44, 52) or that the mod changes runs stale code. | `mods/sotn-ultimate/tools/build_weapon1.sh ID...` compiles those weapons for hand 1 (`-DWEAPON1`, `g_HandId = 1`, BSS 0x3000 higher) and splices them into a copy of the retail file as `build/us/WEAPON1.BIN`. `mods/sotn-ultimate/tools/check_weapon_hands.py` fails if a slot changed in WEAPON0 was not rebuilt in WEAPON1. |
| The Saturn's new weapons 60-63 do not fit `WEAPON0/1.BIN`. Those files can't grow, because the files after them are at positions DRA hard-codes. | `BIN/WEAPON2.BIN`, one 0x7000-byte slot per weapon and hand (`mods/sotn-ultimate/tools/build_saturn_weapons.py`, `MOD_FIRST_WEAPON2`, `src/dra/cd.c`). |

### 4. Save format

Defined in `src/save_mgr.h` and `include/game.h`.

- Current saves carry `MOD_SAVE_MAGIC` "SAT4" in `modPadding`. In a retail US
  save those bytes are the unused JP familiar's stats, always 0.
- "SAT4" saves use the **retail** layout (`PlayerStatusRetail`), so the
  unmodded game loads them too. There, the mod's own items, rooms and choices
  are simply missing.
- What the retail layout cannot hold goes in `ModSaveExtra`, in the two
  JP-only familiars' stat slots just before the magic. The US game never
  reads those slots. `ModSaveExtra` holds: the counts of the new items; any
  equipped new item (the retail layout gets the slot's empty item instead);
  and the save room of a mod-only area (the retail layout gets the Castle
  Entrance's, or the Reverse Entrance's). The JP familiar cards'
  relic bits are stored there too (`MOD_EXTRA_CARDS`), because the US game
  would list those familiars.
- `ModStoreRetailStatus` (DRA, `MOD_SAVE_DRA`) writes saves. It keeps each
  retail body position holding an item of its own category, because the
  equip menu lists positions by category.
- SEL converts on load: retail saves (`ModConvertRetailStatus`), "SAT1"
  (92 body slots), "SAT2" (God Speed Shoes stored in `RELIC_JP_0`),
  "SAT3" (the expanded layout), "SAT4".
- SEL's own saves (the ending's, the file menu's) go through DRA's
  `StoreSaveData`, via `g_ModDub.storeSave`, so they are written in the same
  format.
- The Castle Entrance save room moved from (3,42) to (3,43), in both Entrance
  stages (NO3, the first visit, and NP3), and the Reverse Entrance's from
  (60,21) to (60,20). Loading moves old saves made there.
- `src/st/sel/32D6C.c`: the file list shows the real save place of a
  mod-area save.

#### Castle flag bytes used

| Index | Macro | Use |
|---|---|---|
| `0x2F0` | `MOD_CF_GODSPEED` | God Speed Shoes' `RELIC_FLAG_*` bits. The shoes are relic 30, one past `g_Status.relics[]`; code that may see relic 30 goes through `MOD_RELIC_FLAGS()`. |
| `0x2F1`-`0x2F8` | `MOD_CF_FAMILIARS` | The JP familiars' stats (Sprite, then Nose Devil): exp and summon count as little-endian u16. Level is always `exp / 100 + 1`, so it is not stored. |
| `0xF9` | `MOD_PRS_SWITCH_FLAG` (`mods/sotn-ultimate/src/prison/prs/e_mod_switch.h`) | The Prison switch, as the Saturn's `g_CastleFlags[0xF9]`, unused on PS1. |
| `0x167` bit 7 | `MOD_GDN_BOSS_FLAG` 831 (`mods/sotn-ultimate/src/garden/gdn/e_mod_boss.h`) | Garden boss defeated (collectible bit numbering, `0x100 + bit / 8`). |
| `WRP_UNLOCKS` / `RWRP_UNLOCKS` bit 5 | `src/st/wrp/warp.c`, `src/st/rwrp/warp.c` | The sixth warp room, next to the Garden or Hell Garden. |

`include/game.h` states that 0x2F0 and up are bytes no overlay uses: static
and indexed uses stay at or below 0x1E0, except the save and new-game loops.
New prize items use extra prize indices with their own collected flags
(`MOD_EXTRA_PRIZE_INDEX` in `src/st/cat/cat.h`, `nz1/nz1.h`, `no4/no4.h`,
`boss/bo3/bo3.h`).

### 5. Disc layout

#### New files

`config/disk.us.lba` appends 18 files after the last stage directory and
before `XA`:

| File | Content | Made by |
|---|---|---|
| `BIN/WEAPON2.BIN` | Saturn weapons 60-63 | `build_saturn_weapons.py` |
| `BIN/RIC2.BIN` | Richter overlay with the Saturn costume sheet | `build_ric2.py` |
| `BIN/BO6RIC2.BIN` | BO6 with boss Richter in that coat | `build_bo6ric2.py` |
| `BIN/SELRIC2.BIN` | SEL with that Richter in the endings | `build_selric2.py` |
| `BIN/MARIA.BIN` | Saturn Maria player overlay | `build_mar.sh` |
| `BIN/MARIA_SD.BIN`, `MARIA_SD_PSP.BIN`, `MARIA_SD_PS1.BIN` | her voice banks (JP Saturn, PSP English, PS1-dub copy) | `gen_mar_sounds.py`, `build_disc.sh` |
| `BIN/MARIA2.BIN`, `MARIA2_SD.BIN` | PSP Maria overlay and voice | `build_mar2.sh` |
| `BIN/CENP.BIN` | CEN with the PSP Maria fight | `build_cenp.sh` |
| `BIN/ALK_PSP.BIN`, `RIH_PSP.BIN` | PSP-dub player grunts | `gen_psp_grunts.py` |
| `BIN/SFX_PSP.BIN` | PSP-dub stage and familiar banks | `gen_psp_sfx.py` |
| `BIN/MONSTER2.BIN` | Saturn-only bestiary pictures | `build_monster2.py` / builder |
| `BIN/JP_SD.BIN` | JP-dub banks | `gen_jp_sd.py` |
| `BIN/ARC_F2.BIN` | Alucard sprites plus the System popup code | `build_sysopt.sh` |
| `BIN/NO3RIDE.BIN` | NO3 bank for the horse ride | `gen_no3_sounds.py` |

The new stages use the four unused `TE1`-`TE4` stage slots, so they need no
new files (section 7).

Why the files go there: the literal sector numbers in `src/dra/cd.c` and SEL
stay right only while every file before them keeps its place
(`mods/sotn-ultimate/tools/check_cd_lba.py`). The retail `ARC_F.BIN` stays on the disc,
unchanged and unused.

#### Sector numbers

- `tools/sotn-assets/mods/lba.go` adds an `OFF_BIN_*` (and `LEN_BIN_*` where
  needed) entry per new file, plus `OFF_BIN_RIC`. `make disk` writes them into
  `include/lba_us.h`.
- `include/lba_us.h` is regenerated. Most of its diff is shifted stage
  sectors (the modded stage files grew) and a new `OFF_SD_XA_STR1`.
- `src/dra/cd.c` defines each `OFF_BIN_*` it uses as 0 when it is missing.
  That happens on the first compile of `make disk`, before `lba_us.h` has the
  file, and the branch that uses it is left out. `build_disc.sh` starts again
  once whenever `lba_us.h` changed during the build.
- `tools/sotn-disk/iso9660/make.go`: the volume size no longer adds the
  retail 150-sector gap and CD-DA track 2. The image has one track.
- `mods/sotn-ultimate/tools/fix_sector_headers.py`: `make disk` copies the XA sectors raw,
  with their retail MSF headers. Once a file before them grows, the headers
  point at the wrong place, so this rewrites them to the sectors' real
  positions.

#### Load redirections in `src/dra/cd.c`

| Condition | Loads instead |
|---|---|
| Richter, costume 2 (`MOD_RIC_COSTUME2()`) | `RIC2.BIN` for `CdFile_RichterPrg`; `ModRicCostumeApply` patches palettes and VRAM after each load |
| `PLAYER_MARIA` | `MARIA.BIN` (Saturn) or `MARIA2.BIN` (`MOD_MARIA_PSP()`), and her sound banks in place of `SD_RIH` / `SD_MAR` |
| Stage BO6 with `MOD_BOSS_RIC_SATURN()` | `BO6RIC2.BIN` |
| Endings with the coat | `SELRIC2.BIN` |
| Stage CEN with `MOD_MARIA_BOSS_PSP()` | `CENP.BIN`; CEN's sound bank depends on the dub |
| PSP dub on | the stage bank from `SFX_PSP.BIN` (sector kept in the `g_StagesLba` name field), familiar banks (`g_ModPspServantSfx`), `ALK_PSP.BIN` / `RIH_PSP.BIN` grunts |
| JP dub on | the matching part of `JP_SD.BIN` (`MOD_JP_SD_*`, `ModJpSfx`) |
| Richter or Maria in NO3 | `NO3RIDE.BIN` |
| Alucard's sprites | `ARC_F2.BIN` (`g_LbaBinAlucard`) |
| weaponId 60 and up | `WEAPON2.BIN` |
| bestiary bit 146 and up | `MONSTER2.BIN` (`MOD_FIRST_MONSTER2`) |

#### Stage table (`src/dra/lba_stage.c`)

- Slot `0x10` (`STAGE_NO0_ALT`, which nothing references) holds the Cursed
  Prison, using the TE2 files.
- Slot `0x11` holds the Underground Garden, using the TE1 files.
- Slot `0x30` holds the Soul Prison (TE4 files) and `0x31` the Hell Garden
  (TE3 files). Those two were unused fillers. Each inverted stage must be its
  normal stage `| 0x20` because the teleport table serves both castles;
  `include/game.h` enforces this with `#error`.
- Each new row has its own `MU_MOD_*` music id and its own bank in its TE
  slot.
- New teleports `MOD_TP_*` (from 0x83) are appended after the 131 retail
  ones in `D_800A245C` (`src/dra/5087C.c`, which asserts the count).

#### Build-system changes

- `tools/builds/gen.py`: links `mods/sotn-ultimate/config/mod_rel_syms.us.<ovl>.txt` after
  splat's `undefined_syms_auto` when the file exists. splat writes addresses
  inside a symbol (for example `g_Dialogue + 2`) as absolute values, and they
  go stale when the mod moves an overlay's BSS. The rel-syms file reassigns
  them relative to their symbol; the last assignment wins.
  `mods/sotn-ultimate/tools/check_abs_syms.py` finds and writes them. The files exist today
  for BO2, BO5, BO6, RBO2 and RBO6.
- `tools/dirt_patcher.py`: skipped when `SOTN_MOD_NO_DIRT` is set (and
  `build_disc.sh` sets it). The dirt bytes are retail alignment junk written
  at fixed file offsets; on the mod's moved layout they land on live data.
  `mods/sotn-ultimate/tools/check_no_dirt.sh` verifies that `DRA.BIN` is exactly the linked
  ELF.

### 6. Sound and XA

#### XA_STR1 repack (`mods/sotn-ultimate/tools/repack_xa.py`)

Inputs and outputs:

- Input: `build/us/XA_STR1.unpacked`, which is the retail stream plus the
  Saturn tracks appended by `gen_saturn_music.py` (encoded with
  `mods/sotn-ultimate/tools/satxa.c`).
- Output: `build/us/XA_STR1.`, `src/dra/gen/us/xa_music_config.h`, and the
  table in `src/dra/mod_xa_music.h`, all rewritten in place. The table keeps
  its entry count, because `g_SfxScripts` sits right after the retail table.

The repack, in order:

1. Extracts each config's run of sectors and merges runs that end on the same
   sector.
2. Drops the retail filler sectors.
3. Points the JP-only lines at one short silent stream (unless `--keep-all`).
4. Stores dual-mono streams as 37.8 kHz mono (unless `--no-mono`).
5. Packs everything first-fit-decreasing in a 16-slot interleave, each stream
   on its own file/channel.
6. Re-extracts every config and compares it with the original. Any mismatch
   fails the build.

Dub tables:

- `--dub`: each voice id with a PSP English recording gets a second mono
  stream, packed in one region after the others. `src/dra/mod_psp_xa.h`
  stores a bitmap over `MOD_PSP_XA_FIRST`..`MOD_PSP_XA_LAST` plus, per set
  bit, a packed sector offset, length and file. `ModXaConfig`
  (`src/dra/sound.c`) plays that stream while `MOD_PSP_DUB_ON()`.
- `--dub-jp`: a third version, packed after the PSP region. Its table
  (`src/st/sel/mod_jp_xa.h`) is SEL rodata, copied into `g_ModDub.jp`.
- Lines with no PS1 slot (for example `MOD_VO_DID_YOU_NOT`) get new mod
  configs.
- The voice lines come from `mods/sotn-ultimate/tools/psp_dub.py` (PSP `.at3`, decoded and
  encoded with the builder's own WebAssembly audio module, so the builder
  page remakes them byte for byte).
- The script text comes from `mods/sotn-ultimate/tools/psp_script.py`. It writes per-overlay
  copies in `mod_psp_script.h`, which `SetCutsceneScript` swaps in while the
  dub is on. The copies jump inside themselves, so `--apply` fills them in
  the built overlay.

Music-player changes (`src/dra/sound.c`):

- Saturn-only music uses config ids past the retail table (`MOD_XA_MUSIC_TABLE`).
- Pausing as Richter or Maria pauses the XA stream (`CdlPause`) and resumes
  it, as on the Saturn (`MOD_CD_PAUSED`).
- No XA end or loop command is sent while a file loads (`g_CdStep != CdStep_None`),
  because it would take the drive from the read. The dub voices can outlast
  scenes timed for the PS1 lines.

#### Sound banks and id slot reuse

A stage bank (vab 3) is reached through `g_SfxData`. Each sound id names a
(program, tone, note) slot, and those slots belong to whichever bank is
loaded. So:

- **New stages** (`gen_gdn_sounds.py`, `gen_prs_sounds.py`,
  `gen_rgdn_sounds.py`, `gen_rprs_sounds.py`). The Saturn sounds are rendered
  to WAV by `mods/sotn-ultimate/tools/saturn_sfx.py`, a small SCSP-style synth. Saturn-only
  sounds take ids whose slots are free in that bank (`MOD_SFX_*` in the
  generated `mod_*_sfx.h`). Sounds the PS1 code already plays go in the slots
  that code uses. Each bank must fit between the stage bank address and the
  reverb work area, so sample rates are capped until it fits.
- **The Saturn Maria** (`gen_mar_sounds.py`). Her sounds take Richter's and
  Alucard's voice ids, laid out as `SD_RIH` / `SD_MAR` (`MOD_MAR_SD_SECTORS`).
  `RIC_OR_MAR(r, m)` in `src/ric/ric.h` picks her id where Richter's code
  plays a different sound for her.
- **The PSP Maria** (`gen_mar2_sounds.py`). Her voice uses Richter's voice
  ids' vab 1 slots.
- **The PSP dub** (`gen_psp_sfx.py`). It swaps only the samples of tones the
  PS1 plays whose recording differs from the PSP's. Programs, tones, ADSR and
  volumes stay the same, and the VB keeps its retail length.
- **The JP dub** (`gen_jp_sd.py`). Each part keeps the layout of the file it
  stands in for, so `cd.c` only moves a CdFile's sector.

#### Priority table (`g_ModMarSfxPrio`, `src/dra/cd.c`)

Because the Saturn Maria's sounds sit on voice ids, her shots, hits and beasts
had voice priority and took the voice's channel pair, and her slide lost to
stage sounds. While she is loaded, `ModMarSfxPrio` writes her priorities
(voice 90, effects under it, slide 85, common hit sounds 65). It writes the
retail values back for Richter, the PSP Maria and Alucard. Her Seiryuu sound
loops on channels 22/23.

### 7. New stages

| Dir | Stage | Slot / files | Build script | Assets |
|---|---|---|---|---|
| `mods/sotn-ultimate/src/garden/gdn` | Underground Garden (Saturn STAGE_15) | `0x11`, TE1 | `mods/sotn-ultimate/tools/build_gdn.sh` | `mods/sotn-ultimate/tools/gdn_assets.py` |
| `mods/sotn-ultimate/src/prison/prs` | Cursed Prison (Saturn STAGE_16) | `0x10`, TE2 | `mods/sotn-ultimate/tools/build_prs.sh` | `mods/sotn-ultimate/tools/prs_assets.py` |
| `mods/sotn-ultimate/src/garden/rgdn` | Hell Garden (Saturn RSTAGE15) | `0x31`, TE3 | `mods/sotn-ultimate/tools/build_rgdn.sh` | `mods/sotn-ultimate/tools/rgdn_assets.py` |
| `mods/sotn-ultimate/src/prison/rprs` | Soul Prison (Saturn RSTAGE16) | `0x30`, TE4 | `mods/sotn-ultimate/tools/build_rprs.sh` | `mods/sotn-ultimate/tools/rprs_assets.py` |

How they are built:

- The sources started from `src/st/wrp`. Enemies and bosses are C
  translations of the Saturn's SH-2 code, in `e_mod_*.h`.
- The decomp's build only knows the retail overlays. So each script runs its
  asset tool, then `./sotn.sh build-assets` on the generated YAML.
- It compiles every object with the same cc1-psx / maspsx pipeline
  `tools/builds/gen.py` uses.
- It links at `0x80180000` with a copy of WRP's link script (without
  `warp.c`), so WRP must be built first.
- It writes `build/us/TE<n>.BIN`. `make disk` prefers `build/us` files.
- It fails if the overlay, BSS included, ends past `0x801E0000`
  (`CASTLE_MAP_PTR`).

The Saturn map converter, `tools/saturn/assets` (`map convert-psx`,
`src/psx_stage.rs`), narrows the Saturn's 320-px art to 256 px and quantizes
it to PSX tiles. The fork adds:

- `SOTN_MOD_UNCAPPED`, which packs every tile exactly and dumps graphics and
  palettes. The Garden has far more distinct tiles than its F_ file holds. The
  converter's approximation put railings in mid-air, so `gdn_assets.py`
  repacks the tiles itself: into the F_TE1 pages plus three VRAM pages unused
  during play, merging only near-identical background tiles.
- Per-stage settings (stage 15's extras; stage 16's two ordinary rooms).
- 180-degree rotations for the inverted stages.

Loading and save rooms use the PS1's shared tilesets, not converted art,
because a loading room's preload overwrites the stage's own texture area.

The retail stages connect to the new ones through layout and room-table
copies placed in rodata (the `e_red_door.c` files). The stage's data section
must not move, because cutscene scripts address it directly.
`mods/sotn-ultimate/tools/check_stage_data.py` checks this. Generators that touch retail
stages: `gen_mod_layouts.py`, `gen_mod_rooms.py`, `gen_prs_links.py`,
`gen_card_layouts.py`, `gen_np3_sconce.py`, `gen_no3_horse.py`.

### 8. Playable characters

#### Saturn Maria (`MOD_MARIA_SAT`)

On the Saturn, Maria's `MARIA.PRG` is a fork of the Saturn Richter player
code. The fork does the same: `mods/sotn-ultimate/tools/build_mar.sh` compiles `src/ric` a
second time with `-DMOD_MARIA_SAT`, using generated data in `build/us/mod/mar`
(`gen_mar_assets.py`) and a copy of RIC's link script. The output is
`BIN/MARIA.BIN`. `build_mar.sh --same` builds without the define and must
equal `RIC.BIN`.

The deltas are under `MOD_MARIA_SAT` in:

- `src/ric/*.c`: her attacks, specials, double jump, backdash. She has no item
  crash and no whip, and the whip code is dropped from her file for room.
- `mods/sotn-ultimate/src/maria_saturn/mar_attacks.h`, `mar_specials.h`: new files.
- `ric.h`: entity ids as on the Saturn.
- `ric_shared.h`: input bits.

Her images are LZSS-packed because they don't fit raw. Statics are aligned by
`mods/sotn-ultimate/tools/align_bss.py`: maspsx lays out `.lcomm` without alignment, and
real hardware traps a misaligned access.

#### PSP Maria (`MOD_MARIA_PSP()`)

`mods/sotn-ultimate/tools/build_mar2.sh` builds `BIN/MARIA2.BIN` from `src/maria` (the PSP
player overlay) plus the PS1 glue `mods/sotn-ultimate/src/maria_psp/mar2.c`. It uses mipsel gcc,
as upstream's `mods/mariapsp-forpsx` does, and links at `0x8013C000`
(`mods/sotn-ultimate/src/maria_psp/mar2.ld`). It differs from the upstream mod in two ways:

- Nothing in DRA is patched in place, so the Saturn Maria and Richter can
  follow in the same session. The HUD icon table is the DRA pointer
  `g_ModSubwpnIcons`, which `cd.c` resets for every player overlay.
- The overlay clears its own BSS on each load.

The script links against DRA's actual addresses for any symbol the mod moved.
`build_disc.sh` checks again after the final DRA link. One fix in
`src/maria/pl_setstep.c` (crouch after a landing) is under `MOD_SATURN`.

The PSP Maria boss in CEN, `BIN/CENP.BIN` (`mods/sotn-ultimate/tools/build_cenp.sh`), is
CEN's own sources with `-DMOD_CENP` plus `src/st/cen_psp/e_maria*.c` and
`mod_cenp.c`, built with gcc. Both fights don't fit in one overlay. The
`src/st/cen_psp` edits under `MOD_CENP_BOSS` adapt that code to the PS1
(tpages, CLUTs, pad bits, a missing return value).

### 9. Generated files policy

- Generators in `mods/sotn-ultimate/tools` read the extracted discs (`disks/us`,
  `disks/saturn`, and `disks/psp`: the US UMD's `PSP_GAME/USRDIR/res/ps`,
  `mods/sotn-ultimate/tools/extract_psp.sh`). They write headers and binaries into the tree
  and into `build/`. The PSP data comes from the US UMD only, the one the
  builder takes: the PSP Maria's and the PSP boss's assets are extracted with
  `mods/sotn-ultimate/tools/psp_us_assets.yaml` (sotn-decomp's `config/assets.pspeu.yaml`
  entries for `maria.bin` / `cen.bin` at the US files' offsets), and
  `gen_psp_sfx.py` finds PS.ELF's per-bank slot lists by pattern.
- Every generated header is gitignored. See the `.gitignore` section headed
  "SOTN Ultimate mod" (`mod_layout.h`, `mod_rooms.h`, `mod_psp_script.h`,
  `mod_*_sfx.h`, the stage `mod_sprite_*.h` / `mod_tilemaps.h`,
  `src/dra/mod_xa_music.h`, `src/dra/mod_psp_xa.h`, the builder's `dist/`,
  the tile-swap atlases, and others), and `mods/sotn-ultimate/src/.gitignore`
  for those in the mod's folder (the new stages' `gen/` and `mod_*.h`,
  `maria_ps1/mar2_items.h`, `weapon/mod/`, `st/cen/mod_maria_boss_data.h`).
- Nothing game-derived is committed. Where a build needs retail bytes it
  reads them from `disks/` at build time; `src/st/sel/config_us.c`, for
  example, `.incbin`s a range of the retail `SEL.BIN`.
- `mods/sotn-ultimate/tools/mod_work.py` defines the work folder: `$MOD_WORK`, or
  `build/mod-work` by default. It holds renders, caches, pristine retail asm
  copies, and the builder's compiled modules.

### 10. Upstream files touched

These are the tracked files that differ from `698aab2`. All mod code is under
`MOD_SATURN` / `MOD_GODSPEED` (or a narrower switch). Where a file's whole
blocks moved to `mods/sotn-ultimate` (section 0), what is listed here is in
the included header.

#### Repository and tools

| File | What / why |
|---|---|
| `.gitignore` | Ignores generated mod headers, builder output and atlases (section 9). |
| `README.md` | A short pointer to the mod, in front of the upstream README. |
| `mods/sotn-ultimate/README.md` | This file: the discs, building, tests, these notes, licence, credits. |
| `config/disk.us.lba` | The 18 new disc files (section 5). |
| `tools/builds/gen.py` | Links `mods/sotn-ultimate/config/mod_rel_syms.*` (stale absolute splat symbols). |
| `tools/dirt_patcher.py` | `SOTN_MOD_NO_DIRT` skip (retail dirt would land on moved data). |
| `tools/sotn-assets/mods/lba.go` | `OFF_BIN_*` / `LEN_BIN_*` for the new files. |
| `tools/sotn-disk/iso9660/make.go` | Single-track volume size. |
| `tools/saturn/assets/src/psx_stage.rs` | Uncapped mode, per-stage settings, inverted rotations. |

#### Headers

| File | What / why |
|---|---|
| `include/game.h` | Switches, item, stage and teleport ids, castle flags, runtime flags, the `PlayerStatus` expansion and the retail layout, `ModDubState`. |
| `include/lba_us.h` | Regenerated by `make disk` (new files, shifted sectors). |
| `src/config_us.h` | Modded equipment, accessory and enemy tables (generated), JP familiar labels, God Speed Shoes relic record, tables moved to rodata, System popup stubs, `g_ModDataPad`. |
| `src/save_mgr.h` | Save conversions, "SAT4" writer, `ModSaveExtra`, moved save rooms, DRA-only strips. |

#### DRA (`src/dra`)

| File | What / why |
|---|---|
| `42398.c` | Debug and capture code removed, `MOD_DRA_PAD`, scene-seen bits at boot. |
| `47BB8.c` | Sim loader removed; its buffers become `g_ModDub`. |
| `4A538.c` | Rainbow Mantle cape colours; palette-viewer checks removed. |
| `5087C.c` | New teleports, stage music per character, test warp aid. |
| `5D5BC.c` | Relic 30 outside `relics[]`, JP familiar stats in castle flags, Saturn level-up MP rule. |
| `5F60C.c` | Glaring Cloth / Rainbow Mantle cape colours, new-game scene bits, `g_ModSubwpnIcons`. |
| `62DEC.c` | Maria walk-step snap; a write-only reset removed. |
| `66590.c` | Hitbox overlay and an unused function removed. |
| `692E8.c` | Time-stop dim only with PS1 balance; debug fly mode removed. |
| `6E42C.c`, `6FDF8.c` | Sprint step, landing resumes the sprint, the third hand ("Holder"). |
| `71830.c` | Alucard Spear specials; debug fly helpers removed. |
| `7879C.c`, `dra.h` | Saturn blueprints 123-127; `MOD_DRA_STRIP`, `g_UseDisk`, `FntPrint` defines. |
| `d_E294.c` | Alucard Spear's extra player animations. |
| `alu_anim.c` | An unused function removed. |
| `cd.c` | Load redirections, sound bank swaps, Maria priority table, Richter costume, write-only globals made local. |
| `config_us.c` | `MOD_DRA_CONFIG`, `MOD_DATA_PAD`. |
| `demo.c` | PC file reader and recorder removed. |
| `game_handlers.c` | Balance row (`ModApplyBalance`), dub text, `storeSave`; PC familiar loader and debug start removed. |
| `lba_stage.c` | New stage rows; names replaced by PSP sound bank sectors. |
| `menu.c` | Relic list with 31 entries, JP familiars, Holder row, System popup row, shifted special records; unused functions removed. |
| `save_mgr.c` | `MOD_SAVE_DRA`. |
| `sound.c` | XA table extension, dub voice redirection, music pause, no XA command during loads, write-only globals removed. |

#### Players and weapons

| File | What / why |
|---|---|
| `src/ric/*.c`, `ric.h`, `ric_shared.h` | Saturn Maria deltas (`MOD_MARIA_SAT`). |
| `src/ric/pl_whip.c` | Also the JP v1.2 flame whip fix for Richter. |
| `src/maria/pl_setstep.c` | Crouch-after-landing fix. |
| `src/weapon/w_000.c`, `w_001.c`, `w_004.c`, `w_005.c`, `w_014.c`, `w_031.c`, `w_033.c` | Saturn subtypes and hitboxes (Saturn balance). |
| `src/weapon/w_017.c` | Bat Pentagram shard time follows the Balance row. |
| `src/weapon/w_021.c` | The Saturn's two new foods. |

#### Shared stage code (`src/st/*.h`)

| File | What / why |
|---|---|
| `collision.h` | Saturn enemies (ids 0x190 and up) past the US table, bestiary bits. |
| `create_entity.h` | Per-room layout copies (`mod_layout.h`, `mod_layout2.h`). |
| `cutscene_script_text.h`, `set_cutscene_script.h` | PSP script copies. |
| `e_background_lightning.h` | Per-character Entrance themes. |
| `e_bloody_zombie.h` | `MOD_BLOOD_ONLY`. |
| `e_breakable.h` | Saturn candles and stands. |
| `e_collect.h` | New hand item drops, relic 30. |
| `e_explosion_puff_opaque.h` | Garden boss death fire. |
| `e_nova_skeleton.h`, `e_venus_weed.h` | Overridable tpage and CLUT for reuse. |
| `e_red_door.h`, `e_sealed_door.h`, `e_sealed_door_no2.h` | Doors open while sprinting (`MOD_SPRINTING`). |
| `e_subweapon_container.h` | PSP Maria drop rule. |
| `e_trapdoor.h` | The Saturn rule for the trapdoor. |

#### Stages and bosses

| File | What / why |
|---|---|
| `src/st/{cat,no4,nz1,top}/e_red_door.c`, `src/boss/bo3/e_red_door.c` | Layout copy with the mod's entities (rodata, after the data section). |
| `src/st/{cat,no4,nz1,top,no3}/*.h`, `src/boss/bo3/bo3.h` | Layout ids and extra prize indices for the new items. |
| `src/st/{no0,no4,rno0,rno4}/{header.c,e_red_door.c,*.h}` | Prison and Soul Prison doors, loading rooms, transitions. |
| `src/st/{np3,rno3}/*` | Entrance shaft extended to the Garden / Hell Garden, sconces and candelabras, moved save room. |
| `src/st/{wrp,rwrp}/*` | The sixth warp room. |
| `src/st/no1/*` | The optional cut Ghost (`MOD_OUTER_WALL_GHOST`). |
| `src/st/no3/*` | Richter's and Maria's horse ride, Death's music, first-room layout; the Entrance shaft to the Garden on the first visit too (as the Saturn's STAGE_0X), the forest run on row 46, the moved save room. |
| `src/st/cen/*` | Saturn Maria boss fight; `MOD_CENP` hooks. |
| `src/st/cen_psp/*` | PS1 build of the PSP boss (`MOD_CENP_BOSS`); its glue is `mods/sotn-ultimate/src/maria_psp/mod_cenp.c`. |
| `src/st/lib/e_shop.c` | Saturn 164-entry bestiary list. |
| `src/st/{np3,no3,rno3}/e_breakable.c` | Saturn candle tables (retail tables kept in place). |
| `src/st/no0/e_slinger.c`, `src/st/rnz0/e_bitterfly.c` | Bodies moved to `src/st/e_slinger.h` / `e_bitterfly.h` so the new stages can share them. |
| `src/st/sel/2C048.c` | Saturn character select, sound mode, dub and balance rows, JP table copy, menu text. |
| `src/st/sel/32D6C.c`, `34B30.c` | Save place of mod areas; Maria ending sprite; "STAFF" credits-only ending. |
| `src/st/sel/config_us.c` | SEL's unused copy of DRA's tables: data kept (`.incbin` from the retail file), strings dropped for room. |
| `src/st/st0/2A218.c` | START during the skipped prologue dialogue. |
| `src/st/top/post_richter_cutscene.c` | Kneeling Richter in the coat. |
| `*/cutscene.c`, `*/e_cutscene_dialogue.c` in `bo2`, `bo5`, `bo6`, `mar`, `rbo2`, `rbo6`, `dai`, `dre`, `lib`, `no3`, `no4`, `nz0`, `rcen`, `sel`, `st0`, `top`, `cen` | Include `mod_psp_script.h` (the PSP script copies) and room for overflow lines. |

For the exact list, run `git diff --stat 698aab2`.

### 11. The builder (`mods/sotn-ultimate/tools/builder`)

The builder is TypeScript with no npm dependencies, plus a WebAssembly audio
module (`native/build.sh`: a cut-down FFmpeg 7.1.1, `at3dec.c`,
`mods/sotn-ultimate/tools/satxa.c`, `dsp.c`) and the Saturn map converter compiled to
WebAssembly (`node/satconv.ts`). The same modules run in Node, for the
release build and the tests, and in the page.

| Script | Does |
|---|---|
| `node/make_hollow.ts FULL.bin US_TRACK1.bin SATURN.bin PSP.iso OUTDIR` | Release side. It blanks every Saturn or PSP span of the full image and writes the xdelta3 patch from the US Track 1 to that "hollow" image (`-S none`), plus `manifest.json`. There are three kinds of span. File spans lose their user data and EDC/ECC. XA spans are re-encoded exactly as the builder will make them and located in XA_STR1. Array spans are the generated C arrays inside compiled files (`node/array_spans.ts`). A final sweep removes any remaining Saturn or PSP byte runs. |
| `node/make_builder_page.ts PATCHDIR OUT.html` | Joins `web/builder.html`, the bundled modules (`node/bundle.ts` strips types into one classic script, so the page works from `file://`) and the manifest. The page holds no game data. |
| `node/build_disc.ts PATCHDIR PS1.bin SATURN.bin PSP.iso OUT.bin` | Runs the page's whole flow (`src/build.ts`) from Node: identify the discs, apply the patch (`src/vcdiff.ts`), fill each span from the Saturn and PSP discs, then check each span and the whole disc against the manifest's MD5s. |

Parity tests:

- `node/test_*.ts` compare each TypeScript port with the Python generator's
  output from the current build, byte for byte. Examples: `test_psp_sfx.ts`,
  `test_stage_build.ts`, `test_gdn_tile_swap.ts`, `test_mar_assets.ts`,
  `test_saturn_weapons.ts`.
- `test_go_png.ts` checks the zlib output against Go's.
- `test_sector.ts` recomputes EDC/ECC.
- `test_*.cjs` run the bundled `dist/builder_core.js` the way the page runs it.

`mods/sotn-ultimate/tools/make_release.sh PS1_TRACK1.bin SATURN_TRACK1.bin PSP.iso OUTDIR`
does the whole release:

1. `build_disc.sh`, which builds the full disc.
2. `make_hollow.ts`, which writes the patch and manifest.
3. `make_builder_page.ts`, which writes `builder.html`.
4. `build_disc.ts` on the three discs. Its result must be the release disc,
   matching the manifest's MD5.

`mods/sotn-ultimate/tools/build_disc.sh` runs every generator, balances the DRA pads, builds
the extra overlays and files, and runs `make disk`. It then runs the checks:
`check_dra_layout.sh`, `check_cd_lba.py`, `check_no_dirt.sh`,
`check_stage_data.py`, `check_status_shift.py`, `check_menu_text.py`,
`check_psp_dub.py`, `check_abs_syms.py`, `check_weapon_hands.py`. Output
goes to `build/sotn_ultimate.bin` (or `$MOD_OUT`).

## Licence

The code is under the GNU Affero General Public License v3.0 or later, as
sotn-decomp is ([`LICENSE`](../../LICENSE)). The builder's audio module is built from FFmpeg
7.1.1 (LGPL 2.1 or later; source at <https://ffmpeg.org>, configuration in
`mods/sotn-ultimate/tools/builder/native/build.sh`).

*Castlevania* and *Symphony of the Night* are Konami's. This project is not
affiliated with or endorsed by Konami.

## Credits

- Konami: *Castlevania: Symphony of the Night* (PlayStation, 1997),
  *Akumajou Dracula X: Gekka no Yasoukyoku* (Sega Saturn, 1998) and
  *Castlevania: The Dracula X Chronicles* (PSP, 2007).
- The sotn-decomp project (Xeeynamo and contributors): the decompilation
  this mod is built on, including its Saturn and PSP work and tools, without
  it, this mod would not exist.
- Xeeynamo: the upstream `mods/mariapsp-forpsx` mod (sotn-decomp #2638),
  the first playable PSP Maria on the PS1. This mod's PSP Maria glue
  (`src/maria_psp/mar2.c`) is adapted from its `maria.c`.
- sozud: upstream's `tools/maria-sfx` and the PSP Maria's voice support
  (sotn-decomp #3457), which this mod uses for her PSP voice.
- Knight0fDragon: the Saturn English fan translation (v1.021), the source of
  the English bestiary descriptions for the Saturn enemies, used with his
  permission.
- The Castlevania Wiki (Fandom): reference for the Saturn-only items.
- Redump: the reference checksums.
