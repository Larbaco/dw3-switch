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
| G3 | Title TIM shred risk from RGB24 heuristic | `(w % 3 == 0 && h >= 192)` treated any tall upload as RGB24 | MDEC strip geometry only (`w == 24 && h >= 192`); dropped stale `isrgb24` latch on `LoadImage` (R1) |

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
| R1 | Title | Logo / copyright shredded; solid green bar (P2.8) | unset `DW3_START_MODE`; windowed or capture after `mode=0xE00` | **Fixed (G3 follow-up):** `g_disp_rgb24` stayed set after STR; title `timLoaderLoad` ran before `PutDispEnv` cleared it → TIM/CLUT through RGB888 converter. Re-verify window + FB dump; residual OT/tpage polish TBD |
| R2 | Movie present | Double-buffer B / captured 320×480 may be empty while VRAM A is good; possible tear if sampling mid-flip | `DW3_START_MODE=0xE01` + windowed present | PSY-Z `Draw_SetDisplayMode` still ignores `isrgb24` (stores as 5551 via host) |
| R3 | Field | Sprite / VRAM tiling polish | `DW3_START_MODE=0x200` (FIELDSTG + WSTAG260) | Boot reaches field; visual oracle = P3.9 / DuckStation |
| R4 | Battle | Same class as field once `FIGHTSTG` is up | `DW3_FORCE_ENCOUNTER` / P3.3 recipes in `docs/P3.3_BATTLE.md` | Not re-proven on this branch beyond mode entry |
| R5 | PSY-Z LP64 | Upstream `MDEC_in` + `u32` static tables | — | Prefer small psyz fix on a dedicated branch; host wrap is enough for PC |

## Prior fixed (P3.7, already on main)

| ID | Symptom | Doc |
|---|---|---|
| A | Half-width movie band (`disp.x=480` halfwords) | `P3.7_MOVIES.md` Glitch A — `PutDispEnv` `×2/3` |
| B | Flat mid-grey | `P3.7_MOVIES.md` Glitch B — ring wrap `id=1` + quant upload (partial until G1) |

## Out of scope here

- `platforms/switch/` present (P4.2)
- `vendor/psyz` libsnd / Phase 4 spike trees
- Large PSY-Z GPU rewrites (host wraps preferred)
