/* D3 / P6.26: the one-shot migration into the global port_options.cfg
 * (dw3_optcfg_migrate_global) and the folder rule (dw3_optcfg_global_dir).
 *
 *   options_global_test migrate <gdir>
 *       runs the migration; the log lines go to stdout (W: prefix = a warning),
 *       the result is the exit code 0 (none), 10 (done) or 11 (failed)
 *   options_global_test dir <cfgdir-or-"-"> <root>
 *       prints the global folder for that pair
 * Plain C, no game code (tools/tests/test_options_global.py drives it). */
#include "port_options_cfg.h"

#include <stdio.h>
#include <string.h>

static void log_line(void *ud, int warn, const char *line) {
    (void)ud;
    printf("%s%s\n", warn ? "W: " : "", line);
}

int main(int argc, char **argv) {
    if (argc == 3 && strcmp(argv[1], "migrate") == 0) {
        int rc = dw3_optcfg_migrate_global(argv[2], log_line, NULL);

        fflush(stdout);
        return rc == DW3_OPTGLOBAL_DONE ? 10 : rc == DW3_OPTGLOBAL_FAILED ? 11 : 0;
    }
    if (argc == 4 && strcmp(argv[1], "dir") == 0) {
        char out[64];
        int rc = dw3_optcfg_global_dir(strcmp(argv[2], "-") == 0 ? NULL : argv[2], argv[3],
                                       out, sizeof(out));

        printf("%d %s\n", rc, rc == 0 ? out : "");
        return 0;
    }
    fprintf(stderr, "usage\n");
    return 2;
}
