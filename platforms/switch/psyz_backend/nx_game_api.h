/* P6.24: include this LAST in a Switch backend file that calls into the game
 * (port options, overlay host, map view, turbo ...). In a plain build it does
 * nothing. In a `DW3_VERSION=both` image it redirects those calls to the
 * running region's game copy through dw3_game_api.h, so it has to come after
 * the headers that declare them. */
#ifndef DW3_NX_GAME_API_H
#define DW3_NX_GAME_API_H

#if defined(DW3_BOTH) && DW3_BOTH
#include "dw3_game_api.h"
#endif

#endif /* DW3_NX_GAME_API_H */
