# Switch host

devkitPro / libnx target for Digimon World 3.

## Build

```bash
# from repo root (Linux/ext4 clone preferred — see tools/docker/README.md)
./tools/dk.sh make -C platforms/switch
# artifact: platforms/switch/dw3-switch.nro

# NTFS / drvfs (Windows mount): overlay build dir on ext4
DW3_DK_TMPBUILD=/tmp/dw3-dk-build ./tools/dk.sh make -C platforms/switch
```

Current binary is the **P4.2 `gpu_hello` sample** (EGL clear + vsync). No game
data required. See [docs/P4.2_VIDEO.md](../../docs/P4.2_VIDEO.md).

## PSY-Z backend (Phase 4)

P4.1 chose a **native libnx** backend (not SDL3-dkp). `psyz_backend/nx_gpu.c`
is linked; other units are empty placeholders for P4.3–P4.6. Decision:
[docs/SWITCH_BACKEND.md](../../docs/SWITCH_BACKEND.md).
