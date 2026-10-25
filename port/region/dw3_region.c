/* P6.24 slice 1: run-time region choice; see dw3_region.h. */
#include "dw3_region.h"

#include "port_options_cfg.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static int ieq(const char *a, const char *b) {
    for (; *a != '\0' && *b != '\0'; a++, b++) {
        char x = *a, y = *b;

        if (x >= 'A' && x <= 'Z') {
            x = (char)(x - 'A' + 'a');
        }
        if (y >= 'A' && y <= 'Z') {
            y = (char)(y - 'A' + 'a');
        }
        if (x != y) {
            return 0;
        }
    }
    return *a == *b;
}

Dw3Region dw3_region_parse(const char *s) {
    if (s == NULL) {
        return DW3_REGION_NONE;
    }
    if (ieq(s, "us")) {
        return DW3_REGION_US;
    }
    if (ieq(s, "eu")) {
        return DW3_REGION_EU;
    }
    return DW3_REGION_NONE;
}

const char *dw3_region_name(Dw3Region r) {
    switch (r) {
    case DW3_REGION_US:
        return "us";
    case DW3_REGION_EU:
        return "eu";
    default:
        return "none";
    }
}

const char *dw3_region_dir(Dw3Region r) {
    return r == DW3_REGION_NONE ? NULL : dw3_region_name(r);
}

