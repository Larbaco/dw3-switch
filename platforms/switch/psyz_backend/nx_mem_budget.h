/* P5.1 — sampled heap / file-cache / VRAM-emulation budget counters.
 * Enabled when DW3_NX_DEBUG_MEM=1 (default ON for --nxlink CMake builds).
 * See docs/P5.1_MEMORY.md. */
#ifndef DW3_NX_MEM_BUDGET_H
#define DW3_NX_MEM_BUDGET_H

#include <stdint.h>

#ifndef DW3_NX_DEBUG_MEM
#define DW3_NX_DEBUG_MEM 0
#endif

#if DW3_NX_DEBUG_MEM

typedef struct Dw3NxMemBudgetSample {
    int heap_ok; /* 0 = HEAP not walkable yet (pre-initHeap) */
    uint64_t heap_arena_bytes;
    uint64_t heap_used_bytes;
    uint64_t heap_free_bytes;
    uint64_t heap_used_hw;
    uint64_t file_cache_bytes; /* MEM_FILE_CACHE tagged spans */
    uint64_t file_cache_hw;
    uint64_t vram_emu_bytes; /* fixed soft VRAM + pack + host ovl/stage */
    uint64_t libc_used_bytes; /* newlib mallinfo uordblks, 0 if unavailable */
    uint64_t libc_used_hw;
} Dw3NxMemBudgetSample;

/* Walk the game heap + fixed VRAM sizes; update high-water marks. */
void dw3_nx_mem_budget_sample(Dw3NxMemBudgetSample *out);

/* One nxlink-safe line (call at the ~1 Hz frame-dt cadence). */
void dw3_nx_mem_budget_log(const Dw3NxMemBudgetSample *s);

#else /* !DW3_NX_DEBUG_MEM */

#define dw3_nx_mem_budget_sample(out) ((void)0)
#define dw3_nx_mem_budget_log(s) ((void)0)

#endif /* DW3_NX_DEBUG_MEM */

#endif /* DW3_NX_MEM_BUDGET_H */
