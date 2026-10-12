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
| **Minus + ZL + ZR** (edge) | Cycle FIGHTSTG battle render mode (see below) |

- Default **visible ON** when `DW3_NX_DEBUG_OVERLAY` is compiled in (nxlink).
- Default **invisible** is N/A for release (flag off → no HUD).
- Chords use raw hid bits after `Psyz_PadsPoll` / `padUpdate` so they do not
  invent a second pad state. Avoids Plus alone (Start) and face buttons.
- Battle-mode chord works even when the HUD is compiled out (release NRO);
  mode still logs `nx_mesh: battle_mode=…` once per change.

## Battle render modes (FIGHTSTG only)

Runtime override (not getenv-only). Cold boot defaults to **soft** when
`DW3_NX_MESH_SOFT_BATTLE` is ON (shipped default). `DW3_MESH_SOFT=0|1` seeds
the initial mode (`0` → `gpu-tex`, `1` → `soft`). Field never enqueues mesh.

| Token (HUD `battle=`) | Behaviour |
|---|---|
| `soft` | Software `RasterTriangle` + opaque blit (picture OK) |
| `soft+bbox` | **Stub** today: same as `soft`; logs once that idea H is not yet wired |
| `gpu-solid` | Mesh FBO with `DW3_NX_MESH_TPAGE_NOTEXTURE` (vertex colour, ignore CLUT) |
| `gpu-tex` | Existing textured mesh path (may be black; OK for compare) |

Cycle order: `soft` → `soft+bbox` → `gpu-solid` → `gpu-tex` → `soft` …

### Mesh DIAG (blue triage)

Optional `-DDW3_NX_MESH_DIAG=ON` or `DW3_MESH_DIAG=1` (see
[P5.2_SWITCH_GPU_OPTIONS.md](P5.2_SWITCH_GPU_OPTIONS.md)):

- Logs rate-limited `nx_mesh: clear_rect … rgb=`
- On **gpu-solid**: force magenta ClearImage into the mesh FBO, constant
  magenta vertex colours, yellow NDC overlay of the first packet tri, and a
  tiny cyan corner marker (fullscreen cyan retired after OLED ssquad2)
- Present becomes `mesh_only_diag` (skip UI black-discard)

Default **off**. Does not change `DW3_NX_MESH_SOFT_BATTLE` (soft stays the
shipped seed).

## MVP fields (top-left, monospace, ≤12 lines)

| Line | Source |
|---|---|
| `fps ~NN` / `dt us=…` | Same 60-pump window as `frame dt us=` (`Hz = 60e6 / dt_us`) |
| `mode file=0x…` | `dw3_overlay_current_mode_file()` (mesh gate id) |
| `battle=soft\|soft+bbox\|gpu-solid\|gpu-tex` | Runtime battle mode token |
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
style glyphs only (font8x8_basic layout).

**Bit order:** each glyph row byte has **bit 0 (LSB) = leftmost pixel**,
bit 7 = rightmost. `put_glyph` must use that convention; treating MSB as left
mirrors every Latin glyph on the OLED (reads like `gbd 3wd` for `dw3 dbg`).
See `platforms/switch/source/debug_overlay_glyph.h`.

**UV / quad:** same TRIANGLE_STRIP corners as `nx_gpu` present — clip
top-left samples atlas `(0,0)`, bottom-right samples `(u1,v1)` of the used
atlas region. Atlas memory row 0 is the first HUD line; GL uploads that row
at texture V=0, which the top of the viewport samples.

## Scale (OLED readability)

The atlas is still 8×8 glyphs with a soft panel, then drawn with an **integer
nearest-neighbor** upscale (`GL_NEAREST` mag/min). No linear filtering.

Slightly smaller than the first OLED scale pass (#78 used 3×/4×):

| Framebuffer height | Scale | Glyph size on screen |
|---|---|---|
| 720 (handheld) | **2×** | 16×16 px |
| 1080 (docked) | **3×** | 24×24 px |

Rule (in `hud_scale_for_fb`):

```text
scale = clamp(floor(fb_h * 2 / 720), 2, 6)
```

then shrink while `content * scale` would exceed `fb_w` / `fb_h`. Padding
(`HUD_PAD_X/Y = 4`) and line gap (`HUD_LINE_GAP = 2`) are in atlas pixels and
scale with the glyphs. Corner placement stays top-left; the panel must not
cover the whole screen. LSB-left glyph bit order (#81) is unchanged.

## PC host

Optional / deferred. Switch OLED triage is the priority. A later PC pass may
mirror the same fields with SDL debug text; no PC HUD in the MVP.

## Related

- P5.2 mesh / present: [P5.2_BATTLE_GPU.md](P5.2_BATTLE_GPU.md), [NXLINK_DEBUG.md](NXLINK_DEBUG.md)
- P5.4 crash overlay: still open in [ROADMAP.md](ROADMAP.md) (separate from this HUD)
