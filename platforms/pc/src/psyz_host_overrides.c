/*
 * Strong overrides for PSY-Z symbols that misbehave on 64-bit host.
 * Linked as a TU (not only via the engine .a) so they win over libpsyz.a.
 */
#include <stddef.h>

/*
 * PSY-Z SeqStruct grows with 64-bit pointers; the game only reserved
 * SoundState.seqTable[0x4200] (PS1 SS_SEQ_TABSIZ). SsSetTableSize would
 * overflow into HEAP. Use a private oversized table instead.
 */
enum { DW3_HOST_SEQ_S_MAX = 6, DW3_HOST_SEQ_T_MAX = 16 };
enum { DW3_HOST_SEQ_SLOT = 0x100 };

static unsigned char g_host_seq_table[DW3_HOST_SEQ_S_MAX * DW3_HOST_SEQ_T_MAX *
                                     DW3_HOST_SEQ_SLOT];

extern short _snd_seq_s_max;
extern short _snd_seq_t_max;
extern void *_ss_score[32];

void SsSetTableSize(char *table, short s_max, short t_max) {
    short i;
    unsigned char *base = g_host_seq_table;
    size_t stride = DW3_HOST_SEQ_SLOT;

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
