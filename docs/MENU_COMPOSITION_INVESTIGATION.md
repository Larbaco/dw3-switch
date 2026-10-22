# Native menu composition investigation

Status (2026-10-22): the CPU dirty-region cache won and is implemented for the
PC and Switch hosts; see [Decision and Switch port](#decision-and-switch-port-2026-10-22)
and [MENU_COMPOSITION_PLAN](MENU_COMPOSITION_PLAN.md). The text up to the
PC prototype section is the 2026-10-15 proposal and is kept as written; its
GPU-atlas recommendation was tried and dropped. Inspected on 2026-10-15 at
`main` `4b2068fc451e86952e1495c62b96a96427f651f1` and PR [#170](https://github.com/Larbaco/dw3-switch/pull/170)
head `b43be1be87d8a6612abaceb5d31c0112784df3c6`. #170 subsequently landed
as `09eeda6bd2fb0bf87735612336f92f2959325f5f`, a partial optimization;
measurements below retain the pre-merge baseline for comparison.

## Owner result and scope

The owner tested the EU World 2003 #170 candidate with the debug HUD on the
OLED: changing Port options items improved, but still feels somewhat slow.
No exact FPS, frame-time distribution, input latency or per-stage profile was
supplied. This confirms a partial improvement, not a performance completion.
P9.2's reachability/return milestone stays met; its performance follow-up stays
open. No new OLED connection, asset transfer, save operation or release is part
of this investigation.

The first target is the native title/Port options overlay. It already uses GPU
presentation; moving the whole game's PS1 menus to hardware is the separate
P6.10 project. Rewriting PSY-Z, adopting a new engine, changing menu navigation,
and changing the approved Options appearance are unnecessary for this fix.

## What the implementation actually does

- `port/options/port_options.c:po_push_view` exports cursor and values when the
  shared shell changes. The shell owns behavior; hosts own presentation.
- `platforms/pc/src/title_overlay_art.c` rasterizes a straight-alpha RGBA
  960×720 canvas in C. `box`, `word`, `segment` and `pointer` call `pixel`;
  `pixel` evaluates alpha compositing with integer divisions even for opaque
  fills. The Options screen repeatedly covers much of the same window with
  bezel, rim and body rectangles, in addition to the full-screen dim layer.
  This work runs synchronously on the game/present thread.
- On inspected `main`, every options cursor/value change increments the same
  generation as title language changes. Both hosts regenerate five canvases,
  allocate/map a buffer of 13,824,000 bytes (13.18 MiB), then upload the entire
  960×3600 texture. Ordinary title focus changes select an already-cached state
  by UV, without this regeneration; Options movement follows a different path.
- #170 separates the options generation. Ordinary cursor/value changes redraw
  only state 4 and upload 960×720 RGBA: 2,764,800 bytes (2.64 MiB), one fifth of
  the old transfer. It still clears and rasterizes the whole Options canvas.
  Switch also explicitly clears this buffer before the generator clears it.
- `platforms/switch/psyz_backend/nx_title_overlay.c:ensure_gl` allocates/frees
  that CPU buffer and calls `glTexSubImage2D` against the existing atlas. There
  is no explicit GPU wait here; driver synchronization is possible but has not
  been timed. Rendering is a textured quad after the game picture and before
  the HUD; the Options fade is a uniform opacity on the composed texture.
- `platforms/pc/src/title_menu_overlay.c:refresh_art` creates/maps a fresh SDL
  transfer buffer and submits a copy on refresh, then calls
  `SDL_WaitForGPUIdle`. This explicit whole-device wait affects PC evidence;
  it is not a Switch API or proof of the Switch bottleneck. The PC overlay
  currently has no edge-fade tint input (`set_fade` is a no-op), whereas the
  Switch shader applies tint and Options opacity; preserve each current host
  behavior and treat any PC fade-parity change as explicit additional scope.
- The Switch debug HUD independently rebuilds and uploads its own texture on
  every visible frame (`nx_debug_overlay.c:dw3_nx_debug_overlay_draw`). Keep
  HUD visibility identical in A/B runs and measure it separately before
  assigning all visible slowdown to menu work.

Thus the CPU raster and update volume are demonstrably avoidable work. Code
inspection alone does not say how much of an OLED frame belongs to either,
allocation, driver synchronization, HUD, the title backdrop or frame pacing.
At 60 refreshes/s, one-canvas uploads would amount to 165.9 MB/s, but the shell
updates on semantic changes, not necessarily every frame: this is a workload
bound, not measured bandwidth or an explanation by itself.

## Evidence already available

The #170 author reported two Ryubing/Xvfb/LLVMpipe runs per variant. Cursor
movement changed from US 37.9/41.2 to 50.2/50.0 FPS and EU 34.1/39.4 to
41.2/50.1 FPS. Holding Down for two seconds yielded US 43/48 → 54/49 and EU
44/45 → 50/50. These are game `frame dt` log samples, not GPU-stage timing.
The author disclosed a concurrent emulator and no OLED or other EU-language
coverage in those runs. Source: [measurement comment](https://github.com/Larbaco/dw3-switch/pull/170#issuecomment-6194189693).

The subsequent owner EU OLED result above is stronger evidence of the user
experience, but supplies no numeric magnitude. Neither emulator numbers nor
host-only CPU measurements establish OLED upload time.

## Separate CPU microbenchmark

A separate test agent compiled an asset-free harness with GCC 13.3.0 on Linux
x86_64, Intel family 6/model 141, using `-O2` and `-O3`, `-D__SWITCH__`, and
`-lm`. The define selects Switch hint artwork, not AArch64 instructions.
`CLOCK_MONOTONIC_RAW` surrounds artwork drawing plus the redundant host clear;
allocation, process startup, GPU upload/present, game and HUD are excluded.
Cursor cycles through three positions and values vary; title states use built-in
English masks/footer, not captured EU masks. Warm = 200 draws after discarding
one; cold = first draw in 25 independent processes including first-touch faults.
No pinning or frequency changes; sequential runs can have host-load noise.

At `-O3`, warm median / p95 / maximum in milliseconds:

- `main`, five canvases: **45.090 / 47.276 / 51.527**.
- `main`, Options alone: **25.866 / 27.226 / 36.040**.
- #170, Options alone: **26.004 / 27.480 / 31.250**.

The measured CPU reduction from five canvases to one is about 42.3%, not 80%:
Options is much more expensive than each title state. #170 does not change the
Options drawing algorithm. Cold medians were 49.204 ms for the five-canvas
batch and 26.771 ms for #170's one-canvas update. `-O2` warm medians were
46.054 ms and 26.085 ms respectively. These are host generator timings, not
OLED frame times or predictions of Switch FPS.

A separate untimed instrumented copy counted 2,795,830 `pixel` calls for one
Options canvas (cursor 0, all values OFF): 1,652,136 opaque writes, 1,117,460
translucent writes and 26,234 rejected calls, on only 691,200 output pixels.
This confirms repeated large fills and generic opaque blending are actual
CPU work. It does not prove which portion dominates on the OLED.

The [persistent evidence](evidence/menu-art-cpu-2026-10-15/README.md) contains
results, all raw timing samples, pixel counts and exact source/environment
metadata. The asset-free
[harness and reproduction instructions](../tools/menu_art_benchmark/README.md)
extract only the required authored artwork sources from Git into a temporary
directory; they do not require game assets, a display, submodules or the SDK.
From the repository root:

```sh
python3 tools/menu_art_benchmark/run.py --out /tmp/dw3-menu-art-rerun
```

The saved percentile uses `floor(.95*(n-1))` on sorted samples. No optimized
raster or GPU composition prototype was measured in this investigation.

## Alternatives

### Keep the full canvas, optimize its CPU raster

Opaque-fill fast paths, retained scratch memory and cached static backdrop can
reduce CPU work with a small diff. Retaining #170's generation split is useful
regardless of later design. A cached static canvas still needs a 2.64 MiB CPU
copy and full upload for each update if the public interface remains a single
bitmap. This is a low-risk fallback or measurement experiment, not the preferred
long-term composition model.

### Dirty rectangles and partial uploads

Restore old/new focus-row rectangles, the changed value and the description
from a cached static image; rasterize only those regions and subupload them.
Cursor movement affects the old row, new row and description, not just the new
pointer. For example, two 696×52 row boxes plus a hypothetical 696×48
message strip total 423,168 RGBA bytes, about 15% of a canvas, before padding.
This is an illustrative bound; actual glyph/shadow bounds must be calculated.

A clipped raster API is necessary: rendering the entire CPU canvas followed by
a small GPU upload only removes the upload portion. Restore all antialiased
edges and use correct row pitch; GLES3 unpack-row-length and SDL transfer
layout differ. Multiple small copies can be coalesced after profiling. This
approach keeps existing output and its one-quad fade, but incurs invalidation
bookkeeping which grows with each future setting. It is a reasonable bounded
fallback if the component prototype cannot match visual output.

### Pre-render every complete Options state

Three cursor positions × two widescreen values × four turbo values (OFF,
2x, 3x, 4x) × two restart values give 48 canvases: 132,710,400 bytes
(126.56 MiB) in RGBA, excluding
other title states and CPU staging. That would eliminate per-move raster/upload
by selecting UVs, just as title focus already does. Boot generation, texture
limits/packing, languages and additional settings make this a poor general
solution. Do not pre-render the Cartesian product of all settings.

### Cache components and compose textured quads on the GPU (recommended)

Generate the static window/dim layer and finite artwork once per language,
platform button-hint style and graphics-context lifetime. Put row labels,
value words, pointer/focus treatment and descriptions into an atlas. The
existing authored RLE words and runtime EU label masks can supply those images;
no original game images are added to Git. A value/cursor change selects UVs,
positions and tint in a small draw list. It need not upload any pixels.

A static background plus dynamic components scales with the number of labels
and value choices, rather than their combinations. Budget memory by actual
packed dimensions and padding in the prototype. A separate 960×720 RGBA output
texture costs 2.64 MiB; this is an extra GPU allocation unless it replaces the
current Options atlas region. Avoid claiming a net memory saving before
measuring the complete atlas, staging buffers and output target.

For current fades, compose into an offscreen color target only when semantic
state changes, then present that texture with the existing group opacity.
Applying the fade to every overlapping component separately changes the image
at intermediate alpha: it does not equal fading the precomposed screen. Define
straight/premultiplied alpha explicitly for the offscreen target and final
blend, including the semi-transparent dim layer; otherwise borders and text
can develop halos. GPU blend rounding may differ from the C integer raster.
Validate opaque and intermediate fade frames, not only a static screenshot.

A direct batched overlay every frame can remove the offscreen pass later if
nonoverlapping components or correct group opacity are established. Initially,
keep the single final quad and bound GPU recomposition to state changes. Do
not issue one draw per glyph or recreate textures/pipelines per cursor move.
Retain a CPU raster fallback for SDL3 GL/unsupported/allocation-failure paths.

### Draw all decoration and text procedurally on the GPU

Colored quads/triangles and a glyph coverage atlas could remove even the
one-time CPU artwork raster. This is valuable for a broad, expanding menu
system, but introduces shape tessellation, text layout and shader maintenance
across GLES and SDL GPU shader formats. It is more work than the current
three-row screen requires. Start with cached visual components; introduce
procedural geometry only where it makes a measured difference.

## Incremental delivery plan

1. #170 was integrated as a partial optimization at `09eeda6`. The existing
   correctness review, green CI and owner-confirmed improvement support this
   baseline; the remaining performance task stays explicitly open. Its original
   unqualified "cursor moves without a hitch" description must not be read as
   OLED performance acceptance; the actual owner result is recorded above.
2. Add bounded per-event timings/counters in a separate implementation PR:
   raster, allocation/staging, copy-call/submit CPU time, present/frame time,
   bytes uploaded, and generation/cache rebuilds. Record cursor, value-change,
   open/close and language-switch events separately. CPU duration around GL
   upload does not equal GPU execution time; use supported asynchronous GPU
   timing only if available, without inserting a per-frame finish/wait.
3. Implement a shared component/layout description under `port/ui/`, with
   host GPU adapters under `platforms/pc/` and `platforms/switch/`. Current art
   in the PC tree is already shared by Switch; preserve it as a reference and
   move common ownership only in an explicit, reviewable change. The game
   shell, logical inputs and persistence stay in `port/options/`.
4. Prototype static-plus-components on PC SDL GPU, behind a comparison switch.
   Preserve styling, geometry, text, backdrop and group fade. Batch quads and
   retain atlas/vertex/transfer resources; upload only initial or invalidated
   assets. Serialize shared generators on their current single thread.
5. Port the proven composition to GLES, restore all GL state used by the next
   layer, and rebuild atlas/output objects after context invalidation. Avoid
   adding broad field-menu GPU work under P6.10 to this optimization.
6. Have a separate test agent capture PC/replay and Switch-emulator evidence;
   then provide a HUD-enabled EU nxlink candidate for an owner OLED A/B check.
   No release is required merely to benchmark a candidate.

## Acceptance gates

- **PC correctness:** deterministic P6.15 scripts cover Options entry, each
  cursor position, all value words, return, and restart, in US plus the five
  playable EU languages. Add missing scenarios to existing replay coverage;
  current title/language replays alone do not prove Options transitions.
  Compare cached and reference raster captures at settled and partial-fade
  frames; document any justified alpha-rounding tolerance. Exercise fallback,
  resize and lifecycle invalidation without GPU waits in the hot path.
- **Counters:** after warm-up a cursor/value change causes no CPU canvas
  raster and no texture upload on the component path. Idle causes no cache
  rebuild. Language/context changes rebuild only what they invalidate. Keep
  a bounded atlas/output allocation budget and report peak staging memory.
- **Performance:** repeated identical-input runs report median/p95/max frame
  times and input-to-visible-update behavior for idle, repeated cursor moves
  and toggles. Same resolution, renderer, HUD visibility, limiter and stock
  clocks; no concurrent emulator. PC/LLVMpipe is a regression shield, not a
  substitute for OLED acceptance. Report CPU and GPU measurements separately.
- **Owner OLED:** EU English first with installed own-disc runtime and saves
  preserved; move through rows, toggle values and return repeatedly. Target
  stable 60 Hz (16.7 ms budget) while navigating and no perceptible selection
  hitch relative to idle; compare to #170 on the same device/settings. Confirm
  readability, fade, restart-to-language-select and title return. Other EU
  languages/US remain explicit coverage items until checked; no claim that
  this menu test meets the original P5.2 field/battle benchmark.

The immediate owner test has already happened: "improved, still slow" is enough
to proceed with investigation. Further owner testing should wait for a new
candidate with the proposed path and comparable instrumentation.

## PC prototype validation, 2026-10-22

Branch `codex/menu-composition-luna` first added two opt-in PC composition
modes in `platforms/pc/src/title_menu_overlay.c`, `cpu` (retained canvas,
dirty-region raster and upload) and `gpu` (component atlas, 375 prebuilt draw
lists, composition into a retained target). Method: Release PC builds (US and
EU), Xvfb 1280x960, lavapipe pinned with
`VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json`, dummy audio, the
`tools/replay/gen_menu_scenarios.py` recordings from PR #178 (US plus five EU
languages, 284 PNG/PPM checkpoints), references generated by the reference
path. Timing: 10 extra cursor/value cycles, no screenshots, one warm-up round
plus 3 repeats, mode order rotated, `us-title`, `eu-lang-en`, `eu-lang-fr`.
Host CPU call time only; GPU execution time is unavailable and not inferred.

| Mode | Replay vs reference | Cursor move (median / p95, overlay CPU) | Value change (median) | Bytes per update |
|---|---|---|---|---|
| reference | baseline, 284/284 | ~29.5 ms / ~33.9 ms (US) | ~29.6 ms | 2,764,800 |
| `cpu` | 284/284, 0 px differ | ~1.0 ms / ~1.3 ms | ~0.34 ms | 434,304 / 29,952 |
| `gpu` | 194/284, 90 FAIL (3,564 px each, 0.29%) | ~1.2-1.5 ms (mostly `SDL_WaitForGPUIdle`) | ~1.1-1.6 ms | 0 after a ~3.6 MB build on open |

The `gpu` failures sat on the window rim (x=111/112, y=143/144), 9-12 levels
per channel, so the P6.15 gate rejected them, and it was no faster than `cpu`.

## Decision and Switch port, 2026-10-22

Decision (owner delegated): `cpu` is the path.

**Removed:** the `gpu` mode and everything that served only it: the 960x1812
atlas, the retained render target, the 375 draw lists, the premultiplied
composite pipeline, `DW3_MENU_GPU_TEST_FAIL`, and the GPU branches of
`set_state`/`render_overlay`. `DW3_MENU_COMPOSITION=gpu` now prints a notice
and runs the cache. **Kept:** the reference path (`DW3_MENU_COMPOSITION=reference`,
also `DW3_MENU_FORCE_FALLBACK`, and the automatic fallback when the cache
cannot allocate or upload) and `DW3_MENU_PROFILE_CSV`.

**Default:** the cache is the default on PC because it is pixel-identical to
the reference: 284/284 checkpoints with 0 px differing (US plus English,
French, German, Italian and Spanish EU), unset, `reference`, `cpu` and the
retired `gpu` value alike; plus the exhaustive unit test below.

**Shared code** (`platforms/pc/src/title_options_cache.{c,h}`, compiled into
both images, no SDL/GL): retained static and working canvases, the dirty
rectangles per change (cursor move: old row, new row, description, 434,304
bytes; value change: one cell, 29,952 bytes; open, language change, context
loss, retry: whole canvas), `commit`/`invalidate`/`release`, and
`pack_rect` for hosts that need contiguous rows. The component renderer
(`dw3_pc_title_overlay_options_component`) is unchanged.
`tools/tests/test_options_cache.py` compares the canvas, and a texture mirror
fed only by the reported rectangles, with a full reference render after every
step: every cursor/value state in English and German, random multi-row jumps
in all five Options languages, a lost-commit retry, `invalidate`, and
`release`.

**Switch** (`nx_title_overlay.c`): the four title states stay as they were;
the options region of the 960x3600 atlas is filled by the cache. Each draw
while the screen is visible composes and uploads the reported rectangles with
`glTexSubImage2D`, packed contiguously (the context is GLES3 when available,
GLES2 otherwise, and `GL_UNPACK_ROW_LENGTH` is not in GLES2). Full upload on
open, language change, atlas re-upload and context recreation (dock/handheld,
sleep); the canvases are freed when the overlay draws nothing (restart hotkey,
leaving the title); fallback to the old full-canvas path if allocation fails.
Release images contain no timing code; `--nxlink` and HUD builds log
`dw3-title-overlay: options update ... compose=.. us upload=.. us` and show
`menu us c=.. u=.. max=..` on the HUD. These are CPU call times: no GPU wait is
added, so GPU execution is still unmeasured.

PC timing with #178's `profile_menu.py` (this host, loaded, so the reference
is a little slower than the first run; median / p95 / max of the overlay CPU
time per event, `us-title`, `eu-lang-en`, `eu-lang-fr`, 3 repeats):

| Event | reference | cache | Bytes (reference / cache) |
|---|---|---|---|
| cursor move | 35.0-37.5 ms / 38.5-43.8 ms / 66 ms | 1.2-1.4 ms / 1.9-2.5 ms / 6.7 ms | 2,764,800 / 434,304 |
| value change | 34.9-37.8 ms / 40-44.7 ms / 63 ms | 0.43-0.51 ms / 1.3-1.4 ms / 4.2 ms | 2,764,800 / 29,952 |
| open (full rebuild) | 14.7-18.9 ms | 17.7-18.7 ms | |
| idle | 0 ms | 0 ms (1-3 us) | 0 |

Ryubing 1.3.339-canary smoke (own data roots, Xvfb, 640x360 window, Docked,
OpenGL/LLVMpipe, `--nxlink` builds with the HUD; `new` is this branch, `base`
is `main` at c1a7c78 plus a scratch per-update timing line that is not
committed). Driver: title, Options, cursor over the three rows, each value
(Widescreen, Fast-forward OFF/2x/3x/4x, Restart hotkey), 10 s of Down/Up taps
(22-29 presses), 10 s hold Down and Up, close, reopen. Guest log via
`svcOutputDebugString`; median of the per-update CPU time (for `new`, the median of the first 9 logged updates plus the 30-update aggregates: average 7.7-8.8 ms compose and 1.8-5.9 ms upload; the game runs at
about 15 fps there, so absolute times are inflated roughly 7x to 50x against
the PC and only the ratios mean anything):

| Disc / language | base: raster + upload per update | new: dirty compose + upload (median) | New bytes per update | New first build (title menu appears) |
|---|---|---|---|---|
| US | 198 ms + 4.5 ms (n=46) | 7.8 ms + 1.4 ms (n=44) | 434,304 / 29,952 | 345 ms + 7.9 ms |
| EU French | 185 ms + 3.3 ms (n=46) | 8.9 ms + 3.7 ms (n=44) | same | 361 ms + 5.9 ms |
| EU German | 206 ms + 6.0 ms (n=44) | 7.5 ms + 1.5 ms (n=44) | same | 288 ms + 8.1 ms |
| EU English | not run | 8.9 ms + 1.7 ms (n=42) | same | 302 ms + 3.2 ms |

(`base` uploads 2,764,800 bytes per update.) The window pixels of 18 matching
screenshots per disc (open, every cursor row and value, the closing frame,
reopen) are identical between `base` and `new` for US, French and German; the
only differences are the animated backdrop outside the window and the extra
HUD line. Dock/handheld was toggled from the Ryubing status bar with the
screen open: it stayed intact, but the game image never calls
`dw3_nx_gpu_update_mode`, so no GL context was recreated and the
`invalidate` hook itself is only covered by the unit test. The 60-pump frame
`dt` samples are dominated by emulator host load and are not a reliable
metric: in the rapid-tap windows the slowest 60-pump window of `base` was 29.8
fps (US) and 25.6 fps (German) against 71.7 and 65.2 for `new`, but the French
pair (39.6 vs 56.3) and the means move with background load.

Open risks: the cache is built once per title visit, on the first frame the
title menu is up (static raster plus a full upload, about 18 ms on the PC;
unmeasured on the OLED), and holds 5.5 MB while the title is shown; the HUD rebuilds and
uploads its own texture every visible frame, independent of this work; GPU
execution time and input-to-photon latency are unmeasured; OLED acceptance
is pending (checklist in [MENU_COMPOSITION_PLAN](MENU_COMPOSITION_PLAN.md)).
Evidence (git-ignored): `tmp/evidence/menu-composition-luna-2026-10-22/` (PC
prototypes) and `tmp/evidence/menu-switch-cache-2026-10-22/` (PC replay,
profile, Ryubing screenshots and logs).
