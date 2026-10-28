/* P6.24: the boot flow of the combined image; see dw3_region_boot.h. */
#include "dw3_region_boot.h"

#include <stdio.h>
#include <string.h>

static const char *pref_name(Dw3Region r) { return r == DW3_REGION_NONE ? "ask" : dw3_region_name(r); }

void dw3_region_boot(const Dw3RegionBootIn *in, const Dw3RegionBootIO *io,
                     Dw3RegionBootOut *out) {
    Dw3RegionInput ri;
    Dw3BootDecision d;
    Dw3RegionProbe tree;
    char line[320];
    char note[96];
    const char *minus_txt = "n/a";
    int disc_us = 0, disc_eu = 0;
    Dw3Region region;
    const char *how;
    int always = 0;

    memset(out, 0, sizeof(*out));
    memset(&ri, 0, sizeof(ri));
    out->region = DW3_REGION_NONE;

    if (in->forced != DW3_REGION_NONE) {
        snprintf(line, sizeof(line),
                 "dw3-region: forced %s by %s: no probe, no picker, nothing written",
                 dw3_region_name(in->forced), in->forced_by != NULL ? in->forced_by : "?");
        io->log(io->ud, line);
        out->result = DW3_RB_RUN;
        out->region = in->forced;
        out->source = "forced";
        return;
    }

    dw3_region_probe(in->base, &tree);
    ri.probe = tree;
    if (io->disc_waiting != NULL) {
        if (!tree.has_us && io->disc_waiting(io->ud, in->base, DW3_REGION_US)) {
            disc_us = 1;
            ri.probe.has_us = 1;
        }
        if (!tree.has_eu && io->disc_waiting(io->ud, in->base, DW3_REGION_EU)) {
            disc_eu = 1;
            ri.probe.has_eu = 1;
        }
    }
    note[0] = '\0';
    if (disc_us || disc_eu) {
        snprintf(note, sizeof(note), " (%s%s%s: disc image, needs extraction)",
                 disc_us ? "us" : "", disc_us && disc_eu ? ", " : "", disc_eu ? "eu" : "");
    }
    snprintf(line, sizeof(line), "dw3-region: data found us=%d eu=%d under %s%s",
             ri.probe.has_us, ri.probe.has_eu, in->base, note);
    io->log(io->ud, line);

    dw3_region_cfg_read(in->base, &ri.pref, &ri.last);
    /* The hold-Minus sample costs about 200 ms, so take it only when it can
     * change the answer: both present and a region stored (without one the
     * picker appears anyway). */
    if (ri.probe.has_us && ri.probe.has_eu && ri.pref != DW3_REGION_NONE &&
        io->sample_minus != NULL) {
        ri.minus_held = io->sample_minus(io->ud) ? 1 : 0;
        minus_txt = ri.minus_held ? "1" : "0";
    }
    snprintf(line, sizeof(line),
             "dw3-region: stored region_default=%s last_region=%s minus_held=%s",
             pref_name(ri.pref),
             ri.last == DW3_REGION_NONE ? "none" : dw3_region_name(ri.last), minus_txt);
    io->log(io->ud, line);

    d = dw3_region_decide(&ri);
    snprintf(line, sizeof(line), "dw3-region: decision %s region=%s: %s",
             d.action == DW3_BOOT_NO_DATA ? "no-data" : d.action == DW3_BOOT_PICK ? "pick" : "boot",
             dw3_region_name(d.region), d.why);
    io->log(io->ud, line);

    if (d.action == DW3_BOOT_NO_DATA) {
        if (io->show_no_data != NULL) {
            io->show_no_data(io->ud, in->base);
        }
        out->result = DW3_RB_NO_DATA;
        out->source = d.why;
        return;
    }

    region = d.region;
    how = d.why;
    if (d.action == DW3_BOOT_PICK) {
        Dw3Picker p;
        unsigned ms;

        dw3_picker_init(&p, d.region);
        snprintf(line, sizeof(line), "dw3-region: picker shown (default entry %s, timeout %d ms)",
                 dw3_region_name(p.sel), DW3_PICKER_TIMEOUT_MS);
        io->log(io->ud, line);
        ms = io->run_picker != NULL ? io->run_picker(io->ud, &p) : 0;
        if (p.state != DW3_PICKER_CHOSEN) {
            snprintf(line, sizeof(line), "dw3-region: picker quit by + after %u ms", ms);
            io->log(io->ud, line);
            out->result = DW3_RB_QUIT;
            out->source = "picker (+)";
            return;
        }
        region = p.chosen;
        always = p.always;
        how = p.timed_out ? "picker (countdown)" : "picker (button A)";
        snprintf(line, sizeof(line),
                 "dw3-region: picker result %s by %s after %u ms always=%d",
                 dw3_region_name(region), p.timed_out ? "countdown" : "button A", ms, always);
        io->log(io->ud, line);
    }

    /* last_region every time a region boots; region_default only with Y. */
    if (dw3_region_cfg_write(in->base, region, always, region) == 0) {
        if (always) {
            snprintf(line, sizeof(line), "dw3-region: saved last_region=%s region_default=%s",
                     dw3_region_name(region), dw3_region_name(region));
        } else {
            snprintf(line, sizeof(line), "dw3-region: saved last_region=%s",
                     dw3_region_name(region));
        }
    } else {
        snprintf(line, sizeof(line), "dw3-region: could not store the choice (%s/port_options.cfg); "
                 "the game still starts", in->base);
    }
    io->log(io->ud, line);

    out->result = DW3_RB_RUN;
    out->region = region;
    out->source = how;
}
