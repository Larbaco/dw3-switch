<p align="center">
  <img src="docs/assets/banner-1280x640.png" width="720"
       alt="DW3NX: pixel-art crystal and orbital emblem on a dark background">
</p>

<h1 align="center">Digimon World 3 NX</h1>

<p align="center">
  <b>Digimon World 3 / Digimon World 2003, running natively on the Nintendo Switch and the PC.</b><br>
  Widescreen, fast-forward, auto-save, an in-game walkthrough and the controls your way.
</p>

<p align="center">
  <a href="https://github.com/Larbaco/dw3-switch/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/Larbaco/dw3-switch?include_prereleases&label=latest%20release&color=8a2be2"></a>
  <a href="https://github.com/Larbaco/dw3-switch/releases"><img alt="Downloads" src="https://img.shields.io/github/downloads/Larbaco/dw3-switch/total?color=2ea44f"></a>
  <img alt="Platforms: Switch and PC" src="https://img.shields.io/badge/platform-Switch%20%7C%20PC-e60012">
  <img alt="Regions: US and EU" src="https://img.shields.io/badge/region-US%20%7C%20EU-1e90ff">
  <a href="LICENSE"><img alt="License: MIT" src="https://img.shields.io/badge/license-MIT-lightgrey"></a>
</p>

<p align="center">
  <a href="https://github.com/Larbaco/dw3-switch/releases/latest"><b>Download</b></a> ·
  <a href="#play-it-on-switch"><b>Install in 3 steps</b></a> ·
  <a href="docs/PLAYER_GUIDE.md"><b>Player guide</b></a> ·
  <a href="docs/REPORTING_ISSUES.md"><b>Report a bug</b></a>
</p>

<p align="center">
  <img src="docs/assets/screenshots/battle-widescreen.png" width="400"
       alt="Battle at 16:9 on a grassy plain with the Fight, Tech, DV, Tag, Item and Run command menu and HP bars">
  <img src="docs/assets/screenshots/field.png" width="400"
       alt="Field at 16:9: the player and partner Digimon on a sandy patch with wooden planks">
  <br>
  <img src="docs/assets/screenshots/title-menu.png" width="266"
       alt="Title menu with five plaques: New Game, Options, Continue, Load Game and Quit game">
  <img src="docs/assets/screenshots/options-screen.png" width="266"
       alt="Options screen, Gameplay group: Fast-forward, Restart hotkey, Encounter rate, Text speed and Battle cursor">
  <img src="docs/assets/screenshots/map-view.png" width="266"
       alt="Full-screen map view with the game's balloon on the current area, the cursor and the control hints">
</p>
<p align="center"><sub>Captured from the PC build at 16:9 (Widescreen on).</sub></p>

## The 2002 PS1 classic, rebuilt for the Switch

DW3NX is not an emulator. It is the game's own code, rebuilt from the
PlayStation decompilation to run natively on the Switch (and on the PC). That
lets it do things an emulator cannot: draw the field and battles in 16:9, add
new menus inside the game, fix old bugs and speed up the slow parts. You keep
the original story, music and Digimon, from your own disc.

## What's new compared to the original

Everything below is optional. With the options off you get the original game
(widescreen is the one change that is on by default).

### Looks

- **Widescreen 16:9** in the field, in 3D battles and in most menus. Movies and
  the card battle stay 4:3.
- **Full-screen map.** A zoomable, pannable map with your area marked and the
  area names in the game's own text. Open it from the Map screen or straight
  from the field with Select (turn on **Full-screen map** in Options, Display).
- **Upscaled map packs.** Generate one from your own disc and drop it in
  `sdmc:/switch/dw3/mods/` ([how](docs/MOD_MAP_UPSCALE.md)).

### Speed

- **Fast-forward** with R3: 2x, 3x or 4x in the field, in battle and on the
  results screen. The music can keep its normal speed while you rush.
- **Text speed**: Normal, Fast or Instant.
- **Encounter rate**: off, 0.5x, 1x, 1.5x or 2x for random battles. Story
  battles always stay.
- **Battle cursor memory**: the command menu (and the tech list) opens on your
  last pick.

