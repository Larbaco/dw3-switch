# P3 — PC GPU / present glitch inventory

Status: **active follow-up** (Phase 3 gate already met). Branch
`phase3-gpu-glitch`. Owns PC present path only — Switch video is P4.2
(separate).

PSY-Z pin: see `docs/PSYZ.md`. Movie background: `docs/P3.7_MOVIES.md`.
Title boot notes: `docs/P2.8_TITLE_BOOT.md`.

## Runtime

```bash
ln -sfn "$HOME/.cache/dw3-switch/runtime/us" runtime/us
DISC="/mnt/windows/Jogos/PS1/Digimon World 3/Digimon World 3.bin"
cmake -S platforms/pc -B build/pc -G Ninja -DPSYZ_RENDERER=sdl3_gpu \
  -DSDL_X11_XSCRNSAVER=OFF -DSDL_PULSEAUDIO=ON
cmake --build build/pc -j"$(nproc)"
```

Prefer `SDL_VIDEODRIVER=offscreen` (or a real display) for present — not
`dummy` (no SDL_GPU backend).

## Fixed on this branch

| ID | Symptom | Cause | Fix |
|---|---|---|---|
| G1 | Movie frames = macroblock garbage (after half-width / mid-grey fixes) | `DecDCTPutEnv` → `MDEC_in((u_long *)mdec_iq, …)` on LP64 skips/misaligns `u32` quant/IDCT tables | Host `__wrap_DecDCTReset` uploads via `Psyz_MdecCommand` with byte-accurate pointers (`platforms/pc/src/psyz_dma_present.c`) |
| G2 | Same garbage / torn strips under sync MDEC | `STDWTITL_onSliceDecoded` starts next `DecDCTout` before `LoadImage`; nested sync callbacks reuse two `imgbuf`s | Patch `0037` (LoadImage first under `PORT_HOST`) + DecDCTout slice stash in present wrap |
| G3 | Non-MDEC `LoadImage` could take the RGB888 path | `(w % 3 == 0 && h >= 192)` plus a latched `isrgb24` after STR | MDEC strip geometry only (`w == 24 && h >= 192`); drop the stale latch on non-MDEC uploads. Owner confirmed after PR #28 that movies stayed clean while the title menu was still shredded — that shred was R1 |
| R1 | Title logo / copyright vertical shred, column offsets, solid green bar | Host `TimBlock.pixels` is `u_long[]`. PS1 `u_long` is 4 bytes (pixels at file offset 12); host `u_long` is 8, so the flexible array aligns to 16 and `LoadImage` starts two halfwords into the payload. Path B dump (`rgb24_loads=0`) matched `TLOGOUSA` at shift 2 | Patch `0038`: `pixels` is `u32[]` so the offset stays 12. **Owner window confirm (2026-10-10):** title menu readable after pull of #29 |
| R4 | Battle HUD with a flat blue field (no stage, no fighters) | `FIGHTSTG_drawMesh` was linked and called. `GsSetRefView2` was a no-op, so `GsWSMATRIX` stayed 0 and RTPS collapsed the mesh. Mesh OT writes also used a `u_long` stride while host `OT_TYPE` is 16 bytes (HUD uses `getOtEntry`, so the plates still showed) | `GsSetRefView2` writes the look-at into `GsWSMATRIX` (`port/psyz_missing/psyz_missing.c`). `GsSetProjection` calls `SetGeomScreen`. Patch `0039` indexes the mesh OT by `sizeof(OT_TYPE)`. `GsSetFlatLight` is still a no-op |

### Evidence (G1/G2)

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  DW3_START_MODE=0xE01 DW3_RUNTIME=runtime/us DW3_DISC_BIN="$DISC" \
  DW3_FB_DUMP=build/movie-evidence/quant-f30.ppm \
  DW3_FB_DUMP_FRAME=30 \
  timeout -s KILL 22 ./build/pc/dw3-pc --runtime runtime/us
