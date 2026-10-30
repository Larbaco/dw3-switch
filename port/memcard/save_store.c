#include "save_store.h"

#include <stdio.h>
#include <string.h>

#define SAVE_US_TAIL_OFFSET 0x26BAu /* US flags40 bytes 0x1A-0x1B (group 0x40 at 0x26A0) */
#define SAVE_STORE_META_OFFSET 0x14u
#define SAVE_STORE_META_SIZE 5u
#define SAVE_STORE_MIN_HEADER (SAVE_STORE_META_OFFSET + SAVE_SLOT_COUNT * SAVE_STORE_META_SIZE)
#define SAVE_MAGIC_DMW3 0x33574D44u
#define SAVE_EU_VERSION_BYTE SAVE_VERSION_EU

const char *save_store_status_name(int status) {
    switch (status) {
    case SAVE_STORE_OK: return "ok";
    case SAVE_STORE_SHORT: return "truncated";
    case SAVE_STORE_MAGIC: return "not a DW3S file";
    case SAVE_STORE_HEADER: return "header or payload size not understood";
    case SAVE_STORE_NEWER: return "newer format version";
    case SAVE_STORE_CRC: return "payload CRC32 mismatch";
    case SAVE_STORE_LAYOUT: return "payload is not a DW3 card image";
    default: return "unknown";
    }
}

uint32_t save_crc32(const uint8_t *p, size_t n) {
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;
    int k;

    for (i = 0; i < n; i++) {
        crc ^= p[i];
        for (k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
        }
    }
    return ~crc;
}

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void wr16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void wr32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

/* The info section as the game writes it: magic "DMW3" and the XOR of bytes
 * 4 .. sizeof(MemCardFile) in byte 0. */
static int info_valid(const uint8_t *image) {
    const uint8_t *info = image + SAVE_INFO_OFFSET;

    return rd32(info + 4) == SAVE_MAGIC_DMW3 &&
           save_xor(info + 4, SAVE_INFO_SIZE - 4) == info[0];
}

static void info_seal(uint8_t *image) {
    uint8_t *info = image + SAVE_INFO_OFFSET;

    info[2] = SAVE_EU_VERSION_BYTE;
    info[0] = save_xor(info + 4, SAVE_INFO_SIZE - 4);
}

static int info_has_name(const uint8_t *image, int slot) {
    return image[SAVE_INFO_OFFSET + 8 + slot * SAVE_SUMMARY_SIZE] != 0;
}

static const uint8_t *slot_ptr(const uint8_t *image, int s) {
    return image + SAVE_DATA_OFFSET + (size_t)s * SAVE_SLOT_STRIDE;
}

static uint8_t *slot_wptr(uint8_t *image, int s) {
    return image + SAVE_DATA_OFFSET + (size_t)s * SAVE_SLOT_STRIDE;
}

static int extra_chapter(const uint8_t *eu_slot) {
    return (int32_t)rd32(eu_slot + SAVE_PROGRESS_OFFSET) >=
           SAVE_PROGRESS_EXTRA_CHAPTER;
}

int save_store_parse(const uint8_t *f, size_t len, SaveStoreCard *out) {
    uint16_t fmt;
    size_t hdr;
    uint32_t psz;
    int s;

    memset(out, 0, sizeof(*out));
    if (len < 4 || memcmp(f, "DW3S", 4) != 0) {
        return len < 4 ? SAVE_STORE_SHORT : SAVE_STORE_MAGIC;
    }
    if (len < SAVE_STORE_HEADER_SIZE) {
        return SAVE_STORE_SHORT;
    }
    fmt = rd16(f + 4);
    if (fmt == 0) {
        return SAVE_STORE_HEADER;
    }
    if (fmt > SAVE_STORE_FORMAT) {
        return SAVE_STORE_NEWER;
    }
    hdr = rd16(f + 6);
    psz = rd32(f + 8);
    if (hdr < SAVE_STORE_MIN_HEADER || psz != SAVE_IMAGE_SIZE) {
        return SAVE_STORE_HEADER;
    }
    if (len < hdr + (size_t)psz) {
        return SAVE_STORE_SHORT;
    }
    if (save_crc32(f + hdr, psz) != rd32(f + 0xC)) {
        return SAVE_STORE_CRC;
    }
    memcpy(out->image, f + hdr, SAVE_IMAGE_SIZE);
    if (!info_valid(out->image)) {
        memset(out, 0, sizeof(*out));
        return SAVE_STORE_LAYOUT;
    }
    out->last_region = f[0x10];
    out->last_lang = f[0x11];
    for (s = 0; s < SAVE_SLOT_COUNT; s++) {
        const uint8_t *m = f + SAVE_STORE_META_OFFSET + s * SAVE_STORE_META_SIZE;

        out->meta[s].origin_region = m[0];
        out->meta[s].origin_lang = m[1];
        out->meta[s].flags = (uint8_t)(m[2] & (SAVE_META_NEEDS_EXTRA | SAVE_META_US_TAIL));
        out->meta[s].us_tail[0] = m[3];
        out->meta[s].us_tail[1] = m[4];
    }
    return SAVE_STORE_OK;
}

