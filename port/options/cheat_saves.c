/* P6.20: the "written with cheats" mark of a save, kept in a sidecar file
 * beside the flat save, never in the save (docs/PORT_OPTIONS.md, "Cheats",
 * "The cheat mark of a save"). Compiled only with DW3_CHEATS.
 *
 * <memcard dir>/save<N>.cheats, one per card (save<N>.bin), in the key=value
 * form of port_options.cfg:
 *
 *   # dw3-switch: slots of save0.bin written with cheats (docs/PORT_OPTIONS.md)
 *   version=1
 *   slot1=10368:9916:1a2b3c4d
 *   end=1
 *
 * slot<S>=<offset>:<size>:<hash>: the slot's byte offset in save<N>.bin, the
 * size of its game data and the FNV-1a 32 hash of those bytes when they were
 * written. A mark counts only while the slot still holds those bytes, so a
 * slot that a build without cheats (or a copied save<N>.bin) replaced is not
 * reported. A clean save over a marked slot removes the mark; the file is
 * deleted when no mark is left. Written with the rename-atomic rule of the
 * flat saves and the options file. */
#include "port_cheats.h"
#include "port_options_cfg.h"
#include "cheats_internal.h"
#include "memcard_host.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { CS_PORTS = MEMCARD_HOST_SLOTS, CS_SLOTS = 3 };

static char g_dir[DW3_OPTCFG_PATH_MAX];

void dw3_cheat_saves_set_dir(const char *memdir) {
    size_t n;

    g_dir[0] = '\0';
    if (memdir == NULL || memdir[0] == '\0') {
        return;
    }
    n = strlen(memdir);
    if (n + 16 >= sizeof(g_dir)) {
        CHEAT_WARN("dw3-cheats: memcard dir too long; saves are not marked");
        return;
    }
    memcpy(g_dir, memdir, n + 1);
    while (n > 1 && (g_dir[n - 1] == '/' || g_dir[n - 1] == '\\')) {
        g_dir[--n] = '\0';
    }
}

