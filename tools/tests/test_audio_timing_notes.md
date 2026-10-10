# Audio timing notes (P3.6 investigation)

No disc assets, no CMake. Facts for Task A6 host pump checks.

## Expected vsync ordering (US)

From `vendor/dw3_decomp/src/engine/gfx/display.c` `vsyncCallback`:

1. Advance `GFX.timeCounter` / `frameTimeCounter` / `GAME.playTime.frames`
   by `VSYNC_STEP_NTSC`
2. Optional `GFX.vsyncFunc(GFX.vsyncArg)`
3. If `FLIP_PENDING`: flip `GFX.dispBuffer`, `PutDispEnv`
4. **`SsSeqCalledTbyT()`** — libsnd manual tick
5. Clear `FLIP_PENDING`

Installed via `startVSyncCallback` → `VSyncCallback(vsyncCallback)`
(PSY-Z kernel slot 4 → `VSyncDispatch` on vblank IRQ).

## Tick mode

`initSound` (US): `SsSetTickMode(SS_NOTICK)` then `SsStart2()`.

Under `SS_NOTICK`, libsnd does **not** auto-call `SsSeqCalledTbyT`; the
game must call it (as above). EU PAL may OR `50` into the mode for 50 Hz
ticks — US path is the PC bring-up default.

## What A6 should verify on `dw3-pc`

- One `SsSeqCalledTbyT` per presented frame when the window runs at vsync
- No double-tick if both game callback and `SsStart` auto-callback were
  armed (US NOTICK should prevent the latter)
- SDL audio callback (`Psyz_SpuPullSamples`) running so keyed voices are
  audible even before SEP sequencing is implemented upstream
