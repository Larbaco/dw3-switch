# PSY-Z native libnx backend (sketch)

P4.1 layout mirror of `vendor/psyz/psyz/src/psp/`. Stubs only — not linked
into `dw3-switch.nro` yet. Decision and reasons:
[docs/SWITCH_BACKEND.md](../../../docs/SWITCH_BACKEND.md).

| File | Role (target) |
|---|---|
| `nx.h` | Shared Switch helpers / includes |
| `nx_gpu.c` | Present / draw (EGL+GLES or deko3d) |
| `nx_input.c` | `hid` / `pad` → PSY-Z pad frames |
| `nx_audio.c` | `audren` PCM from software SPU |
| `nx_os.c` | Threads, locks, time |
| `nx_file.c` | `romfs` / `sdmc` file I/O |
| `nx_log.c` | Logging to stdout / nxlink |
| `nx_module.c` | Static `Psyz_ModuleOpen` table |

Build integration lands with P4.2+ (CMake or Makefile objects + PSY-Z
platform selection). Until then the Phase 1 console skeleton in
`../source/main.c` remains the only linked host code.
