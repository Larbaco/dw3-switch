# preview.51 owner feedback: design for the fixes

Source: owner OLED test of v0.6.5-preview.51, 2026-10-28 (EU, combined
both-nxlink build). Status: **design only, nothing is implemented by this
note.** A later implementation agent follows it. Every item names the code, the
smallest fix, the log lines (AGENTS.md rule 12), the replay scenarios and the
OLED rows, and an owner question with a recommended default. The results the
owner passed are in [OLED_TEST_PLAN](OLED_TEST_PLAN.md) (section 8 below).

What this note could not do: no build, no replay and no emulator run were made
(light task). Everything marked **unverified** comes from reading code, git
history and the owner's log, not from a run.

Evidence read: the owner log
`/mnt/cabal/tmp/dw3-release51/build/switch-both-nxlink/dw3 (another copy).log`
(`battle_menu_memory=2`, `fast_forward=4`, `widescreen=1`, `nx_mesh:
battle_mode=gpu-tex`) and the top-level `port_options.cfg` next to it.

## 1. Battle menu, Items: the selection cursor is not visible

### What the cursor is

The battle's lists share one cursor task, `FIGHTSTG_updateCursor`
(`vendor/dw3_decomp/src/battle/battle_mode/cursor.c`). It has two parts:

- **Sprites** (`FIGHTSTG_drawCursorSprites`): a sprite per line, drawn only when
  the layout's `sprite` field is not -1.
- **The bar** (`FIGHTSTG_drawCursorBar`): a *vsync callback* (`GFX.vsyncFunc`)
  that builds a small ordering table of `DrawMove` primitives, copying a
  12 x 12 bar frame from VRAM (0.., 0xF4) onto the picked line and erasing the
  line left, then sends it with `BreakDraw()` / `ContinueDraw()` over the frame
  being drawn.

The layouts (`battle/battle_mode/item_menu.c`, `command_menu.c`):

| List | Layout | sprite | So the marker is |
|---|---|---|---|
| Command menu | `FIGHTSTG_commandCursor` `{6, 17, 110, 19, 34, ...}` | 34 | sprites + bar |
| **Items** | `FIGHTSTG_itemCursor` `{1, 14, 69, 14, -1, ...}` | **-1** | **the bar only** |
| Techniques | `FIGHTSTG_techCursor` (same numbers) | -1 | the bar only |
| Partner switch | `FIGHTSTG_switchCursor` `{2, 17, 78, 32, 47, ...}` | 47 | sprites + bar |

So the Items list (and the technique list) shows its selection **only** through
the vsync bar.

### The port's side of the bar (finding)

`port/psyz_missing/psyz_missing.c:226` defines `BreakDraw()` as `return 0`,
`ContinueDraw()` as an empty function and `IsIdleGPU()` as `return 0`.
`docs/P3.3_BATTLE.md` (follow-ups, item 3) says the same: "still no-ops". The
real PSY-Z library has no `ContinueDraw` for the host (only the decomp copy
`vendor/psyz/decomp/src/libgpu/break.c`, which pokes PS1 DMA registers and is
not part of the host library). With the stubs, `FIGHTSTG_drawCursorBar` builds
its OT every vsync and **never submits it**. A grep of `port/` and `platforms/`
for the bar (`cursorBar`, `0xF4`, `DrawMove`) finds no other path that draws it.

Reading only, this says the list bar may never have been drawn by the port. That
would fit a sprite-less list (Items, Techniques). Against it: the owner used
many techniques (D13, 31 minutes) without reporting a missing marker in the
technique list, which has the same layout. **Unverified; first thing to settle
is whether an older build ever drew it** (check A below). If the bar was drawn
somehow, the suspects below apply.

### Suspects, ranked

