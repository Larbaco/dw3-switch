# Provenance of `docs/assets/`

This folder holds the images used by the README and the docs. Each file is
either artwork made for this project or a screenshot the owner approved. No
file here is extracted from the game disc. Following AGENTS.md, nothing taken
from the disc goes into git, and that includes upscales and traces of game
images.

## Project artwork (made for this project)

| File | What it is | Source |
|---|---|---|
| `dw3nx-4c-master.png` | Approved DW3NX emblem and wordmark (1254×1254) | [BRANDING](../BRANDING.md) |
| `icon-256.png` | 256×256 nearest-neighbour export of the master | [BRANDING](../BRANDING.md) |
| `banner-1280x640.png` | README / Open Graph banner derived from the master | [BRANDING](../BRANDING.md) |
| `release-cover-1920x1080.png` | Release cover derived from the master | [BRANDING](../BRANDING.md) |
| `dw3nx-logo-transparent.png` | Master with a transparent background | [BRANDING](../BRANDING.md) |
| `candidates/` | Archive of earlier art candidates | [candidates/README](candidates/README.md) |
| `title-menu/digivice-orange-blue-atlas.png` | Concept reference for the PC title-menu plaques | [TITLE_MENU_DESIGN](../TITLE_MENU_DESIGN.md) |

The banner, the cover and the transparent logo were made with an image
generator, using the approved master as the only reference image. The prompts
are recorded in [BRANDING](../BRANDING.md). The icon is a plain resize of the
master. The candidate in `candidates/icon-256/` was generated from a text prompt
for an abstract wireframe design, and it shows no game art. None of these files
used a game image, logo, character or font as a reference or as an edit target.

The title-menu atlas is a concept image only. Its six plaques are generic
frames with no game art, logo or text. The game does not load it: the PC
overlay draws its own plaques in code (`platforms/pc/src/title_overlay_art.c`,
see [PC_TITLE_OVERLAY_ART](../PC_TITLE_OVERLAY_ART.md)). The title-screen
mockup that showed the original logo and background was left out of git on
purpose.

## Screenshots (owner-approved exception)

`screenshots/*.png` are frames captured from the PC build, running on the
owner's own disc. They show the game's copyrighted art. They are kept here only
as the small set of README screenshots the owner approved (AGENTS.md, owner's
machine rule 3). Do not add to this set without the owner's OK. Every other
capture goes to the git-ignored `tmp/evidence/`.

## Adding a file

When you add an image to this folder, add a row here with its source. If the
file shows any game art, add it only after the owner approves.