void save_store_serialize(const SaveStoreCard *c, uint8_t *out) {
    int s;

    memset(out, 0, SAVE_STORE_HEADER_SIZE);
    memcpy(out, "DW3S", 4);
    wr16(out + 4, SAVE_STORE_FORMAT);
    wr16(out + 6, (uint16_t)SAVE_STORE_HEADER_SIZE);
    wr32(out + 8, SAVE_IMAGE_SIZE);
    wr32(out + 0xC, save_crc32(c->image, SAVE_IMAGE_SIZE));
    out[0x10] = c->last_region;
    out[0x11] = c->last_lang;
    for (s = 0; s < SAVE_SLOT_COUNT; s++) {
        uint8_t *m = out + SAVE_STORE_META_OFFSET + s * SAVE_STORE_META_SIZE;

        m[0] = c->meta[s].origin_region;
        m[1] = c->meta[s].origin_lang;
        m[2] = c->meta[s].flags;
        m[3] = c->meta[s].us_tail[0];
        m[4] = c->meta[s].us_tail[1];
    }
    memcpy(out + SAVE_STORE_HEADER_SIZE, c->image, SAVE_IMAGE_SIZE);
}

int save_store_to_native(const SaveStoreCard *c, int region, uint8_t *native,
                         SaveConvertReport *rep) {
    SaveConvertReport local;
    int s;

    if (rep == NULL) {
        rep = &local;
    }
    if (save_convert_image(c->image, native, region, rep) != 0) {
        return -1;
    }
    if (region == SAVE_REGION_US) {
        for (s = 0; s < SAVE_SLOT_COUNT; s++) {
            uint8_t *out = slot_wptr(native, s);

            if (rep->result[s] != SAVE_SLOT_CONVERTED ||
                (c->meta[s].flags & SAVE_META_US_TAIL) == 0) {
                continue;
            }
            /* The two US-only flag bytes the EU layout has no place for. */
            out[SAVE_US_TAIL_OFFSET] = c->meta[s].us_tail[0];
            out[SAVE_US_TAIL_OFFSET + 1] = c->meta[s].us_tail[1];
            out[0] = save_xor(out + 4, SAVE_SIZE_US - 4);
        }
    }
    return 0;
}

static int unsettled(SaveStoreWriteReport *rep, const char *fmt, int a, int b) {
    /* fmt uses at most the two ints; the extra one is ignored */
    snprintf(rep->reason, sizeof(rep->reason), fmt, a, b);
    return 1;
}

