/* Host shim for tools/cdfs_suite.c in library mode (DW3_CDFS_SUITE_LIB).
 *
 * Stands in for the Switch .nro runner described in docs/P4.5_SD_SUITE.md:
 * the CI/self-test binary is lib-mode plus this single main. `--lib-twice`
 * exercises what a persistent process must survive — two cdfs_suite_main
 * calls in one process, whose per-call counters must not accumulate.
 *
 * Usage:
 *   cdfs_suite_lib <suite args...>
 *   cdfs_suite_lib --lib-twice <suite args...>
 */
#include <stdio.h>
#include <string.h>

int cdfs_suite_main(int argc, char **argv);


int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--lib-twice") == 0) {
        /* cdfs_suite_main keeps main()'s semantics: argv[0] is a name and is
         * skipped. Build the shifted argv so both per-call runs see one. */
        char *call[argc];
        int i;
        int rc1;
        int rc2;

        call[0] = (char *)"cdfs_suite";
        for (i = 2; i < argc; i++) {
            call[i - 1] = argv[i];
        }
        rc1 = cdfs_suite_main(argc - 1, call);
        rc2 = cdfs_suite_main(argc - 1, call);
        printf("lib-twice rc=%d,%d\n", rc1, rc2);
        return rc1 == rc2 ? rc1 : 3;
    }
    return cdfs_suite_main(argc, argv);
}
