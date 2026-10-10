#ifndef PORT_LIBPAD_H
#define PORT_LIBPAD_H

/*
 * PSY-Z exposes advanced pad APIs via libetc.h but does not ship libpad.h.
 * Provide the constants the decomp engine expects and pull prototypes from
 * PSY-Z so <libpad.h> resolves without the Sony psyq_headers NULL=0 clash.
 */

#include <libetc.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define PadStateDiscon 0
#define PadStateFindPad 1
#define PadStateFindCTP1 2
#define PadStateFindCTP2 3
#define PadStateReqInfo 4
#define PadStateExecCmd 5
#define PadStateStable 6

#define InfoModeCurID 1
#define InfoModeCurExID 2
#define InfoModeCurExOffs 3
#define InfoModeIdTable 4

#define InfoActFunc 1
#define InfoActSub 2
#define InfoActSize 3
#define InfoActCurr 4
#define InfoActSign 5

#define PadMaxCurr 60
#define PadCurrCTP1 10

#endif /* PORT_LIBPAD_H */
