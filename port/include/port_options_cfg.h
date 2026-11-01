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
#define DW3_OPTCFG_MAX_ENTRIES 64 /* raised from 48 with the Output filter row: 43 keys were in use, the budget test wants 6 spare */
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

/* Drop `key` (P6.12: button_map only exists while the layout is custom).
 * Returns 1 when it was there, 0 when not. */
int dw3_optcfg_remove(Dw3OptCfg *cfg, const char *key);

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

/* ---- D3 / P6.26: one global options file (docs/OPTIONS_REORG.md section 6.2) ----
 *
 * Every key lives in ONE port_options.cfg: `sdmc:/switch/dw3/port_options.cfg` on
 * the Switch, the top-level file beside the region folders on the PC. Builds up
 * to D2 kept one file per region (`us/port_options.cfg`, `eu/port_options.cfg`)
 * and a top-level file for `region_default` / `last_region`. */

/* The one-shot marker, written by every save of the global file. */
#define DW3_OPTCFG_MARKER_KEY "options_global"

/* The folder of the global file. `cfgdir_env` (DW3_CONFIG_DIR / --config-dir)
 * wins when set. Otherwise `root` is the region's runtime folder
 * (`<base>/us`, `<base>/eu`): the global folder is its parent. A root that is
 * not a us / eu folder (a hand-made runtime) is its own global folder. Returns
 * 0, or -1 if `out` is too small. */
int dw3_optcfg_global_dir(const char *cfgdir_env, const char *root, char *out,
                          size_t cap);

/* One log line, no newline; warn != 0 for a failure. NULL = stderr. */
typedef void (*Dw3OptCfgLogFn)(void *ud, int warn, const char *line);

enum { /* result of dw3_optcfg_migrate_global */
    DW3_OPTGLOBAL_FAILED = -1, /* old files kept, retried at the next start */
    DW3_OPTGLOBAL_NONE = 0,    /* nothing to do (marker present, or no old files) */
    DW3_OPTGLOBAL_DONE = 1     /* merged, written, read back; old files backed up
                                * and deleted */
};

/* The one-shot migration into `<gdir>/port_options.cfg`:
 *   - merges `<gdir>/us/port_options.cfg` and `<gdir>/eu/port_options.cfg` per
 *     key: the file of the region named by `last_region` (read from the global
 *     file; US when it is missing or invalid) wins a conflict, a key in one file
 *     only is taken from it, every conflict is logged;
 *   - `defaults_gen` (D2) takes the older generation, so the D2 migration still
 *     runs on the merged file;
 *   - writes the global file with the marker `options_global=1`, reads it back
 *     and compares it;
 *   - only then copies each old file to `port_options.cfg.us.bak` /
 *     `.eu.bak` beside the global file (verified; an existing different .bak is
 *     never overwritten, the copy goes to .bak.1 ...) and deletes the old file.
 * A failure at any step before the delete deletes nothing, puts the previous
 * global file back, and returns FAILED. Safe to call at every start: with the
 * marker present it only reads the global file. */
int dw3_optcfg_migrate_global(const char *gdir, Dw3OptCfgLogFn log, void *ud);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_OPTIONS_CFG_H */