# expect: DecDCTReset → Psyz_MdecCommand default quant/IDCT (LP64-safe)
# expect: FB dump 1024x512 bbox ~0,36-319,451 (or dual-buffer to ~639)
# expect: recognizable MOVIEOPN frame (eye / Agumon); vs offline bsdec+MDEC
#          mean abs RGB diff ≈ 2–3 (RGB5551 round-trip), not ≈50+
```

Offline oracle (no present): assemble `MOVIEOPN.STR` frame → `bsdec` →
`Psyz_Mdec*` → PPM. In-game VRAM crop should match within 5551 error.

## Still open

| ID | Area | Symptom | Repro | Notes |
|---|---|---|---|---|
| R2 | Movie present | Double-buffer B / captured 320×480 may be empty while VRAM A is good; possible tear if sampling mid-flip | `DW3_START_MODE=0xE01` + windowed present | PSY-Z `Draw_SetDisplayMode` still ignores `isrgb24` (stores as 5551 via host) |
| R3 | Field | Sprite / VRAM tiling polish | `DW3_START_MODE=0x200` (FIELDSTG + WSTAG260) | Boot reaches field; visual oracle = P3.9 / DuckStation |
| R5 | PSY-Z LP64 | Upstream `MDEC_in` + `u32` static tables | — | Prefer small psyz fix on a dedicated branch; host wrap is enough for PC |

### R1 evidence

Path B (unset `DW3_START_MODE`) never enters the RGB24 movie upload
(`rgb24_loads=0`). Before `0038`, title VRAM rows matched `TLOGOUSA`
at a constant shift of 2. Owner confirmed the window on 2026-10-10:
DIGIMON wordmark, WORLD 3, and copyright readable. Do not use
`DW3_START_MODE=0xE00` as title evidence — that path times out into STR.

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  DW3_RUNTIME=runtime/us \
  DW3_FB_DUMP=build/title-evidence/title-b-disp180.ppm \
  DW3_FB_DUMP_DISP=180 \
  timeout -s KILL 25 ./build/pc/dw3-pc --runtime runtime/us
# expect: "disp dump request frame=180 rgb24_loads=0 rgb24=0"
# expect: mode still 0xe00 at that line (timeout to 0xe01 is later)
```

Movies stay on the G1/G2 recipe (`DW3_START_MODE=0xE01`, `w==24` strips).

### R4 evidence

`draw` / `lights` / `model_mesh` are not missing from the `FIGHTSTG` link. `port/CMakeLists.txt` excludes them only from the compile-only `dw3_p31_audit` target. `DW3_FIGHT_SOURCES` globs every `battle_mode/*.c`, and `nm` shows `FIGHTSTG_drawMesh` / `FIGHTSTG_queueMeshDraw` as defined text. A gdb stop on the pre-fix binary (`DW3_START_MODE=0x600`, `DW3_FORCE_ENCOUNTER=1`, no `DW3_FORCE_WIN`) hit both, with `GsSetRefView2` called as `vp=-2432,-1920,-11136 vr=1408,-2496,4864` while `GsWSMATRIX` was still all zeros. `isMeshOnScreen` kept running. The blue field is the stage `setBgColor`, not a missing overlay.

After the view write and the OT stride patch, the same boot's display crop (frame 90, `rgb24_loads=0`) shows the grass stage, the mesa, and Pharaohmon, with the name plates and the "Attack!" box still up. That is the Attack! camera, not the DuckStation Fight menu (Kunemon vs Monmon). `GsSetFlatLight` still discards the three directional lights. This frame did not need them to stop being a flat blue quad.

```bash
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
  DW3_START_MODE=0x600 DW3_FORCE_ENCOUNTER=1 \
  DW3_RUNTIME=runtime/us \
  DW3_FB_DUMP=build/battle-evidence/r4.ppm \
  DW3_FB_DUMP_DISP=90 \
  timeout -s KILL 28 ./build/pc/dw3-pc --runtime runtime/us
# expect: dw3: GsSetRefView2 vp=-2432,-1920,-11136 ... m00 != 0
# expect: FIGHTSTG loaded WFIGHTMN, no DW3_FORCE_WIN
# expect: display crop is the stage and a fighter, not flat blue (59,115,233)
```

