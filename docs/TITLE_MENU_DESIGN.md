# Title menu: orange and blue Digivice direction

This note records the selected visual direction for a future title-menu pass.
It is a design reference; the generated art is not wired into the renderer yet.

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
- **Text:** use the existing game pixel font, centered in each plaque, in a
  warm off-white with a one-pixel dark shadow. Render the labels exactly as
  **New Game**, **Options**, and **Load Game**.
- **Pixel treatment:** build for the 320x240 game viewport. Use integer-scale
  nearest-neighbor enlargement; avoid smooth gradients, antialiasing, blur,
  and thin subpixel details.

The balance should stay close to the original title screen: the blue body ties
into the game's Digital World palette, while orange echoes the logo and marks
focus. Do not add large character illustrations, extra menu entries, or a new
brand mark over the original logo.

## Reference art

- [Selected button atlas](assets/title-menu/digivice-orange-blue-atlas.png):
  three plaque variations; each row pairs idle (left) with selected (right).
- [Title-screen mockup](assets/title-menu/title-screen-orange-blue-mockup.png):
  visual composition with the specified labels and the first row selected.

The atlas and mockup are concept images. Crop/rebuild the plaques as needed for
the game's renderer, then validate text fit, contrast, and selection feedback
at native game resolution before treating the design as implemented.
