/* Diagnostic for the PORT_HOST NULL guards of the decomp (docs/BALLOON_NULL_ACTOR.md,
   patches 0058 and 0059).

   The PS1 reads through a NULL pointer without a fault (low RAM is readable
   kernel memory), so the original game tolerated a few script-driven lookups
   that return NULL. On a 64-bit host the same read faults, so the port
   patches skip the read. DW3_NULL_GUARD_HIT("site") records, once per site and
   process, that such a guard was taken, so a PC or nxlink log shows whether
   these paths fire in real play.

   Compiled in only when DW3_NULL_GUARD_LOG is defined: PC Debug builds,
   -DDW3_NULL_GUARD_LOG=ON, and the Switch --nxlink build (port/CMakeLists.txt).
   Release builds expand it to nothing, so the guards cost one NULL test.

   Output (stderr): "dw3: null-guard hit: <site>". */
#ifndef DW3_PORT_NULL_GUARD_H
#define DW3_PORT_NULL_GUARD_H

#ifdef DW3_NULL_GUARD_LOG
#include <stdio.h>
#define DW3_NULL_GUARD_HIT(site)                                          \
    do {                                                                  \
        static int dw3NullGuardSeen;                                      \
        if (!dw3NullGuardSeen) {                                          \
            dw3NullGuardSeen = 1;                                         \
            fprintf(stderr, "dw3: null-guard hit: %s\n", (site));         \
            fflush(stderr);                                               \
        }                                                                 \
    } while (0)
#else
#define DW3_NULL_GUARD_HIT(site) ((void)0)
#endif

#endif
