/* P6.24 slice 2: pure helpers of the Switch launcher NRO. See dw3_launcher.h. */
#include "dw3_launcher.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int file_exists(const char *path) {
    struct stat st;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int dw3_launcher_nro_path(const char *base, Dw3Region r, char *out, size_t cap) {
    const char *v = dw3_region_dir(r);
    char alt[512];

    if (out == NULL || cap == 0) {
        return 0;
    }
    out[0] = '\0';
    if (v == NULL || base == NULL) {
        return 0;
    }
    snprintf(out, cap, "%s/dw3-%s.nro", base, v);
    if (file_exists(out)) {
        return 1;
    }
    snprintf(alt, sizeof(alt), "%s/dw3-switch-%s.nro", base, v);
    if (file_exists(alt)) {
        snprintf(out, cap, "%s", alt);
        return 1;
    }
    return 0;
}

static const char *label(Dw3Region r) {
    return r == DW3_REGION_EU ? "EU" : "US";
}

size_t dw3_launcher_text(Dw3LauncherScreen kind, const Dw3Picker *p,
                         const char *base, Dw3Region region, const char *path,
                         unsigned rc, char *out, size_t cap) {
    int n = 0;

    if (out == NULL || cap == 0) {
        return 0;
    }
    out[0] = '\0';
    if (base == NULL) {
        base = DW3_LAUNCHER_BASE;
    }
    if (path == NULL) {
        path = "";
    }
    switch (kind) {
    case DW3_SCREEN_PICKER: {
        int s;
        char tail[48];

        if (p == NULL) {
            return 0;
        }
        s = dw3_picker_seconds_left(p);
        if (s >= 0) {
            snprintf(tail, sizeof(tail), "Starting %s in %d...", label(p->sel), s);
        } else {
            snprintf(tail, sizeof(tail), "Press A to start %s.", label(p->sel));
        }
        n = snprintf(out, cap,
                     "\n\n  Digimon World 3\n\n"
                     "  Both versions were found. Choose one:\n\n"
                     "      %c US %c        %c EU %c\n\n"
                     "  Always start this one:  %s\n\n"
                     "  %s\n\n\n"
                     "  Left / Right  choose      A  start\n"
                     "  Y  always start this one  +  quit\n\n"
                     "  Hold - (Minus) while starting to see this screen again.\n",
                     p->sel == DW3_REGION_US ? '>' : ' ',
                     p->sel == DW3_REGION_US ? '<' : ' ',
                     p->sel == DW3_REGION_EU ? '>' : ' ',
                     p->sel == DW3_REGION_EU ? '<' : ' ',
                     p->always ? "ON" : "off", tail);
        break;
    }
    case DW3_SCREEN_NO_DATA:
        n = snprintf(out, cap,
                     "\n\n  Digimon World 3\n\n"
                     "  No game data found.\n\n"
                     "  Looked for:\n"
                     "    %s/us  (files_us.json + SLUS_014.36)\n"
                     "    %s/eu  (files_eu.json + SLES_039.36)\n\n"
                     "  Put your own disc there as described in\n"
                     "  docs/RUNTIME_LAYOUT.md (run the game NRO once with\n"
                     "  the BIN/CUE in that folder and it extracts itself).\n\n\n"
                     "  +  quit\n",
                     base, base);
        break;
    case DW3_SCREEN_NO_NRO:
        n = snprintf(out, cap,
                     "\n\n  Digimon World 3 (%s)\n\n"
                     "  The %s game file is missing:\n"
                     "    %s\n\n"
                     "  Copy the %s build of dw3-switch.nro there under that\n"
                     "  name (docs/SINGLE_BUILD.md).\n\n\n"
                     "  +  quit\n",
                     label(region), label(region), path, label(region));
        break;
    case DW3_SCREEN_NO_LOADER:
        n = snprintf(out, cap,
                     "\n\n  Digimon World 3 (%s)\n\n"
                     "  This launcher cannot start another program here.\n"
                     "  It must be opened from the Homebrew Menu (hbmenu or\n"
                     "  Sphaira) in applet mode, not through a title takeover.\n\n"
                     "  You can start the game file directly instead:\n"
                     "    %s\n\n\n"
                     "  +  quit\n",
                     label(region), path);
        break;
    case DW3_SCREEN_LOAD_FAILED:
        n = snprintf(out, cap,
                     "\n\n  Digimon World 3 (%s)\n\n"
                     "  Could not queue the game file:\n"
                     "    %s\n"
                     "  error 0x%X\n\n"
                     "  You can start that file directly from the Homebrew Menu.\n\n\n"
                     "  +  quit\n",
                     label(region), path, rc);
        break;
    }
    if (n < 0) {
        out[0] = '\0';
        return 0;
    }
    return (size_t)n < cap ? (size_t)n : cap - 1;
}
