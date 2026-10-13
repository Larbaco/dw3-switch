# Title menu: orange and blue Digivice direction

This note records the selected visual direction for the PC title-menu pass.
The PC SDL3 GPU renderer uses a transparent 960×720 RGBA presentation overlay
over the original title image. It preserves the game's logo, background, and
copyright. SDL3 GL retains the earlier 320×240 PS1-primitive fallback. The
concept PNGs below remain reference art and are not loaded by the game. Switch
retains its existing title-row presentation.

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
- [Title-screen mockup](assets/title-menu/title-screen-orange-blue-mockup.png):
  visual composition with the specified labels and the first row selected.

The atlas and mockup are concept images. The game uses a compact authored
overlay atlas rather than cropping these images: 500×54 navy plaques, layered
steel-blue bevels, clipped corner clamps with square amber fasteners, internal
rails, and readable centered labels. The three plaque states are packed into
one 960×2160 RGBA texture and selected by UV range. A six-vertex quad presents
the active 960×720 state in one GPU draw. Idle rails stay subdued; selection
lights rails and fasteners without shifting text or geometry. The title patch
keeps vanilla New Game and Continue choice values: **Load Game** enters the
original Continue flow and memory-card screen; **Options** opens the existing
P9.2 shell, whose experimental toggles remain no-op stubs. The PC menu starts
on New Game; Switch selection and row styling are unchanged.

Build and Xvfb/Vulkan validation with the US runtime confirmed New Game and
Options selection and shell open in an exact 1280×720 window at 3× internal
resolution (960×720). New Game and Options show the full title image; the shell
capture confirms the plaques hide while P9.2 is open. Automated shell return
and Load Game selection were not confirmed. Captures are
under `/tmp/dw3-pc-title-menu-redo/evidence-redesign/`:
`hires-final-new-game.png`, `hires-verify-options.png`, and
`hires-verify-options-shell.png`. Load Game selection capture is still
outstanding. Only Linux/Vulkan was run; Windows/DXIL and macOS/Metal runtime
checks remain open. See [PORT_OPTIONS.md](PORT_OPTIONS.md) and
[ROADMAP.md](ROADMAP.md).
