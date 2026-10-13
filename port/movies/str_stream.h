/* File-backed STR stream bridge (P3.7 Wave 3–4).
 *
 * Resolves movie LBAs from FILE_TABLE.getPos / CdlLOC against the packed
 * runtime map (files_us.json → AAA/STR/*.STR). A restored STR is raw
 * Mode2/2352, one sector per map sector. An older pack is 2048-byte user
 * payloads; the bridge synthesizes Mode2/Form2 around those and the last
 * two XA ADPCM blocks are missing unless DW3_DISC_BIN is set.
 *
 * Video vs XA: user magic 0x0160 → Data|Form2|RT subheader; otherwise
 * Audio|Form2|RT + stereo CI so CdlModeRT XA decode accepts the sector.
 *
 * Optional: DW3_DISC_BIN=/path/to/game.bin (Mode2/2352) supplies full
 * Form2 user data (packed STR truncates XA at 2048 bytes).
 *
 * Path resolution:
 *   CdlLOC / absolute LBA → cdfs_lookup_lba → <runtime>/<path>
 *   e.g. US opening file id 0x7E5 → LBA 239842 → AAA/STR/MOVIEOPN.STR
 */
#ifndef PORT_STR_STREAM_H
#define PORT_STR_STREAM_H

#include <libcd.h>
#include <psyz/cd.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Open the runtime STR whose extent covers loc's absolute LBA. */
int dw3_str_stream_open_at_loc(const CdlLOC *loc);

/* Open by absolute disc LBA (same resolution as open_at_loc). */
int dw3_str_stream_open_at_lba(int lba);

/* 1 if a stream file is open. */
int dw3_str_stream_active(void);

/* Synthesize one Mode2/Form2 2352-byte sector for absolute LBA into out.
 * Uses the open stream when LBA is in-range; otherwise tries lookup+open.
 * Returns 0 on success, -1 on failure. */
int dw3_str_stream_read_sector(int lba, uint8_t out[2352]);

void dw3_str_stream_close(void);

/* Psyz_CdSetReadCB hook: fill read->buffer with a 2352-byte sector. */
int dw3_str_cd_read_cb(struct PsyzCdRead *read);

/* Register the CD read CB (call after cdfs_load_map). */
void dw3_str_stream_install_cd_bridge(void);

/* Bridge contention counters, cumulative since boot. `reads` counts every
 * dw3_str_cd_read_cb entry; `overlaps` counts the entries that found another
 * bridge read already in flight, counted before taking the audio lock. This
 * measures contention between video (main) and XA (audio) readers; a non-zero
 * count does not indicate corruption after serialization by the lock. Either
 * pointer may be NULL. Both counters are plain statistics, not control flow. */
void dw3_str_stream_get_debug(unsigned long *reads, unsigned long *overlaps);

/* If a Mode2/2352 BIN is next to the runtime (or named by DW3_DISC_BIN /
 * DW3_DISC_CUE) and a form-2 STR is still the 2048-byte extract, rewrite
 * that file in place as raw sectors. boot_tim.bin is not a disc image.
 * A CUE whose FILE name is missing still selects the only other .bin in
 * that directory when the file covers the STR extents. Already-restored
 * files are left alone.
 * Returns 0 when there is nothing to do or the rewrite finished. A missing
 * disc image is not an error. Returns -1 if a rewrite was needed and failed. */
int dw3_str_restore_from_disc(void);

/* Optional callback during a rewrite or a runtime extract. The Switch host
 * paints from this. msg is NULL on a byte tick, which is not a new log line.
 * done is bytes written this run. total is the bytes still to copy. done is 0
 * before the first byte. A NULL callback leaves the stderr log as the only
 * output. The runtime extract reports through dw3_str_restore_emit(). */
void dw3_str_restore_set_progress(void (*fn)(const char *msg,
                                             unsigned long long done,
                                             unsigned long long total));

#ifdef __cplusplus
}
#endif

#endif /* PORT_STR_STREAM_H */