### Saves

- **Auto-save** on each new area, in its own file, never over your slots.
  **Continue** on the title picks it up.
- **One save folder per Switch user.**
- **US and EU saves carry over**: a region with no save of its own imports the
  other region's save, without changing the original.
- **Quit game**, **Return to title** and an opt-in **restart** chord, so you
  never have to close the app to start over.

### Controls

- **PlayStation or Nintendo layout.** Keep the PS1 positions (B confirms) or
  switch to Nintendo style (A confirms, B cancels).
- **Remap any button** in Options, Controls, Customize buttons.
- **Switch button glyphs** in the game's prompts, if you want them.

### Help inside the game

- **Walkthrough** for the whole main story, and the EU extra chapter, that
  opens on your current step. Answers stay hidden until you ask, so nothing
  ahead of you gets spoiled.
- **Help** pages: setup, controls, fast-forward, saves, the map and a FAQ.
- **Options anywhere**: from the title, from the field menu, and in the middle
  of a battle with Minus (the battle waits while you change things).

### Fixes and extras

- **Bug fixes**, such as the Digivolve command crash in battle, the
  Reliability Spot crash and the Underground Path canal water
  ([P7.7](docs/P7.7_BUG_FIXES.md)).
- **One file for US and EU**: `dw3-switch.nro` picks your region at boot, with
  a picker when you have both discs.
- **Five languages** on the EU disc (English, French, German, Spanish and
  Italian), port menus included.
- **Crash reports**: if the game crashes, it writes a report you can attach to
  a bug, and tells you about it at the next launch.
- A **Cheats** screen for testing (money, warp, items), hidden behind a master
  switch.

Every option and its default: [player guide](docs/PLAYER_GUIDE.md#options-screen).
What's coming next (language packs such as Brazilian Portuguese, texture packs
and more): [ROADMAP](docs/ROADMAP.md).

## Play it on Switch

**You need:** a Switch running Atmosphère with hbmenu, and your own disc dumped
as a Mode2 BIN/CUE pair:

- Digimon World 3 (USA, SLUS-01436), or
- Digimon World 2003 (Europe, SLES-03936).

No game data ships with the port.

