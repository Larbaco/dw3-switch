/* P6.25 save conversion CLI (host test driver and a small user tool).
 *
 *   save_convert_tool table
 *       print the flag-group layout table the converter uses
 *   save_convert_tool convert <in> <out> us|eu
 *       convert a 32 KiB card image (saveN.bin) to the layout of the given
 *       region and write a NEW file. Never overwrites: <out> must not exist
 *       and must not be <in>. The input is only read.
 *   save_convert_tool inspect <in>
 *       report each slot (empty / region / valid)
 *
 * Exit status: 0 ok, 1 usage, 2 unreadable input, 3 not convertible (fail
 * closed, nothing written), 4 output exists or cannot be written.
 */
#include "save_convert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_image(const char *path, uint8_t *buf) {
    FILE *fp = fopen(path, "rb");
    size_t n;

    if (fp == NULL) {
        return -1;
    }
    n = fread(buf, 1, SAVE_IMAGE_SIZE, fp);
    if (n != SAVE_IMAGE_SIZE || fgetc(fp) != EOF) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

static int region_arg(const char *s) {
    if (strcmp(s, "us") == 0) {
        return SAVE_REGION_US;
    }
    if (strcmp(s, "eu") == 0) {
        return SAVE_REGION_EU;
    }
    return SAVE_REGION_NONE;
}

int main(int argc, char **argv) {
    static uint8_t in[SAVE_IMAGE_SIZE];
    static uint8_t out[SAVE_IMAGE_SIZE];
    SaveConvertReport rep;
    int i;

    if (argc == 2 && strcmp(argv[1], "table") == 0) {
        for (i = 0; i < g_save_flag_group_count; i++) {
            const SaveFlagGroup *g = &g_save_flag_groups[i];

            printf("%02X %04X %X %04X %X\n", g->group, g->us_off, g->us_size,
                   g->eu_off, g->eu_size);
        }
        printf("sizes %04X %04X versions %d %d converter %d\n", SAVE_SIZE_US,
               SAVE_SIZE_EU, SAVE_VERSION_US, SAVE_VERSION_EU,
               SAVE_CONVERT_VERSION);
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "inspect") == 0) {
        const uint8_t *info = in + SAVE_INFO_OFFSET;

        if (read_image(argv[2], in) != 0) {
            fprintf(stderr, "cannot read a 32 KiB image: %s\n", argv[2]);
            return 2;
        }
        for (i = 0; i < SAVE_SLOT_COUNT; i++) {
            int named = info[8 + i * SAVE_SUMMARY_SIZE] != 0;
            int r = save_slot_region(in + SAVE_DATA_OFFSET +
                                     (size_t)i * SAVE_SLOT_STRIDE);

            printf("slot %d named=%d region=%d\n", i, named, r);
        }
        return 0;
    }
    if (argc == 5 && strcmp(argv[1], "convert") == 0) {
        int dst = region_arg(argv[4]);
        FILE *fp;

        if (dst == SAVE_REGION_NONE) {
            fprintf(stderr, "region must be us or eu\n");
            return 1;
        }
        if (strcmp(argv[2], argv[3]) == 0) {
            fprintf(stderr, "refusing to overwrite the input\n");
            return 4;
        }
        if (read_image(argv[2], in) != 0) {
            fprintf(stderr, "cannot read a 32 KiB image: %s\n", argv[2]);
            return 2;
        }
        if (save_convert_image(in, out, dst, &rep) != 0 || rep.usable == 0) {
            printf("not convertible usable=%d results=%d,%d,%d\n", rep.usable,
                   rep.result[0], rep.result[1], rep.result[2]);
            return 3;
        }
        fp = fopen(argv[3], "rbx");
        if (fp != NULL) {
            fclose(fp);
            fprintf(stderr, "output exists: %s\n", argv[3]);
            return 4;
        }
        fp = fopen(argv[3], "wbx");
        if (fp == NULL || fwrite(out, 1, SAVE_IMAGE_SIZE, fp) != SAVE_IMAGE_SIZE) {
            fprintf(stderr, "cannot write %s\n", argv[3]);
            if (fp != NULL) {
                fclose(fp);
            }
            return 4;
        }
        fclose(fp);
        printf("ok converter=%d usable=%d converted=%d results=%d,%d,%d "
               "warn=%u,%u,%u\n",
               SAVE_CONVERT_VERSION, rep.usable, rep.converted, rep.result[0],
               rep.result[1], rep.result[2], rep.warn[0], rep.warn[1],
               rep.warn[2]);
        return 0;
    }
    fprintf(stderr,
            "usage: save_convert_tool table | inspect <in> | convert <in> "
            "<out> us|eu\n");
    return 1;
}
