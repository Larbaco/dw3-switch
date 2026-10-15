# Native menu composition investigation

Status: proposal, no production renderer changed. Inspected on 2026-10-15 at
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
