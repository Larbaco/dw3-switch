# Decomp patches

Patches against `vendor/dw3_decomp` live in `port/patches/` and are applied
only to a **build copy** under `build/decomp-src/` by
`tools/apply_patches.sh`. Never edit or commit a dirty tree inside
`vendor/dw3_decomp`.

## Policy

1. **Build-copy only.** `apply_patches.sh` copies the submodule (excluding
   `.git`) into `build/decomp-src/` and applies `port/patches/*.patch` in
   sorted order. The submodule itself must stay clean after every task
   (`git -C vendor/dw3_decomp status --porcelain` empty).
2. **Declare Upstream-bound.** Every patch file must start with a header
   that includes `Upstream-bound: yes` or `Upstream-bound: no`:
   - `yes` — match-neutral fix intended for an upstream PR later
   - `no` — port-only change (host/`PORT_HOST`, AArch64, etc.)
3. **Pin the submodule.** `port/patches/DECOMP_PIN` holds the expected
   `vendor/dw3_decomp` commit SHA. `tools/apply_patches.sh --check` fails
   if `git -C vendor/dw3_decomp rev-parse HEAD` differs, if the submodule
   working tree is dirty, or if any patch fails `git apply --check`.
4. **Empty series is valid.** With no `*.patch` files, copy + apply must
   succeed and `build/decomp-src/` matches the pinned submodule contents.

## Commands

```bash
tools/apply_patches.sh --check   # pin, clean tree, dry-run apply
tools/apply_patches.sh           # refresh build/decomp-src + apply
```

## Series

| Patch | Upstream-bound | Notes |
|---|---|---|
| `0001-PORT_HOST-scratchpad-stack-noops.patch` | no | `SetSpadStack` / `ResetSpadStack` no-ops under `PORT_HOST` |
| `0002-PORT_HOST-game_bss-complete-types.patch` | no | Include complete types before `game_bss.c` object defs on host |
