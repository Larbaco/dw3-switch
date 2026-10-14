# Port options menu

Controls, physical mappings, and diagnostic chord overlap: [HOTKEYS.md](HOTKEYS.md).

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
| Convenience | **Fast-forward**: R3 toggles 2x/3x/4x in field and battle; row picks the speed or OFF ([P6.4](P6.4_FAST_FORWARD.md)) | Phase 6 |
| Localization | Active language pack (if port-selected) | Phase 6 |
| Community QoL parity | Share EXP, quest log HUD, NPC bubbles, Fast Start, … | Phase 7 |
| Texture packs | Enable overlay / select pack name | Phase 8 |
| Switch host | Rumble on/intensity, confirm A/B layout, `swkbd` name, CRT/sharp, BGM/SFX volumes, system-language hint | Phase 10 (+ P4.4 / P5.3 always-on lifecycle) |
| Restart game | Optional L1 + R1 + Start + Select in the shared PS1 pad map; OFF by default | Phase 6.9 |

Defaults: **off** or equivalent to stock PS1 behaviour unless a feature is
purely host presentation with no gameplay impact (document exceptions).
Widescreen is the documented exception: it defaults **on** (owner request
after the preview.22 OLED pass). It is a real, session-only toggle since
the P6.1 first pass: field and battle draw side gutters, everything else
stays 4:3; `DW3_WIDESCREEN=0` turns it off at boot. See
[P6.1_WIDESCREEN.md](P6.1_WIDESCREEN.md).
Fast-forward's row defaults to **2x**, which only arms R3: the game still
starts and stays at normal speed until R3 is pressed, so stock behaviour is
kept. OFF disables R3. See [P6.4_FAST_FORWARD.md](P6.4_FAST_FORWARD.md).

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

The restart hotkey is the first live setting in the registry. It is OFF by
default and remains in RAM for the current process; P9.4 persistence is still
open. When enabled, the chord flushes dirty P5.3 slots before returning the
game to its title boot flow. A failed flush cancels the restart. This does not
format, delete, or change clean save files. See [P6.9_RESTART.md](P6.9_RESTART.md).

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

## Title row (P9.2 follow-up)

The title now offers **Start / Port options / Continue** (the first item
keeps the owner's original New Game/Start sprite). Up/Down moves one row
at a time without wrapping. Confirm on the middle row opens the existing
shell; Triangle closes it and leaves the middle row selected. Confirm
does not toggle a stub on the opening frame. New Game and Continue retain
their original title `choice` values and blink/fade flow.

Patch `0045-PORT_HOST-p92-title-options-row.patch` extends the game menu
through `PORT_HOST`; `port/options/title_entry.c` draws the added label
with the game's resident proportional font. The original title labels are
pre-rendered sprites, so this new label uses the in-game font rather than
an identical new title sprite. No owner artwork or generated art is bundled
into the runtime.

**Presentation (title-options UI pass).** The Switch and baseline PC row is
made to read as the third original button rather than a debug overlay:

- The original sprite backgrounds are 164x16 with the selection frame one
  pixel outside. The added row now uses the same 164x16 one-pixel
  black/grey stripe background (it was 14 high, which left a visible 2 px
  gap between rows). Rows sit at y=149/165/181 on a 16-pixel pitch, so the
  three backgrounds butt together and the selected row's frame is the only
  separator. The last frame line stays above the copyright text.
- The label glyph quads are scaled 5/4 with a one-pixel bold pass and a
  drop shadow so the weight and height are closer to the heavier original
  lettering (the resident font is about 8 px tall, the sprite lettering
  about 11). The glyphs are vertically centred on the capital P.
- Selected: font blue row at full brightness. Unselected: the same blue row
  at about 60 percent brightness. The font's dark-blue row lost the letter
  counters at this size and was unreadable on a 720p window.
- Navigation is unchanged: Up/Down move one row without wrapping, Confirm
  on the middle row opens the shell, Triangle returns to the middle row.

The original cursor frame highlights only the selected row. Draw ordering keeps that
frame above the added background.
PRESS START retains its separate two-sprite spread animation, then the
three-row layout appears when the options state starts.

Select/Backspace remains a shortcut; shoulder-button combinations do not
open the shell, preserving the Switch diagnostic chords. The clipped
standalone shortcut hint is no longer drawn because the visible row is
the entry. Independent PC visual/input validation passed as recorded below.
Switch hardware validation passed below; P9.3 effects and P9.4 persistence remain open.

### PC Digivice title-menu follow-up

The PC SDL3 GPU title presents **New Game / Options / Load Game** in a
transparent 960×720 RGBA overlay, drawn over the original title image by one
SDL3 GPU texture and one six-vertex quad. The texture contains three visual states;
selection changes the lighting without moving the plaque or label. Plaques
have navy faces, steel-blue bevels, clipped corner hardware, amber fasteners,
internal rails, and legible centered text. The Switch draws the same
canvas and logo over its GLES present (`DW3_TITLE_NATIVE_OVERLAY`, set by PC
`sdl3_gpu` and by the Switch game image), with focus starting on New Game.
The #117 concept atlas remains a design reference (the mockup is kept
out of Git because it shows game artwork) and are not packaged as
runtime art. Selecting `PSYZ_RENDERER=sdl3_gl` keeps the earlier PS1-primitive
title drawing path and does not compile the SDL3 GPU overlay.

