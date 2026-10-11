# Nxlink debug logging (Switch game image)

How to get useful live logs for P5.2 mesh / present work without flooding the
nxlink TCP stream.

## Builds

| Binary | Command | nxlink stdio | Mesh summary |
|---|---|---|---|
| Release / SD | `./tools/build_switch_game.sh` | off | off |
| Dev (nxlink) | `./tools/build_switch_game.sh --nxlink` | on | **on** (default) |

The nxlink NRO lands at `build/switch-nxlink/dw3-switch-nxlink.nro`.

Override the mesh summary without turning off stdio:

```bash
# inside the same CMake cache dir the build script uses
cmake -S platforms/switch -B build/switch-nxlink \
  -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake \
  -DDW3_NXLINK_STDIO=ON -DDW3_NX_DEBUG_MESH=OFF
```

Standard `dw3-switch.nro` never calls `nxlinkStdio()` and does not define
`DW3_NX_DEBUG_MESH`.

## Launch

See [platforms/switch/README.md](../platforms/switch/README.md) (NetLoader **Y**,
then `nxlink -a <IP> -s …`). Do not probe TCP 28280 before `nxlink -s`.

`psyz_log` / `INFOF` always fan out to:

1. stderr (what nxlink forwards)
2. `sdmc:/switch/dw3/dw3.log`
3. `svcOutputDebugString` (emulator-friendly)

## What already logs (any build)

Sampled / one-shot by design — safe for long runs:

| Line | When |
|---|---|
| `vsync pump #N` | First 16 pumps, then powers of two |
| `frame dt us=` | Every 60 VSync pumps (~1 s at 60 Hz) |
| `display on: vram …` | Display size change |
| `present viewport … mesh_avail=` | Letterbox viewport change |
| `mesh gpu: N tris flushed` | Every ~1000 lifetime tris while composite is on |
| `nx_mesh: GLES3 mesh FBO ready` / ES2 fallback | Boot |
| Game `dw3: mode=` / `stageFile=` | Mode / stage transitions (decomp) |

## Mesh debug summary (`DW3_NX_DEBUG_MESH`)

Default **on** for `--nxlink` builds. Adds **one sibling line** next to each
`frame dt us=` sample (still ~1 Hz), not per-triangle spam:

```
mesh dbg: avail=1 tris_enq=… tris_flush=… frames_mesh=… frames_blit=…
          soft=… flush_us=… upload_us=… present_us=…
```

| Field | Meaning |
|---|---|
| `avail` | GLES3 mesh path initialized |
| `tris_enq` / `tris_flush` | Triangles enqueued / drawn to the mesh FBO in the last 60 pumps |
| `frames_mesh` | Presents that used mesh FBO + UI discard overlay |
| `frames_blit` | Presents that used the opaque CPU VRAM blit |
| `soft` | Triangle polys that fell to software because mesh was unavailable (ES2 / init fail) |
| `flush_us` | Time in `dw3_nx_mesh_flush` (includes VRAM `glTexSubImage`) |
| `upload_us` | Subset of flush spent packing + uploading sample VRAM (0 when the dirty flag skips the upload) |
| `present_us` | Composite/blit after flush (not including flush) |

Also, only while the flag is on:

- `present path: mesh+ui` / `opaque_blit` on path changes
- First 16 distinct GP0 poly codes that hit GLES (`mesh poly code=0x.. textured=`)

## Rate limits / cost

- Do **not** log every triangle or every frame over nxlink. Stdio is
  synchronous; flooding stalls the game and can drop the link.
- Keep new probes behind `DW3_NX_DEBUG_MESH` (or another compile bucket) and
  sample at the 60-pump cadence unless the event is rare (mode change, first N).
- `fflush` on every `psyz_log` line is intentional for crash survivability; it
  makes volume discipline more important on hardware.

## Field vs battle (after FIGHTSTG gate / #71)

Mesh enqueue is allowed only while the mode overlay is `FILE_FIGHTSTG`.
Use the summary to confirm the gate:

- **Field / non-battle:** expect `tris_enq≈0`, `frames_blit` dominant,
  no new `mesh poly code=` samples.
- **Battle (`FIGHTSTG`):** may show `frames_mesh`, non-zero `tris_enq` /
  `tris_flush`, and first `mesh poly code=` lines when GLES receives polys.

Pair with existing `dw3: mode=` / `FIELDSTG_pickStage` / fight-stage lines.
`upload_us` staying near zero on Fight-menu idle (unchanged VRAM) is the
dirty-sample path working.
