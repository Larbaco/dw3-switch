/* P6.24: the one door between the host and a game image.
 *
 * `DW3_VERSION=both` links the game side twice (once built as VERSION_US, once
 * as VERSION_EU) into one program. Each copy is a relocatable object whose
 * only global symbol is its `dw3_game_api` table, renamed `us_dw3_game_api` /
 * `eu_dw3_game_api` (tools/localize_region_obj.py); everything else in it is
 * local. The host (PSY-Z, the platform backend, cdfs, memcard, movies) is
 * linked once, picks the running region at boot (dw3_region_rt.h) and reaches
 * the game only through this table.
 *
 * The plain `us` and `eu` images do not build the table; the host macros below
 * do nothing there and the host calls the game's functions directly, as it
 * always did. See docs/SINGLE_BUILD.md, "As built (slice 3)".
 *
 * game_api.c (per region) fills the table; host code includes this header
 * AFTER the headers that declare the functions it calls. */
#ifndef DW3_GAME_API_H
#define DW3_GAME_API_H

#include <stdint.h>

#include "dw3_region.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Dw3MapViewDraw;

#define DW3_GAME_API_ABI 6

typedef struct Dw3GameApi {
    unsigned abi;       /* DW3_GAME_API_ABI */
    Dw3Region region;   /* the region this copy was built for */

    int (*game_main)(void);
    int (*host_load_boot_tim)(const char *path);
    void (*overlay_register_title)(void);

    /* overlay host (port/overlays/overlay_host.h): the Switch backend and the
     * crash report ask which mode and stage file are loaded */
    int32_t (*overlay_current_mode_file)(void);
    int32_t (*overlay_current_sub_file)(void);
    const char *(*overlay_file_name)(int32_t file_id);
    void (*overlay_set_strict)(int on);

    /* P6.32 map view */
    void (*map_view_init)(const char *config_dir);
    void (*map_view_precompute_run)(void);
    void (*map_view_precompute_cancel)(void); /* ABI 5: BUG-1, stop it on the way out */
    void (*map_view_set_zoom_stick)(int raw_up);
    void (*map_view_get_draw)(struct Dw3MapViewDraw *d);
    void (*map_view_tex_taken)(unsigned gen);

    /* port options */
    int (*port_options_persist_init)(const char *dir);
    void (*port_options_flush)(void);
    int (*port_options_is_open)(void);
    int (*port_options_in_game)(void);
    int (*port_battle_options_holding)(void);
    int (*port_options_level)(int index);
    unsigned (*port_options_locked_mask_for)(int in_game);
    void (*port_options_note_shown)(void);
    int (*port_options_bg_phase)(void); /* ABI 6: D4, the menu background's scroll */
    int (*port_screen_fade_level)(void);
    int (*port_turbo_speed)(void);
    int (*port_turbo_present_due)(void);
    int (*port_turbo_rate_x2)(void);
    void (*cheats_soak_arm)(const char *spec); /* NULL without DW3_CHEATS */
    void (*port_audio_health_exit)(void); /* P4.3 run-total line */

    /* P9.8 confirm / cancel layout (port/options/button_map.c): the pad host
     * passes port 1's raw pad word (16 bits, a bit clear while pressed) through
     * the map of the running region's options. ABI 4 added it. */
    unsigned (*button_map_apply)(unsigned word);

    /* data of the game image the host reads (runtime extraction, memory
     * sampler); pointers, because the arrays live in the localized copy */
    const int *file_sectors;
    const unsigned short *file_sector_counts;
    const int *file_count;
    const int32_t *language;          /* EU: LANGUAGE (set by CNTY_SEL); NULL in the US copy */
    const void *heap;                 /* the game's HEAP (HeapView ABI) */
    unsigned char *host_heap;         /* DW3_HOST_HEAP arena */
    const unsigned long *host_heap_size;
} Dw3GameApi;

/* Defined by game_api.c in each game copy of a `both` image, where the symbol
 * is renamed per region by tools/localize_region_obj.py. The plain `us` / `eu`
 * images do not build game_api.c. */
extern const Dw3GameApi dw3_game_api;

#if defined(DW3_BOTH) && DW3_BOTH
/* The running region's table. Valid after dw3_region_rt_select(). */
const Dw3GameApi *dw3_game(void);
#define DW3_API(member) (dw3_game()->member)
#endif

#if defined(DW3_BOTH) && DW3_BOTH
/* Host code (compiled with DW3_BOTH) calls the game through the table. The
 * functions keep their names; only the call changes. Include after the
 * headers that declare them. */
#define dw3_game_main() DW3_API(game_main)()
#define dw3_host_load_boot_tim(path) DW3_API(host_load_boot_tim)(path)
#define dw3_map_view_init(dir) DW3_API(map_view_init)(dir)
#define dw3_map_view_precompute_run() DW3_API(map_view_precompute_run)()
#define dw3_map_view_precompute_cancel() DW3_API(map_view_precompute_cancel)()
#define dw3_map_view_set_zoom_stick(v) DW3_API(map_view_set_zoom_stick)(v)
#define dw3_map_view_get_draw(d) DW3_API(map_view_get_draw)(d)
#define dw3_map_view_tex_taken(g) DW3_API(map_view_tex_taken)(g)
#define dw3_port_options_persist_init(d) DW3_API(port_options_persist_init)(d)
#define dw3_port_options_flush() DW3_API(port_options_flush)()
#define dw3_port_options_is_open() DW3_API(port_options_is_open)()
#define dw3_port_options_in_game() DW3_API(port_options_in_game)()
#define dw3_port_battle_options_holding() DW3_API(port_battle_options_holding)()
#define dw3_overlay_current_mode_file() DW3_API(overlay_current_mode_file)()
#define dw3_overlay_current_sub_file() DW3_API(overlay_current_sub_file)()
#define dw3_overlay_file_name(id) DW3_API(overlay_file_name)(id)
#define dw3_overlay_set_strict(on) DW3_API(overlay_set_strict)(on)
#define dw3_port_options_level(i) DW3_API(port_options_level)(i)
#define dw3_port_options_locked_mask_for(m) \
    DW3_API(port_options_locked_mask_for)(m)
#define dw3_port_options_note_shown() DW3_API(port_options_note_shown)()
#define dw3_port_options_bg_phase() DW3_API(port_options_bg_phase)()
#define dw3_port_screen_fade_level() DW3_API(port_screen_fade_level)()
#define dw3_port_turbo_speed() DW3_API(port_turbo_speed)()
#define dw3_port_turbo_present_due() DW3_API(port_turbo_present_due)()
#define dw3_port_turbo_rate_x2() DW3_API(port_turbo_rate_x2)()
#define dw3_cheats_soak_arm(s) DW3_API(cheats_soak_arm)(s)
#define dw3_port_audio_health_exit() DW3_API(port_audio_health_exit)()
#define dw3_button_map_apply(w) DW3_API(button_map_apply)(w)
#endif

#ifdef __cplusplus
}
#endif

#endif /* DW3_GAME_API_H */
