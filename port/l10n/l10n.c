/* P6.7 phase 1: the text pack, bound to the game (docs/LOCALIZATION.md section 3.3).
 *
 * The engine reads a text file as FILE_CACHE.load(id): it sizes a heap block from
 * FILE_SECTOR_COUNTS[id] and the host fills it through cdfs_read(id) (patch 0009, CD_READER
 * on the host reads by file id, not by sector). So pointing an id at a pack file needs two
 * things and no engine patch: a cdfs override (port/cdfs) and the sector arrays
 * (port/l10n/l10n_core.c). The English slot of the disc is the one overridden, so the
 * title's banks, the sprite tables and the movie choice, all indexed by LANGUAGE, keep
 * working unchanged. */
#include "port_l10n.h"

#include "cdfs.h"
#include "l10n_core.h"
#include "port_options.h"
#include "engine/file.h"

#include <psyz/log.h>
#include <stdio.h>
#include <string.h>

/* not <stdlib.h>: engine/game.h declares its own random() and atoi() */
extern int atexit(void (*fn)(void));
extern char *getenv(const char *name);

extern int FILE_SECTORS[];
extern unsigned short FILE_SECTOR_COUNTS[];
extern int FILE_COUNT;

#if defined(VERSION_EU) && VERSION_EU
#define L10N_REGION "eu"
#else
#define L10N_REGION "us"
#endif

static Dw3L10n g_l10n;
static int g_boot;
static int g_level;
static unsigned g_text_peak;  /* most bytes of pack-id text files the cache has held at once */
static unsigned g_text_now;
static unsigned g_text_peak_en; /* the same files at their original size, at that moment */

static void l10n_sink(const char *line) {
    INFOF("%s", line);
}

/* Bytes the file cache holds now in files the pack replaces, and its peak. */
static void text_sample(void) {
    unsigned bytes = 0, english = 0;
    int i;

    if (!g_boot || !g_l10n.pack_ok) {
        return;
    }
    for (i = 0; i < FILE_CACHE_SLOTS; i++) {
        const FileSlot *s = &FILE_CACHE.slots[i];
        int sectors;

        if (s->file != 0 && s->data != NULL && (sectors = dw3_l10n_core_original_sectors(&g_l10n, s->file)) > 0) {
            bytes += (unsigned)FILE_TABLE.getSectorCount(s->file) * 2048u;
            english += (unsigned)sectors * 2048u;
        }
    }
    g_text_now = bytes;
    if (bytes > g_text_peak) {
        g_text_peak = bytes;
        g_text_peak_en = english;
    }
}

/* Called from the file cache's slot policy (port/file_cache/file_cache_host.c) on every request. */
void dw3_l10n_sample_text(void) {
    text_sample();
}

static void l10n_atexit(void) {
    text_sample();
    if (g_l10n.pack_ok) {
        INFOF("dw3-l10n: heap high water text=%u (the same files in English %u, %+d bytes; cached at exit %u, active=%d)",
              g_text_peak, g_text_peak_en, (int)g_text_peak - (int)g_text_peak_en, g_text_now, g_l10n.applied);
    }
}

/* Drop the cached text files that are loaded: safe at the title, where no speech task or talk box exists. */
static int evict_loaded(void) {
    int i, n = 0;

    for (i = 0; i < FILE_CACHE_SLOTS; i++) {
        const FileSlot *s = &FILE_CACHE.slots[i];

        if (s->file != 0 && s->state == FILE_LOADED && dw3_l10n_core_has_id(&g_l10n, s->file)) {
            FILE_CACHE.free(s->file);
            n++;
        }
    }
    return n;
}

static void apply(int level) {
    g_level = level;
    if (level == DW3_TEXTLANG_PTBR) {
        if (g_l10n.pack_ok) {
            INFOF("dw3-l10n: option text_language=ptbr region=%s", L10N_REGION);
            (void)dw3_l10n_core_apply(&g_l10n);
        } else {
            INFOF("dw3-l10n: option text_language=ptbr region=%s but off reason=%s (English)", L10N_REGION,
                  g_l10n.reason);
        }
    } else {
        if (g_l10n.applied) {
            INFOF("dw3-l10n: restored %d file(s) to the original text", dw3_l10n_core_restore(&g_l10n));
        }
        if (g_l10n.pack_ok) {
            INFOF("dw3-l10n: off reason=option_original");
        }
    }
}

void dw3_l10n_boot(int level) {
    const char *root = cdfs_root();

    if (!g_boot) {
        dw3_l10n_set_log(l10n_sink);
        dw3_l10n_core_bind(&g_l10n, FILE_SECTORS, FILE_SECTOR_COUNTS, FILE_COUNT, "ptbr", L10N_REGION);
        atexit(l10n_atexit);
        g_boot = 1;
    }
    if (root == NULL || root[0] == '\0') {
        INFOF("dw3-l10n: off reason=no_pack (no runtime folder)");
        snprintf(g_l10n.reason, sizeof(g_l10n.reason), "no_pack");
        g_l10n.pack_ok = 0;
        return;
    }
    {
        /* DW3_L10N_SHOW_KEYS=1: the pack's key variants (build_pack.py --keys), every string
         * starts with its key "(TALK03:412) ", so a line that looks wrong is found by sight. */
        const char *keys = getenv("DW3_L10N_SHOW_KEYS");

        dw3_l10n_core_set_keys(&g_l10n, keys != NULL && keys[0] == '1');
    }
    (void)dw3_l10n_core_load(&g_l10n, root);
    apply(level);
}

void dw3_l10n_option_changed(int level) {
    int dropped = 0;

    if (!g_boot || level == g_level) {
        return;
    }
    apply(level);
    if (g_l10n.pack_ok && !dw3_port_options_in_game()) {
        dropped = evict_loaded();
        INFOF("dw3-l10n: %d cached text file(s) dropped, the next load reads the %s text", dropped,
              g_l10n.applied ? "pack" : "original");
    } else if (g_l10n.pack_ok) {
        INFOF("dw3-l10n: in game: cached text files keep their language until the game loads them again");
    }
}

int dw3_l10n_pack_available(void) {
    return g_boot && g_l10n.pack_ok;
}

int dw3_l10n_active(void) {
    return g_boot && g_l10n.applied;
}

const char *dw3_l10n_off_reason(void) {
    return g_boot && !g_l10n.pack_ok ? g_l10n.reason : "";
}
