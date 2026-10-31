/* P6.38: restart into the chosen region; see dw3_region_relaunch.h. */
#include "dw3_region_relaunch.h"
#include "dw3_disc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Dw3Region dw3_relaunch_arg_region(int argc, char **argv) {
    const size_t n = strlen(DW3_RELAUNCH_ARG);
    int i;

    for (i = 1; i < argc; i++) {
        if (argv[i] == NULL || strncmp(argv[i], DW3_RELAUNCH_ARG, n) != 0) {
            continue;
        }
        if (argv[i][n] == '=') {
            return dw3_region_parse(argv[i] + n + 1);
        }
        if (argv[i][n] == '\0' && i + 1 < argc) {
            return dw3_region_parse(argv[i + 1]);
        }
    }
    return DW3_REGION_NONE;
}

int dw3_relaunch_nx_args(const char *nro_path, Dw3Region target, char *out, size_t cap) {
    const char *name = dw3_region_dir(target);
    int quote;
    int n;

    if (nro_path == NULL || nro_path[0] == '\0' || name == NULL || out == NULL) {
        return -1;
    }
    /* hbloader splits the string on spaces and honours double quotes. */
    quote = strchr(nro_path, ' ') != NULL;
    n = snprintf(out, cap, quote ? "\"%s\" %s %s" : "%s %s %s", nro_path, DW3_RELAUNCH_ARG,
                 name);
    return (n < 0 || (size_t)n >= cap) ? -1 : n;
}

char **dw3_relaunch_pc_argv(int argc, char **argv, Dw3Region target) {
    static char flag[] = DW3_RELAUNCH_ARG;
    static char value[4];
    char **out = (char **)calloc((size_t)argc + 3, sizeof(char *));
    int n = 0;
    int i;

    if (out == NULL) {
        return NULL;
    }
    snprintf(value, sizeof(value), "%s", dw3_region_name(target));
    out[n++] = argc > 0 ? argv[0] : flag;
    for (i = 1; i < argc; i++) {
        const int has_value = i + 1 < argc;

        if (strcmp(argv[i], "--region") == 0 || strcmp(argv[i], "--runtime") == 0 ||
            strcmp(argv[i], DW3_RELAUNCH_ARG) == 0) {
            if (has_value) {
                i++; /* its value goes with it */
            }
            continue;
        }
        if (strncmp(argv[i], "--region=", 9) == 0 || strncmp(argv[i], "--runtime=", 10) == 0 ||
            strncmp(argv[i], DW3_RELAUNCH_ARG "=", sizeof(DW3_RELAUNCH_ARG)) == 0) {
            continue;
        }
        out[n++] = argv[i];
    }
    out[n++] = flag;
    out[n++] = value;
    out[n] = NULL;
    return out;
}

int dw3_relaunch_target_present(const char *base, Dw3Region target) {
    Dw3RegionProbe tree;
    Dw3DiscScan scan;

    if (base == NULL || target == DW3_REGION_NONE) {
        return 0;
    }
    dw3_region_probe(base, &tree);
    if (target == DW3_REGION_US ? tree.has_us : tree.has_eu) {
        return 1;
    }
    /* A disc image waiting for the extraction counts as present (issue #322). */
    dw3_disc_scan(base, &tree, &scan);
    return scan.hit[target - DW3_REGION_US].found != 0;
}
