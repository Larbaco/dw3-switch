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
| R4 | Battle | Same class as field once `FIGHTSTG` is up | `DW3_FORCE_ENCOUNTER` / P3.3 recipes in `docs/P3.3_BATTLE.md` | Not re-proven on this branch beyond mode entry |
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

## Prior fixed (P3.7, already on main)

| ID | Symptom | Doc |
|---|---|---|
| A | Half-width movie band (`disp.x=480` halfwords) | `P3.7_MOVIES.md` Glitch A — `PutDispEnv` `×2/3` |
| B | Flat mid-grey | `P3.7_MOVIES.md` Glitch B — ring wrap `id=1` + quant upload (partial until G1) |

## Out of scope here

- `platforms/switch/` present (P4.2)
- `vendor/psyz` libsnd / Phase 4 spike trees
- Large PSY-Z GPU rewrites (host wraps preferred)
