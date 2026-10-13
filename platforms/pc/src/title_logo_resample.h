#ifndef DW3_PC_TITLE_LOGO_RESAMPLE_H
#define DW3_PC_TITLE_LOGO_RESAMPLE_H
#include <stdint.h>
/* CPU resampling of the owner's original RGBA artwork; no assets are embedded.
 * filter selects Mitchell bicubic or sharper Lanczos2 (recommended).
 * Returns 0 on success, -1 on invalid dimensions.
 * Input/output are straight-alpha RGBA8888. Invoke only when logo pixels change. */
enum { DW3_PC_LOGO_MITCHELL = 0, DW3_PC_LOGO_LANCZOS2 = 1, DW3_PC_LOGO_LANCZOS2_FINE = 2 };
int dw3_pc_title_logo_resample(const uint8_t *src, int sw, int sh, int spitch,
    uint8_t *dst, int dw, int dh, int dpitch, int filter);
#endif
