/* P6.35 walkthrough: the step resolver (docs/P6.35_WALKTHROUGH.md, section
 * 1.5). Pure C90, no game headers: the flag check is injected. */
#include "port_walkthrough.h"

#include <stdio.h>
#include <string.h>

static int conds_hold(const DW3WtState *st, const DW3WtCond *c, int n) {
    int i;

    for (i = 0; i < n; ++i) {
        if (!st->check || !st->check(st->ctx, c[i].code, c[i].value)) return 0;
    }
    return 1;
}

static int in_range(const DW3WtStep *s, int progress) {
    if (progress < s->from_progress) return 0;
    return s->to_progress < 0 || progress <= s->to_progress;
}

static int region_ok(const DW3WtStep *s, int region) {
    return s->region == WT_REGION_ALL || s->region == region;
}

static void set(DW3WtResult *out, int status, const DW3WtStep *step, int index,
                const char *rule) {
    out->status = status;
    out->step = step;
    out->index = index;
    out->rule = rule;
}

/* The MAIN rows that cover `progress` and apply to the region, in table order
 * (the table is ordered by from_progress, then order). The current step is the
 * first whose unlock holds and whose done condition does not; if every
 * offered row is done, the last offered one. */
void dw3_wt_resolve_table(const DW3WtStep *table, int count, const DW3WtState *st,
                          DW3WtResult *out) {
    int i, lo = 0x7FFFFFFF, hi = -1, any_range = 0, offered = 0;
    int first_open = -1, last_offered = -1, decided_by_cond = 0;

    set(out, WT_UNKNOWN, NULL, -1, "unknown");
    if (!table || count <= 0 || !st) return;
    if (st->progress < 0 || st->progress > WT_PROGRESS_MAX) return;

    for (i = 0; i < count; ++i) {
        const DW3WtStep *s = &table[i];

        if (s->kind != WT_MAIN || !region_ok(s, st->region)) continue;
        if (s->from_progress < lo) lo = s->from_progress;
        if (s->to_progress < 0) hi = WT_PROGRESS_MAX;
        else if (s->to_progress > hi) hi = s->to_progress;
        if (!in_range(s, st->progress)) continue;
        any_range = 1;
        if (!conds_hold(st, s->unlock, s->nunlock)) {
            if (s->nunlock) decided_by_cond = 1;
            continue;
        }
        ++offered;
        last_offered = i;
        if (first_open < 0 && !(s->ndone > 0 && conds_hold(st, s->done, s->ndone))) {
            first_open = i;
            if (s->nunlock || s->ndone) decided_by_cond = 1;
        } else if (first_open < 0) {
            decided_by_cond = 1;
        }
    }
    if (hi < 0) return; /* no MAIN row for this region at all */
    if (!any_range) {
        if (st->progress < lo) set(out, WT_BELOW, NULL, -1, "below-table");
        else if (st->progress > hi) set(out, WT_ABOVE, NULL, -1, "above-table");
        else set(out, WT_NO_MATCH, NULL, -1, "gap");
        return;
    }
    if (offered == 0) {
        set(out, WT_NO_MATCH, NULL, -1, "no-row-offered");
        return;
    }
    if (first_open >= 0) {
        set(out, WT_OK, &table[first_open], first_open,
            decided_by_cond ? "progress+flag" : "progress");
    } else {
        set(out, WT_OK, &table[last_offered], last_offered, "all-done");
    }
}

int dw3_wt_format_line(char *buf, unsigned size, const DW3WtState *st, const DW3WtResult *r) {
    if (r->status == WT_OK && r->step) {
        return snprintf(buf, size,
                        "dw3-walkthrough: progress %d half %d area %d -> chapter %s step %s (rule %s)",
                        st->progress, st->half, st->area, r->step->chapter, r->step->id, r->rule);
    }
    return snprintf(buf, size,
                    "dw3-walkthrough: progress %d half %d area %d -> chapter - step - (rule %s)",
                    st->progress, st->half, st->area, r->rule ? r->rule : "none");
}
