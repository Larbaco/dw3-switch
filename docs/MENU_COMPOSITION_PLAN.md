# Native menu composition implementation plan

Status: plan recorded on 2026-10-15 from `origin/main` at `6a02919`.
Production behavior is unchanged until comparative evidence selects a path.

## Scope

Optimize only the native title and Port options screens. Keep the current
appearance, logical inputs, US/EU text, per-host fade behavior, and existing
fallback. Do not build a general menu framework, migrate PS1 gameplay menus,
change saves or assets, overclock, or connect to an OLED during development.

The comparison has three selectable modes on PC: current/reference, A (retained
CPU artwork with opaque and dirty-region raster updates), and B (component
atlas with GPU composition into a retained target only when semantic state
changes). Instrument raster, staging/allocation, upload-call, submit, and
present CPU durations; bytes, calls, cache rebuilds, and generation counts.
Keep instrumentation bounded and low overhead. Do not add GPU finish/wait
calls to obtain timings. Preserve any wait already present in the reference
path consistently across comparisons and record its CPU duration; do not add
or selectively remove waits. Report host call time separately from GPU
execution. PC evidence selects a candidate for OLED validation; it does not
prove Switch performance.

## Steps

1. Record this plan and the evidence gates. Instrument the existing path and
   capture a reproducible reference baseline under Xvfb.
2. Prototype A and B behind an explicit comparison selector. Retain the
   current path as the reference/fallback during evaluation; preserve visual,
   input, language, fade, and lifecycle behavior.
3. Have the independent test workstream run identical PC replay scenarios and
   compare captures and counters. Select a winner only after that report;
   document the decision and remove the losing experimental backend from the
   product path.
4. Port only the proven winner to the thin Switch host adapter, keeping a
   fallback for unsupported or failed GPU resources. Build US and EU with the
   local `devkitpro/devkita64:latest` image and verify
   `DW3_NXLINK_STDIO=ON` and `DW3_NX_DEBUG_OVERLAY=ON` in the resulting build.
5. Obtain Terra's review and the independent test report, address blockers,
   then open a PR against `main`. Leave OLED acceptance explicitly pending for
   the owner; do not publish a release or merge.

The final candidate checkpoint must build standard and nxlink/HUD variants
for both US and EU, with correct candidate metadata/version. The release
workflow is outside this change; root owns preview packaging and will verify
the then-current release sequence before publishing.

## Required PC coverage

Use the existing capture/replay tooling under a dedicated Xvfb display, never
the owner's active desktop. Cover title-to-Options entry, all three cursor
positions, every value word (Widescreen OFF/ON, Turbo OFF/2x/3x/4x, Restart
OFF/ON), Triangle return, restart flow, and US plus EU English, French,
German, Italian, and Spanish. Compare reference and candidate at settled
frames. Preserve the PC host's existing no-op fade behavior; do not add PC
fade parity here. Validate intermediate group-fade frames on the Switch GLES
path, where that behavior exists. Record renderer, resolution, HUD state,
limiter, input sequence, and run duration. Also exercise fallback, resize, and
graphics context invalidation.

## Evidence gates

- Both paths have zero raster/upload and zero cache rebuild while idle after
  warm-up. A may raster and upload dirty regions on cursor/value changes, but
  must avoid full-canvas redraw/reallocation and report changed pixels, bytes,
  copies, and allocations. B must do zero CPU raster and pixel upload for a
  cursor/value change unless an asset/language/context invalidation occurred.
  Report bytes, calls, allocations, cache rebuilds, and peak
  staging/atlas/target memory for both.
- Repeated same-condition runs report median, p95, and maximum frame times,
  plus input-to-visible-update latency for idle, cursor movement, and toggles.
  No concurrent emulator and no synchronization wait inserted for timing.
- Visual comparisons preserve geometry, colors, text, readability, and each
  host's existing behavior. PC fade stays a no-op; validate intermediate
  group-fade frames on the Switch GLES path, where that fade exists. Document
  alpha tolerance if needed.
- US and EU Switch game images build successfully with nxlink stdio and debug
  overlay enabled. Any device behavior remains unverified until the owner
  chooses to test the candidate on OLED. Release and merge belong to the root
  orchestrator.

## Roadmap status

This work is the open P5.2 Port options performance follow-up described in
`MENU_COMPOSITION_INVESTIGATION.md`. P9.2 reachability remains done; this plan
does not change the P6.10 gameplay-menu GPU goal or declare any performance
gate complete.