US-runtime validation under Xvfb/Vulkan at `DW3_PC_INTERNAL_RES=3` captured
New Game and Options selection plus P9.2 shell open in an exact 1280×720
window. Automated shell return and Load Game selection were not confirmed.
**Load Game** maps to the existing vanilla Continue /
`STGMCARD` flow; **Options** opens the existing P9.2 shell and the overlay
hides while it is open. Captures are local under
`/tmp/dw3-pc-title-menu-redo/evidence-redesign/`: `hires-final-new-game.png`,
`hires-verify-options.png`, and `hires-verify-options-shell.png`. The 960×720
4:3 viewport is centered, so the 1280×720 output has pillarboxes. Windows/DXIL
and macOS/Metal runtime checks
remain open.

The labels preserve the original flow: New Game uses vanilla choice 1;
Options opens the P9.2 shell and its current no-op stubs; Load Game maps to
vanilla Continue (choice 2) and the original `STGMCARD` flow. It does not
implement another save loader. PC defaults focus to New Game so it matches the
concept. The build and patch application check pass. Earlier 320×240 primitive
captures and 1280×960 window notes are superseded by the GPU validation above.
The Linux/Vulkan run used the US runtime and `DW3_PC_INTERNAL_RES=3`; Windows
and macOS runtime rendering remain untested.

## PC bring-up (P9.2 MVP shortcut)

While **New Game / Continue** are shown on the title menu, press
**Select** to open the port options shell. On the PSY-Z SDL keyboard map
that is **Backspace**. Close with **Triangle** (**S** on the keyboard).
The shell stays on `MODE_TITLE` and does not set the title menu `choice`
(New Game / Continue / timeout still work after close).

| Action | Pad | PC keyboard (PSY-Z) | Switch |
|---|---|---|---|
| Open Config | `PAD_SELECT` | **Backspace** | Minus (alone; avoid debug chords) |
| Close / Back | `PAD_TRIANGLE` | **S** | **X** (Triangle) |
| Confirm toggle | `PAD_CROSS` | **X** | **B** (Cross; stub no-ops for now) |

Implementation: `port/options/`, `port/ui/port_text.c`, patch
`0044-PORT_HOST-p92-title-port-options.patch`. Stub toggles (widescreen
default **off**, fast-forward) are document-only until P9.3.

Headless smoke: `DW3_AUTO_PAD=start,select` then
`DW3_PORT_OPTIONS_FB_DUMP=/tmp/dw3-p92-options-ttb/shell.ppm`
`DW3_PORT_OPTIONS_QUIT_AFTER_DUMP=1`.

