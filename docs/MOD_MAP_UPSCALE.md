# MOD: map upscale (design note and as built, phase 2)

Status: **phase 2 built** (2026-10-28): the loader, the extraction tool and the
tests are in the tree; see [section 7](#7-as-built-phase-2) for what exists and
how to install the mod on the SD card. Sections 1 to 6 are the design as
written in phase 1 and are kept for the reasons behind it. Owner request (2026-10-28,
translated): "generate some AI-upscaled images for me to use later; this will
be the first MOD, upscaling some assets, starting with the map". The owner's
preferred in-game scaler on the map view is Nearest (pixel look). Related:
[P6.32_MAP_ZOOM.md](P6.32_MAP_ZOOM.md) (map view, cache, scalers),
[TEXTURE_PACKS.md](TEXTURE_PACKS.md) (P8.2 pack layout, P8.5 modding API),
[ROADMAP.md](ROADMAP.md).

## 1. What phase 1 produced (local only, never in git)

Rule from [AGENTS.md](../AGENTS.md): anything derived from the owner's disc,
upscales included, stays out of git. Only original fan art with a LICENSE and
a PROVENANCE.md may be committed. So this repo gets **loader code and a manifest
format**, never images.

Locally, under `tmp/assets/mods/map-upscale/<date>/` (git-ignored): the two
bare map bases (392 x 352, Asuka h0 and Amaterasu h1; US and EU are
byte-identical), x2 and x4 results from 11 methods (Real-ESRGAN x4plus,
x4plus-anime, animevideov3; Upscayl ultrasharp, remacri, digital-art, LSDIR
compact, Nomos8kSC; and non-AI nearest, Lanczos, Scale2x), contact sheets, and
a README with tools, commands, timings (0.4 to 3.1 s per image on an RX 6700
through Vulkan) and licence notes. Observations that shape the design:

- The AI models cost seconds per image, so this is a **one-time, offline step by
  the user** (or a tool in `tools/`), not something the console runs.
- Clean-edge models (x4plus-anime, animevideov3) keep the banner lettering and
  glyphs; texture-adding models invent detail. The owner picks per pack.
- A pack is pure derived data: it is generated on the user's machine from their
  own disc and shared nowhere by this project.

## 2. Pack layout (proposal for P8.2 / P8.5)

```
sdmc:/switch/dw3/mods/<pack>/
  mod.json                  manifest (section 3)
  map/
    map_<region>_h<half>.png          region us|eu, half 0|1; same size as the source
    map_<region>_h<half>@2x.png       explicit scale in the name
    map_<region>_h<half>@4x.png
    reveal/
      towns@4x.png          optional sprite sheet of the town icons (section 4, option B)
      areas@4x.png          optional sprite sheet of the visited-area icons
```

- The scale is a **whole multiple** of the source 392 x 352 (784 x 704 at 2x,
  1568 x 1408 at 4x). A file with any other size is rejected with a log line.
- PNG, 8-bit RGB or RGBA. The loader uses the highest scale present that the
  device budget allows (4x RGBA is 8.8 MB per half, 2x is 2.2 MB).
- Missing files fall back to the vanilla map; a pack is never required to boot
  (same rule as TEXTURE_PACKS).
- The files are per region because a region with edited text (the patched EU,
  P7.2) may differ; the manifest can declare a pack valid for both
  (`"regions": ["us", "eu"]`), since the bare bases are identical today.

## 3. Manifest (`mod.json`)

```json
{
  "format": 1,
  "id": "map-upscale-example",
  "name": "Map upscale (example)",
  "version": "0.1.0",
  "author": "user",
  "regions": ["us", "eu"],
  "map": { "scales": [2, 4], "filter": "nearest" },
  "generator": "realesrgan-x4plus-anime"
}
```

`filter` is a hint (`nearest`, `linear`, `sharp`); the player's scaler option on
the map view still wins. Unknown keys are ignored; a `format` greater than the
loader's refuses the pack with a log line. Load order and conflicts between
mods belong to the P8.5 API; phase 2 only needs "one active pack, chosen in
Port options".

## 4. How the map view composites at higher resolution

Today ([P6.32 section 7](P6.32_MAP_ZOOM.md)): `map_view.c` takes the bare base
(cache `map_<region>_h<half>_s0.rgba` or a VRAM capture), draws the **reveal
layer** (towns and visited-area icons, cells copied from live VRAM by
`compose_reveal`) **into the 392 x 352 image**, then runs a scaler (nearest,
sharp bilinear, Lanczos2, FSR 1 at `MV_SCALE` 3) and shows the texture. The
reveal layer changes with progress, so it must not be baked into a pack image.

With a pack:

1. **Base.** If the pack has `map_<region>_h<half>[@Nx].png`, decode it into an
   RGBA buffer of `392*N x 352*N` and use it as the base at scale N instead of
   the captured image. The built-in scaler stage is skipped for the base (it is
   already upscaled); with `filter: nearest` the GPU samples it with nearest,
   which the owner prefers.
2. **Reveal.** In order of preference:
   - **A (phase 2 default): composite at 1x, scale only the overlay.** Keep
     `compose_reveal` as is, but into a separate 392 x 352 RGBA layer with
     alpha; scale **that layer alone** to N with nearest and alpha-blend it over
     the upscaled base when the texture is built. The icons stay crisp pixel art
     while the terrain is upscaled, and the reveal still follows progress
     because it is rebuilt per open (about 1 ms at 1x).
   - **B (later): pack-supplied sprite sheets.** `towns@4x.png` and
     `areas@4x.png` indexed by the game's frame ids; `compose_reveal` draws from
     them at N. Needs the sprite id mapping from patch `0146` and a sheet
     producer in `tools/`; only worth it if the owner wants AI-upscaled icons.
3. **Geometry.** All map-space maths (spot positions, hit boxes, scroll limits,
   the 392 x 352 box) stays in **source pixels**; only the texture is N times
   larger and the quad keeps its source-space size. No logic change. With no
   pack active the exact current path runs, so replays and caches are unchanged.
4. **Memory and SD.** 4x RGBA is 8.8 MB per half; load one half per open and
   free it on close. The existing cache stays valid: it holds only the bare 1x
   base and the outputs of the built-in scalers.

## 5. What stays in git and what does not

| In git | Not in git |
|---|---|
| Loader code (`port/map_view/` pack hook, a `port/mods/` manifest parser) | Any pack PNG (derived from the owner's disc) |
| The manifest format and this note | Contact sheets, test outputs, upscaler binaries and models |
| A `tools/` script that runs a user-installed upscaler over the user's own exported base (no models, no images) | The user's `mods/<pack>/` folders |
| Unit tests with synthetic images (generated gradients, not game art) | Replay references that include a pack |
| An optional pack of original fan art, with LICENSE and PROVENANCE.md | Redistributing AI outputs of the game map, whatever the model licence |

Model licences vary (some community models are non-commercial share-alike);
the project ships none and recommends none on licence grounds, so the user is
the generator and the owner of their output.

## 6. Phase 2 step list (loader implementation)

1. `port/mods/`: pure-C manifest parser (`mod.json`, small JSON subset), pack
   discovery under `sdmc:/switch/dw3/mods/` (PC: `<config dir>/mods/`), an
   active-pack key in `port_options.cfg` (P9.4), unit tests with synthetic
   manifests.
2. PNG decode for the map: reuse a decoder already in the tree or vendor a
   small one (check licence and Switch size); validate size = 392 x 352 times
   N, N in 2..4, reject anything else.
3. Hook in the map view open: the pack base replaces the capture when present,
   the VRAM path stays as fallback; log `dw3-map: pack <id> base Nx (file)`.
4. Reveal option A: render `compose_reveal` into a separate 1x alpha layer,
   nearest-scale it and blend over the pack base at texture build; keep the
   legacy in-place path when no pack is active (byte-identical).
5. Texture size and filter plumbing in the SDL3 overlay (PC) and the GLES path
   (Switch): quad in source space, texture N times larger; check seams and the
   16:9 centre of patch `0080`.
6. Replay tests: pack off must match the existing `us-map-screen` and
   `eu-map-screen` references (0 differing pixels); a pack-on test uses a
   synthetic pack built by a fixture script, not committed images.
7. `tools/map_pack/`: helper that exports the user's base as PNG, runs a
   user-chosen upscaler command, and writes `mod.json`.
8. Docs: modder section (P8.5), update TEXTURE_PACKS and the roadmap; an OLED
   check with the owner on a real pack (the scaler choice stays his).

Open questions for the owner: which model the example generator command should
name (see the local contact sheets), whether reveal option B is wanted, and
whether `mods/<pack>/` should also host later mods (title, battle), which this
layout allows. **Answered 2026-10-28:** the model is realesr-animevideov3, the
reveal sprites are upscaled too, and `mods/<pack>/` stays open for later mods.

## 7. As built (phase 2)

Owner decisions (2026-10-28): the model is **realesr-animevideov3** (through
realesrgan-ncnn-vulkan on the GPU); the reveal layer is upscaled too (the
game's cells one by one, a variant of option B of section 4); no Options row is
added, the mod is on when a valid pack is in the folder.

### What is in git

| Path | What |
|---|---|
| `port/mods/mod_pack.[ch]` | pure-C loader: the `mod.json` subset parser, pack discovery, validated PNG loads, alpha blend |
| `port/mods/png_stb.c`, `port/mods/third_party/stb_image.h` | the PNG decoder (stb_image v2.30, public domain / MIT, PNG only, from memory), in its own TU |
| `port/map_view/map_view.c` | `mod_build()`: builds the N x texture when a pack is valid; one log line says what happened |
| `port/map_view/map_view_core.[ch]` | `dw3_mv_disc_load_entry` (entries 1 and 2 of the image file, for the extractor) |
| `tools/mods/extract_map_assets.py`, `tools/mods/map_assets_dump.c` | extract the base map and the reveal sprites from the user's own `runtime/<region>`, run the user's upscaler, write the SD layout |
| `tools/mods/make_synthetic_pack.py` | gradient pack for tests (no game art) |
| `tools/tests/test_mod_pack.py`, `mod_pack_main.c` | unit test of the loader |
| replay `us-map-view-mod`, `eu-map-view-mod` | the map view with the synthetic pack |

No image, frame or pack is in git.

### Pack layout (format 1)

```
sdmc:/switch/dw3/mods/<pack>/            PC: <config dir>/mods/<pack>/
  mod.json
  map/map_h<half>@<N>x.png              SHARED by US and EU (P51 item 3), half 0|1, N 2..4
  map/reveal/h<half>/<kind>_<id>@<N>x.png     shared sprites
  map/map_<region>_h<half>@<N>x.png     one region's own file: wins over the shared one
  map/reveal/<region>_h<half>/<kind>_<id>@<N>x.png
```

**One pack for both regions (P51 item 3, 2026-10-29).** The loader tries the
region's own file first and then the shared one, for the map and for each
sprite (`dw3_mod_load_map` / `dw3_mod_load_sprite`; an old pack with only
per-region files works as before). A pack that serves both discs lists
`"regions": ["us", "eu"]`. The log line of an open ends with
`shared=A region=B`: how many of the pack's files (the map and each distinct
sprite) came from the shared names and how many from the region's own.
**Compared on the owner's discs (2026-10-29, `--extract-only` on both
runtimes):** the two bare maps are identical in both halves, and so are
91 of 94 reveal sprites of half 0 and 84 of 94 of half 1. The ones that
differ between US and EU are `areas` 59, 60 and 61 (both halves) and, in half 1,
`towns` 0, 34, 46, 47, 48, 49 and 50; a pack made from both discs keeps those as
per-region files and shares the rest.
`kind` is `towns` (map sheet entry 1) or `areas` (entry 2, the visited-area
icons); `id` is the sheet's frame id (the `frame` of a reveal piece of patch
`0146`). A map file must be exactly **392 x N by 352 x N**; a sprite exactly its
frame box times N (the box comes from the game's own sheet at run time), with
straight alpha (the original is VRAM colour 0 = transparent). Any other size is
refused and logged. The loader takes the highest N the manifest lists that
loads (cap 4). Manifest keys read: `format` (1), `id`, `name`, `version`,
`regions`, `map.scales`, `map.filter` (a hint; the scaler row on the view
wins), `map.reveal` (true when the sprites are present), `generator`. Unknown
keys are ignored; a `format` above 1 refuses the pack.

### What the view does with a pack

- Which pack: the first folder (by name) under the mods folder whose manifest is
  valid and lists the running region. `DW3_MOD_PACK=<folder>` picks one (PC,
  tests); `DW3_MODS_DIR` moves the mods folder (tests). Off switch:
  `map_mod=0` in `port_options.cfg` (a file-only key, default 1, like
  `map_screen`). No pack, an unreadable one, a wrong size: the original path
  runs, unchanged and bit-identical (the replays prove it).
- The picture: the pack's map, then every reveal piece the game sends (the
  towns of the progress list, the visited areas) blended on top at N x: the
  pack's upscaled sprite, or, where the pack has none that is valid, the game's
  own cell drawn N times larger with nearest. Geometry stays in source pixels
  (spots, cursor, name box, scroll limits, the 392 x 352 box); only the texture
  is N times larger and the quad keeps its source-space size. The cursor, the
  balloon and the name box are the game's own, drawn as before.
