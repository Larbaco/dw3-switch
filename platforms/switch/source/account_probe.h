/* P6.26 account probe: one diagnostic block in the boot log. Logs only; it
 * changes no save path and no game behaviour. See docs/PER_USER_SAVES.md. */
#ifndef DW3_NX_ACCOUNT_PROBE_H
#define DW3_NX_ACCOUNT_PROBE_H

/* Logs `account probe:` lines (applet type, account service, preselected,
 * listed, last opened, no-interaction user). With DW3_ACCOUNT_PROBE_PSEL it
 * also opens the system profile selector once and logs the result; the choice
 * is ignored. Every failure is logged and skipped. Call after the log is open
 * and before the GPU is up. */
void dw3_nx_account_probe(void);

#endif
