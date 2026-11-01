/* P6.11 internal resolution, the Switch host's half (nx_res.c): the log helper
 * and the console mode word the mesh layer and the port's log lines share. */
#ifndef DW3_NX_RES_H
#define DW3_NX_RES_H

/* One line into dw3.log (psyz_log), for the `dw3-res:` lines. */
void dw3_nx_res_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/* "handheld" or "docked". */
const char *dw3_nx_res_mode_name(void);

#endif /* DW3_NX_RES_H */
