/* US runtime tree from the owner's Mode2/2352 disc image.
 *
 * Call this only on the US build. EU stays on a pre-packed runtime/eu tree.
 *
 * When files_us.json, SLUS_014.36, boot_tim.bin, and every mapped file are
 * already present, this only stats those paths. When they are not, and
 * dw3_str_find_disc() can see a BIN, this creates the runtime root and
 * extracts it. A missing disc is not an error: the host's existing
 * cdfs_load_map failure remains the boot error if the tree is still absent.
 * Returns -1 when a disc image was found and the extract failed.
 *
 * Creates <root>/memcard/ once the root directory exists. Does not create
 * or replace memcard0.mcr / memcard1.mcr.
 *
 * Byte progress uses dw3_str_restore_emit(), so the Switch bar registered
 * with dw3_str_restore_set_progress() moves during the copy.
 */
#ifndef PORT_RUNTIME_EXTRACT_H
#define PORT_RUNTIME_EXTRACT_H

int dw3_runtime_ensure(const char *root);

/* stderr line when msg != NULL, then the restore/extract progress callback.
 * msg == NULL is a byte tick and is not logged. */
void dw3_str_restore_emit(const char *msg, unsigned long long done,
                          unsigned long long total);

#endif /* PORT_RUNTIME_EXTRACT_H */
