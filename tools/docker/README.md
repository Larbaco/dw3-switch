# Docker images

| Image | Use |
|---|---|
| `devkitpro/devkita64:latest` | Switch homebrew builds via `tools/dk.sh` |
| `vendor/dw3_decomp` Dockerfile | Matching/decomp builds (optional; not required for Phase 1 hosts) |

Override the Switch image with `DW3_DK_IMAGE=...`.

## WSL / drvfs (Windows mounts)

`tools/dk.sh` runs Docker with `-u $(id -u):$(id -g)` so build artifacts are owned by your user. On WSL, checkouts under `/mnt/windows` (drvfs/NTFS) often cannot create or write `platforms/*/build/` inside the container (permission denied). Use a Linux/ext4 clone (e.g. `~/src/dw3-switch`) or CI with a normal Linux workspace for Switch builds via this wrapper.
