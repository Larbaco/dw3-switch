# PC original-logo resampling

The PC title keeps the original logo's lettering, shapes, palette, and
composition. `platforms/pc/src/title_logo_resample.c` reconstructs a larger RGBA
image from the runtime pixels; it embeds no game artwork and never changes the
animated background filter. The integration supplies the original RGBA and owns
GPU texture upload, placement, and visibility.

The selected presentation is Lanczos2, using a 4x4 source neighborhood at each
destination pixel center. Mitchell-Netravali bicubic interpolation (B=C=1/3) is
available for a softer comparison. Both kernels interpolate premultiplied color and
alpha together, then convert back to straight RGBA, avoiding the dark fringes
produced by interpolating RGB against black transparent pixels. Final channels
are clamped to their valid ranges and each visible source neighborhood color
range, preventing new overshoot colors around high-contrast edges. These compact mathematical implementations
have no external library dependency or copied third-party implementation.

The component accepts arbitrary source crop dimensions and output dimensions;
use `DW3_PC_LOGO_MITCHELL` or `DW3_PC_LOGO_LANCZOS2`. Do this once when the logo
source changes, then cache the resulting texture. A local 300x136-to-700x317
Mitchell pass took about 26ms with an optimized Linux host build; this is a
load-time measurement, not a per-frame budget guarantee.

Local comparisons using only the owner's runtime artwork showed that nearest
sampling leaves large stair steps. Exact-color Scale3x retained too many of
those steps in shaded/antialiased source artwork. Mitchell smoothed the borders with a softer small-text appearance. The owner
selected the sharper Lanczos2 preview; premultiplied alpha and local color clamps
limit edge fringes, with runtime ringing inspection still required.
Scale3x was therefore not included in the runtime component. The mathematical
Scale3x comparison followed the [published algorithm](https://www.scale2x.it/algorithm)
without linking its library. No runtime asset or generated logo preview belongs
in Git. The final PC screenshot remains the integration validation gate.

## US original composition facts

`TLOGOUSA.BIN` is file 2195 (0x0893). Archive entry 0 is the sprite sheet;
entry 1 holds the TIM archive. Original title artwork comprises:

- Sprite 0 at (19,24): local bounds 0..256 by 0..92.
- Sprite 1 at (19,111): local bounds 0..248 by 0..32.
- Sprite 2 at (253,100), drawn by the lit glint task: the colored numeral 3,
  56x56. It is essential to preserve the complete game name.
- Sprite 3 at (8,28): logo/numeral shadow and palette animation, local bounds
  (9,-6)..(309,130), 4-bit CLUT at x=640 plus each part's clutX and
  y=part.clutY plus animation row. The terminal row is 11.

Sprites 0/1/2 use 8-bit pixels and the alternate CLUT at (0,496).
Sprite 3 uses semi-transparency mode ABR2: only texels with the PS1 STP bit set
use subtraction; non-STP nonzero texels stay opaque. Its part geometry never
changes during palette animation. Sprite 4 is the animated glint over the
numeral; its task eventually stops drawing it. Sprites 5/6 occupy the copyright
area, separate from the logo, and are replaced by the native legal footer.

The complete original logo/shadow union is x=17..317, y=22..158 (300x136).
The original asset is local runtime data, not a new logo drawing or shipped art.
