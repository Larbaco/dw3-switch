# Output filter (Options > Display > Output filter)

How the 2D picture is stretched from the PS1 pixels to the screen. Added 2026-11-01 as the item
recommended by [P6.11 section 10.6](P6.11_INTERNAL_RES.md#106-recommendation) (point 4): docked, a
320x240 picture is stretched 4.5x to 1080p, so with nearest sampling a PS1 pixel is 4 or 5 output
pixels wide in turn, which is the roughness the owner sees on a TV. Rule 8 (AGENTS.md): it is a
Display row, registry 29, key `output_filter`. No decomp patch, no game-logic change, no replay
impact of its own (the PS1 frame is the same; only the window grab changes, and only when the filter
is on).

| Value | Word in the file | What it does |
|---|---|---|
| **Nearest** (default) | `nearest` | What the port always did. The programs and the blits are untouched. |
| Bilinear | `bilinear` | A linear blend of the four nearest texels. Blurs at every scale, 3x handheld included. |
| **Sharp bilinear** | `sharp` | With P = floor(output pixels per source pixel), a source pixel keeps its colour over all of itself except a seam of 1/P of a pixel at each edge, where it blends with its neighbour. Same picture as an integer nearest prescale by P followed by a bilinear stretch to the target. At an integer scale (handheld 3x) it is exactly Nearest. |

Left / Right and Confirm step Nearest, Bilinear, Sharp bilinear and wrap. The row is not locked in
game and a change shows on the next present. Labels in EN / FR / DE / IT / ES and a PT-BR draft
(`EXTRA_ROWS['of']` in `tools/generate_pc_title_lettering.py`, `tools/l10n/ptbr_options.py`); the three
values are product names, the same in every language (the Map scaler's `opt_val_sc_*` words).
`DW3_OUTPUT_FILTER=0|1|2` pins the row for one run (a test aid; the replay runner uses it).

**Default: Nearest.** The Switch uploads the VRAM as `GL_NEAREST` textures and the PC blits with
`SDL_GPU_FILTER_NEAREST`, so Nearest is today's look on both and nothing changes until the player picks
a filter. Proposal for the owner (not done): if the OLED rows below say Sharp bilinear is better docked,
flip the default for a new generation (`DW3_DEFAULTS_GEN` 3, as D2 did), ideally *docked only* (handheld
is already 3x and sharp bilinear equals Nearest there, so a single default of Sharp bilinear costs
nothing handheld and fixes the TV). That needs the migration text and a dock-independent row, so it is
the owner's call.

## 1. What the present does (studied 2026-11-01)

### Switch (GLES 3, `platforms/switch/psyz_backend/`)

`nx_gpu.c` `dw3_nx_gpu_present` draws the 1024x512 RGB555 VRAM (CPU raster) in three steps:

1. The VRAM sub-rectangle that is shown (or the wide texture: the display between its 53 px gutters,
   P6.1) is uploaded into an `R16UI` texture (P6.22 pass 4); the shader expands RGB555 itself.
2. With a battle mesh (`dw3_nx_mesh_has_content`): `dw3_nx_mesh_present` blits the mesh FBO (RGBA8,
   1024x512 at 1x, plus the gutter FBO) with `BLIT_FS`, then the CPU layer is drawn over it with
   `UI_FS`, which drops the cleared black so the mesh shows (and keeps black inside `u_keep`, the
   partner view).
3. Without a mesh: one opaque `QUAD_FS` draw.

The output is 1280x720 handheld and 1920x1080 docked; the picture is fitted by aspect (4:3 = 960x720
handheld, 1440x1080 docked: **3x and 4.5x**; 16:9 widescreen 426x240 = 3.005x and 4.5x). The native
overlays (title menu, Options, the full-screen map) draw after the picture at output resolution and are
not touched.

`usampler2D` textures cannot be filtered by the sampler, so the filtered programs fetch the texels
themselves.

### PC (SDL3 GPU, `vendor/psyz`)

`PlatformBackend_Present` blits the render target (or the gutter target) to the swapchain with
`SDL_BlitGPUTexture(..., SDL_GPU_FILTER_NEAREST)`. The window is 1280x960 by default (4.0x for 4:3,
3.005x for 16:9 widescreen); any other window size is a non-integer stretch.

## 2. Implementation

**Switch** (`nx_output_filter.h`, `nx_present16.h`, `nx_gpu.c`, `nx_mesh.c`). Three filtered
fragment programs, built the first time a filter is wanted (`present_build_filter`,
`dw3_nx_mesh_set_filter`); with Nearest nothing is compiled or bound that was not before. One-pass
sharp-bilinear math (`flt_tap`): the fragment's position `t` in texels, `P = max(floor(scale), 1)` with
`scale` = viewport size over the shown texels (`u_flt`), `f = fract(t) - 0.5`, `r = 0.5 - 0.5/P`,
`pos = floor(t) + (f - clamp(f, -r, r)) * P + 0.5`; then four `texelFetch` taps clamped to the shown
rectangle (so a neighbouring part of VRAM never bleeds in) and a `mix`. In the plateau the weights are
exactly zero and it is one fetch. All of it in `highp`.

* Opaque program (movies, a picture with no battle mesh): `DW3_NX_P16_QUAD_FS_F`.
* Overlay program (CPU layer over the mesh): `DW3_NX_P16_UI_FS_F`. A texel the Nearest program drops
  (black without the mask bit, outside `u_keep`) stays dropped, decided by the texel under the
  fragment, and does not enter the blend of its neighbours (weights renormalised), so a sprite keeps
  its hard outline and has no dark seam against the 3D.
* Mesh blit: `DW3_NX_MESH_BLIT_FS_F` (RGBA8 `sampler2D`, `texelFetch`), same math, so the 3D and the
  2D agree. It uses the mesh target's own size, so it already works at an internal resolution above 1x
  (P6.11): the scale is then smaller and the seams are plain bilinear.
* The ES 2 context and `DW3_PRESENT_16=0` (the CPU pack) keep Nearest and log it; a program that does not
  build on the driver logs once and the present stays on Nearest.

**PC** (psyz fork `Larbaco/psyz` branch `dw3-output-filter`, `Psyz_VideoSetOutputFilter`). Bilinear is a
`SDL_GPU_FILTER_LINEAR` final blit. Sharp bilinear first blits the picture with NEAREST into a texture
`P` times its size (`P = min(dst.w / src.w, dst.h / src.h)`, 2 to 16; below 2 it is plain bilinear), then
blits that with LINEAR: the same picture as the analytic form, with no shader to bake for SPIR-V / DXIL
/ MSL. The row is handed to the renderer from the overlay frame callback
(`platforms/pc/src/title_menu_overlay.c`, `output_filter_sync`), so a change shows on the next frame. The
native canvas path (`DW3_PC_INTERNAL_RES=3`) is not filtered.

### Widescreen gutters and 4:3

Nothing special is needed. In widescreen the shown rectangle is the display plus both gutters (the wide
texture on the Switch, `gutter_render` on the PC), already one rectangle; the scale is the viewport over
its width (3.005x handheld, 4.5x docked) and the clamp keeps the picture's own edge, not the texture's.
In 4:3 the rectangle is the 320x240 display. Handheld 4:3 is exactly 3x, so Sharp bilinear equals
Nearest there; the 16:9 handheld picture at 3.005x shows seams of 1/3 pixel (invisible). Movies are
24-bit and go through the same present: they are filtered like the rest.

## 3. Cost

CPU: none (same upload, same draws; the choice of program is one integer compare). GPU: the filtered
programs do 1 fetch where a fragment is in the middle of a texel and 4 where it is in a seam. At the
docked 4.5x (P = 4) the seam is 1/4 of a texel per axis, so 44% of the fragments take 4 taps and 56% one:
2.3 fetches on average against 1 for Nearest, plus the RGB555 expansion per tap. Bilinear is 4 taps for
every fragment. Measured on this PC with Mesa llvmpipe (a CPU rasteriser, so only the ratios mean
anything; 1440x1080 for a 320x240 picture, median of 41 draws, `output_filter_gpu_test --bench`):

| Program | Nearest | Bilinear | Sharp bilinear |
|---|---|---|---|
| Opaque | 4.0 ms | 4.5 ms | 5.3 ms |
| Overlay | 3.3 ms | 6.2 ms | 6.7 ms |

On the Switch's own GPU the cost is read with the P6.11 probe (`DW3_NX_RES_PROBE`, `pres_gpu_us`, the
GPU time of the present: upload, mesh blit, UI draw); rows OF3 and OF4 in the [OLED test plan](OLED_TEST_PLAN.md)
compare the three values. Estimated from the probe's section 3.3 numbers (docked 1.5 to 2.4 Gfrag/s): a
2.07 Mpixel draw with about 2.3 taps is well under 1 ms.

## 4. Side finding: the Nearest programs are not an exact nearest

The existing programs compute the sample position in `mediump float` (deliberately, P6.22 pass 4: "the
same texel as before") and let the sampler pick the texel. On Mesa llvmpipe the test finds that the Nearest
program picks the neighbouring texel for **8.3% of the pixels** at 4.5x (6.4% at 3x, 2.1% at 2x) against an
ideal nearest (the texel under the pixel centre, in double). The filtered programs use `highp` and
`texelFetch` and are compared with the ideal. The cause was not isolated (`mediump` coordinate arithmetic or
the sampler's own rounding); whether the Switch driver does the same is not known, the OLED rows say (a
Nearest picture with uneven 4 / 5 pixel columns is expected from the 4.5x alone). Not changed here: the default
must stay as it is, and a more exact Nearest would be a separate, replay-visible change.

## 5. Tests

* `tools/output_filter_gpu_test.c` (`tools/tests/test_output_filter.py`): the real shader strings on Mesa
  GL ES 3.2 (llvmpipe, surfaceless EGL) against a CPU reference in double, for the opaque, overlay and
  mesh programs at 1440x1080, 960x720, 1920x1080, 640x480, 1152x1080 and 400x300 on a random VRAM with a
  different random around the rectangle (a tap that leaks shows). Every channel within 2 (measured 0.6);
  discarded pixels of the overlay program equal the reference's; Sharp at 3x and 2x is identical to the ideal
  Nearest; at 4.5x the longest run of pixels off Nearest in a row is 2.
* `tools/tests/test_options_persist.py` and `tools/tests/port_options_main.c`: the row, its words, its
  default, a bad word.
* Replay: `us/eu-output-sharp-bilinear` (widescreen), `-43` (4:3) and `us/eu-output-bilinear`, the battle
  report in a 1200x900 window (`DW3_PC_WINDOW`, a test aid; the Xvfb screen is 1280x960), with refs of their
  own. The window grab is the only reference a filter changes. The runner exports `DW3_OUTPUT_FILTER=0` for
  every scenario that does not seed `output_filter`, so no other reference moves if the default does.
  `options-display` steps the row (Nearest to Sharp bilinear) and reads it back after a restart.

## 6. Log lines

| Line | When |
|---|---|
| `dw3-present: output filter <nearest\|bilinear\|sharp>` | At the first present and whenever the effective filter changes (Switch `nx_gpu.c`, PC `title_menu_overlay.c`) |
| `dw3-present: output filter <name> scale 4.500x4.500 (320x240 texels on 1440x1080), prescale 4x4` | Switch, with a filter on, when the viewport or the shown rectangle changes |
| `dw3-present: output filter <name> is not available (<why>); using nearest` | The ES 2 context, `DW3_PRESENT_16=0`, or a program that did not build (Switch); an unsupported renderer (PC) |
| `dw3-port-options: output filter <name>` | A step of the row |
| `dw3-options: file keys ... output_filter=<word>` | Boot, with the other file keys |

## 7. Files

`port/include/port_options.h`, `port_options_groups.h`, `port/options/port_options.c`;
`platforms/pc/src/title_overlay_art.c`, `title_overlay_lettering.inc` (generated),
`title_menu_overlay.c`; `platforms/switch/psyz_backend/nx_output_filter.h` (new), `nx_present16.h`,
`nx_gpu.c`, `nx_mesh.c`, `nx_mesh.h`; `vendor/psyz` (`dw3-output-filter`); help text in the five
languages and the PT-BR draft; `tools/output_filter_gpu_test.c`, `tools/tests/test_output_filter.py`.
