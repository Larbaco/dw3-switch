/* P6.37: the port version label (see port_version.h). */
#include "port_version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <psyz/log.h>
#ifndef __SWITCH__
#include <psyz.h>
#endif

#include "dw3_region_rt.h"
#include "dw3_version_gen.h" /* generated: tools/dw3_nro_version.py --write-header */

#define DW3_VERSION_NAME "dw3-switch"

static const char *region_tag(void) {
    switch (dw3_region()) {
    case DW3_REGION_US:
        return "US";
    case DW3_REGION_EU:
        return "EU";
    default:
        return "US+EU"; /* a combined image before the region is picked */
    }
}

/* The label part: the generated one, or the replay placeholder. */
static const char *label_text(void) {
#if defined(DW3_NX_REPLAY)
    return "replay";
#else
    const char *o = getenv("DW3_VERSION_OVERRIDE");

    return (o != NULL && o[0] != '\0') ? o : DW3_VERSION_LABEL;
#endif
}

void dw3_version_text(char *buf, size_t cap) {
    if (cap == 0) {
        return;
    }
    snprintf(buf, cap, "%s %s %s", DW3_VERSION_NAME, label_text(), region_tag());
}

const char *dw3_version_build_date(void) {
    return DW3_VERSION_BUILD_DATE;
}

void dw3_version_log_line(char *buf, size_t cap) {
    char text[160];

    dw3_version_text(text, sizeof(text));
    snprintf(buf, cap, "dw3-version: %s build=%s", text, DW3_VERSION_BUILD_DATE);
}

void dw3_version_announce(void) {
    static char last[256];
    char line[256];

    dw3_version_log_line(line, sizeof(line));
    if (strcmp(line, last) == 0) {
        return;
    }
    snprintf(last, sizeof(last), "%s", line);
    INFOF("%s", line);
#ifndef __SWITCH__
    {
        char text[160];
        char title[200];

        dw3_version_text(text, sizeof(text));
        snprintf(title, sizeof(title), "dw3-pc - %s", text);
        Psyz_SetTitle(title);
    }
#endif
}

#ifndef __SWITCH__
/* PC: the first line of the log, before main() and the region pick. */
__attribute__((constructor)) static void version_announce_at_start(void) {
    dw3_version_announce();
}
#endif
