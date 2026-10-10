/* P4.1 sketch — modules (PSP: psp_module_host.c).
 * Target: static Psyz_ModuleOpen table; libnx has no dlopen (P4.6 / P2.6).
 * Not linked yet. */
#include "nx.h"

#if 0
#include <psyz/module.h>
PsyzModule Psyz_ModuleOpen(const char *name, void *param);
void Psyz_ModuleClose(PsyzModule descriptor);
#endif
