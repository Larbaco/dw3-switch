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
bank; see [EU disc](#eu-disc) below. The concept PNGs below are references and are not loaded by the
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
overlay that shell is a fifth canvas state, a full screen in the style of
the game's own windows rather than of these plaques; see
[PORT_OPTIONS.md](PORT_OPTIONS.md#options-screen-2026-10-14). The PC and
Switch menus start on New Game.

The original logo art is not redrawn or resampled into a new font. Its game
sprites and glint receive a uniform 21/25 SpriteDrawer scale and matching
pivot on the GPU path; they are rasterized through the existing 3× internal
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
`tools/generate_pc_title_lettering.py`. On EU the built-in US credit lines are
dropped and the game keeps drawing its own credit sprites (logo sprites 5/6).

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
