# Runtime data layout

Shared relative layout for the Phase 1+ hosts. PC and Switch use the same
paths under a version root; only the mount prefix differs.

## Paths

| Host | Root |
|---|---|
| PC (dev) | `runtime/<ver>/` (repo-local, gitignored) |
| Switch SD | `sdmc:/switch/dw3/<ver>/` |

`<ver>` is `us` or `eu`. Example US trees:

```text
runtime/us/                    # PC
sdmc:/switch/dw3/us/           # Switch (same relative layout)
  files_us.json
  SLUS_014.36
  boot_tim.bin                 # extracted by packer when available
  # BOOT_IMAGE.txt             # fallback if the executable tail is unavailable
  AAA/
    PRO/
    DAT/
    STR/
    ...
  memcard/                     # required on Switch; can start empty
  port_options.cfg             # Port options (P9.4), created on the first change
```

`port_options.cfg` is the only file the running game writes next to the
runtime data besides `memcard/` (and `dw3.log` / crash reports one level up
on the Switch). Format and rules: [PORT_OPTIONS.md](PORT_OPTIONS.md#persistence).

EU uses `files_eu.json` and `SLES_039.36` instead of the US names, plus its
own linked file-sector table and `STAGE_VRAM` address for `boot_tim.bin`.

## Packing

From a completed disc extract + file map (`docs/DISC_SETUP.md`):

```bash
python3 tools/pack_runtime.py --version us --disks disks/us --out runtime/us
python3 tools/build_file_map.py --check runtime/us/files_us.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c

# EU (owner disc; same layout, SLES_039.36 + files_eu.json):
python3 tools/pack_runtime.py --version eu --disks disks/eu --out runtime/eu
python3 tools/build_file_map.py --check runtime/eu/files_eu.json \
  --file-table vendor/dw3_decomp/src/engine/file/file_table.c
```

`pack_runtime.py` refuses to run if `files_<ver>.json` is missing. If
`--out` already exists and is non-empty, pass `--force` to wipe it first.

Form-2 `.STR` files in the extract are 2048 bytes per sector. That drops
the last two XA ADPCM blocks and the opening soundtrack runs slow. On
boot, the game rewrites any such file to raw Mode2/2352 when it can see
the owner's disc image. It looks for `DW3_DISC_BIN`, `DW3_DISC_CUE`, then
`disc.cue` / `disc.bin` / a single `*.cue` or `*.bin` in the runtime root
and in the parent directory. `boot_tim.bin` is never a disc image. A CUE
whose `FILE` name is not on the card still selects the only other `.bin`
in that directory when the file is large enough to cover the STR extents.
Put the pair here on the card:

```text
sdmc:/switch/dw3/Digimon World 3.cue
sdmc:/switch/dw3/Digimon World 3.bin
sdmc:/switch/dw3/us/AAA/STR/    # rewritten in place on first launch
```

The EU NRO also extracts from an owner-supplied Mode2/2352 image. For EU,
place the pair in `sdmc:/switch/dw3/eu/` (the runtime root), for example
`disc.cue` + `disc.bin`; the parent folder remains a supported fallback. The
EU executable must be `SLES_039.36`; a disc with only `SLUS_014.36` is rejected.
The NRO writes `files_eu.json` to a temporary file only after all assets and
the EU boot TIM have been extracted. Because FAT/exFAT does not replace an
existing file with `rename`, runtime outputs move an existing destination to
`.dw3-replace-backup` before installing the complete temporary file; an install
failure restores the previous destination. After a reset, a retry recovers any
leftover backup before replacing that output. The versioned map is the
completion marker, and CDFS validates its identity and mapped file sizes
before game startup. A failed or interrupted extraction is not accepted and
can resume from complete files on a later launch. US uses `files_us.json` /
`SLUS_014.36` and retains its existing root and disc lookup defaults.

A US tree created by the startup extract below already stores `.STR` as
raw sectors, so this rewrite only stats those files. A tree packed earlier
at 2048 bytes per sector still takes the rewrite. That copy is about 260 MB.
On Switch the screen keeps the dark blue clear and draws a progress bar
while the log says `restore` (see `docs/P3.7_MOVIES.md`). The next launch
only checks file sizes.
The same rewrite can be done on the PC before copying:

```bash
python3 tools/restore_form2_str.py \
  --runtime runtime/us \
  --bin "/path/to/Digimon World 3.bin"
```

A tree left at 2048 still boots; the movie picture is paced, and the
sound stays slow until a disc image is visible at startup.

EU `dw3-pc` link uses `-DDW3_VERSION=eu` (`docs/P3_EU_BUILD.md`); run still
needs a local `runtime/eu` tree (gitignored). Switch EU NRO:
`./tools/build_switch_game.sh --eu` sets
`DW3_DEFAULT_RUNTIME=sdmc:/switch/dw3/eu` (same CMake `DW3_VERSION` flag as
PC). Create empty `runtime/eu/memcard/` before copying to the card.

### US boot extracts the runtime when the tree is missing

`dw3_runtime_ensure()` in `port/cdfs/runtime_extract.c` runs from the PC and
Switch boot paths before `cdfs_load_map`. It selects the matching version's
map, executable, linked sector table, and boot-TIM address at build time.
Switch EU builds accept a packed `runtime/eu` tree or extract EU from the
matching Mode2 BIN/CUE found in `sdmc:/switch/dw3/eu` or its parent.

When the matching `files_<ver>.json`, executable, `boot_tim.bin`, and every
mapped file are already in the runtime root, boot only stats them. A Form-2 `.STR` whose
size is `sectors * 2048` still counts, and the rewrite above upgrades it when
a disc image is visible. The second boot does not read the BIN.

When that tree is missing and the same BIN search as the STR rewrite finds a
matching Mode2/2352 image, boot creates the runtime root (`DW3_RUNTIME`, or
`sdmc:/switch/dw3/<ver>` / `runtime/<ver>`) and extracts:

| Output | What is written |
|---|---|
| `files_<ver>.json` | Version-linked `FILE_SECTORS` / `FILE_SECTOR_COUNTS` joined to ISO9660 paths, the same way `tools/build_file_map.py` builds the map. The JSON is written last and is the completion marker. |
| `SLUS_014.36` (US) or `SLES_039.36` (EU), plus other Form-1 files | 2048-byte user payload at offset 24 of each 2352-byte sector. |
| `AAA/STR/*.STR` | Raw Mode2 sectors, `FILE_SECTOR_COUNTS * 2352`. |
| `boot_tim.bin` | PS-X EXE tail at US `STAGE_VRAM` `0x800A4CA4` or EU `0x800A5DE0`, load `0x80010000`. Same parse as `pack_runtime.py`. |
| `memcard/` | Empty directory. It is not on the disc. |

A missing disc image is not a boot failure when the matching tree is already
present. For EU, an existing incomplete root without a matching disc fails
before CDFS; a wrong-region executable is rejected before extraction.
Preview.4 only rewrote the 13 `.STR` files; removing `us/` on 2026-10-11
stopped that build. This extract is the follow-up. It was checked with a
synthetic Mode2 image (`tools/tests/test_runtime_extract.py`). It has not
been run on the OLED. The NRO and the git tree still contain no game bytes.

On Switch the copy drives the same progress hook as the STR rewrite
(`dw3_str_restore_set_progress`). The bar swaps about every 300 ms, matching
the restore, because `eglSwapBuffers` waits for vsync.

### Memory-card directory

The Switch host expects `<runtime-root>/memcard/` to exist. US and EU boot
create that empty directory after accepting the matching runtime. Saves are `save0.bin` /
`save1.bin` (plus optional `saveN.name`). An empty directory is enough;
`memcard_host_ensure()` does not invent a save file. Older `memcardN.mcr`
images from this port migrate once into the flat file and are renamed to
`.mcr.bak` (see `port/memcard/README.md`).

The runtime packer does not preserve empty directories. Switch boot creates
the directory for a valid packed tree; when testing the PC host, create it
under the selected runtime root:

```bash
mkdir -p runtime/us/memcard
```

For Switch this is `sdmc:/switch/dw3/<ver>/memcard/`. An empty directory is
enough. The 2026-10-10 hardware smoke initially stopped at `memcard_host_ensure()`
until the empty directory was created.

## Boot image

On PS1 the boot TIM is the executable's tail, resident at `OVERLAY_VRAM`
before overlays overwrite that region (`BOOT_IMAGE_RECT` in
`vendor/dw3_decomp/src/engine/system/main.c`). The packer writes
`BOOT_IMAGE.txt` explaining the gap; extract the tail into
`runtime/us/boot_tim.bin` when assets are available. `pack_runtime.py`
extracts the TIM from the version executable (STAGE_VRAM) when
`disks/<ver>/boot_tim.bin` is missing. US boot uses that same tail parse
when it extracts `SLUS_014.36` from the BIN. The PC host loads the file into
`STAGE_VRAM` before `dw3_game_main` (see `docs/P2.8_TITLE_BOOT.md`).

## Formal CDFS checks

PC and SD share this layout. After packing (or copying the card tree):

```bash
./build/cdfs_suite --root runtime/us
./build/cdfs_suite --root /path/to/switch/dw3/us   # mounted/copied SD
```

See `docs/P4.5_STATUS.md` and `port/cdfs/README.md`.

## Git

`/runtime/` is gitignored. Never commit game bytes from `disks/` or `runtime/`.
