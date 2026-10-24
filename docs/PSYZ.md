# PSY-Z (vendor/psyz)

PC Psy-Q compatible shim used for Phase 1+ bring-up before a Switch
libnx backend exists.

## Pin

| Field | Value |
|---|---|
| Upstream | https://github.com/Xeeynamo/psyz.git |
| Working fork (submodule URL) | https://github.com/Larbaco/psyz.git |
| Commit | `a79bc8c` (`dw3-seq-nrpn-loop`: libsnd SEQ/SEP NRPN loop count from Data Entry, music loops again, one commit on top of `fa0aee5`; see below. `fa0aee5` is `dw3-spu-bus-volume`: P6.17 host mixer buses with per-bus gain, libsnd track routing and bus stats, two commits on top of `2caa505`. `2caa505` is `dw3-spu-envx-release`: backport of upstream `a7862ba` / #205, a voice that ends without repeat reads ENVX 0, fixes #188 voice starvation; its parent `fabc2bf` is `dw3-pal-aspect` PAL 240-line aspect) |
| Base upstream | `6d01339aadf16c5c9d6e44f44c0d66514752e472` |
| License | MIT — see [`vendor/psyz/LICENSE`](../vendor/psyz/LICENSE) |

Update the pin intentionally; call out submodule SHA changes in the
commit message. Submodule URL is the Larbaco fork until the MDEC wire
lands on Xeeynamo/psyz (READ-only upstream).

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
