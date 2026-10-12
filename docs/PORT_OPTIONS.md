# Port options menu

Phase 9 goal note. See [ROADMAP.md](ROADMAP.md) P9.1–P9.5.

Vanilla Digimon World 3 / 2003 menus stay as the decomp implements them.
Anything this port adds that **did not exist on the original PS1 game**
(host polish, native QoL parity, texture packs, fast-forward, etc.) should be
reachable from a **port options** entry on the title / main menu, with
per-feature toggles and defaults that preserve original behaviour.

## Scope

| In scope | Out of scope |
|---|---|
| Toggle port-only features | Replacing or restyling every vanilla menu |
| Persist options on owner storage (PC dir / `sdmc:/switch/dw3/…`) | Shipping a default “modded” experience with no vanilla path |
| Same menu on PC and Switch hosts | Requiring a companion app to change settings |
| Clear labels: feature → which phase/doc owns it | Mixing port flags into the PS1 `.mcr` by default |

EU `CNTY_SEL` / `LANGUAGE` and US/EU disc selection are **original** (or
original-adjacent) flows — expose them where the game already does (see
P6.2). The port menu covers **port additions**, not a second language
select unless we add a port-only override.

## Layering

| Layer | Responsibility |
|---|---|
| Title / main menu hook | Entry point into port options (minimal glue; prefer `port/` over editing decomp in place) |
| `port/` | Option registry, defaults, apply-on-boot / apply-on-change |
| Owner config file | JSON or similar under the runtime layout — gitignored |
| `platforms/*` | Input/present only; no feature policy in the Switch tree |

## Toggle registry (seed)

Populate as features ship. Every non-native feature should land here or be
explicitly marked N/A in this note.

| Feature area | Examples | Roadmap |
|---|---|---|
| Display / timing | Integer scale, **widescreen** (wanted; default 4:3), PAL pacing 50 vs 60, NTSC host options | Phase 6 (+ P4 present) |
| Convenience | Fast-forward / frame-skip (hold button + multiplier) | Phase 6 |
| Localization | Active language pack (if port-selected) | Phase 6 |
| Community QoL parity | Share EXP, quest log HUD, NPC bubbles, Fast Start, … | Phase 7 |
| Texture packs | Enable overlay / select pack name | Phase 8 |
| Switch host | Rumble on/intensity, confirm A/B layout, `swkbd` name, CRT/sharp, BGM/SFX volumes, system-language hint | Phase 10 (+ P4.4 / P5.3 always-on lifecycle) |

Defaults: **off** or equivalent to stock PS1 behaviour unless a feature is
purely host presentation with no gameplay impact (document exceptions).
Widescreen defaults **off** (4:3) but remains a first-class toggle when P6.1
lands — not a “skip” item.

Always-on (not toggles; see [SWITCH_FEATURES.md](SWITCH_FEATURES.md)):
focus-loss pause + delta clamp, memcard write-through/flush, rotating
`.mcr` backups, docked/handheld re-present, vsync wait.

## Persistence

- Path TBD with P1.6 layout (e.g. `config/port_options.json` next to
  runtime data).
- Load before game systems that read the flags; save on confirm or on
  toggle (pick one in P9.4 and stick to it).
- Do not write port options into the memcard image unless the user opts
  into an explicit “embed in save” experiment later.

## Entry investigation (P9.2)

Vanilla US/EU title (`STDWTITL`) has no Options item: after PRESS START the
menu only offers **New Game** / **Continue** (`menus/title/menu.c`
`runOptions`). Confirm is `PAD_CROSS`; choices `1` / `2` / `3` drive
`STDWTITL_leaveTitle` → `MODE_NEW_GAME` / `MODE_CONTINUE` / opening movie.
There is no third TIM sprite for a Config label.

| Piece | Location |
|---|---|
| Menu list + pad SM | `vendor/dw3_decomp/src/menus/title/menu.c` |
| Mode leave / fade | `…/title/title.c` (`STDWTITL_stepTitle` / `leaveTitle`) |
| Overlay root | `…/title/scene.c` (`STDWTITL_start`) |
| Cursor XY | `…/title/data/stdwtitl.c` (`STDWTITL_menuCursorPositions`) |
| Host link | `port/CMakeLists.txt` → `DW3_TITLE_SOURCES` |

