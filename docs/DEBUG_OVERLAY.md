# On-screen debug overlay (Switch)

Toggleable HUD for OLED triage (preview.10 blue battle, mesh gate, FPS)
without reading nxlink for every number. Complements the sampled stdio lines
in [NXLINK_DEBUG.md](NXLINK_DEBUG.md); it does **not** replace them.

## Build flag

| Binary | Command | Overlay |
|---|---|---|
| Release / SD | `./tools/build_switch_game.sh` | **off** |
| Dev (nxlink) | `./tools/build_switch_game.sh --nxlink` | **on** (default) |

Compile definition: `DW3_NX_DEBUG_OVERLAY` (same nxlink-default pattern as
`DW3_NX_DEBUG_MESH` / `DW3_NX_DUMP_VRAM`).

```bash
cmake -S platforms/switch -B build/switch-nxlink \
  -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake \
  -DDW3_NXLINK_STDIO=ON -DDW3_NX_DEBUG_OVERLAY=OFF
```

When the flag is off, the overlay TUs compile to empty stubs; release NROs
carry no HUD code path cost beyond a few `#if` sites.

## Present path

Draw **after** the game composite inside `dw3_nx_gpu_present` (mesh FBO + UI
or opaque VRAM blit), **before** `eglSwapBuffers`. The overlay never owns the
game framebuffer permanently: it is a second GLES pass (textured corner quad)
on the current EGL surface, then the existing swap proceeds.

P5.4 crash / assert overlay is out of scope; leave a hook comment only.

## Toggle

| Chord | Action |
|---|---|
| **Minus + L + R** (edge) | Show / hide HUD |

- Default **visible ON** when `DW3_NX_DEBUG_OVERLAY` is compiled in (nxlink).
- Default **invisible** is N/A for release (flag off → no HUD).
- Chord uses raw hid bits after `Psyz_PadsPoll` / `padUpdate` so it does not
  invent a second pad state. Avoids Plus alone (Start) and face buttons.

## MVP fields (top-left, monospace, ≤12 lines)

| Line | Source |
|---|---|
| `fps ~NN` / `dt us=…` | Same 60-pump window as `frame dt us=` (`Hz = 60e6 / dt_us`) |
| `mode file=0x…` | `dw3_overlay_current_mode_file()` (mesh gate id) |
| `mesh avail=… path=mesh+ui\|opaque` | `dw3_nx_mesh_available` + last present `use_mesh` |
| `tris enq/flush=…` | Last completed `Dw3NxMeshDebugWindow` (when `DW3_NX_DEBUG_MESH`) |
| `frames m/b/soft=…` | Same window |
| `flush/up/pres us=…` | Optional timing from the same window |
| `build: nxlink mesh dump` | Which compile flags are defined |

Stage `stageFile` is **not** scraped from logs. MVP shows mode file id only;
a future latch can expose the last restored sub-overlay without touching
memcard / flat-save work.

## Stdio discipline

The overlay is visual only. It must **not** emit a new nxlink/`psyz_log` line
every frame. Existing ~1 Hz `frame dt us=` / `mesh dbg:` lines stay unchanged.

## Font / IP

Tiny embedded 8×8 ASCII bitmap (no Digimon art or game fonts). Public-domain
style glyphs only.

## PC host

Optional / deferred. Switch OLED triage is the priority. A later PC pass may
mirror the same fields with SDL debug text; no PC HUD in the MVP.

## Related

- P5.2 mesh / present: [P5.2_BATTLE_GPU.md](P5.2_BATTLE_GPU.md), [NXLINK_DEBUG.md](NXLINK_DEBUG.md)
- P5.4 crash overlay: still open in [ROADMAP.md](ROADMAP.md) (separate from this HUD)
