/* P5.4 — Switch crash report to SD (readable text + last mode/stage id). */
#ifndef DW3_NX_CRASH_H
#define DW3_NX_CRASH_H

#include <stdint.h>

#ifndef DW3_NX_FORCE_CRASH
#define DW3_NX_FORCE_CRASH 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Open the crash report sink under sdmc:/switch/dw3/. Call after
 * dw3_nx_fs_mount_sd() (and preferably after dw3_nx_log_open()). Providing
 * __libnx_exception_handler + __nx_exception_stack* is what enables the
 * user-mode path; this just prepares the SD file and logs once.
 *
 * Startup latch (P5.4): if a report from a previous run is still on SD, it is
 * rotated to sdmc:/switch/dw3/crash_report_prev.txt before the sink is
 * truncated, and its reason/version/mode/stage are logged, so one crash does
 * not overwrite the next and the log says the previous run ended in a fault. */
void dw3_nx_crash_install(void);

/* Write a readable report without taking a CPU fault (assert-style). Safe to
 * call from normal code; the exception handler uses the same sink. */
void dw3_nx_crash_write_report(const char *reason);

/* Frame-loop progress for the report: completed vsync pumps and the last
 * completed 60-pump frame dt (0 before the first sample). Cheap two-word
 * stores, called once per VSync pump by the Switch boot host, so the report
 * can tell a boot fault from a mid-game fault. */
void dw3_nx_crash_note_vsync(uint32_t pumps, uint64_t last_dt_us);

#if DW3_NX_FORCE_CRASH
/* Debug-only: deliberate null deref so the exception handler runs. */
void dw3_nx_crash_force_fault(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DW3_NX_CRASH_H */
