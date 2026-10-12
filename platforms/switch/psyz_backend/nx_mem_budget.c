/* P5.1 memory-budget instrumentation. Empty TU when DW3_NX_DEBUG_MEM is off. */
#include "nx_mem_budget.h"

#if DW3_NX_DEBUG_MEM

#include "nx.h"
#include "nx_vram_pack.h"

#include <malloc.h>
#include <stdint.h>
#include <string.h>

/* Match port/overlays/host_vram.c sizes (not exported as symbols). */
enum {
    DW3_NX_HOST_OVERLAY_BYTES = 0x40000, /* 256 KiB */
    DW3_NX_HOST_STAGE_BYTES = 0x80000    /* 512 KiB */
};

/* Soft RGB555 VRAM (PSY-Z) + full RGBA pack buffer in nx_gpu.c. */
enum {
    DW3_NX_SOFT_VRAM_BYTES = DW3_NX_VRAM_W * DW3_NX_VRAM_H * 2,
    DW3_NX_PACK_RGBA_BYTES = DW3_NX_VRAM_W * DW3_NX_VRAM_H * 4
};

/* ABI mirror of engine Heap / MemBlock on LP64 (PORT_HOST). Only the fields
 * needed to walk the list — do not pull engine/heap.h into the backend. */
enum {
    DW3_MEM_FREE = 0,
    DW3_MEM_END = 1,
    DW3_MEM_FILE_CACHE = 3
};

typedef struct Dw3MemBlockView {
    struct Dw3MemBlockView *prev;
    struct Dw3MemBlockView *next;
    int32_t tag;
} Dw3MemBlockView;

typedef struct Dw3HeapView {
    int32_t size;
    Dw3MemBlockView *first;
    Dw3MemBlockView *end;
} Dw3HeapView;

extern Dw3HeapView HEAP;
extern unsigned char DW3_HOST_HEAP[];
extern const unsigned long DW3_HOST_HEAP_SIZE;

static uint64_t s_heap_used_hw;
static uint64_t s_file_cache_hw;
static uint64_t s_libc_used_hw;

static int ptr_in_arena(const void *p) {
    const unsigned char *base = DW3_HOST_HEAP;
    const unsigned char *end = base + (size_t)DW3_HOST_HEAP_SIZE;
    const unsigned char *q = (const unsigned char *)p;

    return q >= base && q < end;
}

static uint64_t libc_used_bytes(void) {
    struct mallinfo mi = mallinfo();

    if (mi.uordblks <= 0) {
        return 0;
    }
    return (uint64_t)mi.uordblks;
}

static int walk_heap(uint64_t *used_out, uint64_t *free_out,
                     uint64_t *fcache_out) {
    Dw3MemBlockView *b;
    Dw3MemBlockView *next;
    unsigned steps = 0;
    uint64_t used = 0;
    uint64_t free_b = 0;
    uint64_t fcache = 0;

    if (!used_out || !free_out || !fcache_out) {
        return 0;
    }
    *used_out = 0;
    *free_out = 0;
    *fcache_out = 0;

    b = HEAP.first;
    if (!b || !ptr_in_arena(b)) {
        return 0;
    }

    for (; b != NULL && steps < 100000u; b = next, steps++) {
        size_t span;

        if (!ptr_in_arena(b)) {
            return 0;
        }
        if (b->tag == DW3_MEM_END) {
            break;
        }
        next = b->next;
        if (!next || !ptr_in_arena(next) || (const unsigned char *)next <=
                                                 (const unsigned char *)b) {
            return 0;
        }
        span = (size_t)((const unsigned char *)next - (const unsigned char *)b);
        if (b->tag == DW3_MEM_FREE) {
            free_b += (uint64_t)span;
        } else {
            used += (uint64_t)span;
            if (b->tag == DW3_MEM_FILE_CACHE) {
                fcache += (uint64_t)span;
            }
        }
    }
    if (steps >= 100000u) {
        return 0;
    }

    *used_out = used;
    *free_out = free_b;
    *fcache_out = fcache;
    return 1;
}

void dw3_nx_mem_budget_sample(Dw3NxMemBudgetSample *out) {
    uint64_t used = 0;
    uint64_t free_b = 0;
    uint64_t fcache = 0;
    uint64_t libc;
    int ok;

    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->heap_arena_bytes = (uint64_t)DW3_HOST_HEAP_SIZE;
    out->vram_emu_bytes =
        (uint64_t)DW3_NX_SOFT_VRAM_BYTES + (uint64_t)DW3_NX_PACK_RGBA_BYTES +
        (uint64_t)DW3_NX_HOST_OVERLAY_BYTES + (uint64_t)DW3_NX_HOST_STAGE_BYTES;

    ok = walk_heap(&used, &free_b, &fcache);
    out->heap_ok = ok;
    if (ok) {
        out->heap_used_bytes = used;
        out->heap_free_bytes = free_b;
        out->file_cache_bytes = fcache;
        if (used > s_heap_used_hw) {
            s_heap_used_hw = used;
        }
        if (fcache > s_file_cache_hw) {
            s_file_cache_hw = fcache;
        }
    }
    out->heap_used_hw = s_heap_used_hw;
    out->file_cache_hw = s_file_cache_hw;

    libc = libc_used_bytes();
    out->libc_used_bytes = libc;
    if (libc > s_libc_used_hw) {
        s_libc_used_hw = libc;
    }
    out->libc_used_hw = s_libc_used_hw;
}

void dw3_nx_mem_budget_log(const Dw3NxMemBudgetSample *s) {
    if (!s) {
        return;
    }
    if (s->heap_ok) {
        psyz_log(1, "nx_mem_budget.c", __LINE__, __func__,
                 "mem dbg: heap=%lluK/%lluK hw=%lluK free=%lluK "
                 "fcache=%lluK/hw=%lluK vram=%lluK libc=%lluK/hw=%lluK",
                 (unsigned long long)(s->heap_used_bytes / 1024ull),
                 (unsigned long long)(s->heap_arena_bytes / 1024ull),
                 (unsigned long long)(s->heap_used_hw / 1024ull),
                 (unsigned long long)(s->heap_free_bytes / 1024ull),
                 (unsigned long long)(s->file_cache_bytes / 1024ull),
                 (unsigned long long)(s->file_cache_hw / 1024ull),
                 (unsigned long long)(s->vram_emu_bytes / 1024ull),
                 (unsigned long long)(s->libc_used_bytes / 1024ull),
                 (unsigned long long)(s->libc_used_hw / 1024ull));
    } else {
        psyz_log(1, "nx_mem_budget.c", __LINE__, __func__,
                 "mem dbg: heap=n/a vram=%lluK libc=%lluK/hw=%lluK",
                 (unsigned long long)(s->vram_emu_bytes / 1024ull),
                 (unsigned long long)(s->libc_used_bytes / 1024ull),
                 (unsigned long long)(s->libc_used_hw / 1024ull));
    }
}

#endif /* DW3_NX_DEBUG_MEM */
