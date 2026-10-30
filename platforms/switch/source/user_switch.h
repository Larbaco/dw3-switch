/* P6.26: Switch account / profile selector wrapper over port_user_saves.h.
 * Called once from main() before the GPU comes up. */
#ifndef DW3_SWITCH_USER_SWITCH_H
#define DW3_SWITCH_USER_SWITCH_H

#define DW3_USER_BOOT_START 0   /* user chosen: the save folder is ready */
#define DW3_USER_BOOT_STOP 1    /* cancel or error: leave, nothing written */
#define DW3_USER_BOOT_SKIPPED 2 /* forced directory (replay builds, DW3_MEMCARD_DIR) */

/* `root` is the runtime root (sdmc:/switch/dw3/us), `region` "us" or "eu". */
int dw3_switch_user_boot(const char *root, const char *region);

/* Save folder for the chosen user, or NULL when skipped / not resolved. */
const char *dw3_switch_user_memdir(void);
/* P6.25 store: the profile folder (parent of the region folders), or NULL. */
const char *dw3_switch_user_profile_dir(void);

/* accountExit when the service was opened; safe to call always. */
void dw3_switch_user_exit(void);

#endif
