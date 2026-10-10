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
| `0003-PORT_HOST-allocMemHigh-return-pointer.patch` | no | `allocMemHigh` returns the pointer under `PORT_HOST` (not v0) |
| `0004-PORT_HOST-gte-software-macros.patch` | no | `include/engine/gte.h` → PSY-Z software GTE under `PORT_HOST` |
| `0005-PORT_HOST-overlay-addrs-boot-tim-stub.patch` | no | Host overlay addrs from `port/overlays`; boot TIM upload stubbed |
| `0006-match-neutral-engine-sizeof-sizes.patch` | yes | `sizeof`/`offsetof` for `PAD_DATA_SIZE`, `GFX.prim` clear span, `MemBlock` split, `createInn` |
| `0007-match-neutral-engine-pointer-casts.patch` | yes | `freeFilesFrom(void *)`; archive/text pointer+offset (no pointer↔`u32`/`s32`) |

### Upstream `make compare` (P2.4)

Match-neutral patches 0006/0007 are intended to stay byte-identical on
PS1, but this host has no MIPS/Psy-Q toolchain image for
`vendor/dw3_decomp` `make compare`. Blocker: run compare in the decomp’s
Docker/toolchain when available before opening the upstream PR.
