/* P6.25 slice 2: the `.dw3s` canonical card container and the pure functions
 * around it (no file access; memcard_host.c does the I/O). Design:
 * docs/CROSS_REGION_SAVES.md.
 *
 * A `.dw3s` file holds one 32 KiB card image in the CANONICAL layout (the EU
 * one, version byte 4) behind a 0x40 byte header with a CRC32 of the payload
 * and per-slot origin metadata that the game's full-slot write cannot reach.
 * The running build never sees it: save_store_to_native() builds the image in
 * the build's own region layout when the card is opened, and
 * save_store_from_native() folds the slots the game wrote back into the
 * canonical card when it is flushed.
 */
#ifndef PORT_MEMCARD_SAVE_STORE_H
#define PORT_MEMCARD_SAVE_STORE_H

#include "save_convert.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SAVE_STORE_FORMAT 1
#define SAVE_STORE_HEADER_SIZE 0x40u
#define SAVE_STORE_FILE_SIZE (SAVE_STORE_HEADER_SIZE + SAVE_IMAGE_SIZE)
#define SAVE_STORE_CARDS 2
#define SAVE_STORE_LANG_NONE 0xFFu

/* Per-slot metadata flags. */
#define SAVE_META_NEEDS_EXTRA 0x01u /* the save is in the EU extra chapter */
#define SAVE_META_US_TAIL 0x02u     /* us_tail holds the US flags40 bytes 0x1A-0x1B */

typedef struct SaveStoreSlotMeta {
    uint8_t origin_region; /* SAVE_REGION_*, 0 = unknown */
    uint8_t origin_lang;   /* EU LANGUAGE, SAVE_STORE_LANG_NONE when not known */
    uint8_t flags;
    uint8_t us_tail[2];
} SaveStoreSlotMeta;

typedef struct SaveStoreCard {
    uint8_t image[SAVE_IMAGE_SIZE]; /* canonical payload */
    uint8_t last_region;
    uint8_t last_lang;
    SaveStoreSlotMeta meta[SAVE_SLOT_COUNT];
} SaveStoreCard;

typedef enum SaveStoreStatus {
    SAVE_STORE_OK = 0,
    SAVE_STORE_SHORT = 1,   /* shorter than header + payload */
    SAVE_STORE_MAGIC = 2,   /* not a "DW3S" file */
    SAVE_STORE_HEADER = 3,  /* format 0, header or payload size not understood */
    SAVE_STORE_NEWER = 4,   /* a format version this build does not know */
    SAVE_STORE_CRC = 5,     /* payload CRC32 differs */
    SAVE_STORE_LAYOUT = 6   /* payload is not a DW3 card (info magic/checksum) */
} SaveStoreStatus;

const char *save_store_status_name(int status);

uint32_t save_crc32(const uint8_t *p, size_t n);

/* Parse a whole file. On SAVE_STORE_OK `out` is filled; otherwise it is
 * zeroed and the status says why (the file must then be left alone). */
int save_store_parse(const uint8_t *file, size_t len, SaveStoreCard *out);

/* Write the file image (SAVE_STORE_FILE_SIZE bytes) of a card. */
void save_store_serialize(const SaveStoreCard *c, uint8_t *out);

/* Canonical -> the image of `region` (SAVE_IMAGE_SIZE bytes into native).
 * rep (may be NULL) is the save_convert report: a SAVE_SLOT_REFUSED slot is the
 * EU extra chapter shown to the US build with its bytes kept, so the US game
 * shows its own error 8. Returns -1 when the card's info section is not valid. */
int save_store_to_native(const SaveStoreCard *c, int region, uint8_t *native,
                         SaveConvertReport *rep);

typedef struct SaveStoreWriteReport {
    unsigned changed;            /* bit s: slot s was rewritten in the card */
    int header_changed;          /* sectors 0..0x1FF or the info section changed */
    int slot_result[SAVE_SLOT_COUNT];
    unsigned warn[SAVE_SLOT_COUNT]; /* SAVE_WARN_* of the conversion */
    char reason[96];             /* why a -1 return refused, for the log */
} SaveStoreWriteReport;

/* Fold the image the game of `region` holds (`native`) into the card: only the
 * slots whose bytes differ from `base` (the image shown at open; NULL = every
 * slot) are converted, the others stay as they are. The header, icon and info
 * sectors are taken from the game when they changed.
 * Returns 0 on success (a no-op success has rep->changed == 0 and
 * rep->header_changed == 0), 1 when the image is not settled and must be
 * tried again later (the card is untouched, rep->reason says why), -1 for a
 * bad argument. "Not settled" is the state between the bursts of one game save
 * (the game writes the header, then the info section, then the slot): an info
 * section that is not a DW3 one, a named slot whose data is not a valid save of
 * `region`, or (unless `final`) an info section that changed while no slot data
 * did. With `final` (the process is ending) the last of these is accepted. */
int save_store_from_native(SaveStoreCard *c, int region, const uint8_t *native,
                           const uint8_t *base, int lang, int final,
                           SaveStoreWriteReport *rep);

/* ---- first-fit import of legacy flat cards (migration) ---- */

#define SAVE_IMPORT_MAX_SOURCES 4
#define SAVE_IMPORT_CAPACITY (SAVE_STORE_CARDS * SAVE_SLOT_COUNT)

typedef enum SaveImportOutcome {
    SAVE_IMPORT_EMPTY = 0,    /* no save in that slot */
    SAVE_IMPORT_IMPORTED = 1, /* placed in the store (target set) */
    SAVE_IMPORT_DUPLICATE = 2,/* same bytes as a slot already imported */
    SAVE_IMPORT_INVALID = 3,  /* wrong version or checksum: not imported */
    SAVE_IMPORT_LEFTOVER = 4, /* all six slots taken: stays in the legacy folder */
    SAVE_IMPORT_SOURCE_BAD = 5/* the source is not a DW3 card (info magic/checksum) */
} SaveImportOutcome;

typedef struct SaveImportSource {
    const uint8_t *image; /* SAVE_IMAGE_SIZE bytes, or NULL (no file) */
} SaveImportSource;

typedef struct SaveImportReport {
    int outcome[SAVE_IMPORT_MAX_SOURCES][SAVE_SLOT_COUNT];
    int target[SAVE_IMPORT_MAX_SOURCES][SAVE_SLOT_COUNT]; /* 0..5, -1 */
    int src_region[SAVE_IMPORT_MAX_SOURCES][SAVE_SLOT_COUNT];
    int imported;
    int duplicates;
    int invalid;
    int leftover;
    int card_used[SAVE_STORE_CARDS]; /* 1 when the card got at least one slot */
} SaveImportReport;

/* Sources in the order given (design: US card A, EU card A, US card B, EU card
 * B). Every named slot whose version and exact checksum are valid is converted
 * to canonical and placed first-fit into card 0 slots 0..2, then card 1 slots
 * 0..2. The sources are only read. Returns the number imported. */
int save_store_import(SaveStoreCard cards[SAVE_STORE_CARDS],
                      const SaveImportSource *src, int nsrc,
                      SaveImportReport *rep);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MEMCARD_SAVE_STORE_H */
