/*
 * Host stand-ins for PS1 overlay load addresses (US layout sizes).
 * Full restore/.bss semantics land in E5 / P2.6; E3 only needs linkable
 * OVERLAY_ADDRESS / SUB_OVERLAY_ADDRESS (+ VRAM symbol arrays).
 *
 * US: OVERLAY_VRAM 0x80082448, STAGE_VRAM 0x800A4CA4, HEAP_BASE 0x800AA800
 * → mode region 0x2285C, stage region 0x5B5C. Buffers are padded for later
 * static-overlay work.
 */

#ifndef PORT_HOST
#error "port/overlays/host_vram.c is host-only"
#endif

enum {
    DW3_HOST_OVERLAY_BYTES = 0x40000, /* 256 KiB */
    /* Boot TIM is ~300 KiB (320x480 16-bit); keep headroom for sub-overlays. */
    DW3_HOST_STAGE_BYTES = 0x80000 /* 512 KiB */
};

/* Match decomp overlay.h: extern u8 OVERLAY_VRAM[] / STAGE_VRAM[] */
unsigned char OVERLAY_VRAM[DW3_HOST_OVERLAY_BYTES];
unsigned char STAGE_VRAM[DW3_HOST_STAGE_BYTES];

void *const OVERLAY_ADDRESS = OVERLAY_VRAM;
void *const SUB_OVERLAY_ADDRESS = STAGE_VRAM;
