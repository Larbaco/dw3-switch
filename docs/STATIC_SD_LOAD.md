# Static files on the SD card

Decision note, 2026-10-11. The US runtime is already a tree of files under
`sdmc:/switch/dw3/us/`. First boot extracts it from the owner's Mode2 BIN
when the tree is incomplete (`dw3_runtime_ensure` in
`port/cdfs/runtime_extract.c`). Later boots only `stat` the files. The BIN
is not read again. Git does not contain the BIN or the extracted tree.

This note records what may stop imitating a CD, and what must keep a
sector pace. It does not change clocks. Do not call `clkrstSetClockRate`
or `appletSetCpuBoostMode`.

## What is already a file

`cdfs_read` and `cdfs_read_at` (`port/cdfs/cdfs.c`) open the file for that
numeric id and `fread` it. No queue, no sector, no busy bit. Map headers,
`FILE_CACHE` (sound banks, battle files), and `boot_tim.bin` use that
path. Host overlays do not read `.PRO` at runtime: patch `0008` restores
`.data` and clears `.bss` in the linked binary (`port/overlays/overlay_host.c`).

STR movies are also files (`AAA/STR/*.STR`, 2352 bytes per sector). The
reader is a file. The delivery is still one sector at a time.

## What still imitates a CD

The decomp still calls `CD_READER`. On the host, patches `0009`, `0031`,
and `0041` finish the copy before `readFile` returns and leave the reader
idle, except for one case.

Field tiles. `FIELDSTG_seekStream` (`vendor/dw3_decomp/src/field/field_mode/stream.c`)
is the only caller that passes a non-NULL `done` pointer.
`cdfs_read_field_tile` copies the tile and sets `g_field_tile_busy`.
`isCdReading` stays true until `cdfs_poll`, which `vsyncCallback` calls
once per VSync. That is why preview.7 issues one tile per frame. The bytes
are already in RAM; the flag only blocks the next `fread`. Without the
flag, one frame used to open the map file once per visible tile, up to
about 30 times, and walking hitched.

Movies and XA. `STDWTITL_readStream` fills a 32-sector ring.
`dw3_str_cd_read_cb` reads the static `.STR` by LBA. `Psyz_CdPumpStream`
releases a budget of 150 sectors per second. The picture and the ADPCM
blocks share that file. Delivering the whole STR into the ring runs the
picture ahead of the audio. A 2048-byte-per-sector STR also drops the last
ADPCM blocks. `dw3_str_restore_from_disc` exists for trees that were packed
that way. A tree extracted by current `dw3_runtime_ensure` is already 2352
and that pass only checks the size.

## What can drop the CD model

Field, yes. Preload the whole map file once, when the area opens. Tile
reads become a copy from that buffer, `*done = 1`, reader idle on return.
`FIELDSTG_requestTiles` may then copy every visible tile in the same call.
Those copies are into buffers the streamer already allocated. The SD is
not read on each step. The decompressor in `FIELDSTG_drawMapTiles` still
draws one tile per frame, so the picture can still fill in tile by tile.

Sound banks and battle files, already. They use `cdfs_read_at` of the
whole file. `isCdReading` must be idle when `readFile` returns. If it
stays busy, `updateFileCache` never marks `FILE_LOADED`, the field banner
does not start, and `requestBattleFiles` never runs.

Movies and XA, no. Keep the file as the source. Keep one sector per
callback and the 150/s budget.

## Two steps

**Minimal, for the next preview.** On map open, one `fread` of that map
file into a host buffer keyed by file id. `readFile` with `done != NULL`
copies `offset * 2048` from that buffer. Remove `g_field_tile_busy`.
`isCdReading` is only `state != IDLE`, which is idle as soon as
`readFile` returns. Remove the `cdfs_poll` calls added for the one-tile
pace (the VSync hook and the one inside `FILE_CACHE.load`). Header and
cache reads stay on `cdfs_read_at`. The decomp is unchanged. Movie pace
is unchanged.

**Later.** Ordinary file reads use a byte offset. LBA stays inside the
movie callback. `files_us.json` and the numeric id stay, because the
decomp does not open paths.

## Do not

- Leave `isCdReading` true. Cache, field entry, and battle loads stop.
- Remove the busy bit and keep `fopen` / `fread` / `fclose` per tile.
  The hitch comes back.
- Pour the STR into the 32-sector ring, or drop the 150/s budget.
- Turn XA into a separate WAV. The blocks sit inside the video file.
- Mix memory-card work into this. `memcard0.mcr` stays on
  `memcard_host_ensure`.
- Extract EU here. The extractor and the linked `FILE_SECTORS` are US.
- Commit a BIN, a BIOS, or the extracted tree.
- Raise the CPU or GPU clock.
- Rewrite `RasterTriangle`.
- Send these patches to upstream PSY-Z. The movie pace lives in the
  pinned fork. The file shim stays in `port/` and `PORT_HOST` patches.

## Preview.7 OLED log (2026-10-11)

`v0.6.5-preview.7` booted on the OLED over nxlink. EGL came up at
1280×720. The overlay table registered 255 overlays. Slot 1 loaded. The
log shows field walking, button commands, then `FIGHTSTG`. The log client
was closed while the game was still in battle, so the capture stops there.

`frame dt us` : 156 samples, each covering 60 VSyncs. The mean was 1.017 s
per 60 frames, about 16.96 ms/frame, about 59 FPS. Most samples sat near
1 s. Some reached 1.51 s per 60 frames, about 25.2 ms/frame across that
window. P5.2 stays open. The done-when is no frame over 16.7 ms in field
and battle, docked and handheld. This log is 60-frame sums from one
session, and some windows are well above that bar.

One error during PSY-Z init, and the game continued:

```
[E] KernelInit: cannot create the kernel synchronisation objects
```

The log does not say whether the failed object was a mutex or an event.
Treat that as an open defect. It did not stop this boot. It is not part
of the map-preload change.

`VSync(n>1) is not fully implemented` was logged once. That warning is
the known partial `VSync` path. The session did not stop on it.
