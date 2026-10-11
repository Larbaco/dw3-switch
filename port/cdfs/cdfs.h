/* Host CD → filesystem: file-id reads over a packed runtime tree.
 *
 * Wave 1: synchronous completion.
 * P3.7: LBA lookup for file-backed STR streaming.
 */
#ifndef PORT_CDFS_H
#define PORT_CDFS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Set the packed runtime root (e.g. "runtime/us"). Copies the path. */
int cdfs_set_root(const char *root);

/* Load a files_<ver>.json map. If json_path is NULL, tries
 * <root>/files_us.json then <root>/files_eu.json. */
int cdfs_load_map(const char *json_path);

/* Host file size for id, or (uint32_t)-1 if unknown / unreadable. */
uint32_t cdfs_file_size(uint32_t id);

/* Read the full host file for id into buf.
 * Returns bytes written (== file size) on success.
 * Returns -1 if id is unknown, the file is missing, or buflen < size. */
int cdfs_read(uint32_t id, void *buf, size_t buflen);

/* Read `len` bytes at byte `offset` into buf (partial / sector window).
 * Short reads at EOF zero-fill the remainder of buf.
 * Returns bytes copied from the file (0..len), or -1 on error.
 * Not paced: file-cache and map-header reads use this. */
int cdfs_read_at(uint32_t id, void *buf, size_t offset, size_t len);

/* Field-map tile (a sector window). The first read of a file id loads that
 * whole file into one resident buffer. A different id replaces it. Later
 * tiles copy from the buffer. Idle on return: a second tile in the same
 * frame succeeds. cdfs_shutdown frees the buffer. */
int cdfs_read_field_tile(uint32_t id, void *buf, size_t offset, size_t len);

/* Resolve absolute disc LBA to the map entry that covers it.
 * On success fills optional outs and returns 0; -1 if no entry. */
int cdfs_lookup_lba(uint32_t lba, uint32_t *id_out, char *relpath_out,
                    size_t relpath_len, uint32_t *base_lba_out,
                    uint32_t *sectors_out, uint32_t *form_out);

/* Absolute path under the runtime root for a relative map path. */
int cdfs_join_root(const char *rel, char *out, size_t outlen);

/* Runtime root set by cdfs_set_root, or "" if unset. */
const char *cdfs_root(void);

typedef struct CdfsMapInfo {
    uint32_t id;
    const char *path; /* valid only for the duration of the visit callback */
    uint32_t lba;
    uint32_t sectors;
    uint32_t form;
    uint32_t bytes; /* JSON byte length; 0 if the map omitted it */
} CdfsMapInfo;

/* Call fn for each map entry. fn returns non-zero to stop. Returns 0, or the
 * fn's non-zero value. */
int cdfs_visit(int (*fn)(const CdfsMapInfo *info, void *user), void *user);

/* No-op. Older callers (the one-tile pace) may still call this. */
void cdfs_poll(void);

/* Free map state (optional; next cdfs_load_map replaces the table). */
void cdfs_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CDFS_H */