**Recommended hook:** keep the shell in new `port/options/`; add a thin
`PORT_HOST` patch on `runOptions` / `STDWTITL_tickMenu` so opening Config
never sets `choice` and never calls `leaveTitle`. Stay on `MODE_TITLE`
until the shell closes (Triangle / Back), then resume the vanilla options
substate. Prefer a third cursor row drawn with a shared `port/` text path
(PC + Switch); Select-only (`PAD_SELECT`, Switch **Minus**) is an acceptable
MVP if the third-row drawer is not ready. Do **not** put the product entry
in `nx_debug_overlay` / battle-mode chords (`platforms/switch`).

**Risks:** accidental `choice` 1/2 while the shell is open; input passthrough
confirming Continue underneath; PRESS START timeout (`choice=3`) if entry is
wired too early; clipping a third row near y=194 on 240p.

Full write-up (session notes, not git): 
`~/.cursor/projects/mnt-windows-Jogos-dw3-switch/notes/p92-main-menu-port-options-entry.txt`.

## PC bring-up (P9.2 MVP)

While **New Game / Continue** are shown on the title menu, press
**Select** to open the port options shell. On the PSY-Z SDL keyboard map
that is **Backspace**. Close with **Triangle** (**S** on the keyboard).
The shell stays on `MODE_TITLE` and does not set the title menu `choice`
(New Game / Continue / timeout still work after close).

| Action | Pad | PC keyboard (PSY-Z) | Switch (later) |
|---|---|---|---|
| Open Config | `PAD_SELECT` | **Backspace** | Minus (alone; avoid debug chords) |
| Close / Back | `PAD_TRIANGLE` | **S** | B / Y cancel layout TBD |
| Confirm toggle | `PAD_CROSS` | **X** | A (stub no-ops for now) |

Implementation: `port/options/`, `port/ui/port_text.c`, patch
`0044-PORT_HOST-p92-title-port-options.patch`. Stub toggles (widescreen
default **off**, fast-forward) are document-only until P9.3.

Headless smoke: `DW3_AUTO_PAD=start,select` then
`DW3_PORT_OPTIONS_FB_DUMP=/tmp/dw3-p92-options-ttb/shell.ppm`
`DW3_PORT_OPTIONS_QUIT_AFTER_DUMP=1`.

## PC validation (2026-10-12)

Tested `main` commit `1d33f4d` with the owner's US runtime, separate
temporary memory-card storage, and the `sdl3_gpu` PC build. The SDL build
used `SDL_X11_XSCRNSAVER=OFF` because that optional development dependency
was absent. The two `test_port_options.py` checks passed.

The automated title-to-shell framebuffer dump was legible at 320x240;
both stub values started OFF. A separate keyboard test on a private Xvfb
display confirmed the following after the intro and PRESS START flow:

- Backspace opens the shell while New Game / Continue are shown.
- X changes the selected stub, Down moves to the second row, and X changes
  that stub too. Both show ON and log a safe no-op; the title remains active.
- S closes the shell and returns to the same Continue selection.
- Backspace reopens it with the in-process stub values retained; S closes
  it again. This is not disk persistence (P9.4 is still open).
- X on Continue after closing reaches the vanilla memory-card load screen
  (`STGMCARD_createScreen loading=1`), confirming the title flow resumes.

Local evidence is under `/tmp/dw3-menu-evidence/`: `shell.ppm`,
`isolated-sequence.png`, `isolated-toggled.png`, `isolated-closed.png`,
`isolated-reopened.png`, `isolated-continue.png`, `ttb.log`, and
`isolated.log`. Screenshots include owner-disc artwork and are not committed.
UI automation must use a private display, never the owner's active desktop.

**Visual follow-up:** the title hint `Select/Backspace: Port options` clips
at the right edge of its 240-pixel panel. The shell itself is legible.
No feature code changed in this validation. Switch input/chord validation,
the third cursor row, and the P9.2 cross-host completion remain open.
