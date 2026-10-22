/* P9.4 option file: parse, serialize and atomic write of
 * <config dir>/port_options.cfg.
 *
 * Platform independent (stdio only) so the PC and Switch hosts share it and
 * tools/tests can build it with a plain host compiler. It knows nothing about
 * which options exist: it is a small ordered key=value store. The registry
 * (port/options/port_options.c) owns the keys, ranges and defaults.
 *
 * Format (docs/PORT_OPTIONS.md, "Persistence"):
 *   # comment
 *   version=1
 *   widescreen=1
 *   ...
 *   end=1
 * One `key=value` per line, '#' or ';' starts a comment line, blank lines
 * are ignored. Unknown keys are kept and written back unchanged. `end=1`
 * is the last line of every file this code writes; it lets a reader tell a
 * complete file from a torn write. */
#ifndef DW3_PORT_OPTIONS_CFG_H
#define DW3_PORT_OPTIONS_CFG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DW3_OPTCFG_FILE "port_options.cfg"
#define DW3_OPTCFG_VERSION 1
#define DW3_OPTCFG_MAX_ENTRIES 48
#define DW3_OPTCFG_KEY_MAX 32
#define DW3_OPTCFG_VAL_MAX 48
/* Larger files are not ours (or are damaged): ignored, not parsed. */
#define DW3_OPTCFG_TEXT_MAX 4096
#define DW3_OPTCFG_PATH_MAX 512

typedef struct Dw3OptCfgEntry {
    char key[DW3_OPTCFG_KEY_MAX];
    char val[DW3_OPTCFG_VAL_MAX];
} Dw3OptCfgEntry;

typedef struct Dw3OptCfg {
    int version;  /* `version=` of the parsed file; DW3_OPTCFG_VERSION if none */
    int complete; /* the parsed text ended with `end=1` */
    int skipped;  /* lines ignored: malformed, too long, or no room left */
    int count;
    Dw3OptCfgEntry entry[DW3_OPTCFG_MAX_ENTRIES];
} Dw3OptCfg;

/* Result of dw3_optcfg_load. */
enum {
    DW3_OPTCFG_LOAD_ERROR = -1,    /* unreadable, or larger than the limit */
    DW3_OPTCFG_LOAD_MISSING = 0,   /* no file: cfg left empty */
    DW3_OPTCFG_LOAD_OK = 1,        /* port_options.cfg */
    DW3_OPTCFG_LOAD_RECOVERED = 2  /* complete port_options.cfg.tmp (a write
                                    * died between unlink and rename) */
};

void dw3_optcfg_init(Dw3OptCfg *cfg);

/* Parse `text` (not necessarily NUL terminated) into an empty cfg. Never
 * fails: bad lines are counted in cfg->skipped. Later duplicates win. */
void dw3_optcfg_parse(Dw3OptCfg *cfg, const char *text, size_t len);

/* Value of `key`, or NULL. */
const char *dw3_optcfg_get(const Dw3OptCfg *cfg, const char *key);

/* Add or replace `key`. Returns 0, or -1 if the key or value is invalid or
 * the table is full. Replacing keeps the entry's position. */
int dw3_optcfg_set(Dw3OptCfg *cfg, const char *key, const char *val);

/* Strict decimal integer ("-3", "12"); no spaces, no suffix, fits an int.
 * Returns 0 and sets *out, or -1. */
int dw3_optcfg_parse_int(const char *val, int *out);

/* Write the file text to `buf`. Returns the length (without a NUL, which is
 * also stored), or 0 if `cap` is too small. */
size_t dw3_optcfg_serialize(const Dw3OptCfg *cfg, char *buf, size_t cap);

/* `<dir>/port_options.cfg` into `out`; -1 if it does not fit. */
int dw3_optcfg_path(const char *dir, const char *suffix, char *out,
                    size_t cap);

/* Read the file in `dir` into `cfg` (initialised here). */
int dw3_optcfg_load(Dw3OptCfg *cfg, const char *dir);

/* Atomic write: `<file>.tmp` is written and closed, the old file is removed
 * (FAT/exFAT on the Switch SD, and Windows, refuse to rename over an
 * existing file), then the temp file is renamed into place. A failure
 * before the remove leaves the old file untouched. Returns 0, or -1 with
 * errno set. */
int dw3_optcfg_save(const Dw3OptCfg *cfg, const char *dir);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_OPTIONS_CFG_H */
