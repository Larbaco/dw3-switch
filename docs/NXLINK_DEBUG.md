# Nxlink debug logging (Switch game image)

How to get useful live logs for P5.2 mesh / present work without flooding the
nxlink TCP stream.

## Builds

| Binary | Command | nxlink stdio | Mesh summary | Battle dump | On-screen HUD |
|---|---|---|---|---|---|
| Release / SD | `./tools/build_switch_game.sh` | off | off | off | off |
| Dev (nxlink) | `./tools/build_switch_game.sh --nxlink` | on | **on** (default) | **on** (default) | **on** (default) |

The nxlink NRO lands at `build/switch-nxlink/dw3-switch-nxlink.nro`.

Override the mesh summary without turning off stdio:

```bash
# inside the same CMake cache dir the build script uses
cmake -S platforms/switch -B build/switch-nxlink \
  -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake \
  -DDW3_NXLINK_STDIO=ON -DDW3_NX_DEBUG_MESH=OFF
```

Standard `dw3-switch.nro` never calls `nxlinkStdio()` and does not define
`DW3_NX_DEBUG_MESH`, `DW3_NX_DUMP_VRAM`, or `DW3_NX_DEBUG_OVERLAY`.

## On-screen debug overlay (`DW3_NX_DEBUG_OVERLAY`)

Default **on** for `--nxlink` builds. Draws a small top-left monospace HUD
(FPS / mode file / mesh window / build tags) after the game present and before
`eglSwapBuffers`. Toggle with **Minus + L + R**. Visual only — no extra
per-frame nxlink lines. Full field list and constraints:
[DEBUG_OVERLAY.md](DEBUG_OVERLAY.md).

```bash
cmake -S platforms/switch -B build/switch-nxlink \
  -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake \
  -DDW3_NXLINK_STDIO=ON -DDW3_NX_DEBUG_OVERLAY=OFF
```

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
| `dw3: map_preload id=… bytes=… us=…` | First tile of a field map file id (one fread) |
| `dw3: memcard ensure … us=` / `migrate … us=` | Flat load or one-time `.mcr` migration |

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

## Battle VRAM / mesh FBO dump (`DW3_NX_DUMP_VRAM`)

Default **on** for `--nxlink` builds. On the **first** `mesh+ui` present
(typically the first FIGHTSTG frame that composites the mesh FBO), the game
writes the current display rect to the SD card and logs one nxlink line
starting with `vram dump:`. It does **not** stream the pixels over nxlink.

Files (display rect only, usually 320×240):

| Path | Contents | Approx size |
|---|---|---|
| `sdmc:/switch/dw3/dump_vram_display.rgb555` | CPU VRAM RGB555, tight rows | ~150 KiB |
| `sdmc:/switch/dw3/dump_vram_display.ppm` | Same, P6 for a quick look | ~225 KiB |
| `sdmc:/switch/dw3/dump_mesh_fbo.rgba` | Mesh FBO `glReadPixels` RGBA8 | ~300 KiB |
| `sdmc:/switch/dw3/dump_mesh_fbo.ppm` | Same, P6 | ~225 KiB |

The log line reports write ok/fail, non-black pixel counts, and mean RGB so
you can tell whether the mesh FBO had content without opening the files.

Retrieve with Sphaira FTP (anonymous, TCP **5000**, PASV), for example:

```sh
curl --ftp-pasv -O ftp://<switch-ip>:5000/sdmc:/switch/dw3/dump_mesh_fbo.ppm
curl --ftp-pasv -O ftp://<switch-ip>:5000/sdmc:/switch/dw3/dump_vram_display.ppm
```

Override:

```bash
cmake -S platforms/switch -B build/switch-nxlink \
  -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake \
  -DDW3_NXLINK_STDIO=ON -DDW3_NX_DUMP_VRAM=OFF
```
