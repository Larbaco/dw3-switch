/* Host harness for the P4.5 Switch backend fixes.
 *
 * Compiles the real nx_file.c / nx_os.c against the libnx stand-in and drives
 * the two conditions the fixes address. Prints one `key=value` line per result;
 * the pytest wrapper asserts on them. Exit code is always 0 so the wrapper sees
 * the values (a missing line is itself a failure).
 */
#define _GNU_SOURCE /* mkdtemp under -std=c11 */

#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nx.h"     /* dw3_nx_fs_mount_sd, Psyz_OsThreadStart */
#include "switch.h" /* libnx stand-in + stub controls */

static void thread_body(void) {}

static double now_ms(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

/* OBS-7: a waiter parked in a bell with no timeout; the main thread rings it. */
static void *bell_waiter(void *bell) {
    Psyz_OsBellWait(bell, UINT32_MAX);
    return NULL;
}

static void make_runtime_dirs(void) {
    /* dw3_nx_fs_mount_sd() does mkdir("sdmc:/switch/dw3"); pre-create it so the
     * call hits EEXIST (the normal second-run case), not ENOENT. */
    mkdir("sdmc:", 0777);
    mkdir("sdmc:/switch", 0777);
    mkdir("sdmc:/switch/dw3", 0777);
}

int main(void) {
    char tmpl[] = "/tmp/dw3stubXXXXXX";
    if (mkdtemp(tmpl) == NULL || chdir(tmpl) != 0) {
        perror("mkdtemp/chdir");
        return 1;
    }
    make_runtime_dirs();

    /* A: fsdevMountSdmc() failed but sdmc: is already mounted -> success. */
    stub_reset();
    stub_set_mount_result(1 /* failure */);
    stub_set_device_present(true);
    printf("fs_mount_already_mounted=%d\n", dw3_nx_fs_mount_sd() ? 1 : 0);

    /* B: mount failed and no device -> genuine failure. */
    stub_reset();
    stub_set_mount_result(1);
    stub_set_device_present(false);
    printf("fs_mount_genuine_failure=%d\n", dw3_nx_fs_mount_sd() ? 1 : 0);

    /* C: fresh mount -> success. */
    stub_reset();
    stub_set_mount_result(0);
    stub_set_device_present(false);
    printf("fs_mount_success=%d\n", dw3_nx_fs_mount_sd() ? 1 : 0);

    /* D: threadCreate() succeeds, threadStart() fails: the live handle must be
     * closed, not leaked/freed. */
    stub_reset();
    stub_set_thread_start_result(1 /* failure */);
    printf("thread_start_rc=%d\n", Psyz_OsThreadStart(thread_body, "test"));
    printf("thread_create_count=%d\n", stub_thread_create_count());
    printf("thread_close_count=%d\n", stub_thread_close_count());
    printf("thread_handle_live=%d\n", stub_is_handle_live());

    /* E (BUG-1): a started PSY-Z thread is joined and closed by the exit path;
     * a second call has nothing left to close. */
    stub_reset();
    printf("os_start_rc=%d\n", Psyz_OsThreadStart(thread_body, "test"));
    printf("os_shutdown_still_running=%d\n", dw3_nx_os_threads_shutdown());
    printf("os_shutdown_close_count=%d\n", stub_thread_close_count());
    printf("os_shutdown_handle_live=%d\n", stub_is_handle_live());
    printf("os_shutdown_again_close_count=%d\n",
           dw3_nx_os_threads_shutdown() + stub_thread_close_count());

    /* F (OBS-7): the bells take no kernel event (the process's event limit was
     * used up at boot on the OLED, so eventCreate failed with 0x10801) and keep
     * the auto-reset contract. */
    {
        void *bell;
        double t0, ms_ring, ms_again, ms_timeout, ms_wake;
        pthread_t th;

        stub_reset();
        bell = Psyz_OsBellCreate();
        printf("bell_created=%d\n", bell != NULL ? 1 : 0);
        /* a ring before the wait is kept: the wait returns at once */
        Psyz_OsBellRing(bell);
        t0 = now_ms();
        Psyz_OsBellWait(bell, 2000000);
        ms_ring = now_ms() - t0;
        printf("bell_ring_then_wait_fast=%d\n", ms_ring < 500.0 ? 1 : 0);
        /* auto-reset: the ring was consumed, the next wait runs to its timeout */
        t0 = now_ms();
        Psyz_OsBellWait(bell, 60000);
        ms_again = now_ms() - t0;
        printf("bell_autoreset_waits_timeout=%d\n",
               ms_again >= 50.0 && ms_again < 1500.0 ? 1 : 0);
        /* a zero timeout does not wait */
        t0 = now_ms();
        Psyz_OsBellWait(bell, 0);
        printf("bell_zero_timeout_returns=%d\n", now_ms() - t0 < 500.0 ? 1 : 0);
        /* no ring: the wait ends at its timeout and not before */
        t0 = now_ms();
        Psyz_OsBellWait(bell, 120000);
        ms_timeout = now_ms() - t0;
        printf("bell_timeout_elapsed=%d\n",
               ms_timeout >= 100.0 && ms_timeout < 1500.0 ? 1 : 0);
        /* a waiter with no timeout is woken by a ring from another thread */
        pthread_create(&th, NULL, bell_waiter, bell);
        usleep(50000);
        t0 = now_ms();
        Psyz_OsBellRing(bell);
        pthread_join(th, NULL);
        ms_wake = now_ms() - t0;
        printf("bell_ring_wakes_waiter=%d\n", ms_wake < 1500.0 ? 1 : 0);
        printf("bell_kernel_events=%d\n", stub_event_create_count());
    }

    return 0;
}
