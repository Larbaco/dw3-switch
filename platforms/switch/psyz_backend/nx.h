/* Shared declarations for the Switch PSY-Z backend sketch (P4.1).
 * Not linked into dw3-switch.nro yet — see README.md. */
#ifndef DW3_SWITCH_PSYZ_NX_H
#define DW3_SWITCH_PSYZ_NX_H

#ifdef __SWITCH__
#include <switch.h>
#endif

/* Placeholder until P4.2 wires InitPlatform / present. */
int dw3_nx_backend_sketch_version(void);

#endif /* DW3_SWITCH_PSYZ_NX_H */
