#include "save_convert.h"

#include <string.h>

const SaveFlagGroup g_save_flag_groups[] = {
    /* group  US off  size   EU off  size */
    {0x02, 0x2644, 0x0D, 0x2644, 0x12},
    {0x04, 0x2651, 0x02, 0x2656, 0x02},
    {0x06, 0x2653, 0x01, 0x2658, 0x01},
    {0x08, 0x2654, 0x01, 0x2659, 0x01},
    {0x0A, 0x2655, 0x02, 0x265A, 0x04},
    {0x0C, 0x2657, 0x08, 0x265E, 0x08},
    {0x0E, 0x265F, 0x0C, 0x2666, 0x0C},
    {0x10, 0x266B, 0x02, 0x2672, 0x04},
    {0x18, 0x266D, 0x01, 0x2676, 0x02},
    {0x1A, 0x266E, 0x09, 0x2678, 0x09},
    {0x1C, 0x2677, 0x0B, 0x2681, 0x0B},
    {0x20, 0x2682, 0x1E, 0x268C, 0x1E},
    {0x40, 0x26A0, 0x1C, 0x26AA, 0x1A},
};
const int g_save_flag_group_count =
    (int)(sizeof(g_save_flag_groups) / sizeof(g_save_flag_groups[0]));

uint8_t save_xor(const uint8_t *p, size_t n) {
    uint8_t x = 0;
    size_t i;

    for (i = 0; i < n; i++) {
        x ^= p[i];
    }
    return x;
}

static size_t region_size(int region) {
    return region == SAVE_REGION_US ? SAVE_SIZE_US : SAVE_SIZE_EU;
}

static uint8_t region_version(int region) {
    return region == SAVE_REGION_US ? SAVE_VERSION_US : SAVE_VERSION_EU;
}

int save_slot_region(const uint8_t *slot) {
    int region;

    if (slot[2] == SAVE_VERSION_US) {
        region = SAVE_REGION_US;
    } else if (slot[2] == SAVE_VERSION_EU) {
        region = SAVE_REGION_EU;
    } else {
        return SAVE_REGION_NONE;
    }
    if (save_xor(slot + 4, region_size(region) - 4) != slot[0]) {
        return SAVE_REGION_NONE;
    }
    return region;
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

int save_convert_slot(const uint8_t *in, uint8_t *out, int dst_region,
                      unsigned *warn, int *src_region_out) {
    int src;
    int i;
    unsigned w = 0;

    if (warn != NULL) {
        *warn = 0;
    }
    if (src_region_out != NULL) {
        *src_region_out = SAVE_REGION_NONE;
    }
    if (dst_region != SAVE_REGION_US && dst_region != SAVE_REGION_EU) {
        memcpy(out, in, SAVE_SLOT_STRIDE);
        return SAVE_SLOT_INVALID;
    }
    src = save_slot_region(in);
    if (src == SAVE_REGION_NONE) {
        /* Unknown version or wrong checksum: leave it as it is. */
        memcpy(out, in, SAVE_SLOT_STRIDE);
        return SAVE_SLOT_INVALID;
    }
    if (src_region_out != NULL) {
        *src_region_out = src;
    }
    if (src == dst_region) {
        memcpy(out, in, SAVE_SLOT_STRIDE);
        return SAVE_SLOT_NATIVE;
    }
    if (dst_region == SAVE_REGION_US &&
        (int32_t)rd32(in + SAVE_PROGRESS_OFFSET) >= SAVE_PROGRESS_EXTRA_CHAPTER) {
        /* The extra chapter has no stage table in the US game. */
        memcpy(out, in, SAVE_SLOT_STRIDE);
        return SAVE_SLOT_REFUSED;
    }

    memset(out, 0, SAVE_SLOT_STRIDE);
    memcpy(out, in, SAVE_FLAGS_OFFSET); /* identical block 0x0000-0x2643 */
    for (i = 0; i < g_save_flag_group_count; i++) {
        const SaveFlagGroup *g = &g_save_flag_groups[i];
        uint16_t from_off = src == SAVE_REGION_US ? g->us_off : g->eu_off;
        uint16_t from_size = src == SAVE_REGION_US ? g->us_size : g->eu_size;
        uint16_t to_off = dst_region == SAVE_REGION_US ? g->us_off : g->eu_off;
        uint16_t to_size = dst_region == SAVE_REGION_US ? g->us_size : g->eu_size;
        uint16_t n = from_size < to_size ? from_size : to_size;
        uint16_t k;

        memcpy(out + to_off, in + from_off, n);
        for (k = n; k < from_size; k++) {
            if (in[from_off + k] != 0) {
                w |= SAVE_WARN_DROPPED_FLAGS;
            }
        }
    }
    out[2] = region_version(dst_region);
    out[0] = save_xor(out + 4, region_size(dst_region) - 4);
    if (warn != NULL) {
        *warn = w;
    }
    return SAVE_SLOT_CONVERTED;
}

int save_convert_image(const uint8_t *src, uint8_t *dst, int dst_region,
                       SaveConvertReport *rep) {
    SaveConvertReport local;
    const uint8_t *info;
    uint8_t *dinfo;
    int s;

    if (rep == NULL) {
        rep = &local;
    }
    memset(rep, 0, sizeof(*rep));
    if (src == NULL || dst == NULL || src == dst ||
        (dst_region != SAVE_REGION_US && dst_region != SAVE_REGION_EU)) {
        return -1;
    }
    memset(dst, 0, SAVE_IMAGE_SIZE);

    /* The info section must be what the game writes: magic "DMW3" and its
     * XOR checksum (bytes 4 .. sizeof(MemCardFile)). Otherwise the card is
     * not a DW3 save and there is nothing to carry. */
    info = src + SAVE_INFO_OFFSET;
    if (rd32(info + 4) != 0x33574D44u ||
        save_xor(info + 4, SAVE_INFO_SIZE - 4) != info[0]) {
        return -1;
    }

    /* Header and icon sectors (0x000-0x1FF) are region independent: verbatim. */
    memcpy(dst, src, SAVE_INFO_OFFSET);
    dinfo = dst + SAVE_INFO_OFFSET;
    /* The whole info sector run (0x100), the bytes after the structure too. */
    memcpy(dinfo, info, SAVE_DATA_OFFSET - SAVE_INFO_OFFSET);

    for (s = 0; s < SAVE_SLOT_COUNT; s++) {
        const uint8_t *in = src + SAVE_DATA_OFFSET + (size_t)s * SAVE_SLOT_STRIDE;
        uint8_t *out = dst + SAVE_DATA_OFFSET + (size_t)s * SAVE_SLOT_STRIDE;
        /* MemCardSave.name is the first field of each summary */
        int has_name = info[8 + s * SAVE_SUMMARY_SIZE] != 0;

        if (!has_name) {
            rep->result[s] = SAVE_SLOT_EMPTY;
            memset(dinfo + 8 + s * SAVE_SUMMARY_SIZE, 0, SAVE_SUMMARY_SIZE);
            continue;
        }
        rep->result[s] = save_convert_slot(in, out, dst_region, &rep->warn[s],
                                           &rep->src_region[s]);
        if (rep->result[s] == SAVE_SLOT_NATIVE ||
            rep->result[s] == SAVE_SLOT_CONVERTED) {
            rep->usable++;
        }
        if (rep->result[s] == SAVE_SLOT_CONVERTED) {
            rep->converted++;
        }
    }
    dinfo[2] = region_version(dst_region);
    dinfo[0] = save_xor(dinfo + 4, SAVE_INFO_SIZE - 4);
    return 0;
}
