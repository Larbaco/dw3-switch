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
