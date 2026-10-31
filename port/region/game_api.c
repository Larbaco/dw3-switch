/* P6.24: the game copy's table for the host (port/region/dw3_game_api.h).
 *
 * Compiled once per region into a `DW3_VERSION=both` image, with that
 * region's macros, next to the game it describes. After `ld -r` it is the only
 * global symbol of the copy (tools/localize_region_obj.py renames it
 * `us_dw3_game_api` / `eu_dw3_game_api`), so it names exactly what the host may
 * call: nothing else of the game is visible to the host or to the other
 * region. It includes no decomp header; the few game arrays it points at are
 * declared by hand with the types the game defines them with. */
#ifndef PORT_HOST
#error "port/region/game_api.c is host-only"
#endif

#include "dw3_game_api.h"
#include "boot_tim_host.h"
#include "map_view.h"
#include "overlay_host.h"
#include "port_battle_options.h"
#include "port_button_map.h"
#include "port_options.h"
#include "port_screen_fade.h"
#include "port_turbo.h"
#include "port_audio.h"
#ifdef DW3_CHEATS
#include "port_cheats.h"
#endif

#if defined(VERSION_EU) && VERSION_EU
#define DW3_THIS_REGION DW3_REGION_EU
#define DW3_REGISTER_TITLE dw3_overlay_register_eu_title
#elif defined(VERSION_US) && VERSION_US
#define DW3_THIS_REGION DW3_REGION_US
#define DW3_REGISTER_TITLE dw3_overlay_register_us_title
#else
#error "game_api.c needs VERSION_US or VERSION_EU"
#endif

int dw3_game_main(void);

#if defined(VERSION_EU) && VERSION_EU
extern int32_t LANGUAGE; /* src/engine/data/eu_settings.c, set by CNTY_SEL */
#define DW3_LANGUAGE_PTR &LANGUAGE
#else
#define DW3_LANGUAGE_PTR 0
#endif

/* game: engine/file/file_table.c, engine heap, port/overlays/host_heap.c */
extern int FILE_SECTORS[];
extern unsigned short FILE_SECTOR_COUNTS[];
extern int FILE_COUNT;
extern unsigned char HEAP[];
extern unsigned char DW3_HOST_HEAP[];
extern const unsigned long DW3_HOST_HEAP_SIZE;

const Dw3GameApi dw3_game_api = {
    DW3_GAME_API_ABI,
    DW3_THIS_REGION,
    dw3_game_main,
    dw3_host_load_boot_tim,
    DW3_REGISTER_TITLE,
    dw3_overlay_current_mode_file,
    dw3_overlay_current_sub_file,
    dw3_overlay_file_name,
    dw3_overlay_set_strict,
    dw3_map_view_init,
    dw3_map_view_precompute_run,
    dw3_map_view_precompute_cancel,
    dw3_map_view_set_zoom_stick,
    dw3_map_view_get_draw,
    dw3_map_view_tex_taken,
    dw3_port_options_persist_init,
    dw3_port_options_flush,
    dw3_port_options_is_open,
    dw3_port_options_in_game,
    dw3_port_battle_options_holding,
    dw3_port_options_level,
    dw3_port_options_locked_mask_for,
    dw3_port_options_note_shown,
    dw3_port_screen_fade_level,
    dw3_port_turbo_speed,
    dw3_port_turbo_present_due,
    dw3_port_turbo_rate_x2,
#ifdef DW3_CHEATS
    dw3_cheats_soak_arm,
#else
    0,
#endif
    dw3_port_audio_health_exit,
    dw3_button_map_apply,
    FILE_SECTORS,
    FILE_SECTOR_COUNTS,
    &FILE_COUNT,
    DW3_LANGUAGE_PTR,
    HEAP,
    DW3_HOST_HEAP,
    &DW3_HOST_HEAP_SIZE,
};
