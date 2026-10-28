# Title menu: orange and blue Digivice direction

This note records the selected visual direction for the PC title-menu pass.
The PC SDL3 GPU renderer uses a transparent 960×720 RGBA presentation overlay
over the original title image. The animated background and original pixel-art
logo remain game sprites; on the GPU path, the logo sprites and glint are drawn
at 3× internal resolution and uniformly transformed to make room for the menu.
The overlay replaces the vanilla menu and footer sprites. The Switch draws the
same overlay and logo texture over its GLES present
(`platforms/switch/psyz_backend/nx_title_overlay.c`); SDL3 GL retains the
existing PS1-primitive fallback. The EU disc is supported in every `LANGUAGE`
bank; see [EU disc](#eu-disc) below. How the overlay art is drawn (plaque
geometry, label rasterization, presentation shaders) is in
[PC_TITLE_OVERLAY_ART](PC_TITLE_OVERLAY_ART.md). The concept PNGs below are references and are not loaded by the
game.

## Menu labels and order

The title screen presents three vertically stacked choices, in this order:

1. **New Game**
2. **Options**
3. **Load Game**

The first row is selected on the initial menu. Up and Down move the focus one row
at a time; the selected treatment follows focus. Keep the game's existing title
logo, background, 4:3 composition, and copyright area unobstructed.

## Visual specification

- **Shape:** wide, shallow, rectangular hardware-like plaques with clipped,
  reinforced corners. Keep the silhouette compact so all three rows fit below
  the logo and above the copyright line.
- **Face:** very dark indigo/navy blue with a subtle blue-black inset. The center
  stays quiet and flat so the original pixel font remains readable over it.
- **Frame:** layered steel-blue bevels and a thin amber/orange inner rail. Small
  square amber fasteners at the corners and restrained segmented marks at the
  sides suggest a Digivice/data terminal without adding character art.
- **Idle state:** subdued blue bevel and amber trim, no glow. Maintain enough
  edge contrast against the title background to distinguish each row.
- **Selected state:** illuminate the amber/orange rails and corner indicators;
  add a short, hard-edged pixel glow just outside the frame. Keep its size and
  position identical to the idle state so focus changes do not shift the text.
- **Text:** use a clean, legible pixel-style face, centered in each plaque, in
  a warm off-white with a one-pixel dark shadow. Render the labels exactly as
  **New Game**, **Options**, and **Load Game**.
- **Pixel treatment:** the user's PC presentation request overrides the
  original 320×240 implementation target. Draw the overlay in a native
  960×720 canvas and present it in a centered 4:3 output viewport. Keep plaque
  geometry on integer canvas pixels; pre-rasterized label coverage uses
  antialiasing for legibility. At native 720p the overlay is sampled 1:1.
  Avoid blur, gradients, and subpixel geometry.

The balance should stay close to the original title screen: the blue body ties
into the game's Digital World palette, while orange echoes the logo and marks
focus. Do not add large character illustrations, extra menu entries, or a new
brand mark over the original logo.

## Reference art

- [Selected button atlas](assets/title-menu/digivice-orange-blue-atlas.png):
  three plaque variations; each row pairs idle (left) with selected (right).
- The title-screen mockup is kept outside Git: it shows the original logo,
  background, and legal text, which are game artwork.

