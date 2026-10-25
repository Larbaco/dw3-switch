/* P6.25 cross-region save conversion (US <-> EU), pure functions on byte
 * buffers. No file or game dependency: the host (memcard_host.c) and the unit
 * tests call it. Design and field list: docs/CROSS_REGION_SAVES.md.
 *
 * Input and output are the 32 KiB flat card image the game addresses
 * (header 0x000, icons, info section 0x200, three data slots from 0x300,
 * stride 0x2700). The image layout is the same in both regions; only each
 * data slot differs (version byte 3 US / 4 EU, flag area, checksum).
 */
#ifndef PORT_MEMCARD_SAVE_CONVERT_H
#define PORT_MEMCARD_SAVE_CONVERT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bumped when the conversion output changes. Written in the import marker. */
#define SAVE_CONVERT_VERSION 1

#define SAVE_REGION_NONE 0
#define SAVE_REGION_US 1
#define SAVE_REGION_EU 2

#define SAVE_IMAGE_SIZE 0x8000u
#define SAVE_INFO_OFFSET 0x200u
#define SAVE_INFO_SIZE 0xD4u /* sizeof(MemCardFile) */
#define SAVE_SUMMARY_SIZE 0x44u /* sizeof(MemCardSave), name first */
#define SAVE_DATA_OFFSET 0x300u
#define SAVE_SLOT_STRIDE 0x2700u
#define SAVE_SLOT_COUNT 3

#define SAVE_SIZE_US 0x26BCu /* GAME_SAVE_SIZE */
#define SAVE_SIZE_EU 0x26C4u
#define SAVE_VERSION_US 3
#define SAVE_VERSION_EU 4
#define SAVE_FLAGS_OFFSET 0x2644u /* first flag group, both regions */
#define SAVE_PROGRESS_OFFSET 0x263Cu
#define SAVE_PROGRESS_EXTRA_CHAPTER 0x2D /* EU only (wstag780 endEvent1500) */

/* One event flag group: where it sits in each region's save and how big. */
typedef struct SaveFlagGroup {
    uint8_t group;
    uint16_t us_off, us_size;
    uint16_t eu_off, eu_size;
} SaveFlagGroup;

extern const SaveFlagGroup g_save_flag_groups[];
extern const int g_save_flag_group_count;

/* What happened to one data slot. */
typedef enum SaveSlotResult {
    SAVE_SLOT_EMPTY = 0,     /* info name empty: nothing to carry */
    SAVE_SLOT_NATIVE = 1,    /* already in the target layout: copied as is */
    SAVE_SLOT_CONVERTED = 2, /* converted from the other region */
    SAVE_SLOT_REFUSED = 3,   /* EU extra chapter into US: bytes kept as they
                                are, so the US game shows its own error 8 */
    SAVE_SLOT_INVALID = 4    /* unknown version, bad checksum: kept as is
                                (the game shows error 8), never "repaired" */
} SaveSlotResult;

/* Warning bits per slot (conversion went ahead, but dropped set bits). */
#define SAVE_WARN_DROPPED_FLAGS 0x1u /* nonzero bytes with no place in the target */

typedef struct SaveConvertReport {
    int result[SAVE_SLOT_COUNT];
    unsigned warn[SAVE_SLOT_COUNT];
    int src_region[SAVE_SLOT_COUNT]; /* SAVE_REGION_*, NONE if empty/invalid */
    int usable;                      /* slots that are NATIVE or CONVERTED */
    int converted;                   /* slots that are CONVERTED */
} SaveConvertReport;

/* XOR of every byte (the game's computeChecksum). */
uint8_t save_xor(const uint8_t *p, size_t n);

/* Region of a data slot by version byte and exact checksum, or SAVE_REGION_NONE. */
int save_slot_region(const uint8_t *slot);

/* Convert the whole image. Writes SAVE_IMAGE_SIZE bytes to dst (distinct from
 * src). Returns 0 when the source is a well-formed DW3 card image (info magic
 * and checksum valid) and dst was produced; -1 otherwise, with dst zeroed and
 * nothing to import. A source with no usable slot returns 0 and rep->usable == 0
 * (the caller should not create a file). */
int save_convert_image(const uint8_t *src, uint8_t *dst, int dst_region,
                       SaveConvertReport *rep);

/* Convert one data slot (SAVE_SLOT_STRIDE bytes in, same out) to dst_region.
 * Returns a SaveSlotResult; out is always fully written (a copy of the input
 * for REFUSED and INVALID). warn and src_region may be NULL. */
int save_convert_slot(const uint8_t *in, uint8_t *out, int dst_region,
                      unsigned *warn, int *src_region);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MEMCARD_SAVE_CONVERT_H */
