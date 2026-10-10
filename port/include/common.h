#ifndef COMMON_H
#define COMMON_H

/*
 * Port-host stand-in for vendor/dw3_decomp/include/common.h.
 * Keeps version.h / include_asm.h / FLEXIBLE / INTERRUPT_SHARED, but pulls
 * integer typedefs from PSY-Z so -std=gnu89 does not hit typedef redefinition
 * when game headers include <libgte.h> → <psyz/types.h>.
 */

#include "version.h"
#include "include_asm.h"

#include <psyz/types.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#if __GNUC__ >= 3
#define FLEXIBLE
#else
#define FLEXIBLE 0
#endif

#if __GNUC__ >= 3
#define INTERRUPT_SHARED volatile
#else
#define INTERRUPT_SHARED
#endif

#endif /* COMMON_H */
