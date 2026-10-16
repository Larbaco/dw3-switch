# Native menu prototype validation

Baseline: `6a02919` (the integrated #170 options-only upload). Tests use an
independent worktree and Release PC builds, SDL GPU, Xvfb 1280x960, default
internal resolution and pacing, dummy audio, and a fresh temporary memory-card
directory per process. Own-disc US/EU runtimes are read from the owner's cache;
no captures, runtime files, or saves belong in this branch.

`tools/replay/gen_menu_scenarios.py OUT` extends the existing six P6.15 boot
sequences with Options entry, all three rows, widescreen OFF/ON, fast-forward
3x/4x/OFF/2x, restart ON/OFF/ON, reverse cursor movement, return, reopen, and
in-process restart. Pass `OUT` to `tools/replay_tests.sh --scen-dir OUT`.
The initial boot and title menu checkpoints remain intact.

Reference and candidates use identical recordings. Compare full window PNGs
and PS1 PPMs using the existing 8/255 channel and 0.1% pixel threshold, but
record exact differing-pixel counts and inspect differences. Same-machine
integer CPU output is expected to match exactly; GPU alpha rounding requires
explicit evidence. Baseline references must be visually inspected before
acceptance, since matching an incorrect replay does not establish coverage.

`--performance` generates the same input frames without screenshots. These
runs must be launched directly (the standard runner correctly rejects recordings
without checkpoints). Screenshot capture blocks the game thread and must never
be included in performance distributions. Report median, p95 and maximum by
semantic event, plus upload bytes, raster work, allocations and cache rebuilds.
CPU call duration is not GPU execution time; absent asynchronous timer queries,
GPU timing must be marked unavailable. Repeat runs sequentially with no other
emulator/game process. PC has no equivalent Switch debug HUD here; keep host
configuration identical and leave the HUD-enabled OLED check to the owner.

Required follow-up cases: reference/cpu/gpu comparisons; idle/cursor/value
invalidation; resource resize/recreation and explicit fallback; restart into
US title or EU language selection. PC fade is currently a no-op, so Switch
intermediate group fade/halos and owner perceived-hitch acceptance remain open.
This plan and replay generator are preparation, not evidence that gates passed.

## Baseline execution, 2026-10-15

The baseline built successfully for both discs. All six extended recordings
exited normally: US has 22 checkpoints and each EU language has 24, totaling
142 PNG + 142 PPM captures outside Git. This generates references; it is not
a candidate comparison. Every language logged both Options entries, widescreen
OFF/ON, fast-forward 3x/4x/OFF/2x, restart ON/OFF/ON, two closes and the restart
request. US Options entry and US/EU-English post-restart captures were visually
inspected: title on US and language selection on EU. Baseline event excerpts,
paths and PNG hashes are in [baseline.json](evidence/menu-validation-2026-10-15/baseline.json).
The five existing replay-tool tests passed in an isolated `uv` environment
(system Python has no pytest). No candidate performance result is claimed yet.

For repeated measurement, `--cycles 10 --performance` adds 80 cursor/value
transitions before return/restart, without screenshots. Even cycle counts restore
the toggled values so restart remains enabled. Use identical cycle counts across
modes and repeated processes. Capturing runs keep the default zero extra cycles.

`tools/replay/profile_menu.py` launches the screenshot-free recordings in a
private Xvfb, sequentially by repetition/mode/language. Supply both binaries,
runtimes, `--scen-dir`, and `--out`; default modes are reference/cpu/gpu and
three repetitions. It preserves raw CSV/logs outside Git and writes a JSON
summary per event using `floor(.95*(n-1))` p95. Present-callback intervals are
explicitly a proxy, not full-frame GPU time or input-to-photon latency. Seven
replay-tool tests pass, including identical stress input with/without capture,
unique/increasing checkpoint bounds and rejecting odd stress-cycle counts
that would disable restart before the final chord.

A second US + EU-English run passed all 92 PNG/PPM comparisons with zero pixels
above the configured tolerance; [baseline-repeat.json](evidence/menu-validation-2026-10-15/baseline-repeat.json)
records exact file-byte equality separately. Binary hashes, compiler and pinned
submodules are recorded with the baseline metadata. Telemetry may additionally
provide `state_update_to_draw_us`; this measures CPU state update to overlay
draw callback and is not input-to-photon or completed GPU presentation.

The profiling runner rotates mode order by repetition (Latin square for three
modes), rotates scenario order, records the exact run order, and runs one full
warm-up round by default. Warm-up telemetry is preserved under `warmup_profile`
and excluded from measured `profile` summaries. Ambiguous `options-change`
events are rejected; cursor and value timings must be separate.

Vulkan inspection exposes RADV RX 6700 and llvmpipe. Pinning RADV under this
Xvfb fails SDL GPU initialization (`No supported SDL_GPU backend found`), so
measurements will pin `/usr/share/vulkan/icd.d/lvp_icd.json` (Mesa 25.2.8,
LLVM 20.1.2). US reference replay with this pin passes all 44 image comparisons.
The original visual baseline is therefore reusable; every timing baseline and
candidate must run with the same explicit pin. No RADV timings are usable.

The profiler rejects empty CSVs and recordings without both cursor and value
events. Eight tooling tests now pass, including these false-success guards.

## CPU prototype first verification

`4dda8ee` failed 13 US PNG checkpoints after the first change (up to 101,387
pixels over tolerance). Review identified a wrong destination X in partial
uploads. The corrected `dd5cdea` passes all 44 US PNG/PPM checks byte-for-byte.
This run used the developer's default-build US binary only for correctness and
counters; performance will use independently built Release binaries for both
discs with identical flags.

[CPU counters](evidence/menu-validation-2026-10-15/cpu-a-us-counters.json), taken
from this screenshot run, show idle without raster/upload/rebuild/allocation,
cursor updates of 434,304 bytes in three rectangles, and value updates of
29,952 bytes in one rectangle. Cursor/value events still allocate one transfer
buffer each. First close includes one full 2,764,800-byte update; its source
must be distinguished from Options-cache invalidation. These are workload
counts, not accepted timing measurements.
