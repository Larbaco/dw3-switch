/* P6.15 replay recording parser. See port/include/dw3_replay.h. */
#include "dw3_replay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int ev_push(Dw3ReplayEv **arr, size_t *n, size_t *cap, unsigned frame,
                   unsigned mask, const char *name) {
    Dw3ReplayEv *e;

    if (*n == *cap) {
        size_t ncap = *cap ? *cap * 2 : 256;
        Dw3ReplayEv *grown = (Dw3ReplayEv *)realloc(*arr, ncap * sizeof(**arr));

        if (grown == NULL) {
            return -1;
        }
        *arr = grown;
        *cap = ncap;
    }
    e = &(*arr)[(*n)++];
    e->frame = frame;
    e->mask = mask;
    e->name[0] = '\0';
    if (name != NULL) {
        strncpy(e->name, name, DW3_REPLAY_NAME_MAX - 1);
        e->name[DW3_REPLAY_NAME_MAX - 1] = '\0';
    }
    return 0;
}

static int ev_cmp(const void *a, const void *b) {
    const Dw3ReplayEv *x = (const Dw3ReplayEv *)a;
    const Dw3ReplayEv *y = (const Dw3ReplayEv *)b;

    return (x->frame > y->frame) - (x->frame < y->frame);
}

/* qsort is not stable; keep file order for equal frames. */
static void ev_sort_stable(Dw3ReplayEv *arr, size_t n) {
    size_t i;

    for (i = 1; i < n; i++) {
        Dw3ReplayEv tmp = arr[i];
        size_t j = i;

        while (j > 0 && ev_cmp(&arr[j - 1], &tmp) > 0) {
            arr[j] = arr[j - 1];
            j--;
        }
        arr[j] = tmp;
    }
}

static int parse_line(Dw3Replay *r, const char *line) {
    unsigned f, m;
    char name[DW3_REPLAY_NAME_MAX];
    char fmt[32];

    if (sscanf(line, " pad %u %x", &f, &m) == 2) {
        return ev_push(&r->pad, &r->npad, &r->cpad, f, m, NULL) == 0 ? 1 : -1;
    }
    snprintf(fmt, sizeof(fmt), " shot %%u %%%ds", DW3_REPLAY_NAME_MAX - 1);
    if (sscanf(line, fmt, &f, name) == 2) {
        return ev_push(&r->shot, &r->nshot, &r->cshot, f, 0, name) == 0 ? 1 : -1;
    }
    if (sscanf(line, " end %u", &f) == 1) {
        r->end = f;
        return 1;
    }
    return 0;
}

int dw3_replay_parse(Dw3Replay *r, const char *text, size_t len, int *bad) {
    size_t i = 0;
    int nbad = 0;

    memset(r, 0, sizeof(*r));
    while (i < len) {
        char line[192];
        size_t n = 0;
        const char *hash;
        int rc;

        while (i < len && text[i] != '\n') {
            if (n + 1 < sizeof(line)) {
                line[n++] = text[i];
            }
            i++;
        }
        i++; /* newline */
        line[n] = '\0';
        hash = strchr(line, '#');
        if (hash != NULL) {
            line[hash - line] = '\0';
        }
        {
            const char *p = line;

            while (*p == ' ' || *p == '\t' || *p == '\r') {
                p++;
            }
            if (*p == '\0') {
                continue;
            }
        }
        rc = parse_line(r, line);
        if (rc < 0) {
            dw3_replay_free(r);
            return -1;
        }
        if (rc == 0) {
            nbad++;
        }
    }
    ev_sort_stable(r->pad, r->npad);
    ev_sort_stable(r->shot, r->nshot);
    if (bad != NULL) {
        *bad = nbad;
    }
    return 0;
}

int dw3_replay_load(Dw3Replay *r, const char *path, int *bad) {
    FILE *fp = fopen(path, "rb");
    char *buf = NULL;
    size_t len = 0, cap = 0;
    int rc;

    if (fp == NULL) {
        return -1;
    }
    for (;;) {
        size_t got;

        if (len + 4096 > cap) {
            char *grown;

            cap = cap ? cap * 2 : 65536;
            grown = (char *)realloc(buf, cap);
            if (grown == NULL) {
                free(buf);
                fclose(fp);
                return -1;
            }
            buf = grown;
        }
        got = fread(buf + len, 1, cap - len, fp);
        if (got == 0) {
            break;
        }
        len += got;
    }
    fclose(fp);
    rc = dw3_replay_parse(r, buf != NULL ? buf : "", len, bad);
    free(buf);
    return rc;
}

int dw3_replay_advance(Dw3Replay *r, unsigned frame, Dw3ReplayShotFn shot_fn,
                       void *ctx, unsigned *mask) {
    while (r->shot_i < r->nshot && r->shot[r->shot_i].frame <= frame) {
        if (shot_fn != NULL) {
            shot_fn(r->shot[r->shot_i].name, frame, ctx);
        }
        r->shot_i++;
    }
    if (r->end != 0 && frame >= r->end) {
        if (mask != NULL) {
            *mask = r->mask;
        }
        return 1;
    }
    while (r->pad_i < r->npad && r->pad[r->pad_i].frame <= frame) {
        r->mask = r->pad[r->pad_i++].mask;
    }
    if (mask != NULL) {
        *mask = r->mask;
    }
    return 0;
}

void dw3_replay_free(Dw3Replay *r) {
    free(r->pad);
    free(r->shot);
    memset(r, 0, sizeof(*r));
}
