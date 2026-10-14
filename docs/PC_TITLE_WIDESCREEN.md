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

Runtime screenshot validation is required before marking this visual experiment
complete. Evidence belongs in the ignored `tmp/evidence/` directory.