1. **Download** `dw3-switch.nro` from the
   [latest release](https://github.com/Larbaco/dw3-switch/releases/latest) and
   copy it to `sdmc:/switch/`. It is one file for both regions.
2. **Copy your disc** (the BIN and the CUE) into `sdmc:/switch/dw3/`. A US and
   an EU disc can sit there side by side; the game tells them apart.
3. **Launch it** from hbmenu. The first boot extracts the game data from your
   disc and shows a progress bar. Let it finish, and you are in.

```text
sdmc:/switch/dw3-switch.nro
sdmc:/switch/dw3/Digimon World 3.cue        your disc (also accepted in us/ or eu/)
sdmc:/switch/dw3/Digimon World 3.bin
sdmc:/switch/dw3/us/                        created on first boot (eu/ for EU)
sdmc:/switch/dw3/saves/<uid>/<us|eu>/memcard/   your saves, per Switch user
sdmc:/switch/dw3/dw3.log                    the log; attach it to a bug report
```

**Tips**

- A forwarder (for example Sphaira) is the best way to launch it, and the way
  to pick your Switch user.
- The port does not overclock.
- Back up `saves/` before you delete a region folder.
- Coming from an older preview? Delete any `dw3-switch-eu.nro`, `-both` or
  `-nxlink` files and recreate your forwarder. Saves and region folders do not
  change.

Setup details and troubleshooting: [PLAYER_GUIDE](docs/PLAYER_GUIDE.md).
Release history and builds to avoid: [RELEASES](docs/RELEASES.md).

## Controls

By default the face buttons follow the PS1 positions. Prefer A to confirm?
Pick the **Nintendo** layout in Options, Controls.

| Action | Switch (default layout) |
|---|---|
| Confirm / back | B / X |
| Show the title menu | Plus |
| Fast-forward on or off | R3 (click the right stick) |
| Options (title and battle) | Minus |
| Full-screen map from the field | Select (Minus), with Full-screen map on |
| Skip an opening movie | Plus |

Full table, PC keys and every hotkey: [HOTKEYS](docs/HOTKEYS.md) and the
[player guide](docs/PLAYER_GUIDE.md#controls-and-hotkeys).

## Status

**Early preview, playable.** Tags are `v0.9.0-preview.N` until the 0.9 public beta (`v0.9.0`), then `v1.0.0-rc.N` and `v1.0.0`; see [RELEASES](docs/RELEASES.md#naming). On the PC the game plays from the title to the
field and to battle, on a US or EU disc. On a Switch OLED the title, field,
battles, movies, audio and saves work (US is the most tested; EU came later).

Expect bugs, rough visuals in some scenes and the occasional crash. A full run
on real hardware from the start to the first boss is confirmed on the OLED for US
(2026-10-30) and EU (2026-10-31), pending the owner's check, and
many new features are checked on PC and in the Ryubing emulator before they
reach a console; each release's notes say which. Known open items include EU
audio after many techs
([#188](https://github.com/Larbaco/dw3-switch/issues/188)) and the opening
movie, which runs at about 15 fps.

Found a bug? Open an [issue](https://github.com/Larbaco/dw3-switch/issues) with
your `dw3.log` (and `crash_report.txt` if there is one). How:
[REPORTING_ISSUES](docs/REPORTING_ISSUES.md). Never attach disc images or game
files.

## FAQ

**Is this an emulator?** No. The `.nro` has no PS1 core. The game's C code,
from the decompilation, is compiled for the Switch. That is why it can do
widescreen and new menus.

**Do I need the game?** Yes. You need your own US or EU disc. The port ships
no game data.

**Which versions work?** Digimon World 3 (USA, SLUS-01436) and Digimon World
2003 (Europe, SLES-03936). Other regions are not supported.

**Can I play on PC?** Yes, by building it yourself; see
[Build the PC version](#build-the-pc-version).

**Where are my saves?** In `sdmc:/switch/dw3/saves/`, one folder per Switch
user and per region. See [Saves](docs/PLAYER_GUIDE.md#saves).

**Which version am I running?** A dim line under the Options screens and in
the title's bottom right corner, such as `dw3-switch v0.9.0-preview.2
US`. It is also the first line of `dw3.log`.

## For developers

### How it works

The game logic is the C code of the matching PS1 decompilation,
[ReGame-Labs/dw3_decomp](https://github.com/ReGame-Labs/dw3_decomp). Console
APIs go through [PSY-Z](https://github.com/Xeeynamo/psyz), a Psy-Q compatible
shim. A thin host supplies video, audio, input and files: SDL3 on PC, libnx
on the Switch.

```text
decomp C  ->  PSY-Z  ->  SDL3 backend (PC)
                     ->  libnx backend (Switch)
```

This is a source port, not an emulator wrapper, not a static recompilation and
not a remake with a new engine. How the layers fit:
[ARCHITECTURE](docs/ARCHITECTURE.md), [PSYZ](docs/PSYZ.md),
[SWITCH_BACKEND](docs/SWITCH_BACKEND.md), [PATCHES](docs/PATCHES.md). Work
order: [ROADMAP Priority](docs/ROADMAP.md#priority). Per-item state:
[ROADMAP Status at a glance](docs/ROADMAP.md#status-at-a-glance).

### Build the PC version

You need your own US or EU disc. No disc is needed to run the tests.

```bash
git clone https://github.com/Larbaco/dw3-switch.git
cd dw3-switch
git submodule update --init vendor/dw3_decomp vendor/psyz
git -C vendor/psyz submodule update --init nugget external/SDL

# pack your own disc into runtime/us (layout: docs/DISC_SETUP.md)
python3 tools/pack_runtime.py --version us --disks disks/us --out runtime/us

cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/pc -j"$(nproc)"
./build/pc/dw3-pc
```

Do not initialize nugget recursively
([CI_SUBMODULES](docs/CI_SUBMODULES.md)). `-DDW3_VERSION=eu` builds EU and
`-DDW3_VERSION=both` builds one binary with both regions
([platforms/pc/README.md](platforms/pc/README.md)). Switch build
(`tools/build_switch_game.sh`, needs devkitPro), crash symbolization and
replay tests: [CONTRIBUTING.md](CONTRIBUTING.md),
[platforms/switch/README.md](platforms/switch/README.md),
[P6.15_REPLAY_TESTS](docs/P6.15_REPLAY_TESTS.md). Faster local builds and
tests (`tools/build_pc.sh`, ccache, `--changed` replay selection):
[BUILD_SPEED](docs/BUILD_SPEED.md).

### Documentation

| Doc | Contents |
|---|---|
| [PLAYER_GUIDE](docs/PLAYER_GUIDE.md) | Setup, controls, menus, saves and crash reports for players. |
| [Docs index](docs/README.md) | One line per doc in `docs/`. |
| [ROADMAP](docs/ROADMAP.md) | Priority, status and phases. |
| [RELEASES](docs/RELEASES.md) | Release naming, index, builds to avoid. |
| [PORT_OPTIONS](docs/PORT_OPTIONS.md) | Every option, its key in `port_options.cfg` and its design note. |
| [CONTRIBUTING](CONTRIBUTING.md) and [AGENTS.md](AGENTS.md) | Rules, build and test commands for contributors and AI agents. |
| [REPORTING_ISSUES](docs/REPORTING_ISSUES.md) | How to report a bug. Never attach disc images or game files. |

## Credits

This port stands on other people's work.

| Project | Thanks for |
|---|---|
| [ReGame-Labs/dw3_decomp](https://github.com/ReGame-Labs/dw3_decomp) | The matching Digimon World 3 / 2003 decompilation, the game logic this port builds on (`vendor/dw3_decomp`). |
| [Xeeynamo/psyz](https://github.com/Xeeynamo/psyz) | PSY-Z, the Psy-Q compatible shim (`vendor/psyz`). PC uses its SDL3 backends; Switch uses the libnx backend in `platforms/switch/psyz_backend/`. Pin: [PSYZ](docs/PSYZ.md). |
| [libsdl-org/SDL](https://github.com/libsdl-org/SDL) | SDL3, used through PSY-Z's desktop backends. |
| [devkitPro](https://devkitpro.org/) and [libnx](https://github.com/switchbrew/libnx) | The Switch homebrew toolchain and APIs. |
| [Xive080/D-W-3-Recomp](https://github.com/Xive080/D-W-3-Recomp) (OpenWorld2003) | A public PC recomp of the same game; a reference for CD and filesystem pitfalls and for launcher and mod catalog shape. Not a dependency. |
| [markisha64/ddw3](https://github.com/markisha64/ddw3) | Parallel Digimon World 2003 decomp (Rust/asm); reference for Digimon profiles, TIM/map tooling and language tables. |
| [dorijantomic/dw2003-play-code](https://github.com/dorijantomic/dw2003-play-code) | Browser recreation built from ddw3; a research oracle for battle formulas, the SPU and the save-block size. |
| [rsigristc/dw2003-qol-patch](https://github.com/rsigristc/dw2003-qol-patch) (**Harmony**, with Flawe, guiomatos, markisha64, EmeraldPhoenix and others) | The EU-only (SLES-03936) quality-of-life patch suite, tracked for native reimplementation: [COMMUNITY_QOL](docs/COMMUNITY_QOL.md). |

## Legal

Use a legally obtained copy of Digimon World 3 (USA, SLUS-01436) or Digimon
World 2003 (Europe, SLES-03936). This repository and its releases contain no
game data: never commit disc images, BIOS files or extracted trees (`disks/`,
`runtime/` and disc-image extensions are gitignored). The screenshots above are
the one owner-approved exception ([AGENTS.md](AGENTS.md)).

*Digimon* and *Digimon World* are trademarks of their respective owners.
This is an unofficial fan project, not affiliated with Bandai Namco or
Nintendo.

Port scaffolding and original glue code are under the [MIT License](LICENSE).
Game code under `vendor/dw3_decomp` remains under that project's license.
Original game assets belong to their owners and are not redistributed here.
