/* P6.7 phase 1: the text pack loader, the part that needs no game symbols
 * (docs/LOCALIZATION.md section 3.3). It reads <root>/l10n/<lang>/pack.json, checks
 * it against the runtime, and (only while asked) points the text file ids at the
 * pack: cdfs reads the pack file for that id (cdfs_override_set) and the two
 * file-table arrays the engine sizes its FILE_CACHE buffers from are rewritten
 * (FILE_SECTORS to a virtual LBA above the disc, FILE_SECTOR_COUNTS to the pack
 * file's sectors). Restoring puts every array entry and every read path back.
 *
 * port/l10n/l10n.c binds this to the game's arrays; tools/tests/l10n_main.c runs
 * it on synthetic packs. Nothing here knows a string of the game. */
#ifndef DW3_L10N_CORE_H
#define DW3_L10N_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    DW3_L10N_MAX_FILES = 64,        /* the EU disc has 50 text files */
    DW3_L10N_STEM_MAX = 16,
    DW3_L10N_KEYS_MAX = 32,         /* fallback keys listed in pack.json, per file */
    DW3_L10N_KEY_LEN = 24,
    DW3_L10N_VLBA_BASE = 0x01000000 /* above any CD (a 80 minute disc ends near 360000) */
};

typedef struct Dw3L10nFile {
    int id;
    char stem[DW3_L10N_STEM_MAX];
    uint32_t bytes;
    uint32_t sectors;
    uint32_t src_bytes;
    uint32_t keys_bytes;    /* the DW3_L10N_SHOW_KEYS variant (<id>.k.bin), 0 = none */
    uint32_t keys_sectors;
    char sha256[65];
    char src_sha256[65];
    int strings;
    int translated;
    int fallback;
    int nkeys;
    char keys[DW3_L10N_KEYS_MAX][DW3_L10N_KEY_LEN];
} Dw3L10nFile;

typedef struct Dw3L10nPack {
    char region[4];
    char language[16];
    char synthetic[16];
    int proof_chars;    /* the glyph proof pack: how many characters its strings carry */
    int count;
    int strings;
    int translated;
    int fallback;
    Dw3L10nFile files[DW3_L10N_MAX_FILES];
} Dw3L10nPack;

/* Parse the text of a pack.json. Returns 0, or -1 with `err` set. */
int dw3_l10n_pack_parse(const char *text, size_t len, Dw3L10nPack *out, char *err, size_t errcap);

/* Why the pack is not used ("" while it is, or before it is judged). */
typedef struct Dw3L10n {
    /* bound by dw3_l10n_core_bind */
    int *sectors;
    unsigned short *counts;
    int file_count;
    char lang[16];
    int ready;          /* bound */
    /* load */
    int loaded;         /* a load was tried */
    int pack_ok;        /* the pack passed every check */
    char reason[24];    /* no_pack | bad_pack | region_mismatch | short_file | hash_mismatch | bad_id */
    char root[512];
    char region[4];
    char src_hash[65];  /* sha256 of the sorted source hashes: the "src=" of the log line */
    Dw3L10nPack pack;
    /* apply */
    int want_keys;      /* DW3_L10N_SHOW_KEYS=1: read the key variants when the pack has them */
    int keys_ok;        /* every file has a key variant of the size pack.json says */
    int keys_on;        /* the key variants are the ones applied */
    int applied;
    int saved_sector[DW3_L10N_MAX_FILES];
    unsigned short saved_count[DW3_L10N_MAX_FILES];
    uint32_t vlba[DW3_L10N_MAX_FILES];
} Dw3L10n;

/* Log sink: one whole line (with its "dw3-l10n: " prefix), no newline. NULL = stderr. */
typedef void (*Dw3L10nLogFn)(const char *line);
void dw3_l10n_set_log(Dw3L10nLogFn fn);
/* Log one "dw3-l10n: ..." line through the sink. */
void dw3_l10n_say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Bind the two arrays of the file table (FILE_SECTORS, FILE_SECTOR_COUNTS), their
 * length, and the language folder name ("ptbr"). */
void dw3_l10n_core_bind(Dw3L10n *l, int *sectors, unsigned short *counts, int file_count,
                        const char *lang, const char *region);

/* Ask for the key variants of the pack files (<id>.k.bin, build_pack.py --keys: every string
 * starts with its key, "(TALK03:412) "), for the next apply. A pack without them logs it and
 * plays its normal text. */
void dw3_l10n_core_set_keys(Dw3L10n *l, int on);

/* Read and judge <root>/l10n/<lang>/pack.json against the runtime the cdfs map describes
 * (sizes of the pack files; sha256 of each English source file). Sets pack_ok and, when
 * not ok, reason. Logs `pack ok ...` or `off reason=...`. Safe to call again. Returns pack_ok. */
int dw3_l10n_core_load(Dw3L10n *l, const char *root);

/* Point the text ids at the pack. Needs pack_ok. Returns the number of files overridden, or -1. */
int dw3_l10n_core_apply(Dw3L10n *l);

/* Put every id, array entry and read path back. Returns the number restored. */
int dw3_l10n_core_restore(Dw3L10n *l);

/* The sectors the engine reads for file `id` with no pack (what FILE_SECTOR_COUNTS held before
 * apply), or 0 when `id` is not one of the pack's ids. Valid while the pack is judged ok. */
int dw3_l10n_core_original_sectors(const Dw3L10n *l, int id);

/* 1 when file `id` is one of the pack's ids. */
int dw3_l10n_core_has_id(const Dw3L10n *l, int id);

#ifdef __cplusplus
}
#endif

#endif /* DW3_L10N_CORE_H */
