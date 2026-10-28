/* P6.33 Items menu usability: the policy behind the hooks in the equip panel
 * (patch 0128 partner cycle). Policy and logging in
 * port/items_menu/items_menu.c; design and owner decisions:
 * docs/P6.33_ITEMS_MENU.md. Nothing here touches the save: the panel's state
 * lives in the menu tasks, which are rebuilt on every open. */
#ifndef DW3_PORT_ITEMS_MENU_H
#define DW3_PORT_ITEMS_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

/* Key of port_options.cfg. It has no row on the Options screen (the
 * window is full at nine rows), so it is a file-only key: 0 or 1, default 1
 * (the owner decided "on by default", 2026-10-26). The environment variable
 * DW3_ITEMS_PARTNER_SWITCH overrides the file for tests. */
#define DW3_ITEMS_PARTNER_SWITCH_KEY "items_partner_switch"

/* ZL / ZR (L2 / R2) on the equip panel's slot list change the partner. */
int dw3_port_items_partner_switch_enabled(void);

/* The party member `dir` steps (-1 previous, +1 next) from `member` in a party
 * of `count`, wrapping at both ends. With fewer than two members, or a
 * `member` outside the party, it returns `member` (no change). */
int dw3_port_items_partner_next(int member, int count, int dir);

/* The partner moved from party member `from` to `to` (partner id `partner`).
 * Logs the totals of GAME.items and GAME.equippedItems: a switch must not
 * change them. */
void dw3_port_items_partner_switched(int from, int to, int partner);

/* Cross was pressed on an item of the Items menu's list that the game does
 * not act on (patch 0180): `list` is the option (0 the consumables, 1..4 the
 * others), `item` its id, `usable_list` 1 for a list other than the first,
 * `equip_kind` 3 weapon, 4 armor, 5 accessory, 0 none. Logs one
 * `dw3-items-menu:` line saying what the original game allows; changes
 * nothing. */
void dw3_port_items_menu_selected(int list, int item, int other_list, int equip_kind);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_ITEMS_MENU_H */
