/* Version-matched runtime tree from the owner's Mode2/2352 disc image.
 *
 * US and EU builds use their linked FILE_SECTORS tables, executable identity,
 * map filename, and PS-X EXE boot-TIM address. EU only accepts SLES_039.36.
 *
 * A complete versioned map, executable, boot_tim.bin, and mapped files are
 * only checked. Otherwise a matching BIN/CUE is extracted to the runtime
 * root, with the map atomically written last as its completion marker. An EU
 * image with the US executable is rejected before extraction. EU boot fails
 * if its root exists but is incomplete and no matching image is available.
 * Missing roots with no image retain the existing CDFS boot failure.
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
