/* Host proofs of the P5.5 stage sweep (port/sweep/stage_sweep.c) against a
 * fake game whose modes follow requestMode() at once.
 *
 *   stage_sweep_test <dir> <scenario> [mode]
 *
 * Scenarios: ok (every request works), stuck <mode> (a request for that mode
 * is ignored), noreturn <mode> (the stage never goes back to the select),
 * hang <mode> (the game stops ticking at the request; the watchdog ends the
 * process after 1.5 s), leave <mode> (the stage leaves for the select by itself), crash <mode> (the
 * process dies at the request, as a fault would), only <mode> (the sweep is
 * limited to one mode, DW3_STAGE_SWEEP_ONLY). The sweep runs until it exits
 * (DW3_STAGE_SWEEP_EXIT). The log is on stderr; the progress file is in <dir>.
 * Plain C, no game code. */
#include "port_stage_sweep.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {
    unsigned long clock_ms = 1000;
    int mode = 0xE00;
    int arg = 0;
    int in_mode = 0;
    long i;
    const char *scen;

    if (argc < 3) {
        return 2;
    }
    scen = argv[2];
    if (argc > 3) {
        arg = (int)strtol(argv[3], NULL, 0);
    }
    setenv("DW3_STAGE_SWEEP", "1", 1);
    setenv("DW3_STAGE_SWEEP_DIR", argv[1], 1);
    setenv("DW3_STAGE_SWEEP_FRAMES", "30", 1);
    setenv("DW3_STAGE_SWEEP_EXIT", "1", 1);
    setenv("DW3_STAGE_SWEEP_HANG_MS", "1500", 1);
    if (strcmp(scen, "only") == 0) {
        setenv("DW3_STAGE_SWEEP_ONLY", argv[3], 1);
    }
    for (i = 0; i < 4000000; i++) {
        int req;

        clock_ms += 17;
        dw3_sweep_test_set_clock(clock_ms);
        in_mode++;
        if (strcmp(scen, "leave") == 0 && mode == arg && in_mode == 5) {
            mode = DW3_SWEEP_SELECT_MODE;
            in_mode = 0;
        }
        req = dw3_sweep_tick(mode);
        if (req != 0) {
            if (strcmp(scen, "crash") == 0 && req == arg) {
                fflush(NULL);
                _Exit(77);
            }
            if (strcmp(scen, "hang") == 0 && req == arg) {
                sleep(30); /* the game thread is stuck: no more ticks */
            }
            if (strcmp(scen, "stuck") == 0 && req == arg) {
                continue;
            }
            if (strcmp(scen, "noreturn") == 0 && mode == arg &&
                req == DW3_SWEEP_SELECT_MODE) {
                continue;
            }
            mode = req;
            in_mode = 0;
        }
    }
    return 3; /* the sweep never finished */
}
