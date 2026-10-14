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

## Runtime validation

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
