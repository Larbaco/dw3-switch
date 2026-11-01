/* P6.7 / P6.6 text language (docs/LOCALIZATION.md). The Options row `Text language`
 * (registry row DW3_PORT_OPT_TEXT_LANGUAGE, key text_language) picks Original (the
 * game's own text) or a local text pack (Portuguese, BR). The pack is built on the
 * owner's PC from the owner's own disc and translation, lives beside the runtime
 * (<runtime>/l10n/ptbr/) and is never in git or in a release. port/l10n/ holds the
 * loader; this header is what the options registry and the hosts see. */
#ifndef DW3_PORT_L10N_H
#define DW3_PORT_L10N_H

#ifdef __cplusplus
extern "C" {
#endif

/* The values of the Text language row (stored order; the cfg holds the words
 * original | ptbr). A later language is appended (P6.8). */
enum {
    DW3_TEXTLANG_ORIGINAL = 0,
    DW3_TEXTLANG_PTBR = 1,
    DW3_TEXTLANG_COUNT = 2
};

/* The cfg word of a value ("original", "ptbr"; also the folder name of the pack). */
static inline const char *dw3_textlang_word(int level) {
    return level == DW3_TEXTLANG_PTBR ? "ptbr" : "original";
}

/* The value of a cfg word / DW3_LANG value, or -1. "en" and "off" mean Original. */
static inline int dw3_textlang_parse(const char *word) {
    if (word == 0) {
        return -1;
    }
    if (word[0] == 'p' && word[1] == 't' && word[2] == 'b' && word[3] == 'r' && word[4] == 0) {
        return DW3_TEXTLANG_PTBR;
    }
    if ((word[0] == 'o' && word[1] == 'r' && word[2] == 'i' && word[3] == 'g' && word[4] == 'i' &&
         word[5] == 'n' && word[6] == 'a' && word[7] == 'l' && word[8] == 0) ||
        (word[0] == 'e' && word[1] == 'n' && word[2] == 0) ||
        (word[0] == 'o' && word[1] == 'f' && word[2] == 'f' && word[3] == 0)) {
        return DW3_TEXTLANG_ORIGINAL;
    }
    return -1;
}

/* Boot, once, after port_options.cfg and DW3_LANG are known (dw3_port_options_persist_init):
 * finds and judges the pack under the cdfs root (always, so the Options row can say why a
 * pack is not usable), and applies it when `level` is DW3_TEXTLANG_PTBR. Logs as in
 * docs/LOCALIZATION.md section 3.6. */
void dw3_l10n_boot(int level);

/* The row changed (Options screen or API). Applies or restores the pack; the engine reads the
 * text files again from the next load. Files already cached are dropped at the title and kept
 * (their old language) in game, where a speech task or a talk box may still hold the table. */
void dw3_l10n_option_changed(int level);

/* Sample how many bytes of replaced text files the file cache holds (called from the cache's
 * slot policy on every request); the peak is logged at exit as `heap high water text=`. */
void dw3_l10n_sample_text(void);

/* 1 while a usable pack is installed for this region (hashes and sizes checked at boot). */
int dw3_l10n_pack_available(void);
/* 1 while the pack's files are the ones the engine reads. */
int dw3_l10n_active(void);
/* Why there is no usable pack: "no_pack", "bad_pack", "region_mismatch", "short_file",
 * "hash_mismatch" or "bad_id"; "" when there is one. */
const char *dw3_l10n_off_reason(void);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_L10N_H */