### R4 follow-up — Attack camera aim

The first view write drew the stage, and it aimed beside the reference. `ratan2(y, x)` matches Psy-Q / `decomp/src/libgte/ratan.c` (first argument is the opposite side). `RotMatrix`’s view +Z row is `(-sin yaw, sin pitch, cos yaw · cos pitch)`, so the yaw that points +Z at `(dx, dz)` is `-ratan2(dx, dz)` and the pitch that includes `dy` is `+ratan2(dy, xz)`. The first port used the opposite pair of signs.

Same boot, first `GsSetRefView2` (`vp=-2432,-1920,-11136`, `vr=1408,-2496,4864`). Offline check with PSY-Z `RotMatrix` / `ratan2` / `ApplyMatrix`:

| | `GsWSMATRIX.t` | reference in view space |
|---|---|---|
| Before | `4957,1556,10325` | `7458,-1094,14637` (right of the axis) |
| After | `-225,2320,11322` | `12,3,16463` (on +Z) |

`SetGeomOffset` stays `(0, 0)`. The battle 3D layers add GPU offset `(0xA0, 0x78)` (`FIGHTSTG_CREATE_LAYERS`), so a point on the view axis lands at the center of the 320×240 crop. Frame 90 before the sign fix: grass, mesa, and sky, Pharaohmon cut off on the right, Kotemon not in the crop. After: Pharaohmon large near center, Kotemon in the lower right, ground filling the frame. That is still the Pharaohmon **Attack!** camera, not the DuckStation Fight menu (Kunemon vs Monmon). P3.9 stays open.

Checked and left as they are:

- `rz >> 12` matches `battle_camera.c` (`view.rz = current.rz << 12`). This camera’s roll is 0, so the roll sign is not exercised.
- Translation is `-R * eye`. The eye maps to the origin.
- `GsSetProjection` still forwards a positive `h` to `SetGeomScreen`. The framing change did not need a different `H`.

### `GsSetRefView2` super (preview.25 feedback, 2026-10-14)

Source-level correction; the idle-camera orbit still needs in-game visual
validation. Identity-super reasoning is not a substitute for that evidence.

The owner saw the Fight-menu idle camera "moving wrong" on the OLED. That
camera is FIGHTSTG's shot camera (`shot_camera.c`): it turns around the
fighters by stepping the battle camera's **super** coordinate (`rot.vy`,
`turnAroundFighters`) and moves through super's `tz` (`viewPartnerSide`
adds `0x1400` to `vpz`/`vrz` and takes it off `tz`, which only cancels if
super moves the viewpoint). So the viewpoint and reference are in super's
space: the camera sits at super(vp), and the world-screen matrix is
`look · super⁻¹`. The port composed `super · look` (`CompMatrix`), which
turned and shifted the picture in screen space: the camera panned on the
spot instead of orbiting. The first check above passed because that idle
view's super was identity.

`port/psyz_missing/psyz_missing.c` now inverts super (its chain up to the
world as one rigid transform), composes in 64-bit fixed point (no
`SVECTOR` narrowing of translations such as `0x1400`), and applies the
roll after yaw and pitch, about the line of sight (`RotMatrix` is
Rx·Ry·Rz, which rolled about the world Z before the yaw). With super
identity and roll 0 the matrix is unchanged, so the Attack! cameras
above keep their aim.

## Prior fixed (P3.7, already on main)

| ID | Symptom | Doc |
|---|---|---|
| A | Half-width movie band (`disp.x=480` halfwords) | `P3.7_MOVIES.md` Glitch A — `PutDispEnv` `×2/3` |
| B | Flat mid-grey | `P3.7_MOVIES.md` Glitch B — ring wrap `id=1` + quant upload (partial until G1) |

## Out of scope here

- `platforms/switch/` present (P4.2)
- `vendor/psyz` libsnd / Phase 4 spike trees
- Large PSY-Z GPU rewrites (host wraps preferred)