| # | Suspect | Landed | Why it could hide the bar | Confirm or clear with |
|---|---|---|---|---|
| 1 | **The stubbed bar path** (above): the bar is not part of the port at all, in any mode | P3.3 (always) | `ContinueDraw` is a no-op, nothing reaches the GPU | Check A and B below. If the old tech-list reference frames show no bar, it is a feature gap and not a regression: implement it (smallest fix below) |
| 2 | **gpu-tex battle render** (mesh FBO) | default since preview.30 (#193, 2026-10-22) | the bar is a VRAM-to-framebuffer copy over the *display* page; with the GLES mesh the 3D is in an FBO and the VRAM page may not be composed over it (#114 was the same family: a `DRAWENV` TILE painting over the mesh FBO) | Switch: **Minus + ZL + ZR** cycles the battle renderer (`docs/HOTKEYS.md`); open Items in `soft` and in `gpu-tex`. Or build with `-DDW3_NX_MESH_SOFT_BATTLE=ON`, or `DW3_MESH_SOFT=1` (`docs/P5.2_BATTLE_GPU.md`). The log line is `nx_mesh: battle_mode=<token>` |
| 3 | **Widescreen 16:9 battle** (patches 0049, 0074, 0075, 0076; gutter copies in `switch_gpu.c` after `Draw_MoveImage`) | 0049 early; 0074 preview.30-rc8; 0076 preview.31 | a `MoveImage`/`DrawMove` into the displayed rows now also repaints the gutter columns, and menus are re-anchored with `DW3_MENU_RIGHT`; the bar's target x (`task->params.x` 69) is not re-anchored, and the item list sits on the 320 area | `widescreen=0` in the region `port_options.cfg` (needs a relaunch, the row is locked in game), same battle, same list |
| 4 | **Turbo menu 1.5x, patch 0176 / #286** (adds `dw3_port_turbo_menu_frame()` in `FIGHTSTG_updateCursor`) | preview.41 (2026-10-27) | the only patch that touches `cursor.c`. By reading it only stamps a timestamp (no drawing). But the turbo policy skips draw frames it does not present (pass 2 #263 preview.35; pass 3 #331 preview.50), and the owner had `fast_forward=4`. A vsync callback that runs on a skipped frame would lose its bar | `fast_forward=0` (OFF) in the region file, same list; and with `fast_forward=4` but R3 turbo off (1x). Log: `pace dbg: ... skipped=N` near the Items open |
| 5 | **Battle cursor memory, `battle_menu_memory=2`** (patch 0067) | preview.30 (#198) | writes `cursor->sel`, `params.count` on the *technique* menu only; the Items menu is untouched. The command menu opens on the remembered line (`dw3-battle-menu: command menu opens on Items (remembered)`), so the owner reaches Items with the cursor already sitting on it, and `prevSel` is 0 at creation | `battle_menu_memory=0` and the same path. Cheap, and rules out the one patch that changes where the cursor starts |
| 6 | Options in battle hold (patch 0165, slice 2) | preview.37 | only polls Select on the command menu; holds the battle frame loop (`if (!held) RANDOM.next()`), no draw change | low; skip unless 1 to 5 clear |

By commit date the candidates in order of landing are 0067 (preview.30), gpu-tex
default (preview.30), 0049/0074-0076 widescreen (preview.30 to 31), turbo pass 2
(preview.35), 0176 (preview.41), turbo pass 3 (preview.50). **Top suspect:
number 1 combined with number 2 (the bar path is stubbed and the gpu-tex FBO
would hide it anyway); among real regressions, number 4 (#286 / patch 0176,
the only patch in `cursor.c`, preview.41).** Nothing here is proven.

### Confirming it, with no new code

A. **Reference frames.** The PC replays `us/eu-p79-memory-*` and
`us/eu-battle-cursor-persist` capture battle menus, and the technique list has
the same bar-only layout. Open the stored checkpoint pictures of the technique
list (under `tmp/evidence` or the replay references; never commit them) and
look for the highlighted bar. Bar present in an old picture: suspects 2 to 4.
Absent: suspect 1.

B. **A/B on the console**, one change at a time, the same battle, the same Items
list, one photo each (owner), log kept:

| Run | Change | Reads |
|---|---|---|
| 1 | none (baseline) | the bug |
| 2 | Minus + ZL + ZR to `soft` | suspect 2 |
| 3 | `widescreen=0` | suspect 3 |
| 4 | `fast_forward=0` | suspect 4 |
| 5 | `battle_menu_memory=0` | suspect 5 |
| 6 | preview.40 vs preview.41 NRO (same config) | suspect 4 as a regression |

C. **PC (not run here):** `DW3_VERSION=eu` replay with a forced battle opening
Items; the PC path draws through PSY-Z `DrawOTag`, which tells whether the PC
ever drew the bar.

### Smallest fix (once confirmed)

If the bar is a gap (suspect 1): implement `ContinueDraw` for the host in
`port/psyz_missing/psyz_missing.c` for exactly this use: walk the OT (`insaddr`)
and submit each `DR_MOVE` through the existing `Draw_MoveImage` (the framebuffer
rect), then return. `BreakDraw` must return a non-`-1` pointer (it does) and
`IsIdleGPU` 0. That is host code under `port/`, no decomp patch.

If the bar is drawn but hidden (suspect 2): composite the VRAM rows the bar
touches over the mesh FBO in the same place the gutter capture is composited
(`platforms/switch/psyz_backend/nx_mesh.c` dirty-box logic), or draw the bar as
a quad. If suspect 3 or 4: re-anchor the target x with `DW3_MENU_RIGHT`'s left
sibling, or stamp the turbo frame so the callback never lands on a skipped one.

An alternative that avoids all of this: give the item and technique cursors a
sprite (`sprite` field of the two layouts) through a `PORT_HOST` patch. It changes
how the original looks, so it is the fallback, not the first choice.

### Logs, replays, OLED

- New, once per Items open (not per frame):
  `dw3-battle-cursor: list=items count=N sel=S bar=submitted|dropped mode=<gpu-tex|soft> widescreen=W turbo=T`
  and, when the bar is submitted, `dw3-battle-cursor: bar moves submitted N`.
- Replay: a scenario that forces a battle and opens Items (like
  `us/eu-p79-memory-two-battles`, with `DW3_FORCE_ENEMY_HP`) with a checkpoint
  on the Items list: the bar must show (pixels differ from the list without it).
  US and EU (AGENTS.md rule 5).
- OLED rows (new): **B20** Items list in battle shows the highlighted bar on the
  picked line, moving with Up/Down, in `gpu-tex` and in `soft`, at 1x and under
  turbo. Proof: the lines above. **B20b** the technique list and the partner
  switch list show it too.

### 1a. Resolution (built 2026-10-29)

**Root cause: suspect 1, the stubbed bar path.** `ContinueDraw()` was a no-op in
`port/psyz_missing/psyz_missing.c` since P3.3, so `FIGHTSTG_drawCursorBar` built
its ordering table every vsync and never sent it: on every build, in every mode,
the Items and technique lists (`sprite` -1) had no selection marker. It is not a
regression of 0176, of the gpu-tex renderer, of widescreen or of the battle menu
memory. Evidence, all from the PC replay runner (new scenario `battle-list-bar`,
a full bag through the new test hook `DW3_FORCE_BATTLE_BAG`, patch 0189):

| Run | Result |
|---|---|
| main (8af7c95), PC, software raster, 1x, widescreen on, memory off | Items list and technique list: **no marker** on any line (checkpoints `items-line1`, `tech-line1`) |
| the same with `ContinueDraw` drawing its table | the round marker is on the picked line, the line left is empty again, in both lists; US and EU |
| `-t4` (turbo 4x), `-43` (widescreen off) | marker present; the turbo pictures equal the 1x ones (0 px differ) |
| Switch build in Ryubing, `gpu-tex` (default) and `soft` (`DW3_MESH_SOFT=1`) | `mode=gpu-tex` / `mode=soft`, `bar=submitted`, marker visible in the screenshots (US both modes, EU `gpu-tex`) |

Suspects 2 to 5 are cleared by the first row: the baseline has none of them (PC
has no mesh FBO, no turbo, memory off) and still shows no marker; with the fix the
marker shows under gpu-tex, soft, widescreen on and off and turbo. The old
tech-list reference frames (check A) do not exist as pictures (the committed
scenarios never checkpointed a technique list), so the owner question 1 gets its
answer from the code: **the technique list never had a marker either; it was
simply noticed in Items**, where the list is longer and the cursor matters more.

The fix: `ContinueDraw(inst_ot, cont_ot)` runs in `VSync(0)` from `drawFrame`
after the finished frame's `DrawSync` and before the flip (the vsync callback of
patch 0016), so it calls `DrawOTag(inst_ot)` and `DrawSync(0)` on the main thread:
the table's `DR_MOVE` copies of a 12 x 12 VRAM bar frame go through
`Draw_MoveImage` (both display pages, as the game does), onto the frame about to
be shown. On the Switch the same path flushes the mesh layer into VRAM first
(`switch_gpu.c`), which is why gpu-tex shows it. No game state changes.

Logs (patch 0189, once per list open and once per close):
`dw3-battle-cursor: open list=items mode=gpu-tex widescreen=1 turbo=1` and
`dw3-battle-cursor: close list=items count=7 sel=2 bar=submitted vsyncs=216`.
`bar=dropped` would mean the list was open and no table reached the GPU. The
design's "bar moves submitted N" is the `vsyncs=` count (one table per vsync).
The OLED row is **D15** (B20 is taken by the map view).

### Owner questions

1. Did the technique list show its bar on earlier previews (before preview.41)?
   Default if unknown: run B above, run 6 first.

## 2. Map view: no HUD bars, open zoomed

Code: `port/map_view/map_view.c`.

- The bars are `hud_build()` (line 785): a top bar (38 px: `MAP  ZOOM x`, the
  scaler line) and a bottom bar (16 px of hints) painted into the HUD canvas
  (`DW3_MV_HUD_W x DW3_MV_HUD_H`). They are shown while `s.show_hud` is 1;
  `show_hud` starts at 1 (line 1032) and **Circle toggles it** (line 1317).
- `show_hud` also gates **the game's name box** (`shown = s.hover > 0 &&
  s.show_hud ...`, line 1167). So the fix must not simply set it to 0.
- The open zoom: `s.view.step = 1; s.view.zoom = 0.0f` (line 1035), step 1 of
  `MV_ZOOM_TABLE = {FIT, 1.0, 1.25, 1.5, 2.0, 3.0}`
  (`map_view_core.c:700`). Scale at step n is `table[n] * vh / 240`. So it already
  opens at 1.0 (240 of the map's 320 rows visible), centred on the current area.

Smallest fix:

1. Split the flag: `show_hud` (bars) and `show_name` (name box). Start the bars
   at 0 and the name box at 1; the bars lose their only purpose of explaining
   buttons, so keep **Circle** as "show the help bars" (hidden by default) and
   keep the name box on whatever Circle does. Cursor, balloon, spots and the
   home mark are drawn before the `show_hud` test in `hud_build()` and stay.
2. Open at `s.view.step = 3` (1.5x). Why 1.5: at 1.0 the whole 392 x 320 map
   needs 33 % vertical panning and the towns and areas, drawn at the fixed
   size of zoom 1.0 (the #323 preview.48 fix), are small on the OLED; 2.0 shows
   only 56 % of the width in 16:9 (426 / 2 = 213 of 392 px) and the player loses
   the sense of where on the map the cursor is; 1.5 shows 73 % of the width and
   half the height, enough to read the neighbourhood of the cursor, and leaves
   the steps 2.0 and 3.0 one L1/R1 away. With a mod pack loaded (4x image) 2.0 is
   as sharp as 1.0 on the original; keep the default at 1.5 for both so it does
   not jump when a pack is added. Unverified on the OLED.
3. Do not start at FIT (step 0): the owner asked for zoomed.

Logs: the existing `dw3-map: open ...` line gains `zoom=1.50 hud=off`; the
`dw3-map: HUD on|off` line stays for the toggle.
Replay: `us/eu-map-view` and `-43` change references (the bars are in the
picture); update them once, outside git, and add a checkpoint `mv-open-zoom`.
OLED: B16 re-check, "no bars on open, the picture fills the screen, opens at
1.5x on your area; Circle brings the bars back".

Owner question: keep Circle as a toggle for the bars? **Default: yes** (a way
to see the button list is still useful), bars off at start.

## 3. Map upscale pack for both regions

### Do the US and EU map images differ?

From the code and the notes (no run):

- The base map is the same picture. `docs/MOD_MAP_UPSCALE.md` section 1 says the
  two bare bases (392 x 352, h0 and h1) are **byte-identical** between US and EU,
  and `docs/P6.32_MAP_ZOOM.md` row a4 records `eu-map-view: 392 x 352, 0
  differing bytes against the disc build`. Town and area names are not baked into
  the image: the game draws them as text windows (`STSTATUS_showMapArea`, the
  game's own name box), so the EU languages do not change the picture. The
  banner lettering in the image is the same pixels in both.
- The layout is the same, only the file ids differ: `FILE_BASE = {"us": 0x78A,
  "eu": 0x799}` (`tools/mods/extract_map_assets.py:47`, `map_view.c:24-29`); the
  US `files_us.json` has the four map files `STWMP0CS/0TM/1CS/1TM` at ids 1930
  to 1933 (3,416 / 173,640 / 3,416 / 133,580 bytes).
- **Not compared:** the reveal sprites (towns and visited areas, `towns_*` and
  `areas_*`). The doc says so. They are the sheet entries 1 and 2 of the same
  files and are probably the same too. The EU runtime was not available to this
  light task, so this stays **unverified** until the tool runs on both.

### Why the pack is US-only today

Not because of the loader. The pack is named per region
(`map/map_<region>_h<half>@Nx.png`) and `pack_open()` accepts a pack when its
manifest lists the running region (`port/mods/mod_pack.c:328`). The shipped pack
lists `["us"]` because the owner ran the tool with one `--runtime`.

### Proposal: one pack, shared files, per-region overrides

Layout (format 1 stays valid; the new names are extra):

```
mods/<pack>/
  mod.json                          "regions": ["us", "eu"]
  map/map_h<half>@<N>x.png          shared base (new)
  map/map_<region>_h<half>@<N>x.png override for one region (as today)
  map/reveal/h<half>/<kind>_<id>@<N>x.png            shared sprites (new)
  map/reveal/<region>_h<half>/<kind>_<id>@<N>x.png   override (as today)
```

Loader (`port/mods/mod_pack.c`): `dw3_mod_load_map()` tries
`map_<region>_h<half>@Nx.png` first, then `map_h<half>@Nx.png`;
`dw3_mod_load_sprite()` the same for the reveal folders. Size checks and logs
are unchanged; the log says `source=shared|region`. Old packs keep working.
`pack_open()` is unchanged. Unit test: a synthetic pack with only shared files
loads on both regions; a region file wins over the shared one.

Tool (`tools/mods/extract_map_assets.py`): accept both `--runtime` (it already
loops over regions), hash each extracted source PNG per half/kind/id, and write
**one** file when US and EU are identical (shared names) and per-region files
only for the pieces that differ. That also halves the two minutes of upscaling.
Default `regions` in the manifest is the list of runtimes given; with one
runtime it lists that region only (unchanged, honest). A `--shared` check prints
`identical: N of M pieces`; this prints the answer to the unverified question
above on the owner's machine.

Log: the one existing line per open (`dw3-map: mod: pack ... loaded: ...`) gains
`shared=A region=B`. Replay `eu-map-view-mod` runs the synthetic shared pack.
OLED: new row, EU build with the pack (check log `dw3-map: mod: pack ... loaded`
on EU).

"By default": a new owner step is not needed once the tool run covers both
discs; the pack is on when it is valid (`map_mod=1`, default).
Owner question: **default: build one shared pack from both discs**, and fall
back to per-region files only for pieces that differ.

## 4. Combined .nro: region picker without holding Minus

### What happens

`dw3_region_boot()` (`port/region/dw3_region_boot.c`) samples Minus for about
200 ms (`io_sample_minus`, `nx_region_boot.c:205`, 12 frames) only when both
regions are present *and* a `region_default` is stored. The owner's log has
`dw3-region: stored region_default=eu last_region=eu minus_held=0`: Minus was
held, the launcher consumed it (it shows a toast), and the game's pad did not
see it. `dw3_region_decide()` (`dw3_region.c:126`): Minus held, then the picker;
else `region_default`; else the picker.

### An Options row: Region at start: Ask / US / EU

- The key is the top-level `sdmc:/switch/dw3/port_options.cfg`'s
  `region_default` (`DW3_REGION_KEY_DEFAULT`, `ask | us | eu`). Per-region option
  files are in `us/` and `eu/`, so this row must not use the registry's
  per-region file. It reads and writes through `dw3_region_cfg_read()` /
  `dw3_region_cfg_write(base, DW3_REGION_NONE, 1, pref)` with `base =
  sdmc:/switch/dw3`. `dw3_region_cfg_write` already refuses to overwrite a file
  it cannot read and writes `end=1` last.
- Where: a row in the Gameplay group (or the group with the fewest rows), shown
  **only in the combined build** (both regions linked: the same compile-time
  switch the launcher uses). It is a registry row with a file-independent
  storage hook, like `battle_menu_memory` is a file-only key today but in the
  other file. Values: **Ask** (the picker every start, 3 s countdown), **US**,
  **EU**. Plan: `Ask` writes `region_default=ask`.
- Behaviour: takes effect at the next launch. The message line says so: "Takes
  effect the next time the game starts. Ask = choose US or EU at start."
  A switch of the value in game never changes the running region.
- Locked? No. It is allowed in game; the file write is the whole effect.
- Logs: `dw3-options: region_default ask|us|eu saved to <top-level file>`
  (once per change) and, at boot, the existing `dw3-region: stored
  region_default=...` line.
- Tests: a unit test on the cfg round trip (value kept, the other top-level keys
  kept, `end=1` last, never writing the per-region file), the registry row
  count/order tests, replay `options-persist` unchanged on single-region builds
  (the row is absent there).
- The wording of the plaque in five languages follows the other rows; the
  pre-rendered lettering is needed for the new words (Region at start, Ask, US,
  EU), as the Battle cursor row did.

### Another hold button the launcher does not take

Facts known: Sphaira shows a toast when Minus is held; the owner's report. R at
launch is taken by hbmenu for the applet override (`docs/SINGLE_BUILD.md`,
question 2: "R is taken by hbmenu"). **Everything else is unverified** because
this task could not check hbmenu or Sphaira source or run them:

| Button | Status | Note |
|---|---|---|
| Minus | intercepted (owner) | current |
| R | taken by hbmenu (SINGLE_BUILD) | no |
| L, X, Y, Plus, A, B | probably menu actions inside the launcher, **unverified** | do not choose without the launcher's source |
| ZL and ZR together | **unverified**, not known to be used by hbmenu or Sphaira at launch | a two-button hold is rare by accident; the in-game chord Minus+ZL+ZR (renderer cycle) is only read after boot, so no clash |
| Right stick click (R3) | **unverified** | R3 is the fast-forward toggle in game, read after boot |

Suggestion: add **ZL + ZR held** as an extra trigger next to Minus, log which
buttons the sampler saw, and keep Minus too. Tell the owner it is a try; the
Options row is the supported way. `io_sample_minus` becomes `io_sample_pick`:
it samples 12 frames and logs `dw3-region: pick buttons seen minus=M zl=Z zr=R
(12 frames)`. The line also proves in the next test whether the launcher passes
ZL/ZR through. A start with `region_default=ask` needs no button at all.

Owner question: **default: the row (Ask/US/EU) as the main answer, plus ZL+ZR
as a try.** Also: should the default for a fresh install with both regions stay
"Ask"? Default yes (today's behaviour).

## 5. Cheats group not discoverable

Cause: `dw3_optgroups_visible(cheats_on, out)` (`port/include/port_options_groups.h:53`)
skips the Cheats group when `cheats_on` is 0, and `po_cheats_on()`
(`port/options/port_options.c:186`) is `dw3_cheats_enabled()`, the master
switch. The master switch lives **on the Cheats screen**, so with the switch
off the way in is only the Square / Y shortcut, which is not shown on the list
(the old hint appeared once the master was on). The owner could not find it
(A13 stays open).

Smallest fix: list the group always.

- `dw3_optgroups_visible()`: drop the `g == DW3_OPTGRP_CHEATS && !cheats_on`
  skip (keep the parameter for the callers, ignore it, or remove it and update
  the three callers: `port_options.c:197` and `:1283`, `platforms/pc/src/title_overlay_art.c:606`)
  and `dw3_optlist_count()`.
- In game it stays as now: the entry exists and is greyed with the lock note
  ("cheats are locked in game": `po_open_cheats` already logs
  `dw3-port-options: cheats are locked in game`), via
  `DW3_PORT_OPTIONS_LOCK_CHEATS`.
- The list becomes 8 entries at most (Display, Sound, Gameplay, Saves, Cheats,
  Controls, Help, Walkthrough, only the built ones); `DW3_OPTLIST_VISIBLE` is 6,
  so the list scrolls (it already does); keep the cursor logic of
  `dw3_optlist_scroll_for`. The cached layers key on the page code, which does
  not change.
- The message strip of the Cheats plaque says: "Cheats: master switch off.
  Open to turn it on." when `cheats_on` is 0.
- Tests: `test_options_groups` (the group is listed with the master off, both
  title and in game), the title overlay cache tests (page counts), the replay
  scenarios that press Down N times in the list (the cheats group appears one
  slot earlier in the order, so any scenario that counts plaques after Saves
  moves: `us/eu-options-groups`, `cheats-*`; update references once).
- Log: `dw3-port-options: group list n=K (cheats listed, master off|on, locked
  in game 0|1)` on open.
- OLED: A13 stays open until the owner finds and opens it; add "Cheats is on the
  list with the master off".

Owner question: **default: list always at the title; in game greyed.**

## 6. Options background: use the game's menu background

### How the game's menus draw theirs

- The Items, Status, Techniques and Card folders screens run in the `STSTATUS`
  overlay. `STSTATUS_createMenu()` (`vendor/dw3_decomp/src/menus/field_menu/menu.c:104`)
  requests `FILE_STATUS_BG + 1` (+ 3 in the second half of the game) as the
  background archive; on `TASK_INIT` substate 1 it loads three TIM images with
  `TimLoader`: entries 1 and 2 to VRAM (0x380, 0x100) and (0x300, 0x100), and
  entry 0 to (0x280, 0) with the CLUT at (0x140, 0x100).
- `STSTATUS_drawBackground()` (same file, line 41) draws **sprite 0x1D of
  `FILE_STATUS_SPRITES`** with a `SpriteDrawer` on the screen layer, texture
  page (0x280, 0x100), at `(bgScroll, bgScroll)`; `bgScroll` goes up by one
  pixel every second frame and wraps at 0x60. So the background is one tiled
  sprite scrolling diagonally. It is drawn in `TASK_RUN` and `TASK_DONE`.
- The field menu list (`engine/menu/field_menu.c`) and its Options row (patch
  0160) are *not* in STSTATUS: they draw over the field, with no scrolling
  background, which is why the owner sees the difference.

### How Options draws now

- Title and in game: the native overlay (`platforms/pc/src/title_overlay_art.c`
  `options_list_screen`, shared by PC and Switch): the dimmed **title** backdrop
  (deeper in game, bit 31 of the page code), the navy plaques, the header
  plaque. Layers are retained: STATIC (backdrop, header, strip, hints) plus a
  per-group layer composed over a shared base (`group-over-base`), see
  `docs/PORT_OPTIONS.md` ("Groups and sub-screens" and the cache notes). The
  Help pages use `port/pages/` and the same shell.

### Proposal

Replace the dimmed title backdrop of the STATIC layer with the game's own menu
background, scrolling:

1. Source the sprite without the STSTATUS overlay being loaded: at overlay init
   read `FILE_STATUS_SPRITES` (US and EU ids from the file list) and
   `FILE_STATUS_BG + 1` through the existing file path (the same disc-file
   reader the map view uses: `dw3_mv_disc_load_entry`, `runtime_extract`),
   decode sprite 0x1D with the game's palette (the CLUT at (0x140, 0x100) from
   the bg archive entry 0), and keep one RGBA tile in memory (small: the sprite
   is a tile, about 96 px, **unverified size**; 0x60 is the wrap, so the tile
   period is 96).
2. Draw it tiled and scrolled by `(f/2) % 96` on both axes in the overlay's
   *background* layer, which is separate from the retained static canvas: the
   canvas stays static (the dirty-row cache is the whole point of #298/#327), the
   moving background is one extra quad with a texture scroll offset in the
   shader / quad UV, so no CPU recompose per frame. The STATIC layer then needs a
   transparent window area (the plaques are opaque; the backdrop part of the
   canvas becomes alpha 0 or a translucent dim).
3. Keep a mild dim (alpha about 0.35) under the plaques so the lettering reads;
   in game keep the near-opaque dim so the field menu behind does not show.
4. Same for Help, Cheats and the walkthrough, which share the shell.

Cost: **medium**. One new native resource (file read, decode, a tile texture,
about 40 KB), one extra textured quad per frame on the Switch GLES path and the
PC SDL3 path (UV offset only), the STATIC canvas rebuilt with a clear backdrop
(one-time), and new replay references for every Options screen (all US/EU
Options replays and `eu-options-lang-*` checkpoint pictures change). Risks:
mid-frame disc reads in game (it must come from the file cache, not a new CD
read: the field menu already holds `FILE_STATUS_SPRITES`? **unverified**, else
read it once at boot, as the title logo capture does in
`port/options/title_logo_capture.c`), and the memory budget (the Options layers
already hold up to 15.8 MB, `docs/PORT_OPTIONS.md`).

Cheaper first step (low risk, the same look for the field menu path): in game,
the Options overlay draws over the field menu (patch 0160), so a static capture
from the title backdrop machinery (`title_background_capture.c`) is already how
the title backdrop is obtained; the capture could read the STSTATUS background
from VRAM the first time the Items menu opens in the session. That needs the
player to open Items first, so it is **not recommended**; load it from the disc.

Logs: `dw3-options: menu background tile WxH from FILE_STATUS_SPRITES (id N,
sprite 0x1D) ready in N us`, once; `... not available (reason), using the title
backdrop`. Replay: Options checkpoints with the background (new references) and
a pixel check that the background moves between two frames 4 apart. OLED row:
"Options screens show the same scrolling background as Items".

Owner question: **default: yes, scrolling as in Items, dimmed slightly under the
plaques; static (no scroll) if the scroll costs frame time on the OLED.**

## 7. Stage sweep key in the wrong file

The owner's top-level `port_options.cfg` (next to the build log) has
`cheat_enabled=1`, `stage_sweep=1`, `end=1` and the other keys. The top-level
file is the **region picker's** file (`region_default`, `last_region`). The game
reads its options from the region folder (`sdmc:/switch/dw3/<us|eu>/port_options.cfg`,
`dw3_port_options_cfg_dir()`), and `sw_read_config()`
(`port/sweep/stage_sweep.c:316`) reads `dw3_port_options_file_int("stage_sweep")`
from that file only (plus the `DW3_STAGE_SWEEP` environment on the PC). So the
key in the top-level file is silently ignored, and the owner saw no sweep.

Smallest fix (log only; it is safe and one place):

In `sw_read_config()`, when the level is 0 after the normal read and the region
cfg dir has a parent directory (the combined build's base), load the parent's
`port_options.cfg` with `dw3_optcfg_load()` and `dw3_optcfg_get("stage_sweep")`.
If it is set and not 0, log once:

```
dw3-sweep: stage_sweep found in sdmc:/switch/dw3/port_options.cfg, ignored; put it in sdmc:/switch/dw3/eu/port_options.cfg
```

(the region file path comes from `dw3_port_options_cfg_dir()`). No change of
behaviour. A unit test with a temporary parent folder (`DW3_SWEEP_TEST` has a
seam for the directory), one line, once per boot.

Option B (accepting it): treat the top-level `stage_sweep` as a fallback with the
log `dw3-sweep: stage_sweep taken from <top-level file> (not the region file)`.
Risk: the sweep's progress and resume file live in the region folder
(`us/stage_sweep.txt`), a stray top-level key would run the sweep on both regions
without anyone looking, and the instruction `set it back to 0` would have to be
done in a file the owner does not otherwise edit. **Recommended default: A (log
and ignore)**; the OLED row G10 and `docs/P5.5_STAGE_SWEEP.md` already say which
file; add the sentence "not the top-level file" there. Owner question: take it
from the top-level file instead? Default no.

OLED: G10 US; proof line as above in the wrong-file case.

## 8. Bookkeeping in this PR

Marked in [OLED_TEST_PLAN](OLED_TEST_PLAN.md) as **PASSED** (owner OLED,
preview.51, 2026-10-28), and the matching [ROADMAP](ROADMAP.md) rows updated:

| Row | Result |
|---|---|
| H1 to H8 | PASSED **in English only**. FR, DE, IT and ES need native reviewers (the pages are English there, and the translated words are unreviewed) |
| E1.6c | PASSED: B and Start both confirm the EU language |
| A10 | PASSED: no hitch opening the groups |
| B18 | PASSED: item shop in 16:9 |
| B16 | PASSED for the right-stick zoom; HUD bars and open zoom are item 2 here |
| D13 (#188, EU) | PASSED: 31 minutes with warps and techs, underruns 0, key-on 5422 ok 0 failed, voices peaked 24/24, no failure |
| G10 (EU) | PASSED: `dw3-sweep: done 237/237 ok, failed: none; skipped: WSTAG460` (a weak stub), 0 `dw3-autosave:` lines. Expected look: the area-change grid animation and the grey 4:3 debug stage list between stages. **US still open** |
| E5.5 | PASSED again: profile selector cancel returns to the Switch menu |
| A13 (Cheats) | **stays open**: the owner could not find it (item 5) |

## Implementation order (suggestion)

1. Item 1 check A (reference frames) and the A/B runs: cheap, it decides the fix.
2. Items 5, 7, 2: small and independent (one PR each or one together).
3. Item 4 (Options row) and item 3 (loader fallback and tool).
4. Item 6 (background): the largest, needs new references.

## Owner questions, with the recommended default

**Orchestrator defaults, pending the owner's confirmation** (2026-10-29): the
owner asked the orchestrator to proceed, so every default in the table below is
what is being built. Each is cheap to change afterwards.

| # | Question | Default |
|---|---|---|
| 1 | Did the technique list show its bar before preview.41? | answered by the code and the PC A/B: it never had one (item 1, 1a) |
| 2 | Keep Circle as the map's bars toggle, bars off at start, open at 1.5x? | yes |
| 3 | One map pack for both regions, shared files, per-region only where different? | yes |
| 4 | Region row (Ask / US / EU) plus ZL+ZR held as a try? | yes; fresh install stays Ask |
| 5 | Cheats listed always at the title, greyed in game? | yes |
| 6 | Game menu background in Options, scrolling? | yes, dimmed; static if it costs frame time |
| 7 | Take `stage_sweep` from the top-level file? | no, log and ignore |

## 10. Built: PR 2, the small UX fixes (2026-10-29)

All with the orchestrator defaults (pending the owner's confirmation). Items 1 (PR 1,
#343), 3 (PR 3) and 6 (PR 4) are separate PRs.

| Item | What was built | Log, replay, OLED |
|---|---|---|
| 5 Cheats group | `dw3_optgroups_visible()` takes `DW3_OPTGRP_CHEATS_LISTED` (the build has the Cheats feature) instead of the master switch: the group is on the title list with the master off and greyed in game. The strip text is the group's existing description (a new "master off" sentence would need new lettering in five languages; not done). The list is 7 plaques: Cheats sits before Controls and Help, so recordings that walk the list use `GRP_CONTROLS` 5 and `GRP_HELP` 6 | `dw3-port-options: group list n=7 (cheats listed, master off, locked in game 0)`; replay `us/eu-options-cheats-listed`; OLED A16 (A13 stays open until the owner opens it). Every Options replay that shows the group list has a new picture (one more plaque) |
| 4 Region row | Registry row 10 "Region at start" (Ask / US / EU) in the Gameplay group, combined image only (`DW3_OPT_REGION_ROW`); stored as `region_default` of the top-level file through `dw3_region_cfg_write`; five-language lettering (`REGION_TEXT`); ZL + ZR held together as a second hold-to-pick, with `dw3-region: pick buttons seen minus=M zl=Z zr=R (12 frames)`. The ZL + ZR try is **unverified on the console** | `dw3-options: region_default us saved to ... (takes effect at the next start)`; replay `us/eu-options-region` (`--bin-both`); unit tests `test_options_persist.py::test_region_row_*`; OLED A17 |
| 7 stage_sweep | `sw_note_stray_key()` in `port/sweep/stage_sweep.c`: with the sweep off, a `stage_sweep` set in the top-level file logs once and changes nothing (option A) | `dw3-sweep: stage_sweep found in <top-level file>, ignored; put it in <region file>`; `test_stage_sweep.py`; OLED G10 note, P5.5 doc |
| 2 Map view | opens at 1.5x (`MAP_OPEN_STEP` 3) with the bars hidden; `show_hud` (the bars) split from `show_name` (the name box, always on); Circle shows / hides the bars | `dw3-map: open ... zoom=1.50 hud=off`, `dw3-map: HUD on|off (the bars; the name box stays)`; replay `us/eu-map-view` (checkpoints `mv-open`, `mv-zoom-100-open`, `mv-hud-on`); OLED B16; P6.32 section 12 |

The Region row needs the new registry row to be in `dw3_optgroups[Gameplay]` only in the
combined image, so the plain builds' Gameplay screens are unchanged.

## 11. Built: PR 3, one map pack for both regions (2026-10-29)

Orchestrator default 3: one shared pack, per-region files only where the pieces differ.

- Loader (`port/mods/mod_pack.c`): the region file first, then the shared file, for the
  map and the sprites; `Dw3ModImage.shared`; the open line gains `shared=A region=B`.
- Tool (`tools/mods/extract_map_assets.py`): two `--runtime` folders are compared piece by
  piece; identical pieces are upscaled once into the shared names, the others per region;
  `identical: N of M pieces` is printed (also with `--extract-only`).
- **The open question is answered** (run on both runtimes in `~/.cache/dw3-switch/runtime/{us,eu}`):
  the bare maps of both halves are identical; of the 94 reveal sprites per half, 91 (half 0)
  and 84 (half 1) are identical. The 13 that differ are `areas` 59, 60, 61 (both halves) and
  `towns` 0, 34, 46, 47, 48, 49, 50 of half 1 (the regional variants of a few town and area
  icons). So the shared pack is right, with 13 regional sprites.
- Docs: [MOD_MAP_UPSCALE](MOD_MAP_UPSCALE.md); replays `us/eu-map-view-mod-shared`;
  unit tests in `test_mod_pack.py`; OLED B17 (test the pack on US and EU).

## 12. Built: PR 4, the Options background (2026-10-29)

Orchestrator default 6, **static version**: "yes, dimmed; static (no scroll) if the scroll costs frame time".
The scroll does: the retained STATIC layer would be recomposed (about 160 ms) every frame. What
was built is described in [PORT_OPTIONS](PORT_OPTIONS.md#menu-background-p51-item-6-d4--a18): the game's tile
(sprite 0x1D) decoded once from the disc files (4 to 6 ms, logged), baked into the retained layer, dimmed
under the plaques; no per-frame cost, so there was nothing to measure beyond that one-time decode
(`dw3-options: menu background tile ... ready in N us`). The size answers the doc's "about 96 px,
unverified": the frame is a field of 48 x 48 parts repeating every 96 px. The scrolling follow-up needs a
textured quad with a UV offset under the canvas in both hosts, to be built only if the owner wants it
(OLED A18). The tile was checked in the PC replays (US and EU) and the Switch builds compile; the
Switch overlay is the same art code and the same corner-texel trick, **not run on the console**.
Every Options picture of the title changes; those references are refreshed after merge, not here.
