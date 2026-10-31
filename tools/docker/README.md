# Docker images

| Image | Use |
|---|---|
| `devkitpro/devkita64@sha256:1fc388c3…` (= tag `20260219`, was `:latest`) | Switch homebrew builds via `tools/dk.sh` |
| `vendor/dw3_decomp` Dockerfile | Matching/decomp builds (optional; not required for Phase 1 hosts) |

Override the Switch image with `DW3_DK_IMAGE=...`.

## WSL / drvfs (Windows mounts)

`tools/dk.sh` runs Docker with `-u $(id -u):$(id -g)` so build artifacts are owned by your user. On WSL, checkouts under `/mnt/windows` (drvfs/NTFS) often cannot create or write `platforms/*/build/` inside the container (permission denied). Prefer a Linux/ext4 clone (e.g. `~/src/dw3-switch`) or CI. For the NTFS checkout, overlay build/out on ext4:

```bash
DW3_DK_TMPBUILD=/tmp/dw3-dk-build ./tools/dk.sh make -C platforms/switch
# rsyncs platforms/switch → ext4, builds there, copies dw3-switch.nro back
```

## Optional: build dkp SDL3 from source (P4.1 experiment)

Not required for the chosen native libnx backend. Recipe and results are
in [docs/SWITCH_BACKEND.md](../../docs/SWITCH_BACKEND.md) (clone
`devkitPro/SDL` `switch-sdl-3.4`, cmake with `Switch.cmake`, build dir on
a writable `/tmp` volume — do not write into the NTFS checkout).
