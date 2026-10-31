/* BUG-1: the exit path back to the loader. See nx_exit.h. */
#include "nx.h"
#include "nx_exit.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <switch.h>

static u64 s_begin_tick;
static u64 s_deadline_tick;

static u64 now_ms(void) {
    return armTicksToNs(armGetSystemTick()) / 1000000ULL;
}

void dw3_nx_exit_log(const char *fmt, ...) {
    char buf[320];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    psyz_log(1, __FILE__, __LINE__, __func__, "dw3-exit: %s", buf);
}

void dw3_nx_exit_loader_report(void) {
    const char *info = envGetLoaderInfo();
    char shown[80];
    u64 hs = envHasHeapOverride() ? envGetHeapOverrideSize() : 0;

    /* The loader's notice string: "sphaira <version>" for Sphaira's hbl,
     * "hbloader <version>" for nx-hbloader, nothing for a plain title. */
    if (info != NULL && envGetLoaderInfoSize() > 0) {
        snprintf(shown, sizeof(shown), "%.60s", info);
    } else {
        snprintf(shown, sizeof(shown), "none (not started by a homebrew loader)");
    }
    dw3_nx_exit_log("loader: info='%s' envHasNextLoad=%d (a relaunch %s) "
                    "lastLoadResult=0x%x applet_type=%d",
                    shown, envHasNextLoad() ? 1 : 0,
                    envHasNextLoad() ? "can be queued" : "cannot be queued, "
                    "the notice says to restart by hand",
                    (unsigned)envGetLastLoadResult(),
                    (int)appletGetAppletType());
    dw3_nx_exit_log("loader: heap override=%d addr=0x%llx size=%llu MiB "
                    "(the next NRO is read over this range when we return)",
                    envHasHeapOverride() ? 1 : 0,
                    (unsigned long long)(u64)envGetHeapOverrideAddr(),
                    (unsigned long long)(hs >> 20));
}

void dw3_nx_exit_begin(void) {
    s_begin_tick = armGetSystemTick();
    s_deadline_tick = s_begin_tick + armNsToTicks((u64)DW3_NX_EXIT_BUDGET_MS * 1000000ULL);
}

uint32_t dw3_nx_exit_elapsed_ms(void) {
    return (uint32_t)(armTicksToNs(armGetSystemTick() - s_begin_tick) / 1000000ULL);
}

int dw3_nx_exit_join(Thread *t, const char *name) {
    u64 t0 = now_ms();
    u64 left_ns = 0;
    u64 tick = armGetSystemTick();
    Result rc;

    if (s_deadline_tick > tick) {
        left_ns = armTicksToNs(s_deadline_tick - tick);
    }
    rc = waitSingleHandle(t->handle, left_ns);
    if (R_FAILED(rc)) {
        dw3_nx_exit_log("thread %s still running after %llu ms (rc=0x%x); its "
                        "stack stays mapped", name,
                        (unsigned long long)(now_ms() - t0), (unsigned)rc);
        return 1;
    }
    rc = threadClose(t);
    dw3_nx_exit_log("thread %s joined and closed in %llu ms (threadClose rc=0x%x)",
                    name, (unsigned long long)(now_ms() - t0), (unsigned)rc);
    return 0;
}

void dw3_nx_exit_heap_probe(const char *when) {
    u64 start, end, addr;
    int regions = 0, bad = 0;

    if (!envHasHeapOverride()) {
        dw3_nx_exit_log("heap probe (%s): no heap override, nothing to walk", when);
        return;
    }
    start = (u64)envGetHeapOverrideAddr();
    end = start + envGetHeapOverrideSize();
    for (addr = start; addr < end && regions < 4096; ) {
        MemoryInfo mi;
        u32 page = 0;

        memset(&mi, 0, sizeof(mi));
        if (R_FAILED(svcQueryMemory(&mi, &page, addr)) || mi.size == 0) {
            dw3_nx_exit_log("heap probe (%s): svcQueryMemory failed at 0x%llx",
                            when, (unsigned long long)addr);
            break;
        }
        ++regions;
        if (mi.type != MemType_Heap || mi.perm != Perm_Rw || mi.attr != 0 ||
            mi.ipc_refcount != 0 || mi.device_refcount != 0) {
            ++bad;
            if (bad <= 16) {
                dw3_nx_exit_log("heap probe (%s): ABNORMAL 0x%llx +0x%llx "
                                "type=0x%x perm=%u attr=0x%x ipc=%u device=%u",
                                when, (unsigned long long)mi.addr,
                                (unsigned long long)mi.size, (unsigned)mi.type,
                                (unsigned)mi.perm, (unsigned)mi.attr,
                                (unsigned)mi.ipc_refcount,
                                (unsigned)mi.device_refcount);
            }
        }
        addr = mi.addr + mi.size;
    }
    dw3_nx_exit_log("heap probe (%s): %d region(s) in 0x%llx +%llu MiB, %d "
                    "abnormal (%s)", when, regions, (unsigned long long)start,
                    (unsigned long long)((end - start) >> 20), bad,
                    bad == 0 ? "plain read/write heap all through"
                             : "borrowed, IPC- or device-mapped or not "
                               "read/write; only the final probe matters "
                               "to the loader");
}
