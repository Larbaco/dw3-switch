# Native menu composition implementation plan

Status (2026-10-22): the CPU dirty-region cache is the chosen design. It is
implemented once in shared code and used by both hosts:

- shared: `platforms/pc/src/title_options_cache.{c,h}` (retained canvas, dirty
  rectangles; no SDL or GL), proven by `tools/tests/test_options_cache.py`;
- PC SDL3 GPU host (`title_menu_overlay.c`): the cache is the **default**;
  `DW3_MENU_COMPOSITION=reference` selects the previous full-canvas path for
  comparison or fallback;
- Switch GLES host (`nx_title_overlay.c`): same cache, `glTexSubImage2D` per
  dirty rectangle.

The component-atlas GPU experiment (`gpu`) was removed: it was not faster
than the cache and failed the pixel gate (details in
[MENU_COMPOSITION_INVESTIGATION](MENU_COMPOSITION_INVESTIGATION.md#decision-and-switch-port-2026-10-22)).
Emulator and PC numbers are regression shields only. **OLED acceptance is
still pending** and belongs to the owner (checklist below).

## Scope

Optimize only the native title and Port options screens. Keep the current
appearance, logical inputs, US/EU text, per-host fade behavior, and the
reference path. Do not build a general menu framework, migrate PS1 gameplay
menus, change saves or assets, or overclock.

## Design

`title_options_cache.c` keeps two 960x720 RGBA canvases (2.76 MB each): the
static window (dim layer, bezel, title bar, rules, button hints) and the
working canvas. `dw3_options_cache_update()` compares the shared options view
(`dw3_pc_title_overlay_set_options_view`: cursor and three values) with what
the canvas holds and redraws only the affected rectangles through
`dw3_pc_title_overlay_options_component()`, restoring the static pixels under
each rectangle first:

| Change | Rectangles | Bytes |
|---|---|---|
| Cursor move | old row, new row (696x52), description (696x52) | 434,304 |
| Value change | one value cell (144x52) | 29,952 |
| Open, language/label change, context loss, resize, retry | whole canvas | 2,764,800 |
| Idle | none | 0 |

The host uploads the reported rectangles and calls `dw3_options_cache_commit()`.
A report that was not committed (failed upload) is repeated as a full canvas.
`dw3_options_cache_invalidate()` (texture lost or replaced) forces a full
resend without re-rasterizing; `dw3_options_cache_release()` frees both
canvases.

Switch (`nx_title_overlay.c`):

- the four title states are rasterized once per GL context, as before; the
  options state of the atlas starts transparent and is filled by the cache;
- while the options screen is visible (open or fading out) each draw calls
  `update_options_texture()`: compose, then one `glTexSubImage2D` per rectangle
  at `y = 2880 + rect.y`. GLES2 has no `GL_UNPACK_ROW_LENGTH`, so rectangles
  narrower than the canvas are first packed into one contiguous staging buffer
  (`dw3_options_cache_pack_rect`); a full-width rectangle is uploaded straight
  from the canvas. The context is GLES3 when available and GLES2 otherwise, so
  the packing path works on both;
- every full atlas upload and `dw3_nx_title_overlay_invalidate_gl()` (dock or
  handheld switch recreates the context) call `dw3_options_cache_invalidate()`;
- the cache is built (static layer raster plus one full upload) on the first
  draw after the title menu appears, so the one-time cost is not paid when the
  screen opens and opening is a normal dirty-region update. When the overlay
  draws nothing at all (restart hotkey, leaving the title) the canvases and the
  staging buffer are freed and the next title visit builds them again;
- if the canvases cannot be allocated the previous full-canvas path is used;
- timing (nxlink builds, or any build with the HUD): `psyz_log` lines
  `dw3-title-overlay: options update #N dirty|full|rebuild rects=.. bytes=..
  compose=.. us upload=.. us` (the first 8 updates of a session, and every
  full/rebuild update), then an aggregate line per 30 updates and when the
  screen fades out. The HUD gets one line, `menu us c=<compose> u=<upload>
  max=<worst>`. Times are CPU time around the compose and the GL call, not GPU
  execution; no `glFinish` or other wait is added. A release image does not
  read the clock and does not log from this path.

## Evidence gates

- Cached and reference output are byte-identical: unit test (all values and
  cursor positions, five Options languages, uncommitted/invalidate/release
  cases) and PC replay (284 PNG/PPM checkpoints, US and five EU languages,
  from `tools/replay/gen_menu_scenarios.py` in PR #178).
- Cursor move and value change cost a few percent of the reference upload and
  no full-canvas raster after the first open; idle costs nothing.
- US and EU Switch images (release and `--nxlink`) build; the release images
  contain no timing code. Ryubing smoke on both discs checks the screens and
  the log lines. Device behavior stays unverified until the owner tests it.

## OLED checklist (owner)

Compare an `--nxlink` build with the HUD against preview.28, same device and
settings:

1. Hold Up/Down for 10 s in Options. The cursor stops at the ends, so also tap
   Up/Down quickly for 10 s; HUD fps and the `menu us` line should stay flat.
2. Toggle every row: Widescreen, Fast-forward through OFF/2x/3x/4x, Restart
   hotkey; each value change should be visible at once.
3. Repeat on the EU build in French or German (and English).
4. Close and reopen several times: only the title menu's first appearance
   shows a `rebuild` line; opens and reopens must not.
5. Sleep and wake (HOME, resume), then dock and undock: the screen must come
   back intact after one full upload.
6. Restart hotkey (L1+R1+START+SELECT with the option on): the overlay must go
   away; opening Options again afterwards must look correct.
7. Pixel check of the window rim and the text, including mid-fade frames
   (open and close the screen and compare with preview.28).

## Roadmap status

This work is the open P5.2 Port options performance follow-up. P9.2
reachability stays done; this plan does not change the P6.10 gameplay-menu GPU
goal and does not declare a performance gate complete.
