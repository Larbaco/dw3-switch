/* Host harness for the P4.5 Switch backend fixes.
 *
 * Compiles the real nx_file.c / nx_os.c against the libnx stand-in and drives
 * the two conditions the fixes address. Prints one `key=value` line per result;
 * the pytest wrapper asserts on them. Exit code is always 0 so the wrapper sees
 * the values (a missing line is itself a failure).
 */
#define _GNU_SOURCE /* mkdtemp under -std=c11 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "nx.h"     /* dw3_nx_fs_mount_sd, Psyz_OsThreadStart */
#include "switch.h" /* libnx stand-in + stub controls */

static void thread_body(void) {}

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

    return 0;
}
