/*
 * Psy-Q DecDCTvlc / DecDCTvlc2 / DecDCTvlcBuild for the PC host.
 *
 * DW3 title movies call DecDCTvlcBuild + DecDCTvlc2 (not DecDCTvlc).
 * Decode uses vendor/psyz/nugget/bsdec (MIT) — BS frame → MDEC run-level.
 */
#include <libpress.h>
#include <stdint.h>
#include <string.h>

#include "bsdec.h"

/* Title ring is 64 KiB; a single BS frame fits. Cap avoids unbounded reads. */
#define DW3_BS_MAX_IN 0x10000u

/* Output halfwords after the 1-word MDEC command header (title vlcbuf = 0x28000). */
#define DW3_RL_MAX_HALFWORDS ((0x28000u / 2u) - 2u)

static int s_vlc_ready;

static int dw3_bs_to_runlevel(u_long *bs, u_long *buf) {
    uint32_t need;
    uint32_t in_bytes;
    struct BsdecResult r;

    if (bs == NULL || buf == NULL)
        return -1;

    need = bsdecRlHalfwords(bs, 8);
    if (need == 0 || need > DW3_RL_MAX_HALFWORDS)
        return -1;

    /* Exact compressed length is not in the BS header; bsdec tolerates
       oversize input (OVERLONG still usable). Cap at the title ring size. */
    in_bytes = DW3_BS_MAX_IN;

    r = bsdecFrame(bs, in_bytes, (uint16_t *)(buf + 1), need);
    if (!bsdecUsable(r.error))
        return -1;

    buf[0] = (u_long)r.mdecCommand;
    return 0;
}

void DecDCTvlcBuild(u_short *table) {
    /* bsdec has its own codebook; mark the game-allocated table ready. */
    s_vlc_ready = 1;
    if (table != NULL)
        table[0] = 1;
}

int DecDCTvlc2(u_long *bs, u_long *buf, DECDCTTAB table) {
    (void)table;
    /* Psy-Q: DecDCTvlc2(0, 0, table) restarts a suspended decode. We finish
       in one call, so a restart is a no-op success. */
    if (bs == NULL || buf == NULL)
        return 0;
    if (!s_vlc_ready)
        return -1;
    return dw3_bs_to_runlevel(bs, buf);
}

int DecDCTvlc(u_long *bs, u_long *buf) {
    if (bs == NULL || buf == NULL)
        return 0;
    return dw3_bs_to_runlevel(bs, buf);
}
