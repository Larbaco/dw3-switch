# PC title widescreen background

The SDL3 GPU US path copies the original runtime background sprite sheets and
VRAM palettes into a private 320x240 background-only RGBA frame. Static art is
cached; the native title task supplies all eight animation frame IDs and its
random sparkle positions. No game artwork is embedded or committed.

The native center renderer and 960x720 logo/menu geometry remain independent of
this texture. The GPU overlay uses the background frame only for the side panels.
The original title edge fade supplies its subtractive edge darkness to the side
frame. Initialization and task destruction reset the cache, and callbacks reject
non-title modes. Switch, movies, field scenes, SDL3 GL and EU are unchanged.

The reviewed presentation target is 1280×720. Evidence belongs in the ignored
`tmp/evidence/` directory.

## Runtime validation of the original follow-up

US SDL3 GPU production build ran in dedicated Xvfb :108 at 1280×720,
with an empty temporary memcard under `/mnt/cabal/tmp`. The game window
initially recenters after its one-time 3× resize, avoiding the old negative
Y offset on a 720-pixel virtual display. `final-720-a.png` shows all three
approved buttons. A later `final-720-c.png` contains background animation
changes across both the original center and the extended left margin
(pixel difference bounds x43..264, y117..185). At the static reference frame,
the seam columns x159/160 and x1119/1120 match on all 720 rows; no black gap.

Evidence is local and ignored at `tmp/evidence/title-widescreen/`. Linux Vulkan
and SDL3 GL builds and `tools/apply_patches.sh --check` pass. This extension
is enabled only with `DW3_PC_INTERNAL_RES=3`; gameplay widescreen, Switch,
Windows and macOS runtime validation remain outside this title-only change.

Select opened the existing port-options shell and Triangle closed it back into
the three-row menu (`dw3-port-options: close (resume runOptions)`). The background
extension survived both states. The pre-existing logo overlay covers part of the
options panel while it is open; this unrelated foreground layering issue remains.
Load Game also reached the normal memory-card selector with empty temporary
cards, and Triangle returned to the original title. The added background cleared
from the non-title selector and reappeared on title return.

## Main-based follow-up

This replacement isolates PR #126 on current main instead of its closed
PR #121 base. It preserves the native title overlay from #127, including
EU label refresh, logo fade, and EU logo capture. Patch 0054 runs after
0050 so both edge-fade callbacks coexist. The captures above belong to the
earlier branch; fresh replacement evidence is recorded below.

### Fresh main-based validation (2026-10-14)

Feature source `a3665c03f4d664239b3d37c7ed1c8abfa92bc4c0` was validated
with main `bc26e075c0bd98b6a1f54d7e4108c8176c90dad8` (#132) merged in
the isolated validation worktree. Fresh SDL3 GPU and SDL3 GL builds and
patch application, including 0054 after 0050–0053, passed. The US GPU
runtime used private Xvfb and temporary empty cards: Port options open /
return and Load Game / cancel retained the title background, with no
background leakage into the memory-card selector. The title fade was
observed.

At 1280×720, the left seam matched on all 720 rows. The right seam differed
at three pixels on rows 582–584, a small purple fragment rather than a
continuous stripe. This is a remaining visual limitation; both seams are
not claimed to be pixel-perfect. Local evidence is outside Git under
`/mnt/cabal/tmp/dw3-validate-evidence/title-*4.png` and `pr137-*.log`.
Windows/macOS runtime acceptance remains pending; the added backdrop is
still PC US SDL3 GPU / 3× only.
