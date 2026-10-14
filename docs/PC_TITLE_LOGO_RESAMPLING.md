# PC original-logo resampling

The PC SDL3 GPU title keeps the original logo's lettering, shapes, palette, and
composition. At title-image load, `port/options/title_logo_capture.c` reads the
US sprite sheet metadata and snapshots the PSY-Z base VRAM through
`Draw_StoreImage`. It reconstructs the complete static logo from sprites 0, 1,
and 2 plus the terminal shadow palette row of sprite 3. Those runtime pixels
are converted to RGBA and passed to `platforms/pc/src/title_logo_resample.c`.
No game artwork is embedded in the repository or executable. The filtered logo
is uploaded to a private GPU texture when the title animation reaches its stable
state; the animated background keeps its existing path and filter. The Switch
(US) uploads the same filtered logo to a GLES texture. SDL3 GL and non-US
builds retain the original logo rendering path.

The selected presentation is Lanczos2 with the fine local-contrast pass,
using a 4x4 source neighborhood at each destination pixel center. PS1 ABR2
shadow subtraction depends on the animated background under each texel; the
static logo texture approximates its transparent
edge as black at alpha 112. The screenshot confirms that this approximation is
visually acceptable in the captured title frame, but it is not an exact
reproduction of the shadow against every background frame. Mitchell-Netravali bicubic interpolation (B=C=1/3) is
available for a softer comparison. Both kernels interpolate premultiplied color and
alpha together, then convert back to straight RGBA, avoiding the dark fringes
produced by interpolating RGB against black transparent pixels. Final channels
are clamped to their valid ranges and each visible source neighborhood color
range, preventing new overshoot colors around high-contrast edges. These compact
mathematical implementations have no external library dependency or copied
third-party implementation.

The component accepts arbitrary source crop dimensions and output dimensions;
available modes are `DW3_PC_LOGO_MITCHELL`, `DW3_PC_LOGO_LANCZOS2`, and
`DW3_PC_LOGO_LANCZOS2_FINE`. The integration filters
the 300×136 logo crop to 720×326 once and caches it as a GPU texture. The
designer measured about 106ms for that Lanczos2 pass and 41ms for Mitchell on
an optimized Linux host build; this is load-time work, not a per-frame cost.

Local comparisons using only the owner's runtime artwork showed that nearest
sampling leaves large stair steps. Exact-color Scale3x retained too many of
those steps in shaded/antialiased source artwork. Mitchell smoothed the borders
with a softer small-text appearance. The owner selected the sharper Lanczos2
preview. A runtime US/Xvfb capture now confirms
the filtered logo in the title composition. The process still preserves the
original PS1 logo pixel art; only its sampling and display size change.
Scale3x was therefore not included in the runtime component. The mathematical
Scale3x comparison followed the [published algorithm](https://www.scale2x.it/algorithm)
without linking its library. No runtime asset or generated logo preview belongs
in Git. The final PC screenshot is kept outside the repository.

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

## Fine contrast refinement

The owner asked for one more refinement after the Lanczos2 upscale. The optional
`DW3_PC_LOGO_LANCZOS2_FINE` mode runs the existing Lanczos2 pass, then adds a mild
one-pixel local contrast pass to fully opaque lettering only. It uses a 3x3
Gaussian-weighted luminance average, amount 0.28, and a 1.5-level detail threshold.
The change scales RGB by a shared luminance ratio and clamps to the existing local
RGB/luminance ranges. Flat areas, translucent edges, alpha, silhouette, and
placement remain identical. Allocation failure leaves the valid original
Lanczos2 result as a fallback. The menu artwork is outside this component.

Local comparison of the original 300x136 runtime composition at 720x326 changed
63,578 RGB pixels, with maximum per-channel difference 17 and zero alpha changes.
The pass added about 3ms to this local load-time measurement. Visual review
accepted the moderate improvement in small text and inner contours without
invented details or new edge halos. A US runtime capture at 1280×720 confirms
the original logo with the selected filter in the stable three-button title
screen. The capture and original game pixels remain outside Git under
`tmp/evidence/pr121-restored/`.
