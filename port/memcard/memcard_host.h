/* Host memory-card saves: one flat file per port, resident in RAM.
 *
 * On-disk names are save0.bin / save1.bin (32 KiB = 4 PS1 blocks). That is
 * the byte range MemCardReadFile / MemCardWriteFile already address for the
 * Digimon World 3 save. The running port does not keep a live 128 KiB card
 * image or directory chain. Legacy memcardN.mcr is migrated once, then left
 * as memcardN.mcr.bak.
 *
 * Sector helpers remain for building/reading legacy 128 KiB fixtures
 * (migration tests). Runtime STGMCARD traffic uses the flat file API.
 */
#ifndef PORT_MEMCARD_HOST_H
#define PORT_MEMCARD_HOST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMCARD_HOST_SIZE 131072u /* legacy 128 KiB .mcr (migration only) */
#define MEMCARD_HOST_SECTOR_SIZE 128u
#define MEMCARD_HOST_SECTORS (MEMCARD_HOST_SIZE / MEMCARD_HOST_SECTOR_SIZE)
#define MEMCARD_HOST_SLOTS 2
#define MEMCARD_FLAT_SIZE 0x8000u /* 4 blocks × 0x2000 */
#define MEMCARD_FLAT_BLOCKS 4

/* Directory that holds save0.bin / save1.bin (and optional legacy .mcr). */
int memcard_host_set_dir(const char *dir);

/* P6.25 cross-region saves (slice 1). When a port has no save of its own,
 * memcard_host_ensure converts the other region's saveN.bin from `foreign_dir`
 * in memory and writes the result as this port's save. The source is only
 * read; `saveN.import` next to the new save records the import (and keeps it
 * from running again). own_region is SAVE_REGION_US or SAVE_REGION_EU
 * (save_convert.h). Call after memcard_host_set_dir; never set = no import.
 * The sibling form takes the runtime root (".../us" or ".../eu") and looks in
 * "<parent>/<other>/memcard"; it fails (no import) for any other root name. */
int memcard_host_set_cross_import(const char *foreign_dir, int own_region);
int memcard_host_set_cross_import_sibling(const char *runtime_root,
                                          int own_region);

/* Host wiring in one call, after memcard_host_set_dir and before the first
 * memcard_host_ensure. Order: DW3_CROSS_IMPORT=0 turns it off;
 * DW3_MEMCARD_IMPORT_DIR names the source folder (tests, any layout);
 * otherwise the sibling folder of the runtime root, but only when the card
 * folder is the default one (memdir_is_explicit == 0), so a replay run that
 * sets DW3_MEMCARD_DIR never imports. Returns 0 if an import source is set. */
int memcard_host_setup_cross_import(const char *runtime_root, int own_region,
                                    int memdir_is_explicit);

/* Flat save filenames for slot 0 / 1. */
const char *memcard_host_slot_name(int slot);

/* Legacy 128 KiB image names (migration source). */
const char *memcard_host_legacy_slot_name(int slot);

/* Create a freshly formatted empty legacy card image in buf (exactly 128 KiB). */
void memcard_host_format_image(uint8_t *buf);

/* Prepare slot: migrate legacy .mcr if needed, load flat save into RAM.
 * An empty port (no save yet) still succeeds — the "card" is always present. */
int memcard_host_ensure(int slot);

/* True when the port has a resident game save (flat file loaded or created). */
int memcard_host_has_file(int slot);

/* Stored save product name (e.g. BASLUS-01436DMW3-USA), or "". */
const char *memcard_host_file_name(int slot);

/* Create an empty flat save in RAM (and on disk after flush). */
int memcard_host_create_file(int slot, const char *name);

/* Wipe the flat save (format). */
int memcard_host_format_slot(int slot);

/* Byte-range I/O within the resident flat save. Returns bytes, or -1. */
int memcard_host_file_read(int slot, uint32_t offset, void *buf, size_t len);
int memcard_host_file_write(int slot, uint32_t offset, const void *buf,
                            size_t len);

/* Flush dirty RAM to saveN.bin (rename-atomic). No-op if clean. */
int memcard_host_flush(int slot);
int memcard_host_flush_all(void);

/* Flush dirty slots, then clear host state. */
void memcard_host_shutdown(void);

/* Test / diagnostics: successful flush calls since last reset. */
unsigned memcard_host_flush_count(int slot);
void memcard_host_reset_flush_count(int slot);

/* Non-zero if the last flush attempt for this slot failed (still dirty). */
int memcard_host_flush_failed(int slot);

/* Legacy 128 KiB sector helpers (fixtures / migration tooling). */
int memcard_host_legacy_ensure(int slot);
int memcard_host_read(int slot, uint32_t offset, void *buf, size_t len);
int memcard_host_write(int slot, uint32_t offset, const void *buf, size_t len);
int memcard_host_read_sector(int slot, int sector, void *buf);
int memcard_host_write_sector(int slot, int sector, const void *buf);

#ifdef DW3_CHEATS
/* The directory given to memcard_host_set_dir, or NULL (P6.20 save marks). */
const char *memcard_host_dir(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* PORT_MEMCARD_HOST_H */
