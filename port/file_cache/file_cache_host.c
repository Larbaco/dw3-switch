/* File cache slot policy of the host (see port_file_cache.h). */
#include "port_file_cache.h"
#include "port_l10n.h"

#include <psyz/log.h>
#include <stdlib.h>

int dw3_port_file_cache_slots(int total) {
    static int limit;

    dw3_l10n_sample_text(); /* P6.7: the text pack's heap high water, once per file request */

    if (limit == 0) {
        const char *env = getenv("DW3_FILE_CACHE_SLOTS");
        long n = env ? strtol(env, NULL, 0) : 0;

        limit = total;
        if (n >= 1 && n < total) {
            limit = (int)n;
            INFOF("dw3-file-cache: DW3_FILE_CACHE_SLOTS=%d of %d (test hook)", limit, total);
        }
    }
    return limit;
}

void dw3_port_file_cache_evicted(int file, int evicted, int total) {
    static unsigned count;

    /* The first 16 evictions, then every 256th: a long session evicts once
     * per new file, which would flood dw3.log. */
    if (count < 16 || (count & 255u) == 0) {
        INFOF("dw3-file-cache: all %d slots taken, evicted the least recently used file 0x%x for 0x%x"
              " (eviction %u)", dw3_port_file_cache_slots(total), evicted, file, count + 1);
    }
    count++;
}

void dw3_port_file_cache_fatal(int file, int total) {
    INFOF("dw3-file-cache: FATAL all %d slots taken and none loaded to evict, requestFile(0x%x)",
          dw3_port_file_cache_slots(total), file);
    abort();
}
