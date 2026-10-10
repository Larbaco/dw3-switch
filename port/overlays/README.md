# Host overlay symbols (Phase 2)

## E3 / P2.3

`host_vram.c` provides `OVERLAY_VRAM`, `STAGE_VRAM`, `OVERLAY_ADDRESS`, and
`SUB_OVERLAY_ADDRESS` for the host engine build. Sizes are padded US regions
(256 KiB each); exact restore of overlay `.data` / `.bss` is **E5 / P2.6**.

## Boot TIM

On PS1 the boot TIM is the packed exe tail at `OVERLAY_VRAM`, read via
`SUB_OVERLAY_ADDRESS`. Host `main` stubs that upload until
`runtime/us/boot_tim.bin` is loaded (see `docs/RUNTIME_LAYOUT.md`; wire in E6).
