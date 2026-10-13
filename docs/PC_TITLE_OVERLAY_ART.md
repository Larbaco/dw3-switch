# Native-resolution PC title overlay art

The previous PS1 TILE plaque reconstruction was rejected visually. The native
PC presentation now has an independent 960x720 transparent RGBA artwork canvas.
`platforms/pc/src/title_overlay_art.c` provides the graphical component without
SDL, game state, input, or PSY-Z dependencies. It does not draw the original game
logo or background. The PC presentation integration owns when it is visible.

Plaques occupy x=230..729 and y=400..453, 460..513, 520..573. Their steel-blue
bevels, diagonal clipped corners, metal side clamps, amber lamps, and illuminated
focus rails are authored in native canvas pixels. Label coverage is rasterized
at 29px from DejaVu Sans Condensed Bold with grayscale antialiasing. Compiled RLE
coverage avoids a runtime font dependency; the font license is included beside
the component. Focus changes color and light intensity without moving geometry.

The SDL3 GPU PC integration packs the three generated states into one static texture
and draws the selected 960×720 region using a single six-vertex quad. It uses
the SDL3 GPU overlay callbacks and releases its pipeline, vertex buffer,
texture, and sampler in the GPU destroy callback. Linux/Vulkan validation used
the US runtime, temporary empty memcards, Xvfb, and an exact 1280×720 window.
Captures verify menu navigation, shell hiding/return, and composition above the
original copyright area. The 960×720 viewport is centered, so the 1280×720
output has pillarboxes. Windows/DXIL and macOS/Metal runtime validation remains
open; embedded shader blobs alone do not establish those paths.

## Presentation shaders

The title texture uses a separate native presentation quad. Position and UV
vertex inputs are FLOAT2 at locations 0 and 1. Position is normalized 0..1 over
the output surface; GLSL maps its top-down Y to NDC without the game's fixed
1024x512 VRAM coordinate conversion. Fragment texture/sampler is set 2 binding 0,
with one sampler and no uniform buffers. Texture alpha is straight RGBA and needs
SRC_ALPHA / ONE_MINUS_SRC_ALPHA color blending.

`platforms/pc/src/shaders/` contains GLSL sources and embedded SPIR-V, MSL, and
DXIL. SPIR-V was built with glslang 15.1 and validated by spirv-val for Vulkan 1.0.
MSL and HLSL were translated with SPIRV-Cross 2021.01.15; DXIL was compiled by
Microsoft DXC 1.9.2609 (shader model 6.0). Shader entrypoints are `main` for
SPIR-V/DXIL and `main0` for MSL. Linux/Vulkan runtime capture is the validation
gate; generated MSL/DXIL compile artifacts do not establish macOS/Windows runtime
compatibility. Those host runtime checks remain open.

## Full-screen title composition

The owner approved the native-resolution buttons but asked to replace the full
screen presentation because the original PS1 logo/font no longer matched them.
The expanded canvas redraws the title, entry prompt, menu, and footer; the
integration must retain only the original animated background from game draws.
DIGIMON remains the dominant title in metallic blue, with WORLD 3 in amber and
a discreet DW3NX badge. Clipped translucent backing, thin angular seams, and
small amber accents share the hardware finish of the menu without concealing the
background. The original three legal credits are preserved as text with new
12px antialiased typography. No debug, version, or implementation labels appear.

`selected=-1` renders PRESS START; `selected=0..2` renders the three choices.
The menu geometry is unchanged. `tools/generate_pc_title_lettering.py` records
reproducible font sizes/labels and regenerates the RLE coverage include with
Pillow and the licensed DejaVu fonts. The runtime needs neither.

Standalone alpha coverage: menu canvas affects 40.7% of pixels, with 16.5%
fully opaque; prompt canvas affects 29.9%, with 7.5% fully opaque. These are
component measurements, not runtime validation. Composition against the moving
original background and suppression of original title/credits remain integration
checks requiring a real US PC virtual-display capture.
