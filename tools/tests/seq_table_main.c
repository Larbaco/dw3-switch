/* libsnd's SEQ table as the PC/Switch host lays it out
 * (platforms/pc/src/psyz_host_overrides.c, SsSetTableSize).
 *
 * The game opens the system bank's two SEP files as access numbers 0 and 1
 * (16 tracks each) and plays the second file's tracks as SsSepPlay(0, 16..31):
 * the win jingle is 30, the menu sounds 25, 26 and 28. That only works when
 * _ss_score[0][t] for t >= 16 is _ss_score[1][t - 16], i.e. when the rows are
 * contiguous in units of sizeof(struct SeqStruct), like the PS1 table. */
#include <stdio.h>
#include <string.h>

#include "libsnd_private.h"

struct SeqStruct* _ss_score[32];
s16 _snd_seq_s_max;
s16 _snd_seq_t_max;

static int fail(const char* what) {
    printf("FAIL: %s\n", what);
    return 1;
}

int main(void) {
    /* the game hands over its 0x4200 byte table; the host ignores it */
    static char game_table[0x4200];
    int s;
    int t;

    SsSetTableSize(game_table, 6, 16);
    if (_snd_seq_s_max != 6 || _snd_seq_t_max != 16) {
        return fail("table dimensions");
    }
    printf("sizeof(struct SeqStruct) = %zu\n", sizeof(struct SeqStruct));

    /* rows are contiguous: row s starts 16 entries after row s - 1 */
    for (s = 1; s < 6; s++) {
        if (_ss_score[s] != _ss_score[s - 1] + 16) {
            printf("row %d starts at entry %td of row %d, want 16\n", s,
                   _ss_score[s] - _ss_score[s - 1], s - 1);
            return fail("rows are not contiguous");
        }
    }
    /* so a track past 15 of row 0 is the entry of row 1, as on the PS1 */
    for (t = 16; t < 32; t++) {
        if (&_ss_score[0][t] != &_ss_score[1][t - 16]) {
            printf("(0, %d) is not (1, %d)\n", t, t - 16);
            return fail("track >= 16 does not alias the next row");
        }
    }
    if (&_ss_score[0][30] != &_ss_score[1][14]) {
        return fail("the win jingle (0, 30) is not (1, 14)");
    }
    /* a write through one name is visible through the other */
    _ss_score[0][30].flags = 0x1234;
    if (_ss_score[1][14].flags != 0x1234) {
        return fail("flags written as (0, 30) are not seen as (1, 14)");
    }
    _ss_score[0][30].flags = 0;
    /* no entry overlaps another: a write to one leaves its neighbours alone */
    for (s = 0; s < 6; s++) {
        for (t = 0; t < 16; t++) {
            _ss_score[s][t].flags = (unsigned)(s * 16 + t + 1);
        }
    }
    for (s = 0; s < 6; s++) {
        for (t = 0; t < 16; t++) {
            if (_ss_score[s][t].flags != (unsigned)(s * 16 + t + 1)) {
                printf("entry (%d, %d) was overwritten\n", s, t);
                return fail("entries overlap");
            }
        }
    }
    puts("OK: seq table");
    return 0;
}