int save_store_from_native(SaveStoreCard *c, int region, const uint8_t *native,
                           const uint8_t *base, int lang, int final,
                           SaveStoreWriteReport *rep) {
    SaveStoreWriteReport local;
    uint8_t tmp[SAVE_SLOT_STRIDE];
    int changed[SAVE_SLOT_COUNT];
    int s;

    if (rep == NULL) {
        rep = &local;
    }
    memset(rep, 0, sizeof(*rep));
    if (region != SAVE_REGION_US && region != SAVE_REGION_EU) {
        snprintf(rep->reason, sizeof(rep->reason), "unknown region %d", region);
        return -1;
    }
    if (!info_valid(native)) {
        return unsettled(rep, "info section is not a DW3 card yet (magic or checksum)", 0, 0);
    }
    /* Pass 1: everything the game wrote must be a valid save of its region
     * before the card is touched. */
    for (s = 0; s < SAVE_SLOT_COUNT; s++) {
        changed[s] = base == NULL ||
                     memcmp(slot_ptr(native, s), slot_ptr(base, s), SAVE_SLOT_STRIDE) != 0;
        rep->slot_result[s] = SAVE_SLOT_EMPTY;
        if (!changed[s] || !info_has_name(native, s)) {
            continue;
        }
        rep->slot_result[s] = save_convert_slot(slot_ptr(native, s), tmp, SAVE_REGION_EU,
                                                &rep->warn[s], NULL);
        if (rep->slot_result[s] != SAVE_SLOT_NATIVE &&
            rep->slot_result[s] != SAVE_SLOT_CONVERTED) {
            return unsettled(rep, "slot %d is not a valid save of the build's region (result %d)",
                             s, rep->slot_result[s]);
        }
    }
    if (!final) {
        int any_slot = 0;
        int hdr = base == NULL ||
                  memcmp(native, base, SAVE_DATA_OFFSET) != 0;

        for (s = 0; s < SAVE_SLOT_COUNT; s++) {
            any_slot |= changed[s] &&
                        (info_has_name(native, s) || c->meta[s].origin_region != 0);
        }
        if (hdr && !any_slot && base != NULL) {
            /* The game writes the info section before the slot's data. */
            return unsettled(rep, "info section written, waiting for the slot data", 0, 0);
        }
    }
    /* Pass 2: commit. */
    for (s = 0; s < SAVE_SLOT_COUNT; s++) {
        SaveStoreSlotMeta *m = &c->meta[s];
        uint8_t *dst = slot_wptr(c->image, s);

        if (!changed[s]) {
            continue;
        }
        if (!info_has_name(native, s)) {
            /* an empty slot: zeros. It only counts as written when it held something. */
            uint8_t acc = m->origin_region | m->flags;
            size_t k;

            for (k = 0; k < SAVE_SLOT_STRIDE; k++) {
                acc |= dst[k];
            }
            if (acc != 0) {
                rep->changed |= 1u << s;
            }
            memset(m, 0, sizeof(*m));
            memset(dst, 0, SAVE_SLOT_STRIDE);
            continue;
        }
        rep->changed |= 1u << s;
        memset(m, 0, sizeof(*m));
        (void)save_convert_slot(slot_ptr(native, s), dst, SAVE_REGION_EU, NULL, NULL);
        m->origin_region = (uint8_t)region;
        m->origin_lang = region == SAVE_REGION_EU && lang >= 0 && lang < 0xFF
                             ? (uint8_t)lang : SAVE_STORE_LANG_NONE;
        if (extra_chapter(dst)) {
            m->flags |= SAVE_META_NEEDS_EXTRA;
        }
        if (region == SAVE_REGION_US) {
            const uint8_t *in = slot_ptr(native, s);

            /* Kept, so a save that goes US -> EU -> US stays whole. */
            rep->warn[s] &= ~SAVE_WARN_DROPPED_FLAGS;
            if (in[SAVE_US_TAIL_OFFSET] != 0 || in[SAVE_US_TAIL_OFFSET + 1] != 0) {
                m->flags |= SAVE_META_US_TAIL;
                m->us_tail[0] = in[SAVE_US_TAIL_OFFSET];
                m->us_tail[1] = in[SAVE_US_TAIL_OFFSET + 1];
            }
        }
    }
    if (base == NULL || memcmp(native, base, SAVE_INFO_OFFSET) != 0) {
        memcpy(c->image, native, SAVE_INFO_OFFSET);
        rep->header_changed = 1;
    }
    if (base == NULL ||
        memcmp(native + SAVE_INFO_OFFSET, base + SAVE_INFO_OFFSET,
               SAVE_DATA_OFFSET - SAVE_INFO_OFFSET) != 0) {
        memcpy(c->image + SAVE_INFO_OFFSET, native + SAVE_INFO_OFFSET,
               SAVE_DATA_OFFSET - SAVE_INFO_OFFSET);
        info_seal(c->image);
        rep->header_changed = 1;
    }
    if (rep->changed != 0 || rep->header_changed) {
        c->last_region = (uint8_t)region;
        c->last_lang = region == SAVE_REGION_EU && lang >= 0 && lang < 0xFF
                           ? (uint8_t)lang : SAVE_STORE_LANG_NONE;
    }
    return 0;
}

