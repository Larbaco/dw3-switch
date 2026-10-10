/* Host memory-card images: 128 KiB raw PS1 .mcr per slot.
 *
 * Sector contract matches PSY-Z libcard `_card_read` / `_card_write`
 * (128-byte frames). PSY-Z currently uses bu00/ directories and stubs raw
 * sectors; Wave 4 can point those stubs at this backend.
 */
#ifndef PORT_MEMCARD_HOST_H
#define PORT_MEMCARD_HOST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEMCARD_HOST_SIZE 131072u /* 128 KiB */
#define MEMCARD_HOST_SECTOR_SIZE 128u
#define MEMCARD_HOST_SECTORS (MEMCARD_HOST_SIZE / MEMCARD_HOST_SECTOR_SIZE)
#define MEMCARD_HOST_SLOTS 2

/* Directory that holds memcard0.mcr / memcard1.mcr. Copies the path. */
int memcard_host_set_dir(const char *dir);

/* Default filenames for slot 0 / 1. */
const char *memcard_host_slot_name(int slot);

/* Create a freshly formatted empty card image in buf (exactly 128 KiB). */
void memcard_host_format_image(uint8_t *buf);

/* Ensure slot image exists (create formatted empty if missing). */
int memcard_host_ensure(int slot);

/* Byte-range I/O within the 128 KiB image. Returns bytes transferred, or -1. */
int memcard_host_read(int slot, uint32_t offset, void *buf, size_t len);
int memcard_host_write(int slot, uint32_t offset, const void *buf, size_t len);

/* One 128-byte frame (sector). Returns 0 on success, -1 on error.
 * Mirrors PSY-Z `_card_read` / `_card_write` block indexing. */
int memcard_host_read_sector(int slot, int sector, void *buf);
int memcard_host_write_sector(int slot, int sector, const void *buf);

void memcard_host_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MEMCARD_HOST_H */
