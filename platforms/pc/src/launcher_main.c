/* P6.24 slice 1: the PC front end of the single build.
 *
 *   dw3-pc [--region us|eu] [--pick] [--runtime-base DIR] [game args...]
 *
 * Finds which region trees exist under the base folder (default `runtime`,
 * `<base>/us` and `<base>/eu`), applies the boot rules of
 * docs/SINGLE_BUILD.md ("Boot picker"), and starts the region's own
 * binary (`dw3-pc-us` / `dw3-pc-eu`, next to this one) with DW3_RUNTIME set.
 * It links no game code and no PSY-Z: this slice proves the detection, the
 * picker and the stored choice; the two game images still live in two
 * binaries until the dual-image link lands.
 *
 *   one tree present        boot it, no picker
 *   none                    the setup message, exit status 2
 *   both                    picker (3 s, default = last region) unless a
 *                           `region_default` of us/eu is stored
 *   --region / DW3_REGION   boot that region: no picker, no file written
 *   --pick / DW3_REGION_PICK=1  the PC stand-in for holding Minus
 *
 * Picker input on the PC: a terminal (l/1 = US, r/2 = EU, Enter or a = boot,
 * y = toggle "always start this one", q = quit; any key but a/Enter cancels
 * the countdown) or, for tests, DW3_REGION_PICK_KEYS, one character per
 * 100 ms step ('.' idles; l r a y q as above). Without a terminal and without
 * keys the picker starts the default entry at once instead of waiting.
 *
 * Environment: DW3_RUNTIME_BASE (base folder), DW3_CONFIG_DIR (where
 * port_options.cfg with region_default / last_region lives; default the
 * base), DW3_PC_BIN_US / DW3_PC_BIN_EU (binaries). */
#define _POSIX_C_SOURCE 200809L
#include "dw3_region.h"
#include "port_options_cfg.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define STEP_MS 100

static const char *env_or(const char *key, const char *fallback) {
    const char *v = getenv(key);

    return (v != NULL && v[0] != '\0') ? v : fallback;
}

static void print_setup_message(const char *base) {
    fprintf(stderr,
            "dw3-pc: no game data found.\n"
            "  looked for %s/us (files_us.json + SLUS_014.36) and\n"
            "            %s/eu (files_eu.json + SLES_039.36).\n"
            "  Extract your own disc into one of them (docs/RUNTIME_LAYOUT.md),\n"
            "  or pass --runtime-base DIR / --region us|eu.\n",
            base, base);
}

static unsigned key_to_edge(int c, int *quit_requested) {
    switch (c) {
    case 'l': case 'L': case '1': case 'h':
        return DW3_PK_LEFT;
    case 'r': case 'R': case '2':
        return DW3_PK_RIGHT;
    case 'a': case 'A': case '\n': case '\r': case ' ':
        return DW3_PK_A;
    case 'y': case 'Y':
        return DW3_PK_Y;
    case 'q': case 'Q': case '+':
        *quit_requested = 1;
        return DW3_PK_PLUS;
    default:
        return 0;
    }
}

static void show(const Dw3Picker *p) {
    int s = dw3_picker_seconds_left(p);
    char tail[40];

    if (s >= 0) {
        snprintf(tail, sizeof(tail), "Starting %s in %d...",
                 p->sel == DW3_REGION_EU ? "EU" : "US", s);
    } else {
        snprintf(tail, sizeof(tail), "Enter/a = start");
    }
    fprintf(stderr, "\rdw3-pc: region [%cUS%c] [%cEU%c]  always=%s  %-24s",
            p->sel == DW3_REGION_US ? '>' : ' ',
            p->sel == DW3_REGION_US ? '<' : ' ',
            p->sel == DW3_REGION_EU ? '>' : ' ',
            p->sel == DW3_REGION_EU ? '<' : ' ',
            p->always ? "on " : "off", tail);
}

/* Runs the picker. Returns the chosen region, or NONE when the player quit.
 * *always is set when "always start this one" was on. */
static Dw3Region run_picker(Dw3Region default_sel, int *always) {
    Dw3Picker p;
    const char *keys = getenv("DW3_REGION_PICK_KEYS");
    int tty = isatty(STDIN_FILENO) && isatty(STDERR_FILENO);
    struct termios old, raw;
    int have_term = 0;
    size_t ki = 0;

    dw3_picker_init(&p, default_sel);
    *always = 0;
    if (keys == NULL && !tty) {
        fprintf(stderr,
                "dw3-pc: both regions found; no terminal for the picker, "
                "starting %s\n", dw3_region_name(p.sel));
        return p.sel;
    }
    if (keys == NULL && tcgetattr(STDIN_FILENO, &old) == 0) {
        raw = old;
        raw.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;
        have_term = tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
    }
    while (p.state == DW3_PICKER_RUNNING) {
        unsigned edge = 0;
        int quit = 0; /* the edge carries it; kept for key_to_edge */

        if (keys != NULL) {
            if (keys[ki] != '\0') {
                edge = key_to_edge((unsigned char)keys[ki++], &quit);
            }
        } else {
            struct pollfd pf = {STDIN_FILENO, POLLIN, 0};
            unsigned char c;

            if (poll(&pf, 1, STEP_MS) > 0 && read(STDIN_FILENO, &c, 1) == 1) {
                edge = key_to_edge(c, &quit); /* unknown keys: 0, ignored */
            }
        }
        (void)quit;
        dw3_picker_step(&p, edge, STEP_MS);
        if (keys == NULL) {
            show(&p);
        }
    }
    fprintf(stderr, "\n");
    if (have_term) {
        tcsetattr(STDIN_FILENO, TCSANOW, &old);
    }
    if (p.state == DW3_PICKER_QUIT) {
        return DW3_REGION_NONE;
    }
    *always = p.always;
    fprintf(stderr, "dw3-region: picker chose %s%s%s\n",
            dw3_region_name(p.chosen), p.timed_out ? " (countdown)" : "",
            p.always ? " (always)" : "");
    return p.chosen;
}