static int is_file(const char *path) {
    struct stat st;

    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int dw3_region_runtime_complete(const char *root, Dw3Region r) {
    char path[DW3_OPTCFG_PATH_MAX];
    const char *exe;

    if (root == NULL || r == DW3_REGION_NONE) {
        return 0;
    }
    exe = r == DW3_REGION_US ? "SLUS_014.36" : "SLES_039.36";
    if (snprintf(path, sizeof(path), "%s/files_%s.json", root,
                 dw3_region_name(r)) >= (int)sizeof(path) ||
        !is_file(path)) {
        return 0;
    }
    if (snprintf(path, sizeof(path), "%s/%s", root, exe) >=
            (int)sizeof(path) ||
        !is_file(path)) {
        return 0;
    }
    return 1;
}

void dw3_region_probe(const char *base, Dw3RegionProbe *out) {
    char root[DW3_OPTCFG_PATH_MAX];

    out->has_us = 0;
    out->has_eu = 0;
    if (base == NULL) {
        return;
    }
    if (snprintf(root, sizeof(root), "%s/us", base) < (int)sizeof(root)) {
        out->has_us = dw3_region_runtime_complete(root, DW3_REGION_US);
    }
    if (snprintf(root, sizeof(root), "%s/eu", base) < (int)sizeof(root)) {
        out->has_eu = dw3_region_runtime_complete(root, DW3_REGION_EU);
    }
}

Dw3BootDecision dw3_region_decide(const Dw3RegionInput *in) {
    Dw3BootDecision d;

    d.action = DW3_BOOT_NO_DATA;
    d.region = DW3_REGION_NONE;
    d.why = "no game data found";
    if (in->forced != DW3_REGION_NONE) {
        d.action = DW3_BOOT_REGION;
        d.region = in->forced;
        d.why = "region forced";
        return d;
    }
    if (in->probe.has_us && !in->probe.has_eu) {
        d.action = DW3_BOOT_REGION;
        d.region = DW3_REGION_US;
        d.why = "only the US data is present";
        return d;
    }
    if (in->probe.has_eu && !in->probe.has_us) {
        d.action = DW3_BOOT_REGION;
        d.region = DW3_REGION_EU;
        d.why = "only the EU data is present";
        return d;
    }
    if (!in->probe.has_us && !in->probe.has_eu) {
        return d;
    }
    /* Both present. */
    if (in->minus_held) {
        d.action = DW3_BOOT_PICK;
        d.region = in->last != DW3_REGION_NONE ? in->last : DW3_REGION_US;
        d.why = "Minus held: picker";
        return d;
    }
    if (in->pref != DW3_REGION_NONE) {
        d.action = DW3_BOOT_REGION;
        d.region = in->pref;
        d.why = "region_default";
        return d;
    }
    d.action = DW3_BOOT_PICK;
    d.region = in->last != DW3_REGION_NONE ? in->last : DW3_REGION_US;
    d.why = "both present: picker";
    return d;
}

void dw3_picker_init(Dw3Picker *p, Dw3Region default_sel) {
    memset(p, 0, sizeof(*p));
    p->sel = default_sel == DW3_REGION_EU ? DW3_REGION_EU : DW3_REGION_US;
    p->state = DW3_PICKER_RUNNING;
}

Dw3PickerState dw3_picker_step(Dw3Picker *p, unsigned pressed, unsigned dt_ms) {
    if (p->state != DW3_PICKER_RUNNING) {
        return p->state;
    }
    if (pressed & DW3_PK_PLUS) {
        p->state = DW3_PICKER_QUIT;
        return p->state;
    }
    if (pressed & (DW3_PK_LEFT | DW3_PK_RIGHT)) {
        p->cancelled = 1;
        if (pressed & DW3_PK_LEFT) {
            p->sel = DW3_REGION_US;
        }
        if (pressed & DW3_PK_RIGHT) {
            p->sel = DW3_REGION_EU;
        }
    }
    if (pressed & DW3_PK_Y) {
        p->cancelled = 1;
        p->always = !p->always;
    }
    if (pressed & DW3_PK_A) {
        p->chosen = p->sel;
        p->state = DW3_PICKER_CHOSEN;
        return p->state;
    }
    if (!p->cancelled) {
        p->elapsed_ms += dt_ms;
        if (p->elapsed_ms >= DW3_PICKER_TIMEOUT_MS) {
            p->chosen = p->sel;
            p->timed_out = 1;
            p->state = DW3_PICKER_CHOSEN;
        }
    }
    return p->state;
}

int dw3_picker_seconds_left(const Dw3Picker *p) {
    unsigned left;

    if (p->cancelled) {
        return -1;
    }
    if (p->elapsed_ms >= DW3_PICKER_TIMEOUT_MS) {
        return 0;
    }
    left = DW3_PICKER_TIMEOUT_MS - p->elapsed_ms;
    return (int)((left + 999u) / 1000u);
}

void dw3_region_cfg_read(const char *dir, Dw3Region *pref, Dw3Region *last) {
    static Dw3OptCfg cfg;
    const char *v;

    *pref = DW3_REGION_NONE;
    *last = DW3_REGION_NONE;
    if (dir == NULL || dw3_optcfg_load(&cfg, dir) <= 0) {
        return;
    }
    v = dw3_optcfg_get(&cfg, DW3_REGION_KEY_DEFAULT);
    if (v != NULL) {
        *pref = dw3_region_parse(v); /* "ask" and junk both mean ask */
    }
    v = dw3_optcfg_get(&cfg, DW3_REGION_KEY_LAST);
    if (v != NULL) {
        *last = dw3_region_parse(v);
    }
}

int dw3_region_cfg_write(const char *dir, Dw3Region last, int set_pref,
                         Dw3Region pref_value) {
    static Dw3OptCfg cfg;

    if (dir == NULL) {
        return -1;
    }
    if (dw3_optcfg_load(&cfg, dir) == DW3_OPTCFG_LOAD_ERROR) {
        return -1; /* unreadable or foreign file: never overwrite it */
    }
    if (last != DW3_REGION_NONE &&
        dw3_optcfg_set(&cfg, DW3_REGION_KEY_LAST, dw3_region_name(last)) != 0) {
        return -1;
    }
    if (set_pref &&
        dw3_optcfg_set(&cfg, DW3_REGION_KEY_DEFAULT,
                       pref_value == DW3_REGION_NONE
                           ? "ask"
                           : dw3_region_name(pref_value)) != 0) {
        return -1;
    }
    return dw3_optcfg_save(&cfg, dir);
}