unsigned dw3_cheat_saves_hash(const void *data, int size) {
    const unsigned char *p = (const unsigned char *)data;
    unsigned h = 2166136261u;
    int i;

    for (i = 0; i < size; i++) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

static int cs_path(int port, const char *suffix, char *out, size_t cap) {
    int n = snprintf(out, cap, "%s/save%d.cheats%s", g_dir, port, suffix);

    return (n < 0 || (size_t)n >= cap) ? -1 : 0;
}

/* The sidecar of a card into `cfg` (empty when there is none). */
static void cs_load(int port, Dw3OptCfg *cfg) {
    char path[DW3_OPTCFG_PATH_MAX];
    char text[DW3_OPTCFG_TEXT_MAX];
    FILE *fp;
    size_t n;

    dw3_optcfg_init(cfg);
    if (cs_path(port, "", path, sizeof(path)) != 0) {
        return;
    }
    fp = fopen(path, "rb");
    if (fp == NULL) {
        /* P10.2: a write killed between the remove and the rename leaves a
         * complete .tmp (it ends with `end=1`); a torn one is ignored. */
        char tmp[DW3_OPTCFG_PATH_MAX];

        if (cs_path(port, ".tmp", tmp, sizeof(tmp)) != 0 ||
            (fp = fopen(tmp, "rb")) == NULL) {
            return;
        }
        n = fread(text, 1, sizeof(text), fp);
        fclose(fp);
        if (n >= sizeof(text)) {
            return;
        }
        dw3_optcfg_parse(cfg, text, n);
        if (!cfg->complete) {
            dw3_optcfg_init(cfg);
            return;
        }
        if (rename(tmp, path) == 0) {
            CHEAT_WARN("dw3-cheats: %s was missing; recovered %s", path, tmp);
        }
        return;
    }
    n = fread(text, 1, sizeof(text), fp);
    fclose(fp);
    if (n >= sizeof(text)) {
        CHEAT_WARN("dw3-cheats: %s is too large; ignored", path);
        return;
    }
    dw3_optcfg_parse(cfg, text, n);
}

/* Write the sidecar (or remove it when it holds no mark). */
static int cs_store(int port, const Dw3OptCfg *cfg) {
    char path[DW3_OPTCFG_PATH_MAX];
    char tmp[DW3_OPTCFG_PATH_MAX];
    FILE *fp;
    int ok;
    int i;

    if (cs_path(port, "", path, sizeof(path)) != 0 ||
        cs_path(port, ".tmp", tmp, sizeof(tmp)) != 0) {
        return -1;
    }
    if (cfg->count == 0) {
        return remove(path) == 0 || errno == ENOENT ? 0 : -1;
    }
    fp = fopen(tmp, "wb");
    if (fp == NULL) {
        return -1;
    }
    ok = fprintf(fp, "# dw3-switch: slots of save%d.bin written with cheats "
                     "(docs/PORT_OPTIONS.md)\nversion=1\n", port) > 0;
    for (i = 0; ok && i < cfg->count; i++) {
        ok = fprintf(fp, "%s=%s\n", cfg->entry[i].key, cfg->entry[i].val) > 0;
    }
    ok = ok && fprintf(fp, "end=1\n") > 0 && fflush(fp) == 0;
    if (fclose(fp) != 0) {
        ok = 0;
    }
    if (!ok) {
        remove(tmp);
        return -1;
    }
    /* FAT/exFAT and Windows refuse to rename over an existing file. */
    if (remove(path) != 0 && errno != ENOENT) {
        remove(tmp);
        return -1;
    }
    return rename(tmp, path) == 0 ? 0 : -1;
}

/* A mark's three numbers; 0 when the value is not one. */
static int cs_parse(const char *val, unsigned *offset, unsigned *size,
                    unsigned *hash) {
    char tail;

    return val != NULL &&
           sscanf(val, "%u:%u:%x%c", offset, size, hash, &tail) == 3;
}

static int cs_ok(int port, int slot) {
    return g_dir[0] != '\0' && port >= 0 && port < CS_PORTS && slot >= 0 &&
           slot < CS_SLOTS;
}

int dw3_cheat_saves_mark(int port, int slot, int offset, const void *data,
                         int size, int marked) {
    Dw3OptCfg cfg;
    char key[8];
    char val[40];
    int had;

    if (!cs_ok(port, slot) || data == NULL || size <= 0 || offset < 0) {
        return 0;
    }
    cs_load(port, &cfg);
    snprintf(key, sizeof(key), "slot%d", slot);
    had = dw3_optcfg_get(&cfg, key) != NULL;
    if (!marked && !had) {
        return 0; /* a clean save over a clean slot: nothing to write */
    }
    if (marked) {
        snprintf(val, sizeof(val), "%d:%d:%08x", offset, size,
                 dw3_cheat_saves_hash(data, size));
        if (dw3_optcfg_set(&cfg, key, val) != 0) {
            CHEAT_WARN("dw3-cheats: cannot stage the mark of save%d.bin "
                       "slot %d", port, slot);
            return -1;
        }
    } else {
        /* dw3_optcfg has no delete: rebuild it without the key. */
        Dw3OptCfg kept;
        int i;

        dw3_optcfg_init(&kept);
        for (i = 0; i < cfg.count; i++) {
            if (strcmp(cfg.entry[i].key, key) != 0) {
                (void)dw3_optcfg_set(&kept, cfg.entry[i].key, cfg.entry[i].val);
            }
        }
        cfg = kept;
    }
    if (cs_store(port, &cfg) != 0) {
        CHEAT_WARN("dw3-cheats: cannot write %s/save%d.cheats (the mark of "
                   "slot %d is not saved)", g_dir, port, slot);
        return -1;
    }
    if (marked) {
        CHEAT_INFO("dw3-cheats: save%d.bin slot %d marked: written with "
                   "cheats (save%d.cheats)", port, slot, port);
    } else {
        CHEAT_INFO("dw3-cheats: save%d.bin slot %d: written without cheats, "
                   "mark removed", port, slot);
    }
    return 0;
}

int dw3_cheat_saves_is_marked(int port, int slot, const void *data, int size) {
    Dw3OptCfg cfg;
    char key[8];
    unsigned offset;
    unsigned msize;
    unsigned hash;

    if (!cs_ok(port, slot) || data == NULL || size <= 0) {
        return 0;
    }
    cs_load(port, &cfg);
    snprintf(key, sizeof(key), "slot%d", slot);
    return cs_parse(dw3_optcfg_get(&cfg, key), &offset, &msize, &hash) &&
           msize == (unsigned)size && hash == dw3_cheat_saves_hash(data, size);
}

int dw3_cheat_saves_count(int log) {
    static unsigned char buf[MEMCARD_FLAT_SIZE];
    int count = 0;
    int port;

    if (g_dir[0] == '\0') {
        return 0;
    }
    for (port = 0; port < CS_PORTS; port++) {
        Dw3OptCfg cfg;
        int slot;

        cs_load(port, &cfg);
        for (slot = 0; slot < CS_SLOTS; slot++) {
            char key[8];
            unsigned offset;
            unsigned size;
            unsigned hash;

            snprintf(key, sizeof(key), "slot%d", slot);
            if (!cs_parse(dw3_optcfg_get(&cfg, key), &offset, &size, &hash)) {
                continue;
            }
            if (size == 0 || size > MEMCARD_FLAT_SIZE ||
                memcard_host_file_read(port, offset, buf, size) < 0 ||
                dw3_cheat_saves_hash(buf, (int)size) != hash) {
                if (log) {
                    CHEAT_INFO("dw3-cheats: save%d.bin slot %d: the mark no "
                               "longer matches the slot (replaced); ignored",
                               port, slot);
                }
                continue;
            }
            count++;
            if (log) {
                CHEAT_INFO("dw3-cheats: save%d.bin slot %d was written with "
                           "cheats", port, slot);
            }
        }
    }
    return count;
}
