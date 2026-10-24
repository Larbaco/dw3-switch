/* P6.20: what cheats.c and cheat_saves.c share (not for the hooks; those use
 * port_cheats.h). Compiled only with DW3_CHEATS. */
#ifndef DW3_CHEATS_INTERNAL_H
#define DW3_CHEATS_INTERNAL_H

#ifdef DW3_PORT_OPTIONS_TEST
#include <stdio.h>
#define CHEAT_INFO(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#define CHEAT_WARN(...) (fprintf(stderr, __VA_ARGS__), fputc('\n', stderr))
#else
#include <psyz/log.h>
/* psyz_log: on the Switch the lines reach sdmc:/switch/dw3/dw3.log. */
#define CHEAT_INFO(...) INFOF(__VA_ARGS__)
#define CHEAT_WARN(...) WARNF(__VA_ARGS__)
#endif

/* save<N>.cheats beside save<N>.bin (the memcard dir); NULL turns the
 * marks off. */
void dw3_cheat_saves_set_dir(const char *memdir);
/* FNV-1a 32 of the bytes of a slot, the identity a mark holds. */
unsigned dw3_cheat_saves_hash(const void *data, int size);
/* Set (marked != 0) or clear the mark of a slot that was just written with
 * `data`. Returns 0, or -1 when the sidecar cannot be written (logged). */
int dw3_cheat_saves_mark(int port, int slot, int offset, const void *data,
                         int size, int marked);
/* 1 when the slot's mark holds the hash of `data` (the slot just loaded). */
int dw3_cheat_saves_is_marked(int port, int slot, const void *data, int size);
/* Marks whose slot still holds the bytes they were made for, read from the
 * flat saves (memcard_host). `log` prints one line per marked slot. */
int dw3_cheat_saves_count(int log);

#endif