- Scalers: with a pack the CPU scalers are not run; **ZL / ZR** only switches
  the GPU filter (Nearest is nearest, the others bilinear). The HUD adds
  `MOD 4X`.
- Memory: 1568 x 1408 RGBA is 8.8 MB, held while the view is open plus one copy
  for the texture hand-off; freed on close.
- Log, one line per open (AGENTS.md rule 12). In use:

  `dw3-map: mod: pack "map-upscale-animevideov3" 1.0.0 loaded: map_us_h0@4x.png 1568x1408, reveal 17 pieces from the pack, 0 at 1x nearest, 0 skipped, 41 ms`

  followed by `dw3-map: texture 1568x1408 queued for the GPU (...)`. On
  fallback: `dw3-map: mod: original map (<reason>; folder <dir>)`, for example
  `no packs in the mods folder`, `no mods folder ...`,
  `pack "x": map_us_h0@4x.png is 1560x1408, expected 1568x1408`,
  `map_mod=0 in port_options.cfg`. At boot `dw3-map: mods folder <dir>` says
  where it looks.

### Making the mod (extraction and upscale)

```
tools/mods/extract_map_assets.py \
  --runtime runtime/us [--runtime runtime/eu] \
  --out tmp/assets/mods/map-upscale/<date> \
  --upscaler <realesrgan-ncnn-vulkan> --models <its models folder>
```

