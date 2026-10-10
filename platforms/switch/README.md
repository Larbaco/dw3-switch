# Switch host

devkitPro / libnx target for Digimon World 3.

## Build

```bash
# from repo root (Linux/ext4 clone preferred — see tools/docker/README.md)
./tools/dk.sh make -C platforms/switch
# artifact: platforms/switch/dw3-switch.nro
```

No game data is required for this Phase 1 skeleton (`source/main.c`).

## PSY-Z backend (Phase 4)

P4.1 chose a **native libnx** backend (not SDL3-dkp). Sketch stubs live in
`psyz_backend/` (not linked yet). Decision:
[docs/SWITCH_BACKEND.md](../../docs/SWITCH_BACKEND.md).