The atlas is a concept image. The game uses a compact authored
overlay atlas rather than cropping that image: 500×54 navy plaques, layered
steel-blue bevels, clipped corner clamps with square amber fasteners, internal
rails, and readable centered labels. The Press Start prompt and three menu
states are packed into one 960×2880 RGBA texture and selected by UV range. A
six-vertex quad presents one 960×720 state in one GPU draw. Idle rails stay
subdued; selection lights rails and fasteners without shifting text or
geometry. The menu overlay remains active until the options shell opens or the
title flow fades out. The title patch keeps vanilla New Game and Continue
choice values: **Load Game** enters the original Continue flow and
memory-card screen; **Options** opens the P9.2 shell. On the native
overlay that shell is a fifth canvas state. Since P9.9 it is a list of
groups and one sub-screen per group drawn with these plaques, the same focus cue
and lettering ([PORT_OPTIONS.md](PORT_OPTIONS.md#groups-and-sub-screens-p99));
the Cheats screen still has the older window in the game's own style
([PORT_OPTIONS.md](PORT_OPTIONS.md#options-screen-2026-10-14)). The PC and
Switch menus start on New Game.

The original logo art is not redrawn or resampled into a new font. Its game
sprites and glint receive a uniform 4/5 SpriteDrawer scale about the pivot
(132, -10) on the GPU path (patch `0129`; it was 21/25 about (157, -23), 5%
larger than the overlay's logo, which made the logo shrink when the overlay
took over). The overlay logo is 2.4 canvas px per sprite px (0.8 of the 3x
canvas), the US 300x136 crop or the EU opaque box at its own place in the
same 720x326 texture, so the sprite logo and the overlay logo are one size at
one place from the first frame; they are rasterized through the existing 3× internal
renderer. No new logo filter is applied. The original background code is not
modified. The GPU overlay replaces the original menu and footer sprites with
the native-resolution labels and three-line credits.

## EU disc

The EU title bank is per `LANGUAGE`, and the overlay follows it. At title
load, `port/options/title_logo_capture.c` turns the bank's own menu sprites
(`STDWTITL_menuSprites[LANGUAGE]`: PRESS START, New Game, Continue) into 8-bit
coverage masks: the word only, without its dark plate (part 0), luminance-ramped
and scaled to the lettering height. `dw3_pc_title_overlay_set_label` hands them
to `title_overlay_art.c`, which draws them with the existing plaque styling;
the Switch and PC SDL3 GPU hosts re-upload the four states when
`dw3_pc_title_overlay_generation` changes. The port-only Options row has no
original sprite, so it uses authored DejaVu lettering per language (Options,
Optionen, Opciones, Opzioni), generated by
`tools/generate_pc_title_lettering.py`. The three credit lines under the menu
are drawn on EU exactly as on US, in every `LANGUAGE` bank; see
[Title credits](#title-credits) below.

`LANGUAGE` as the EU banks read, observed in Ryubing with a temporary local
override (before the country select was linked in P6.2): 0 Japanese logo and words
(Options stays English), 1 English with the "DIGIMON WORLD 3" logo, 2 English
"DIGIMON WORLD 2003" (the default), 3 French, 4 Italian, 5 German, 6 Spanish.
US visuals are unchanged.

Linux/Vulkan build and US-runtime validation used an Xvfb display with the
window set to 1280×720 and internal content at 960×720. The final menu frame
is saved at `/tmp/dw3-pc-title-menu-redo/evidence-redesign/final/postbuild-menu.png`;
the matching run log is `postbuild-capture.log`. Earlier captures show the New Game
state and P9.2 shell. The shell hides the menu overlay; automated shell return
and Load Game selection were not confirmed. The paired frames did not show a
measurable background-pixel change during the short capture interval, so this
validates composition and source preservation, not visible motion. Only
Linux/Vulkan was run; Windows/DXIL and macOS/Metal runtime checks remain open.
The Switch build is validated in Ryubing on Xvfb with the US disc (logo,
focus states, Options shell, Load Game to the memory-card screen and back,
New Game, edge fades, idle timeout to the attract movie) and with the EU disc
in all seven `LANGUAGE` banks (title and menu), plus the full flow on the
default language 2. OLED validation is pending. Captures are kept outside Git
under `tmp/evidence/pr127-switch-title/`.
See [PORT_OPTIONS.md](PORT_OPTIONS.md) and [ROADMAP.md](ROADMAP.md).

## Title credits

The credits under the menu are three lines of antialiased 12 px lettering
(`credit1`-`credit3` in `tools/generate_pc_title_lettering.py`), drawn by
`title_screen()` in `platforms/pc/src/title_overlay_art.c` into the footer
plaque (x=252..707, y=610..668 of the 960x720 canvas; text lines at y=619,
635, 651). The game's own credits are logo sprites 6 (the dark plate) and 5
(the text, at its animation's CLUT row, 7 once settled), drawn at (29,209) and
(20,202) of the 320x240 frame.

**The US disc and the EU disc carry the same credits.** Sprites 6 and 5 were
composed from VRAM for the US disc and for each of the seven EU `LANGUAGE`
banks (0 Japanese logo, 1 English "DIGIMON WORLD 3" logo, 2 English "2003",
3 French, 4 Italian, 5 German, 6 Spanish; a temporary local hook picked the
bank, nothing of it is committed) and the 320x240 composites are
byte-identical in all eight cases: the language changes the logo and the menu
words, never the credits. There is no distributor, publisher or licensor line
on either disc, so EU needs no lines of its own. The crops are kept outside
the repository under `tmp/evidence/eu-title-credits-2026-10-23/original-sprites/`.

| Line | Original US sprite | Original EU sprites (banks 0-6) | Native lines (US and EU) |
|---|---|---|---|
| 1 | `© Akiyoshi Hongo • Toei Animation` | same | `© Akiyoshi Hongo · Toei Animation` |
| 2 | `TM &© 1997-2002  Bandai.` | same | `TM & © 1997–2002 Bandai.` |
| 3 | `© 2002  Bandai Co.,Ltd` | same | `© 2002 Bandai Co.,Ltd` |

The sprite font pads the first two lines with wide spaces and draws a
2x2-pixel middle dot and a hyphen-length dash; the native lines carry the same
characters with ordinary spacing, a middle dot and an en dash. One real
change: the native third line used to read `© 2002 Bandai Co., Ltd.` (a space
after the comma and a final period that the sprite does not have) and now
reads `© 2002 Bandai Co.,Ltd`, exactly the original. The small 8 px sprite
font makes the punctuation after `Bandai` and `Co` hard to read (it draws a
period with a one-pixel tail); they were read as `.` and `.,` and the crops
are in the evidence folder for a second look.

How the EU disc gets them:

- `port/options/title_logo_capture.c` turns the overlay footer on for every
  EU bank (it used to turn it off).
- Patch `0073` (`menus/title/logo.c`) draws sprites 6/5 only when the overlay
  is not drawing the footer: `dw3_port_title_footer_native()` is 0 (footer
  switched off, or the host overlay has no GPU resources: the PC pipeline,
  texture, buffer, sampler and window, or the Switch GLES program) or the Port
  options screen is open (as in 0056). US takes the same path, so it also
  falls back to the sprites now; before, a US build whose overlay could not
  start had no credits at all. Builds without the native overlay (SDL3 GL)
  never reach that code and keep the sprites untouched.
- The footer is part of the 4:3 title canvas, so it follows the logo and the
  menu: the Switch tints it with the title's edge fade, the PC presents it in
  the 4:3 viewport.

Layout check (the footer plaque is y=610..668 and its top rule y=600; the last
menu plaque ends y=573; the canvas ends y=720; x=252..707 is inside the 4:3
canvas and centered): nothing else is drawn in that band in any bank, in
either aspect.

- PC SDL3 GPU, US and EU English / French / German / Italian / Spanish, in a
  1280x960 screen (4:3), a 1280x720 window (16:9, the `DW3_PC_INTERNAL_RES=3`
  default) and 1920x1080 / 2560x1080 screens (the window stays 1280x720,
  centered): the footer sits under the menu with a free band of 26 px, never
  touching a plaque or the bottom edge. On EU 16:9 the side bars are black
  (the widescreen background extension is US-only; unchanged by this change).
- Switch GPU path in Ryubing (docked 16:9, replay NRO): US and EU English /
  French / German / Italian / Spanish, the footer is drawn inside the 4:3
  title area with the same placement. Handheld mode and the OLED were not
  run; the overlay has no handheld-specific code, only the 4:3 viewport fit.
- **OLED (2026-10-23, preview.30-rc8, owner):** the EU credits appear as
  expected; verified on the OLED. The owner did not say handheld or docked
  (so neither is claimed), and no log or capture came with the report, so the
  EU languages shown are not recorded. The handheld/docked gap above stays
  open for the layout.
- The fallback (footer off) was run on the PC for US and EU: the game's sprites
  draw as before, over the same band, without touching the menu.
- Banks 0 and 1 are not reachable from the country select (it offers 2-6), so
  they were run only with the temporary local `LANGUAGE` hook, PC only.

Captures: `tmp/evidence/eu-title-credits-2026-10-23/`.