It builds `map_assets_dump` with `cc`, reads the sheet and image files by the
file ids of `runtime/<region>/files_<region>.json` (US `0x78A` on, EU `0x799`
on), writes the sources to `OUT/src/<region>/`, upscales with
`-n realesr-animevideov3 -s 4`, and writes
`OUT/sd/switch/dw3/mods/map-upscale-animevideov3/`. The base map goes through
the model as it is. A reveal sprite has alpha and the model works on RGB only,
so the colour is **bled** into the transparent texels (no dark halo), padded by
4 px, upscaled; the alpha goes through the model on its own as a grey image;
both are cropped to exactly N times the box and recombined. animevideov3 is a
x4 model, so N = 4 needs no resize step. `--extract-only` stops after `src/`;
`--model` names another ncnn model (the loader does not care which model made
the files). The US run (2 maps and 171 sprites) took about two minutes on the
RX 6700. The reveal sprites need the game's VRAM layout: entry 1 (towns) of the
image file goes to (0x380, 0x100) and entry 2 (areas) to (0x300, 0x100), where
their 4-bit palettes are the last rows of the page.

### Installing the mod on the SD card

1. Run the tool above, or take a pack it made, so that you have
   `.../sd/switch/dw3/mods/<pack>/`.
2. Copy the `switch` folder onto the SD card root (it merges into
   `sdmc:/switch/dw3/`), so that `sdmc:/switch/dw3/mods/<pack>/mod.json` exists.
