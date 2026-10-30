# Provenance of the Switch glyphs

`a.png b.png x.png y.png l.png r.png zl.png zr.png` are the button glyphs the
port draws over the game's button icons when the Switch glyphs option is on
(P10.6, `docs/P10.6_GLYPHS.md`). Licence: CC0 1.0, see `LICENSE`.

## How they were made

They were drawn for this project by `tools/generate_switch_glyphs.py`, which
builds them from two things written in that script and nothing else:

- a disc: a pixel is part of the disc when its centre is within 5.5 px of the
  point (5, 5) (a plain distance test), the rim is the pixels of the disc that
  touch the outside, and the shade is the disc once more, one row lower;
- the letters A, B, X, Y, L and R as 5x7 pixel bitmaps, and Z, L and R as 3x5
  bitmaps for the two-letter glyphs ZL and ZR. The bitmaps are typed in the
  script, pixel by pixel; no font file is read.

`python3 tools/check_switch_glyphs.py` rebuilds them from the script and fails if
a file differs, so the PNGs are always what the script draws.

## What was NOT used

- No extracted or derived game asset: nothing from the owner's disc, the PS1
  font, the icon cells of the game's font, or any frame of the game. Nothing here
  was traced, converted, resampled, upscaled or run through img2img.
- No Nintendo or console asset: no Joy-Con or controller image, no system font
  or system icon, no screenshot of a console UI. A letter in a ring is the only
  similarity to a controller button, and that is a generic shape.
- No third-party artwork and no AI-generated image.

## Format

12x12, 8-bit palette, transparent index 0. The index is the role, not a colour of
the game: 0 clear, 1 outline, 2 body, 3 letter, 4 shade. The port maps the roles
to the palette indices of the disc's font at run time. To replace the art, put
PNGs with the same names and the same five palette colours in
`<config dir>/glyphs/` (`sdmc:/switch/dw3/glyphs/` on the Switch).
