/* P6.24 slice 2: the region launcher NRO for the Switch.
 *
 * No game code and no PSY-Z: it looks at sdmc:/switch/dw3/us and /eu with
 * port/region/dw3_region.c, applies the same boot rules as the PC launcher
 * (docs/SINGLE_BUILD.md, "Boot picker"), draws a plain console screen when it
 * has to ask, and hands over to the region's own game NRO with libnx
 * envSetNextLoad() (hbloader / hbmenu next-load). The game NROs are the
 * unchanged US and EU builds.
 *
 *   one region present      chain-load it at once
 *   none                    the setup message, + quits
 *   both                    picker (3 s, default = last region) unless a
 *                           region_default of us/eu is stored; holding
 *                           Minus (-) at start forces the picker
 *   no next-load support    a message instead of a crash, + quits
 *
 * The stored choice (region_default / last_region) lives in
 * sdmc:/switch/dw3/port_options.cfg, the same file the PC launcher uses in its
 * base folder. The game NROs ignore their path and arguments: they open
 * sdmc:/switch/dw3/<ver>/ (DW3_DEFAULT_RUNTIME), so chain-loading from
 * another folder still finds the data. */
#include <stdio.h>
#include <string.h>

#include <switch.h>

#include "dw3_launcher.h"
#include "dw3_region.h"

#define MINUS_SAMPLE_FRAMES 12 /* about 200 ms: the pad reports a moment late */

static PadState s_pad;
static char s_text[2048];

static void draw(Dw3LauncherScreen kind, const Dw3Picker *p, Dw3Region region,
                 const char *path, unsigned rc) {
    dw3_launcher_text(kind, p, DW3_LAUNCHER_BASE, region, path, rc, s_text,
                      sizeof(s_text));
    printf("\x1b[2J\x1b[1;1H%s", s_text);
    consoleUpdate(NULL);
}

/* Shows a message screen until + is pressed (or the applet is asked to
 * close). */
static void wait_for_plus(Dw3LauncherScreen kind, Dw3Region region,
                          const char *path, unsigned rc) {
    draw(kind, NULL, region, path, rc);
    while (appletMainLoop()) {
        padUpdate(&s_pad);
        if (padGetButtonsDown(&s_pad) & HidNpadButton_Plus) {
            break;
        }
        consoleUpdate(NULL);
    }
}

static unsigned pad_edges(void) {
    u64 down = padGetButtonsDown(&s_pad);
    unsigned e = 0;

    if (down & (HidNpadButton_Left | HidNpadButton_StickLLeft)) {
        e |= DW3_PK_LEFT;
    }
    if (down & (HidNpadButton_Right | HidNpadButton_StickLRight)) {
        e |= DW3_PK_RIGHT;
    }
    if (down & HidNpadButton_A) {
        e |= DW3_PK_A;
    }
    if (down & HidNpadButton_Y) {
        e |= DW3_PK_Y;
    }
    if (down & HidNpadButton_Plus) {
        e |= DW3_PK_PLUS;
    }
    return e;
}

/* Returns the chosen region, or NONE when the player quit. */
static Dw3Region run_picker(Dw3Region default_sel, int *always) {
    Dw3Picker p;
    u64 last = armTicksToNs(armGetSystemTick());
    int shown = -2;

    dw3_picker_init(&p, default_sel);
    *always = 0;
    while (appletMainLoop() && p.state == DW3_PICKER_RUNNING) {
        u64 now = armTicksToNs(armGetSystemTick());
        unsigned dt = (unsigned)((now - last) / 1000000ULL);
        unsigned e;
        int s;

        last = now;
        padUpdate(&s_pad);
        e = pad_edges();
        dw3_picker_step(&p, e, dt);
        s = dw3_picker_seconds_left(&p);
        /* Redraw on input and when the seconds line changes. */
        if (e != 0 || s != shown) {
            shown = s;
            draw(DW3_SCREEN_PICKER, &p, DW3_REGION_NONE, NULL, 0);
        }
        consoleUpdate(NULL);
    }
    if (p.state != DW3_PICKER_CHOSEN) {
        return DW3_REGION_NONE;
    }
    *always = p.always;
    return p.chosen;
}

/* Queues the region's game NRO. Returns 1 when hbloader will start it after
 * this program exits; otherwise shows why and waits for +. */
static int chain_load(Dw3Region region, int picked, int always) {
    char path[512];
    Result rc;

    if (!dw3_launcher_nro_path(DW3_LAUNCHER_BASE, region, path, sizeof(path))) {
        wait_for_plus(DW3_SCREEN_NO_NRO, region, path, 0);
        return 0;
    }
    if (!envHasNextLoad()) {
        wait_for_plus(DW3_SCREEN_NO_LOADER, region, path, 0);
        return 0;
    }
    /* argv[0] is the NRO path, as hbmenu passes it; the game ignores it. */
    rc = envSetNextLoad(path, path);
    if (R_FAILED(rc)) {
        wait_for_plus(DW3_SCREEN_LOAD_FAILED, region, path, rc);
        return 0;
    }
    if (dw3_region_cfg_write(DW3_LAUNCHER_BASE, region, picked && always, region) != 0) {
        /* Not fatal: the game still starts. */
        fprintf(stderr, "dw3-launcher: could not store the choice\n");
    }
    return 1;
}

int main(int argc, char **argv) {
    Dw3RegionInput in;
    Dw3BootDecision d;
    Dw3Region region;
    int picked = 0, always = 0, chained = 0, i;

    (void)argc;
    (void)argv;
    consoleInit(NULL);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&s_pad);

    memset(&in, 0, sizeof(in));
    /* Minus held at start: sample a few frames, the pad is late to report. */
    for (i = 0; i < MINUS_SAMPLE_FRAMES && appletMainLoop(); i++) {
        padUpdate(&s_pad);
        if (padGetButtons(&s_pad) & HidNpadButton_Minus) {
            in.minus_held = 1;
        }
        /* P51 item 4: ZL + ZR held together asks as well (a try; the Options
         * row "Region at start" is the supported way) */
        if ((padGetButtons(&s_pad) & (HidNpadButton_ZL | HidNpadButton_ZR)) ==
            (HidNpadButton_ZL | HidNpadButton_ZR)) {
            in.minus_held = 1;
        }
        consoleUpdate(NULL);
    }
    dw3_region_probe(DW3_LAUNCHER_BASE, &in.probe);
    dw3_region_cfg_read(DW3_LAUNCHER_BASE, &in.pref, &in.last);
    d = dw3_region_decide(&in);

    if (d.action == DW3_BOOT_NO_DATA) {
        wait_for_plus(DW3_SCREEN_NO_DATA, DW3_REGION_NONE, NULL, 0);
    } else {
        region = d.region;
        if (d.action == DW3_BOOT_PICK) {
            region = run_picker(d.region, &always);
            picked = 1;
        }
        if (region != DW3_REGION_NONE) {
            chained = chain_load(region, picked, always);
        }
    }
    (void)chained;
    consoleExit(NULL);
    return 0; /* hbloader starts the queued NRO, or hbmenu comes back */
}
