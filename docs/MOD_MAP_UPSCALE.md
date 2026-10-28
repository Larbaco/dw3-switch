# MOD: map upscale (design note, phase 1)

Status: design only, no game code changed. Owner request (2026-10-28,
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
layout allows.
