/* File-backed STR stream bridge (P3.7 Wave 3–4).
 *
 * Resolves movie LBAs from FILE_TABLE.getPos / CdlLOC against the packed
 * runtime map (files_us.json → AAA/STR/*.STR). Host packs store 2048-byte
 * user payloads per sector (form: 2); the bridge synthesizes Mode2/Form2
 * 2352-byte sectors for PSY-Z libcd (CdReady / StCdInterrupt / XA pull).
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

#ifdef __cplusplus
}
#endif

#endif /* PORT_STR_STREAM_H */
