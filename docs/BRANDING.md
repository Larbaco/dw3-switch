# DW3NX project artwork

The owner selected **4C — retro pixel art** on 2026-10-14: a blue crystal
inside a silver compass ring, orange pointers, cyan/orange orbital arcs and
DW3NX lettering on navy. These are authored project graphics, not disc assets.

## Active assets

- [`dw3nx-4c-master.png`](assets/dw3nx-4c-master.png): byte-exact approved
  1254×1254 source. SHA-256:
  `2c6199d264bfb6dc7d184c3d06ba9d37552a537ce8c360da1b65ad8f0338fee4`.
- [`icon-256.png`](assets/icon-256.png): 256×256 nearest-neighbor export for
  avatars and launcher packaging.
- [`platforms/switch/icon.jpg`](../platforms/switch/icon.jpg): 256×256 sRGB
  JPEG embedded by the game CMake `nx_create_nro(ICON ...)` path, shared by
  US/EU and ordinary/nxlink builds. The host-hello sample keeps its stock icon.
- [`banner-1280x640.png`](assets/banner-1280x640.png): horizontal README and
  Open Graph artwork, with emblem and wordmark side by side.
- [`release-cover-1920x1080.png`](assets/release-cover-1920x1080.png): centered
  16:9 release/social artwork without a version number.
- [`dw3nx-logo-transparent.png`](assets/dw3nx-logo-transparent.png): reusable
  1254×1254 RGBA emblem and wordmark with transparent outer background.

The banner is used by the README. The cover and transparent logo are reusable
exports, not an automatic change to GitHub repository settings or release
attachments. Published preview.26 assets remain unchanged; the icon is packaged
by subsequent builds. Existing gameplay captures, the historical candidate
archive and the functional title-menu atlas are retained. This update does not
redesign the in-game title/menu or resolve the preview.26 gameplay feedback.

## Derivation and validation

The approved master was copied unchanged. ImageMagick nearest-neighbor
(`-filter point`) resizing produces the 256×256 icon, 1280×640 banner and
1920×1080 cover; the icon is converted to sRGB TrueColor JPEG at quality 90.
See [Switch packaging](../platforms/switch/README.md#homebrew-icon-game-image).

Artistic derivatives used the built-in image generation tool with the approved
4C PNG as the local reference/edit target, inspected before generation. Prompts:

**Banner:**

> Use case: compositing. Asset type: project README banner, wide 2:1 layout.
> Derive from the approved DW3NX pixel art image. Preserve the exact blue
> crystal, silver circular mechanical compass, orange arrow points,
> cyan/orange orbital arcs and chunky DW3NX lettering style. Rearrange into a
> balanced wide composition: emblem at left, large DW3NX wordmark at right.
> Dark navy backdrop with sparse cyan/orange pixel stars. Crisp retro pixel
> edges, no blur, no bloom, no new characters or additional text. Produce one
> banner image.

**Transparent logo:**

> Use case: background-extraction. Asset type: transparent project logo.
> Extract the approved image's complete emblem AND DW3NX wordmark, preserving
> their original placement, original crisp pixel art, silver compass ring,
> blue crystal, orange pointers and cyan/orange orbital arcs. Remove only
> navy background and detached tiny star/confetti pixels. Keep all main emblem
> and wordmark edge outlines intact, no redesign, no extra text. Actual
> transparent background.

**Release cover:**

> Use case: compositing. Asset type: project release cover, landscape 16:9.
> Derive directly from the approved DW3NX emblem, keeping its recognisable
> blue faceted crystal, silver mechanical compass ring, orange arrow points,
> cyan/orange orbital arcs and chunky DW3NX pixel lettering. Center emblem
> above wordmark within wide navy negative space; restrained thin cyan/orange
> pixel circuitry extends horizontally, sparse pixel stars. Crisp retro pixel
> art with no bloom or blur. Exact sole text DW3NX, no subtitles, version
> numbers, extra objects, characters or watermark. One cover image.

The master/source identity, export sizes and alpha were checked. Independent
packaging with the existing preview.26 EU ELF/NACP and the real `elf2nro` tool
confirmed the ASET icon payload exactly matches this JPEG (SHA-256
`674e545336571237a2eed622b484dd6d748de994f46ec9c0b6b0aa48a4707e3c`).
This is packaging evidence; no new release or OLED launcher acceptance is claimed.
