/* Host CD → filesystem: file-id reads over a packed runtime tree.
 *
 * Wave 1: synchronous completion. STR / libcd streaming is out of scope.
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

/* Wave 1: no-op. Reserved for async completion / cdReadyCallback poll. */
void cdfs_poll(void);

/* Free map state (optional; next cdfs_load_map replaces the table). */
void cdfs_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CDFS_H */