int save_store_import(SaveStoreCard cards[SAVE_STORE_CARDS],
                      const SaveImportSource *src, int nsrc,
                      SaveImportReport *rep) {
    SaveImportReport local;
    uint8_t tmp[SAVE_SLOT_STRIDE];
    int next = 0;
    int i;
    int s;
    int c;

    if (rep == NULL) {
        rep = &local;
    }
    memset(rep, 0, sizeof(*rep));
    memset(cards, 0, sizeof(SaveStoreCard) * SAVE_STORE_CARDS);
    memset(rep->target, 0xFF, sizeof(rep->target)); /* -1 */
    if (nsrc > SAVE_IMPORT_MAX_SOURCES) {
        nsrc = SAVE_IMPORT_MAX_SOURCES;
    }
    for (i = 0; i < nsrc; i++) {
        const uint8_t *img = src[i].image;
        const uint8_t *sinfo;

        if (img == NULL) {
            continue;
        }
        if (!info_valid(img)) {
            rep->outcome[i][0] = SAVE_IMPORT_SOURCE_BAD;
            continue;
        }
        sinfo = img + SAVE_INFO_OFFSET;
        for (s = 0; s < SAVE_SLOT_COUNT; s++) {
            const uint8_t *in = slot_ptr(img, s);
            SaveStoreCard *card;
            SaveStoreSlotMeta *m;
            int region;
            int ts;
            int dup = 0;
            int t;

            if (!info_has_name(img, s)) {
                continue; /* EMPTY */
            }
            region = save_slot_region(in);
            if (region == SAVE_REGION_NONE) {
                rep->outcome[i][s] = SAVE_IMPORT_INVALID;
                rep->invalid++;
                continue;
            }
            rep->src_region[i][s] = region;
            (void)save_convert_slot(in, tmp, SAVE_REGION_EU, NULL, NULL);
            for (t = 0; t < next; t++) {
                if (memcmp(slot_ptr(cards[t / SAVE_SLOT_COUNT].image, t % SAVE_SLOT_COUNT),
                           tmp, SAVE_SIZE_EU) == 0) {
                    dup = 1;
                    break;
                }
            }
            if (dup) {
                rep->outcome[i][s] = SAVE_IMPORT_DUPLICATE;
                rep->duplicates++;
                continue;
            }
            if (next >= SAVE_IMPORT_CAPACITY) {
                rep->outcome[i][s] = SAVE_IMPORT_LEFTOVER;
                rep->leftover++;
                continue;
            }
            c = next / SAVE_SLOT_COUNT;
            ts = next % SAVE_SLOT_COUNT;
            card = &cards[c];
            if (!rep->card_used[c]) {
                /* The first save to land in a card brings its header and icon
                 * sectors and the info frame; the summaries are rebuilt. */
                memcpy(card->image, img, SAVE_DATA_OFFSET);
                memset(card->image + SAVE_INFO_OFFSET + 8, 0,
                       SAVE_INFO_SIZE - 8);
                card->image[SAVE_INFO_OFFSET + 1] = (uint8_t)ts;
                rep->card_used[c] = 1;
            }
            memcpy(slot_wptr(card->image, ts), tmp, SAVE_SLOT_STRIDE);
            memcpy(card->image + SAVE_INFO_OFFSET + 8 + ts * SAVE_SUMMARY_SIZE,
                   sinfo + 8 + s * SAVE_SUMMARY_SIZE, SAVE_SUMMARY_SIZE);
            if (sinfo[1] == (uint8_t)s) {
                card->image[SAVE_INFO_OFFSET + 1] = (uint8_t)ts;
            }
            m = &card->meta[ts];
            memset(m, 0, sizeof(*m));
            m->origin_region = (uint8_t)region;
            m->origin_lang = SAVE_STORE_LANG_NONE; /* the language is not in the save */
            if (extra_chapter(tmp)) {
                m->flags |= SAVE_META_NEEDS_EXTRA;
            }
            if (region == SAVE_REGION_US &&
                (in[SAVE_US_TAIL_OFFSET] != 0 || in[SAVE_US_TAIL_OFFSET + 1] != 0)) {
                m->flags |= SAVE_META_US_TAIL;
                m->us_tail[0] = in[SAVE_US_TAIL_OFFSET];
                m->us_tail[1] = in[SAVE_US_TAIL_OFFSET + 1];
            }
            card->last_region = (uint8_t)region;
            card->last_lang = SAVE_STORE_LANG_NONE;
            rep->outcome[i][s] = SAVE_IMPORT_IMPORTED;
            rep->target[i][s] = next;
            rep->imported++;
            next++;
        }
    }
    for (c = 0; c < SAVE_STORE_CARDS; c++) {
        if (rep->card_used[c]) {
            info_seal(cards[c].image);
        }
    }
    return rep->imported;
}
