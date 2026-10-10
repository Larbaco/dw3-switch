/*
 * Psy-Q DecDCTvlc / DecDCTvlc2 / DecDCTvlcBuild for the PC host.
 *
 * DW3 title movies call DecDCTvlcBuild + DecDCTvlc2 (not DecDCTvlc).
 * Decode uses vendor/psyz/nugget/bsdec (MIT) — BS frame → MDEC run-level.
 */
#include <libpress.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsdec.h"

/* Title ring is 64 KiB; a single BS frame fits. Cap avoids unbounded reads. */
#define DW3_BS_MAX_IN 0x10000u

/* Output halfwords after the 1-word MDEC command header (title vlcbuf = 0x28000). */
#define DW3_RL_MAX_HALFWORDS ((0x28000u / 2u) - 2u)

static int s_vlc_ready;
static unsigned s_vlc_frames;

static void dw3_vlc_log_runlevels(const uint16_t *rl, uint32_t need,
                                  struct BsdecResult r) {
    uint32_t i = 0;
    unsigned blocks = 0;
    unsigned ac_blocks = 0;
    unsigned eob_only = 0;
    unsigned ac_coeffs = 0;
    int dc_min = 512;
    int dc_max = -512;

    if (getenv("DW3_VLC_LOG") == NULL)
        return;
    if (s_vlc_frames > 5u && s_vlc_frames != 10u && s_vlc_frames != 30u &&
        s_vlc_frames != 100u && (s_vlc_frames % 300u) != 0u)
        return;

    while (i + 1u < need && blocks < 20000u) {
        uint16_t dcw = rl[i++];
        int dc = (int)(dcw & 0x3ffu);

        if (dc & 0x200)
            dc |= ~0x3ff;
        if (dc < dc_min)
            dc_min = dc;
        if (dc > dc_max)
            dc_max = dc;
        {
            unsigned ac = 0;

            while (i < need) {
                uint16_t v = rl[i++];
                if (v == 0xfe00u)
                    break;
                ac++;
                ac_coeffs++;
            }
            if (ac)
                ac_blocks++;
            else
                eob_only++;
        }
        blocks++;
    }

    fprintf(stderr,
            "dw3-vlc: frame#%u cmd=0x%08x err=%u blocks=%u ac_blocks=%u "
            "eob_only=%u ac_coeffs=%u dc=[%d,%d]\n",
            s_vlc_frames, (unsigned)r.mdecCommand, (unsigned)r.error,
            (unsigned)r.blocks, ac_blocks, eob_only, ac_coeffs, dc_min,
            dc_max);
}

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
    if (!bsdecUsable(r.error)) {
        fprintf(stderr, "dw3-vlc: bsdec unusable err=%u\n", (unsigned)r.error);
        return -1;
    }

    buf[0] = (u_long)r.mdecCommand;
    s_vlc_frames++;
    dw3_vlc_log_runlevels((const uint16_t *)(buf + 1), need, r);
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
