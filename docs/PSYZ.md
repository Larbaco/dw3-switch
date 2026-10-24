# PSY-Z (vendor/psyz)

PC Psy-Q compatible shim used for Phase 1+ bring-up before a Switch
libnx backend exists.

## Pin

| Field | Value |
|---|---|
| Upstream | https://github.com/Xeeynamo/psyz.git |
| Working fork (submodule URL) | https://github.com/Larbaco/psyz.git |
| Commit | `8bde35d` (`dw3-gutter-flush`: `sdl3_gpu` draws the queued primitives before the drawing area start or end changes, one commit on top of `2f931d8`; see [Gutter flush](#gutter-flush-p61)). `2f931d8` is `dw3-soft-raster-fast`: P6.22 faster CPU rasterizer, same pixels, two commits on top of `ef67c4a`: `80fa492` the rasterizer, `2f931d8` triangles skip rows outside the drawing area again (review fix: an area starting at y >= 512 drew row 511); the merge of `dw3-seq-nrpn-loop` into `dw3-gutter-width`; see [CPU rasterizer speed](#cpu-rasterizer-speed-p622). `a79bc8c` is `dw3-seq-nrpn-loop`: libsnd SEQ/SEP NRPN loop count from Data Entry, music loops again, one commit on top of `fa0aee5`; see below. `fa0aee5` is `dw3-spu-bus-volume`: P6.17 host mixer buses with per-bus gain, libsnd track routing and bus stats, two commits on top of `2caa505`. `2caa505` is `dw3-spu-envx-release`: backport of upstream `a7862ba` / #205, a voice that ends without repeat reads ENVX 0, fixes #188 voice starvation; its parent `fabc2bf` is `dw3-pal-aspect` PAL 240-line aspect) |
| Base upstream | `6d01339aadf16c5c9d6e44f44c0d66514752e472` |
| License | MIT — see [`vendor/psyz/LICENSE`](../vendor/psyz/LICENSE) |

Update the pin intentionally; call out submodule SHA changes in the
commit message. Submodule URL is the Larbaco fork until the MDEC wire
lands on Xeeynamo/psyz (READ-only upstream).

## CPU rasterizer speed (P6.22)

`psyz/src/platform/sdl3_software.c` is the renderer the Switch host draws
with outside battle. `dw3-soft-raster-fast` makes rectangles and triangles
4-5x cheaper in field and menu scenes without changing a pixel: rows are
clipped once into runs, texture and CLUT rows are looked up once per
primitive, opaque rectangles fill or copy, triangle edges and attributes step
with exact integer adds instead of per-pixel 64-bit divisions. Any change to
this file is gated on 0 differences against the previous revision:
`tools/soft_raster_bench.sh --ref <previous> -- --fuzz 1000000 <seed>` for a
few seeds (the fuzz covers full 10-bit and inverted drawing areas, gutter x0
0-1023 and width 1-1100, any gutter size, a 16-bit tpage, the horizontal
grid), the same with `--san` (ASan, LeakSanitizer, UBSan) and the replay
traces; method and numbers in [P6.22_FIELD_CPU.md](P6.22_FIELD_CPU.md).

## Gutter flush (P6.1)

`psyz/src/platform/sdl3_gpu.c` (`dw3-gutter-flush`, `8bde35d`): `UpdateScissor`
used to flush the queued batch only when the new drawing area was not empty.
DW3's field text layer is followed by the area name banner layer, which is
clipped to nothing (h=0) while no banner shows, so the text batch stayed
queued, was drawn under the empty area, and the gutter pass (`GutterActive`)
skipped it: field text and message boxes (the intro TV news caption bar) never
reached the side gutters on the PC GPU renderer. `Draw_SetAreaStart/End` now
flush when the area changes. The display gets the same pixels (same scissor);
only the gutters gain what was drawn while the area spanned the display.
Regression gate: the software rasterizer (`sdl3_software.c`) is untouched, so
`tools/soft_raster_bench.sh --ref 2f931d8 -- --fuzz 1000000 <seed>` (seeds 1,
7, 42) and the `--san` run must stay at 0 differences, and the replay suite
must stay green; results in [P6.1_WIDESCREEN.md](P6.1_WIDESCREEN.md). The
Switch software rasterizer draws immediately and is not affected.

## Gutter flush (sdl3_gpu)

`dw3-gutter-flush` (`8bde35d`, one commit on `dw3-soft-raster-fast`) changes
`Draw_SetAreaStart/End` in `psyz/src/platform/sdl3_gpu.c`: the queued batch is
drawn when the area start or end changes. `UpdateScissor` used to flush only
when the new area was not empty, so a layer followed by an empty-area layer
(the field text layer 0x1004 before the area name banner layer, clipped to
h=0) was drawn later under the empty area; the display scissor was the same
but the gutter pass skipped it, so field text and the message box never
reached the side gutters on the PC build (P6.1, news caption bar). The
Switch software rasterizer draws immediately and is not affected.

Gate for this pin: `sdl3_software.c` is byte-identical to `2f931d8`, so the
rasterizer gate above gives 0 differences (`tools/soft_raster_bench.sh --ref
2f931d8 -- --fuzz 1000000 <seed>` for seeds 1, 7, 42, and `--san` with
200000 for seeds 1, 7). The gpu change is gated by the replay suite: with
widescreen off the picture scenarios match the previous build pixel for
pixel (P6.1 doc, Results).

## Mixer buses (P6.17)

`psyz/spu.h` has a host extension that is not PS1 behaviour: voices belong to
one of four buses, each bus has a Q15 gain (`Psyz_SpuSetBusGain`), and the
CD/XA input has its own (`Psyz_SpuSetCdGain`). A voice takes its bus when it
is keyed on (`psyz_snd_voice_bus` reads `_svm_voice[v].seq_sep_no`, which
`SsUtKeyOn` sets to `0x21` and the SEP player sets to `(sep << 8) |
seq_access_num`), and `Psyz_SndSetSeqBus(seq, sep, bus)` is how the host
routes a track. Unity gains skip the multiply: the default mix is
bit-identical to the mixer without buses, which `spu_bus::
BusDefaultMixMatchesTheMixerWithoutBuses` pins with a hash rendered by
`2caa505`. `Psyz_SpuEnableBusStats` / `Psyz_SpuGetBusStats` count energy and
peak per bus before and after the gain (diagnostics only, off by default).
The PSP backend (`psp_sas.c`) does not implement these calls. Use in this
port: [P6.17_VOLUME.md](P6.17_VOLUME.md).

## SEQ/SEP loops (NRPN)

`psyz/src/psyz/midiread_host.c` is the host libsnd SEQ/SEP reader. PS1 songs
loop with NRPN events, not with the end-of-track meta: CC#99 = 20 starts the
loop, CC#6 (Data Entry) right after it is the repeat count (127 = forever),
CC#99 = 30 ends it. The reader used to drop the Data Entry value, so every
song with a loop played one pass and stopped (preview.30-rc7). It now follows
the retail handlers (field use, loop start in `next_sep_pos`, end-of-track
restart at `read_pos`); details and evidence in
[P3.6_AUDIO.md](P3.6_AUDIO.md). Unit tests: `psyz/tests/test_libsnd_loop.c`
(group `snd_loop`). The VAB attribute NRPNs (`SsFCALL.ccentry`) are still
stubs, so the reverb type / depth the songs send are not applied.

## SEQ table geometry (host override, no psyz change)

libsnd indexes the SEQ table as `_ss_score[seq][track]` with rows of
`t_max` entries of `sizeof(struct SeqStruct)` bytes (192 on a 64-bit host,
`SS_SEQ_TABSIZ`). The game's own table is sized for the PS1 struct, so the
port replaces `SsSetTableSize` (`platforms/pc/src/psyz_host_overrides.c`) with
a private table. It must keep the rows contiguous: the game plays the system
bank's second SEP file as tracks 16..31 of seq 0, which is entry `(1, track -
16)`. With a row stride that differs from the entry size those tracks (the win
jingle, menu and battle sounds) never played. Pinned by
`tools/tests/test_seq_table.py`. The psyz pin is unchanged by this fix
([P3.6_AUDIO.md](P3.6_AUDIO.md)).

## Recursive submodules

PSY-Z vendors SDL3 under `vendor/psyz/external/SDL`. After cloning or
updating this repo:

```bash
git submodule update --init --recursive vendor/psyz
```

Other nested deps (cimgui, nugget, decomp tools) come along with
`--recursive`; PC sample builds need at least `external/SDL`.

## PC renderer default

For host builds use:

```bash
-DPSYZ_RENDERER=sdl3_gpu
```

Smoke-test from the repo root (build dir is gitignored via `/build/`):

```bash
cmake -S vendor/psyz/samples/gpu_hello -B build/psyz-gpu-hello -G Ninja \
  -DPSYZ_RENDERER=sdl3_gpu
cmake --build build/psyz-gpu-hello -j"$(nproc)"
test -x build/psyz-gpu-hello/hello
```

On hosts missing optional SDL Linux packages, configure may need flags
such as `-DSDL_X11_XSCRNSAVER=OFF` (and audio backends OFF) until
`libxss-dev` / ALSA / Pulse are installed. See
[SDL3 Linux build dependencies](https://wiki.libsdl.org/SDL3/README-linux#build-dependencies).

## Switch

There is still no **pacman** SDL3 package for Switch homebrew (portlibs
ship SDL2 only). `devkitPro/SDL` branches `switch-sdl-3.2` /
`switch-sdl-3.4` build from source under `devkita64`, but they expose
GLES renderers without SDL_GPU — so PSY-Z's PC default `sdl3_gpu` does
not apply. P4.1 chose a **native libnx** backend instead; see
[SWITCH_BACKEND.md](SWITCH_BACKEND.md).
