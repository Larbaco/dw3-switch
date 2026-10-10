# Roadmap

Approach: stabilize a **PC-capable Psy-Q shim path** first where possible, then
land a **libnx** host. Prefer reusing PsyCross / PSY-Z patterns over rewriting
GPU/GTE/SPU from scratch.

## Phase 0 — Repository (done)

- [x] Dedicated project folder
- [x] Git repo for the Switch port
- [x] `vendor/dw3_decomp` submodule (recursive)
- [x] Scaffold layout + ignore rules for discs/assets

## Phase 1 — Host skeleton

- [ ] Minimal Switch `.nro` that boots, presents a framebuffer, polls input
- [ ] RomFS / sdmc layout for unpacked game files (no ISO required at runtime)
- [ ] Logging (nxlink) and a tiny status screen

## Phase 2 — Platform shims

- [ ] Choose shim layer (PsyCross vs PSY-Z vs custom thin wrappers)
- [ ] Pad → Switch controls
- [ ] LibCD → filesystem reads from unpacked `AAA/` tree
- [ ] Soft GPU blit (ABGR1555 VRAM → EGL/deko3d present)
- [ ] SPU → audren / OpenAL-soft style backend

## Phase 3 — Game bring-up

- [ ] Build game C from `vendor/dw3_decomp` with a modern compiler (`TOOLCHAIN=gcc` lessons)
- [ ] Replace overlay loader with host-side module load
- [ ] Boot through `crt0` / `main` into title (`STDWTITL`)
- [ ] Field (`FIELDSTG`) + one stage

## Phase 4 — Playable slice

- [ ] Battle (`FIGHTSTG` / `WFIGHTMN`)
- [ ] Save/load (virtual memcard → host file)
- [ ] Audio + FMV (STR/XA) as needed for progression

## Phase 5 — Polish

- [ ] Widescreen / integer scale options
- [ ] Performance pass on OLED/handheld clocks
- [ ] EU vs US version switch