3. Start the game, open the Map (Plus, Map), press **Y**. `dw3.log` has the
   `dw3-map: mod: pack ... loaded` line. To go back to the original map, delete
   the folder or set `map_mod=0`.
4. On the PC the same folder goes under `<config dir>/mods/` (next to
   `port_options.cfg`).

A pack made from one runtime lists that region only (honest: nothing was
compared). **Give both** (`--runtime runtime/us --runtime runtime/eu`): the tool
compares every piece pixel for pixel, upscales the identical ones **once** into the
shared names (about half the time of two separate runs), writes the pieces that
differ as per-region files, lists `["us", "eu"]` and prints
`h0: identical: 91 of 94 pieces (regions: us, eu)`. `--extract-only` prints that
line without the upscaler (the comparison needs no GPU). The upscaled files are
derived from the owner's disc: never commit them.

### Tests

- `python3 -m pytest tools/tests/test_mod_pack.py`: the loader on synthetic PNGs
  (valid, missing file, missing folder, wrong size, truncated PNG, newer
  `format`, junk manifest, other region, sprite size mismatch, blend clipping),
  the C90 compile check, and the extractor's bleed and pad helpers.
- `test_mod_pack.py` also covers the shared fallback (a pack with only shared
  files loads on US and EU, a region file or sprite wins over the shared one,
  `Dw3ModImage.shared` says which). Replay `us/eu-map-view-mod-shared`: the synthetic
  pack with one shared map and no per-region file (`--shared` of
  `tools/mods/make_synthetic_pack.py`), log `loaded: map_h0@4x.png ... shared=1 region=0`.
- Replays: `us-map-view` must still match its references with no pack present
  (0 differing pixels, see the PR). `us-map-view-mod` / `eu-map-view-mod` run
  the same recording with the synthetic pack written to the run's config folder
  and check the log; their references are their own (`--update` once, outside
  git).
