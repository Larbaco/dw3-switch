/*
 * Strong overrides for PSY-Z symbols that misbehave on 64-bit host.
 * Linked as a TU (not only via the engine .a) so they win over libpsyz.a.
 */
#include <stddef.h>

/*
 * PSY-Z SeqStruct grows with 64-bit pointers; the game only reserved
 * SoundState.seqTable[0x4200] (PS1 SS_SEQ_TABSIZ). SsSetTableSize would
 * overflow into HEAP. Use a private oversized table instead.
 *
 * The rows must be laid out exactly like libsnd indexes them: _ss_score[s]
 * points at table + s * t_max entries and libsnd reads _ss_score[s][t] as
 * entry t of that row, so an entry is sizeof(struct SeqStruct) bytes and the
 * rows are contiguous. The game relies on it: it opens the system bank's two
 * SEP files as access numbers 0 and 1 with 16 tracks each and then plays the
 * second file's tracks as SsSepPlay(0, 16..31) (the win jingle is track 30,
 * menu sounds are 25, 26, 28). On the PS1 _ss_score[0][30] and _ss_score[1][14]
 * are the same entry, and the ticker plays it as (1, 14). With a row stride of
 * 0x100 and an entry size of 0xC0 they were not (the entry fell inside row 1
 * at a misaligned offset), so those tracks never played.
 */
enum { DW3_HOST_SEQ_S_MAX = 6, DW3_HOST_SEQ_T_MAX = 16 };
/* sizeof(struct SeqStruct) of the host libsnd (SS_SEQ_TABSIZ under __psyz on
 * a 64-bit target); tools/tests/test_seq_table.py checks it against the
 * struct. */
enum { DW3_HOST_SEQ_ENTRY = 192 };

static unsigned char g_host_seq_table[DW3_HOST_SEQ_S_MAX * DW3_HOST_SEQ_T_MAX *
                                     DW3_HOST_SEQ_ENTRY];

extern short _snd_seq_s_max;
extern short _snd_seq_t_max;
extern void *_ss_score[32];

void SsSetTableSize(char *table, short s_max, short t_max) {
    short i;
    unsigned char *base = g_host_seq_table;
    size_t stride = DW3_HOST_SEQ_ENTRY;

    (void)table;
    if (s_max <= 0 || t_max <= 0) {
        return;
    }
    if (s_max > DW3_HOST_SEQ_S_MAX) {
        s_max = DW3_HOST_SEQ_S_MAX;
    }
    if (t_max > DW3_HOST_SEQ_T_MAX) {
        t_max = DW3_HOST_SEQ_T_MAX;
    }
    _snd_seq_s_max = s_max;
    _snd_seq_t_max = t_max;
    for (i = 0; i < 32; i++) {
        _ss_score[i] = NULL;
    }
    for (i = 0; i < s_max; i++) {
        _ss_score[i] = base + (size_t)i * (size_t)t_max * stride;
    }
    /* Zero slots; leave detailed SeqStruct field init to first real use. */
    {
        size_t n = (size_t)s_max * (size_t)t_max * stride;
        size_t k;
        for (k = 0; k < n; k++) {
            base[k] = 0;
        }
    }
}