## PC shortcut validation (2026-10-12, before the third row)

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

**Historical visual follow-up:** the title hint `Select/Backspace: Port options` clips
at the right edge of its 240-pixel panel. The shell itself is legible.
No feature code changed in this validation. The three-row follow-up above
replaces that hint with the visible entry. Switch row entry/return validation
is recorded below; diagnostic chord coverage was not part of that test.

## PC three-row validation checkpoint (2026-10-12)

Independent validation tested code commit `632cb70` (PR #100). The later
`5b59e4f` merge changed documentation only. On resumption, the retained
captures, scripts and logs in `/tmp/dw3-menu-entry-evidence` were inspected;
this checkpoint records that completed run, not a new hardware test.
No feature code or assets changed for this record.

The PC host used SDL_GPU/Vulkan lavapipe on private Xvfb display `:198`,
with dummy audio and a separate temporary memcard directory. Keyboard
input targeted that private display, never the owner's active desktop.
Xvfb did not provide effective vsync: the uncapped run measured about
396 FPS, making the existing frame-based repeat look too fast. The final
smoke used the existing `Psyz_VideoSetVsyncMode(2)` 60 Hz limiter through
GDB; no input or runtime implementation was changed.

- At 60 Hz, Up/Down taps of 55 ms traverse all three rows. One-second
  holds clamp at Start/Continue; releasing then tapping reaches the middle.
- Confirm on Port options opens both stubs OFF without confirm passthrough.
  Triangle/S returns to the middle row. Select/Backspace also opens the shell.
- Stub toggles work within the session; a process restart restores OFF/OFF,
  as expected while persistence is absent. The TTB framebuffer dump captures
  the shell text.
- Start retains choice 1 and reaches `FIELDSTG` stage 474 (Junior street);
  Continue retains choice 2 and reaches `STGMCARD` with `loading=1`.
- Captures show the opaque striped row and the original cursor only on the
  selected item. The resident proportional font is legible, but is not an
  identical title-label sprite.
- Tools suite: **79 passed, 2 skipped** in the retained pytest log.

Representative local evidence: `v4-middle.png`, `v4-shell-defaults.png`,
`v4-continue-load.png`, `newgame-flow.png`, `restart-stubs-off.png`,
`shell-ttb.png`, `sequence60hz.log`, `newgame60hz.log` and `pytest-latest.log`.
These temporary files are not repository assets or durable CI artifacts.
Gamepad input and Switch presentation/input were not covered by this PC
keyboard smoke; the later OLED checkpoint below completes P9.2.
P9.3 effects and P9.4 persistence remain open.

## OLED preview.20 validation (2026-10-12)

The owner tested the nxlink game binary from `v0.6.5-preview.20`, source
`ecefe18e11d679a73ec0cf16a7592fffb5d0ed84`, in handheld mode. At the title,
Plus shows Start / Port options / Continue; D-pad selects Port options,
B opens the shell, both stubs show OFF, and X closes back to the middle row.
Owner confirmation: “Sim, aparece OFF e volta corretamente”. Together with
the PC TTB/input checkpoint above, this meets P9.2's entry/return done-when.

The mapping in `platforms/switch/psyz_backend/nx_input.c` is positional:
B = Cross (confirm), X = Triangle (close), A = Circle, Y = Square,
Plus = Start and Minus = Select. PC keyboard X is Cross; PC S is Triangle.
The tested physical controller model was not recorded; this does not
validate every controller model or PC gamepad configuration.

The switches remain safe no-op stubs. Hardware toggle effects and persistence
were not tested; P9.3/P9.4 remain open and the overall Phase 9 gate is open.

## Title-screen visual reference

The proposed title-screen labels and orange/blue Digivice button styling are
documented in [TITLE_MENU_DESIGN.md](TITLE_MENU_DESIGN.md). The linked atlas
is a visual concept; it does not indicate that the generated artwork is wired
into the game renderer.
