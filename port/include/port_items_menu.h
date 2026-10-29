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

/* ---- P6.34 phase 1: equip from the Items menu (patch 0188) ----------------
 * Design: docs/P6.34_MENU_FLOW.md. Cross on a weapon, armor or accessory in
 * the Items menu's lists opens the partner step the consumables use; Cross on a
 * partner equips through the game's STSTATUS_equip. Defaults are the
 * orchestrator's, pending owner confirmation (section 12 of the design). */

/* Key of port_options.cfg (file-only until the Menus group of phase 5): 0 or
 * 1, default 1. DW3_ITEMS_EQUIP_FROM_ITEMS overrides the file for tests. With 0
 * the Items menu is the original game's and patch 0180's line is logged. */
#define DW3_ITEMS_EQUIP_FROM_ITEMS_KEY "items_equip_from_items"
int dw3_port_items_equip_enabled(void);

/* EQUIP_KIND_* of engine/game_state.h */
enum {
    DW3_EQUIP_RIGHT_HAND = 1,
    DW3_EQUIP_LEFT_HAND = 2,
    DW3_EQUIP_EITHER_HAND = 3,
    DW3_EQUIP_HEAD = 4,
    DW3_EQUIP_BODY = 5,
    DW3_EQUIP_ACCESSORY = 6,
    DW3_EQUIP_BOTH_HANDS = 7,
    DW3_EQUIP_GROUP_ACCESSORY = 8
};

/* Cross on `item` (record kind `kind`, `bag` copies in GAME.items) in the
 * Items menu's list `list`. Returns 1 when the partner step should open (and
 * logs `equip-from-items open`), 0 when the Items menu stays as it is: the key
 * is off (logs patch 0180's line), the kind is not equipment, or no copy is in
 * the bag (the list also shows worn-only items; logs `refused`). */
int dw3_port_items_equip_open(int list, int item, int kind, int bag);

/* The slot (0 head, 1 body, 2 / 3 hands, 4 / 5 accessories) an item of `kind`
 * and group `group` goes to, given the ids worn in the partner's six slots
 * (`worn`, 0 for none) and the group of each worn accessory (`worn_group`,
 * only read for slots 4 and 5). Pure: no game structs. Returns -1 for a kind
 * it does not know. `*rule` is one of head, body, hand-empty, hand-replace,
 * both-hands, acc-empty, acc-replace, group. */
int dw3_port_items_equip_slot(int kind, int group, const int worn[6], const int worn_group[6],
                              const char **rule);

/* Snapshot of the bag and worn totals, taken just before STSTATUS_equip; the
 * next `done` line shows both. */
void dw3_port_items_equip_begin(void);

/* The partner step refused: `reason` is partner-mask, unmapped-kind or
 * not-in-bag. Logs one line; the game state is untouched. */
void dw3_port_items_equip_refused(int partner, int item, const char *reason);

/* The slot chosen, before the equip: `replaces` / `replaces2` are the ids the
 * equip returns to the bag (0 for none; replaces2 only for both hands). */
void dw3_port_items_equip_chosen(int slot, const char *rule, int replaces, int replaces2);

/* After STSTATUS_equip: logs `done` with the bag and worn totals before and
 * after, and arms the `list rebuilt` line of the next reopen of the list. */
void dw3_port_items_equip_done(int partner, int slot, int item);

/* The partner step was left with Triangle (only logged for an equipment item). */
void dw3_port_items_equip_cancelled(void);

/* The list was rebuilt after a use or an equip (STSTATUS_confirmItemUsed):
 * logs `list rebuilt` once, only after an equip. `cursor` is the index of the
 * item in the new list, -1 when it left it. */
void dw3_port_items_equip_list_rebuilt(int count, int cursor);

/* Game-font text (digits 4..13, A..Z 14..39, a..z 40..65, 01 01 a space, 02 01
 * a new line; the encoding of dw3_port_field_return_label) for the result line.
 * `which`: 0 "<a> equipped by <b>" plus, when `c` is not NULL, a second line
 * "<c> returned to the bag"; 1 "<a> cannot equip this". a, b, c are game-font
 * strings (item and partner names); `language` is the EU LANGUAGE (2 EN, 3 FR,
 * 4 IT, 5 DE, 6 ES; the US disc is 2). Returns the byte count (excluding the
 * terminating 0). The FR, DE, IT and ES words are drafts that need a native
 * review before a non-preview release. */
enum { DW3_ITEMS_MSG_DONE = 0, DW3_ITEMS_MSG_REFUSED = 1 };
int dw3_port_items_equip_message(char *out, int cap, int language, int which, const char *a,
                                 const char *b, const char *c);


/* ---- P6.34 phase 2: equip without the long round trip (patch 0190) --------
 * Design: docs/P6.34_MENU_FLOW.md (phase 2, owner question 6, orchestrator
 * default pending confirmation). In Status > Equipment, Cross on an item equips
 * it and the panel returns to the slot list with the panel fades shortened to
 * one frame each (the original game fades the item list, the slot's item and
 * the title out and the slots in, about 70 frames), the slot cursor on the slot
 * just changed. The next list opens on the row of the item worn in that slot
 * when a spare copy of it is in the list, else on "none" as in the original. */

/* Key of port_options.cfg (file-only): 0 or 1, default 1. DW3_ITEMS_EQUIP_STAYS
 * overrides the file for tests. With 0 the panel is the original game's. */
#define DW3_ITEMS_EQUIP_STAYS_KEY "items_equip_stays"
int dw3_port_items_equip_stays_enabled(void);

/* The row of `worn` in the list `items[0..count)` (row 0 is "none", -1), or 0
 * when it is not there (no spare copy) or nothing is worn. Pure. */
int dw3_port_items_stays_row(const short *items, int count, int worn);

/* The first row shown when the list of `count` rows opens with the cursor on
 * `row`: 0 while the row is on the first page of eight, else the page that
 * keeps three rows above it, never past the last page. Pure. */
int dw3_port_items_stays_scroll(int row, int count);

/* Cross equipped `item` in `slot` of `partner`: the quick return starts. Logs
 * `equip-stays begin` with the bag and worn totals. */
void dw3_port_items_stays_begin(int partner, int slot, int item);

/* The panel is back on the slot list after `frames` frames. Logs
 * `equip-stays slot=.. after=equip frames=..` (the original takes about 70). */
void dw3_port_items_stays_landed(int slot, int frames);

/* The list opened for `slot` with the cursor on `cursor` and the page at
 * `scroll` (row = cursor + scroll, `item` the id there, -1 for none). */
void dw3_port_items_stays_list_open(int slot, int row, int cursor, int scroll, int item, int count);

#ifdef __cplusplus
}
#endif

#endif /* DW3_PORT_ITEMS_MENU_H */
