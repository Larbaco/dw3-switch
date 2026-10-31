/* BUG-1 (docs/SPHAIRA_EXIT.md): the exit path back to the loader.
 *
 * Leaving through a loader that keeps the process (Sphaira's hbl, hbmenu's
 * nx-hbloader) means libnx returns to the loader, which unmaps this NRO and
 * reads the next one over the same heap. A heap page this program left in a
 * state the kernel will not hand to IPC (a finished thread whose stack was
 * never threadClose()d stays svcMapMemory-borrowed, Perm_None) fails that read
 * with 0xD401 and the loader svcBreaks. So every worker thread is joined and
 * closed before main() returns, and every step says so in dw3.log with a
 * `dw3-exit:` line.
 *
 * Only the game image uses these (the hello samples do not link them). */
#ifndef DW3_SWITCH_NX_EXIT_H
#define DW3_SWITCH_NX_EXIT_H

#include <stdint.h>

#include <switch.h>

/* Total time the exit path may spend waiting for worker threads, in ms. HOME
 * force-kills 15 s after the request; the saves are flushed long before this. */
#define DW3_NX_EXIT_BUDGET_MS 10000

/* One `dw3-exit:` log line (printf format). */
void dw3_nx_exit_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* Startup: who launched us and what the loader gave (envGetLoaderInfo,
 * envHasNextLoad, heap override, applet type). One block of `dw3-exit:` lines. */
void dw3_nx_exit_loader_report(void);

/* Start the exit-path clock; later joins share DW3_NX_EXIT_BUDGET_MS. */
void dw3_nx_exit_begin(void);

/* Wait for `t` to finish (within what is left of the budget), then
 * threadClose() it. Returns 0 when joined and closed, 1 when the thread was
 * still running (logged; its stack stays mapped), and the thread must not be
 * reused after 0. */
int dw3_nx_exit_join(Thread *t, const char *name);

/* Walk the heap the loader gave us (svcQueryMemory) and log every region that
 * is not plain read/write heap with no borrow, IPC or device reference. This is
 * the state hbl's next read needs; `when` names the point ("before cleanup",
 * "final"). Returns how many regions are borrowed / unreadable (Perm_None or
 * MemAttr_IsBorrowed: the signature of an unclosed thread stack), the only
 * kind that makes the loader's read fail; other odd regions are logged but
 * not counted. */
int dw3_nx_exit_heap_probe(const char *when);

/* Milliseconds since dw3_nx_exit_begin. */
uint32_t dw3_nx_exit_elapsed_ms(void);

#endif /* DW3_SWITCH_NX_EXIT_H */
