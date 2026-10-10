/*
 * PSY-Z libcard raw sector backend → port/memcard .mcr images.
 *
 * Linked with -Wl,--wrap=_card_read -Wl,--wrap=_card_write so calls from
 * PSY-Z / game code hit the host .mcr files instead of the stub events.
 */
#include "memcard_host.h"

#include <stdio.h>
#include <string.h>

/* Provided by the linker when using --wrap. */
long __real__card_read(long chan, long block, unsigned char *buf);
long __real__card_write(long chan, long block, unsigned char *buf);

static int chan_to_slot(long chan) {
    unsigned int port = (unsigned int)(chan >> 4);

    if (port >= MEMCARD_HOST_SLOTS) {
        return -1;
    }
    if ((chan & 15) != 0) {
        return -1; /* multi-tap slots unsupported */
    }
    return (int)port;
}

/* Mirror PSY-Z card_op event completion via the real stub after I/O. */
long __wrap__card_read(long chan, long block, unsigned char *buf) {
    int slot = chan_to_slot(chan);

    if (slot < 0 || buf == NULL || block < 0 ||
        (unsigned long)block >= MEMCARD_HOST_SECTORS) {
        return 0;
    }
    if (memcard_host_ensure(slot) != 0) {
        return 0;
    }
    if (memcard_host_read_sector(slot, (int)block, buf) != 0) {
        return 0;
    }
    return __real__card_read(chan, block, buf);
}

long __wrap__card_write(long chan, long block, unsigned char *buf) {
    int slot = chan_to_slot(chan);

    if (slot < 0 || buf == NULL || block < 0 ||
        (unsigned long)block >= MEMCARD_HOST_SECTORS) {
        return 0;
    }
    if (memcard_host_write_sector(slot, (int)block, buf) != 0) {
        fprintf(stderr, "dw3: _card_write fail chan=%ld block=%ld slot=%d\n", chan,
                block, slot);
        return 0;
    }
    fprintf(stderr, "dw3: _card_write ok chan=%ld block=%ld slot=%d (.mcr)\n", chan,
            block, slot);
    return __real__card_write(chan, block, buf);
}