static int self_dir(char *out, size_t cap) {
    ssize_t n = readlink("/proc/self/exe", out, cap - 1);
    char *slash;

    if (n <= 0) {
        return -1;
    }
    out[n] = '\0';
    slash = strrchr(out, '/');
    if (slash == NULL) {
        return -1;
    }
    *slash = '\0';
    return 0;
}

int main(int argc, char **argv) {
    const char *base = env_or("DW3_RUNTIME_BASE", "runtime");
    const char *cfgdir;
    Dw3RegionInput in;
    Dw3BootDecision d;
    Dw3Region region;
    char bin[1024], root[1024], dir[512];
    const char *override;
    char **args;
    int i, n = 0, picked = 0, always = 0, has_runtime_arg = 0, force_pick;

    memset(&in, 0, sizeof(in));
    in.forced = dw3_region_parse(getenv("DW3_REGION"));
    force_pick = getenv("DW3_REGION_PICK") != NULL &&
                 strcmp(getenv("DW3_REGION_PICK"), "1") == 0;
    args = calloc((size_t)argc + 1, sizeof(char *));
    if (args == NULL) {
        return 1;
    }
    args[n++] = argv[0];
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--region") == 0 && i + 1 < argc) {
            in.forced = dw3_region_parse(argv[++i]);
            if (in.forced == DW3_REGION_NONE) {
                fprintf(stderr, "dw3-pc: --region must be us or eu\n");
                return 2;
            }
        } else if (strcmp(argv[i], "--runtime-base") == 0 && i + 1 < argc) {
            base = argv[++i];
        } else if (strcmp(argv[i], "--pick") == 0) {
            force_pick = 1;
        } else {
            if (strcmp(argv[i], "--runtime") == 0) {
                has_runtime_arg = 1;
            }
            args[n++] = argv[i];
        }
    }
    args[n] = NULL;
    cfgdir = env_or("DW3_CONFIG_DIR", base);

    dw3_region_probe(base, &in.probe);
    if (in.forced == DW3_REGION_NONE) {
        /* D3: fold the old us/ and eu/ port_options.cfg into the global one before the
         * choice below overwrites last_region (a forced region touches no file). */
        (void)dw3_optcfg_migrate_global(cfgdir, NULL, NULL);
    }
    dw3_region_cfg_read(cfgdir, &in.pref, &in.last);
    in.minus_held = force_pick;
    d = dw3_region_decide(&in);
    fprintf(stderr, "dw3-region: base=%s us=%d eu=%d forced=%s default=%s last=%s -> %s\n",
            base, in.probe.has_us, in.probe.has_eu, dw3_region_name(in.forced),
            in.pref == DW3_REGION_NONE ? "ask" : dw3_region_name(in.pref),
            dw3_region_name(in.last), d.why);

    if (d.action == DW3_BOOT_NO_DATA) {
        print_setup_message(base);
        return 2;
    }
    region = d.region;
    if (d.action == DW3_BOOT_PICK) {
        region = run_picker(d.region, &always);
        if (region == DW3_REGION_NONE) {
            fprintf(stderr, "dw3-region: quit from the picker\n");
            return 0;
        }
        picked = 1;
    }
    /* A forced region touches no file (the replay runs must stay clean). */
    if (in.forced == DW3_REGION_NONE) {
        if (dw3_region_cfg_write(cfgdir, region, picked && always, region) != 0) {
            fprintf(stderr, "dw3-region: could not store the choice in %s\n", cfgdir);
        }
    }

    snprintf(root, sizeof(root), "%s/%s", base, dw3_region_name(region));
    if (!has_runtime_arg && getenv("DW3_RUNTIME") == NULL) {
        if (in.forced != DW3_REGION_NONE &&
            !dw3_region_runtime_complete(root, region)) {
            fprintf(stderr, "dw3-pc: no %s data in %s\n", dw3_region_name(region), root);
            print_setup_message(base);
            return 2;
        }
        setenv("DW3_RUNTIME", root, 1);
    }
    override = getenv(region == DW3_REGION_EU ? "DW3_PC_BIN_EU" : "DW3_PC_BIN_US");
    if (override != NULL && override[0] != '\0') {
        snprintf(bin, sizeof(bin), "%s", override);
    } else if (self_dir(dir, sizeof(dir)) == 0) {
        snprintf(bin, sizeof(bin), "%s/dw3-pc-%s", dir, dw3_region_name(region));
    } else {
        snprintf(bin, sizeof(bin), "dw3-pc-%s", dw3_region_name(region));
    }
    fprintf(stderr, "dw3-region: starting %s (%s) runtime=%s\n",
            dw3_region_name(region), bin, getenv("DW3_RUNTIME"));
    fflush(stderr);
    args[0] = bin;
    execv(bin, args);
    fprintf(stderr, "dw3-pc: cannot start %s: %s\n", bin, strerror(errno));
    return 127;
}
