#ifndef PORT_STDERR_H
#define PORT_STDERR_H

#include <stdio.h>

/* 1 while the process stderr can still be written. The Switch host returns 0
 * once it has torn the nxlink stdio down (dw3_nx_log_stderr_detach): fd 2 then
 * has no device, and a write through it faults inside _write_r (BUG-R0-1, the
 * crash after quitting with an nxlink host). PC and every other build keep the
 * weak default in port_options.c, which always returns 1. */
int dw3_stderr_live(void);

/* Weak default, so every translation unit links without the Switch host.
 * psyz_backend/nx_log.c defines the strong version. Weak definitions in a
 * header are merged by the linker. */
__attribute__((weak)) int dw3_stderr_live(void) {
    return 1;
}

/* fprintf(stderr, ...) that is skipped once stderr is detached. Use it for any
 * stderr line that can run from the exit tail or an atexit handler. */
#define DW3_STDERRF(...)                                                       \
    do {                                                                       \
        if (dw3_stderr_live()) {                                               \
            fprintf(stderr, __VA_ARGS__);                                      \
        }                                                                      \
    } while (0)

#endif
