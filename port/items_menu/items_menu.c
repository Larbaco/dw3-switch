/* P6.33 Items menu usability (docs/P6.33_ITEMS_MENU.md). Policy for the
 * equip panel hook of patch 0128 (partner cycle). On by default, a file-only
 * key of port_options.cfg. */
#include "port_items_menu.h"

#include "port_options.h"

/* not <stdlib.h>: engine/game.h declares its own random() and atoi() */
extern char *getenv(const char *name);

#ifdef DW3_ITEMS_MENU_TEST
#include <stdio.h>
#define IM_LOG(...)                    \
    do {                               \
        fprintf(stderr, __VA_ARGS__);  \
        fputc('\n', stderr);           \
    } while (0)
extern int dw3_test_items_total;
extern int dw3_test_equipped_total;
#define IM_ITEMS_TOTAL() dw3_test_items_total
#define IM_EQUIPPED_TOTAL() dw3_test_equipped_total
#else
#include "engine/game.h"
#include <psyz/log.h>
/* psyz_log, not stderr: on Switch stderr only reaches nxlink, and these lines
 * are the check in sdmc:/switch/dw3/dw3.log. */
#define IM_LOG(...) INFOF(__VA_ARGS__)
static int im_sum(const s8 *counts) {
    int total = 0;
    int i;

    for (i = 0; i < 0x193; i++) {
        total += counts[i];
    }
    return total;
}
#define IM_ITEMS_TOTAL() im_sum(GAME.items)
#define IM_EQUIPPED_TOTAL() im_sum(GAME.equippedItems)
#endif

/* 1 unless the environment variable `env` or the file key `key` says 0. */
static int im_flag(const char *env, const char *key) {
    const char *v = getenv(env);

    if (v != NULL && v[0] != '\0') {
        return v[0] != '0';
    }
    return dw3_port_options_file_int(key, 1) != 0;
}

int dw3_port_items_partner_switch_enabled(void) {
    return im_flag("DW3_ITEMS_PARTNER_SWITCH", DW3_ITEMS_PARTNER_SWITCH_KEY);
}

int dw3_port_items_partner_next(int member, int count, int dir) {
    if (count < 2 || member < 0 || member >= count) {
        return member;
    }
    member += dir < 0 ? -1 : 1;
    if (member < 0) {
        member = count - 1;
    } else if (member >= count) {
        member = 0;
    }
    return member;
}

void dw3_port_items_partner_switched(int from, int to, int partner) {
    IM_LOG("dw3-items-menu: partner switch member %d -> %d (partner %d) bag=%d worn=%d",
           from, to, partner, IM_ITEMS_TOTAL(), IM_EQUIPPED_TOTAL());
}

void dw3_port_items_menu_selected(int list, int item, int other_list, int equip_kind) {
    const char *what = equip_kind == 3 ? "weapon" : equip_kind == 4 ? "armor" : equip_kind == 5 ? "accessory" : "item";

    if (equip_kind != 0) {
        IM_LOG("dw3-items-menu: Cross on %s %d (list %d): the original game does nothing here "
               "(the Items menu only uses consumables; equip from Status > Equipment, or see the "
               "P6.34 proposal). Not a port regression (items_partner_switch=%d)",
               what, item, list, dw3_port_items_partner_switch_enabled());
    } else {
        IM_LOG("dw3-items-menu: Cross on %s %d (list %d%s): the original game does nothing here "
               "(%s)", what, item, list, other_list ? ", not the consumables list" : "",
               other_list ? "only the first list opens the use-on-a-partner step"
                          : "no use-from-menu flag on this item");
    }
}

/* ---- P6.34 phase 1: equip from the Items menu (patch 0188) --------------- */

int dw3_port_items_equip_enabled(void) {
    return im_flag("DW3_ITEMS_EQUIP_FROM_ITEMS", DW3_ITEMS_EQUIP_FROM_ITEMS_KEY);
}

static const char *im_kind_name(int kind) {
    switch (kind) {
    case DW3_EQUIP_HEAD:
    case DW3_EQUIP_BODY:
        return "armor";
    case DW3_EQUIP_ACCESSORY:
    case DW3_EQUIP_GROUP_ACCESSORY:
        return "accessory";
    case DW3_EQUIP_RIGHT_HAND:
    case DW3_EQUIP_LEFT_HAND:
    case DW3_EQUIP_EITHER_HAND:
    case DW3_EQUIP_BOTH_HANDS:
        return "weapon";
    default:
        return "unmapped";
    }
}

int dw3_port_items_equip_slot(int kind, int group, const int worn[6], const int worn_group[6],
                              const char **rule) {
    const char *r = "unmapped";
    int slot = -1;

    switch (kind) {
    case DW3_EQUIP_HEAD:
        slot = 0;
        r = "head";
        break;
    case DW3_EQUIP_BODY:
        slot = 1;
        r = "body";
        break;
    case DW3_EQUIP_RIGHT_HAND:
        slot = 2;
        r = worn[2] == 0 ? "hand-empty" : "hand-replace";
        break;
    case DW3_EQUIP_LEFT_HAND:
        slot = 3;
        r = worn[3] == 0 ? "hand-empty" : "hand-replace";
        break;
    case DW3_EQUIP_EITHER_HAND:
        if (worn[2] == 0) {
            slot = 2;
            r = "hand-empty";
        } else if (worn[3] == 0) {
            slot = 3;
            r = "hand-empty";
        } else {
            slot = 2;
            r = "hand-replace";
        }
        break;
    case DW3_EQUIP_BOTH_HANDS:
        slot = 2;
        r = "both-hands";
        break;
    case DW3_EQUIP_ACCESSORY:
        if (worn[4] == 0) {
            slot = 4;
            r = "acc-empty";
        } else if (worn[5] == 0) {
            slot = 5;
            r = "acc-empty";
        } else {
            slot = 4;
            r = "acc-replace";
        }
        break;
    case DW3_EQUIP_GROUP_ACCESSORY:
        /* STSTATUS_equip empties every worn accessory of the same group, so
         * the item goes where its group mate is */
        if (worn[4] != 0 && worn_group[4] == group) {
            slot = 4;
            r = "group";
        } else if (worn[5] != 0 && worn_group[5] == group) {
            slot = 5;
            r = "group";
        } else if (worn[4] == 0) {
            slot = 4;
            r = "acc-empty";
        } else if (worn[5] == 0) {
            slot = 5;
            r = "acc-empty";
        } else {
            slot = 4;
            r = "acc-replace";
        }
        break;
    default:
        break;
    }
    if (rule != NULL) {
        *rule = r;
    }
    return slot;
}

static int g_im_bag0;
static int g_im_worn0;
static int g_im_rebuild_armed;

int dw3_port_items_equip_open(int list, int item, int kind, int bag) {
    if (!dw3_port_items_equip_enabled()) {
        return 0;
    }
    if (kind < DW3_EQUIP_RIGHT_HAND || kind > DW3_EQUIP_GROUP_ACCESSORY) {
        IM_LOG("dw3-items-menu: equip-from-items refused partner=-1 item=%d reason=unmapped-kind (kind %d, list %d)",
               item, kind, list);
        return 0;
    }
    if (bag <= 0) {
        IM_LOG("dw3-items-menu: equip-from-items refused partner=-1 item=%d reason=not-in-bag "
               "(only worn copies; unequip in Status > Equipment)", item);
        return 0;
    }
    IM_LOG("dw3-items-menu: equip-from-items open item=%d kind=%s list=%d", item, im_kind_name(kind), list);
    return 1;
}

void dw3_port_items_equip_begin(void) {
    g_im_bag0 = IM_ITEMS_TOTAL();
    g_im_worn0 = IM_EQUIPPED_TOTAL();
}

void dw3_port_items_equip_refused(int partner, int item, const char *reason) {
    IM_LOG("dw3-items-menu: equip-from-items refused partner=%d item=%d reason=%s", partner, item, reason);
}

void dw3_port_items_equip_chosen(int slot, const char *rule, int replaces, int replaces2) {
    if (replaces2 != 0 && replaces2 != replaces) {
        IM_LOG("dw3-items-menu: equip-from-items slot=%d rule=%s replaces=%d replaces2=%d", slot, rule, replaces,
               replaces2);
    } else {
        IM_LOG("dw3-items-menu: equip-from-items slot=%d rule=%s replaces=%d", slot, rule, replaces);
    }
}

void dw3_port_items_equip_done(int partner, int slot, int item) {
    int bag1 = IM_ITEMS_TOTAL();
    int worn1 = IM_EQUIPPED_TOTAL();

    IM_LOG("dw3-items-menu: equip-from-items done partner=%d slot=%d item=%d bag=%d->%d worn=%d->%d", partner,
           slot, item, g_im_bag0, bag1, g_im_worn0, worn1);
    g_im_rebuild_armed = 1;
}

void dw3_port_items_equip_cancelled(void) {
    IM_LOG("dw3-items-menu: equip-from-items cancelled at=partner");
}

void dw3_port_items_equip_list_rebuilt(int count, int cursor) {
    if (!g_im_rebuild_armed) {
        return;
    }
    g_im_rebuild_armed = 0;
    IM_LOG("dw3-items-menu: equip-from-items list rebuilt count=%d cursor=%d", count, cursor);
}

/* language: 2 EN, 3 FR, 4 IT, 5 DE, 6 ES (the EU disc's LANGUAGE). Capitals,
 * lower case, digits and spaces only: those are the glyphs verified in both
 * font sheets (docs/P6.34_MENU_FLOW.md section 12), so accents and
 * punctuation are left out of the drafts. */
typedef struct ImWords {
    const char *equipped_by; /* "<item> equipped by <partner>" */
    const char *returned;    /* "<item> returned to the bag" */
    const char *cannot;      /* "<partner> cannot equip this" */
} ImWords;

static const ImWords k_words[5] = {
    {" equipped by ", " returned to the bag", " cannot equip this"},              /* EN */
    {" \303\251quip\303\251 par ", " remis dans le sac", " ne peut pas l'\303\251quiper"}, /* FR */
    {" equipaggiato da ", " rimesso nella borsa", " non pu\303\262 equipaggiarlo"},  /* IT */
    {" ausger\303\274stet von ", " zur\303\274ck im Beutel", " kann das nicht tragen"},  /* DE */
    {" equipado por ", " devuelto a la bolsa", " no puede equiparlo"},            /* ES */
};

typedef struct ImOut {
    char *p;
    char *end;
} ImOut;

static void im_put(ImOut *o, char c) {
    if (o->p < o->end) {
        *o->p++ = c;
    }
}

static void im_icon(ImOut *o, int n) {
    im_put(o, 1);
    im_put(o, (char)n);
}

/* UTF-8 (Latin-1 range) in, game-font bytes out. Letters and digits are
 * glyphs; the space, '.', the apostrophe and the accents the drafts use are
 * icons (01 n): 1 space, 5 period, 19 apostrophe, 81 e acute, 88 o grave,
 * 94 u diaeresis. Read off the EU sheet by a probe run; the US sheet has the
 * same first icons. */
static void im_text(ImOut *o, const char *s) {
    for (; *s != '\0'; s++) {
        unsigned char c = (unsigned char)*s;

        if (c >= 'A' && c <= 'Z') {
            im_put(o, (char)(14 + c - 'A'));
        } else if (c >= 'a' && c <= 'z') {
            im_put(o, (char)(40 + c - 'a'));
        } else if (c >= '0' && c <= '9') {
            im_put(o, (char)(4 + c - '0'));
        } else if (c == '.') {
            im_icon(o, 5);
        } else if (c == '\'') {
            im_icon(o, 19);
        } else if (c == 0xC3 && s[1] != '\0') {
            s++;
            switch ((unsigned char)*s) {
            case 0xA9: im_icon(o, 81); break;
            case 0xB2: im_icon(o, 88); break;
            case 0xBC: im_icon(o, 94); break;
            default: im_icon(o, 1); break;
            }
        } else {
            im_icon(o, 1);
        }
    }
}

static void im_name(ImOut *o, const char *s) {
    for (; s != NULL && *s != '\0'; s++) {
        im_put(o, *s);
    }
}

int dw3_port_items_equip_message(char *out, int cap, int language, int which, const char *a,
                                 const char *b, const char *c) {
    const ImWords *w = &k_words[language >= 3 && language <= 6 ? language - 2 : 0];
    ImOut o;

    if (cap < 1) {
        return 0;
    }
    o.p = out;
    o.end = out + cap - 1;
    if (which == DW3_ITEMS_MSG_REFUSED) {
        im_name(&o, a);
        im_text(&o, w->cannot);
        im_text(&o, ".");
    } else {
        im_name(&o, a);
        im_text(&o, w->equipped_by);
        im_name(&o, b);
        im_text(&o, ".");
        if (c != NULL && c[0] != '\0') {
            im_put(&o, 2); /* new line */
            im_put(&o, 1);
            im_name(&o, c);
            im_text(&o, w->returned);
            im_text(&o, ".");
        }
    }
    *o.p = '\0';
    return (int)(o.p - out);
}

/* ---- P6.34 phase 2: equip without the long round trip (patch 0190) ------- */

int dw3_port_items_equip_stays_enabled(void) {
    return im_flag("DW3_ITEMS_EQUIP_STAYS", DW3_ITEMS_EQUIP_STAYS_KEY);
}

int dw3_port_items_stays_row(const short *items, int count, int worn) {
    int i;

    if (worn <= 0 || items == NULL) {
        return 0;
    }
    for (i = 1; i < count; i++) {
        if (items[i] == worn) {
            return i;
        }
    }
    return 0;
}

int dw3_port_items_stays_scroll(int row, int count) {
    int scroll;

    if (count <= 8 || row < 8) {
        return 0;
    }
    scroll = row - 3;
    if (scroll > count - 8) {
        scroll = count - 8;
    }
    return scroll;
}

void dw3_port_items_stays_begin(int partner, int slot, int item) {
    IM_LOG("dw3-items-menu: equip-stays begin partner=%d slot=%d item=%d bag=%d worn=%d", partner, slot, item,
           IM_ITEMS_TOTAL(), IM_EQUIPPED_TOTAL());
}

void dw3_port_items_stays_landed(int slot, int frames) {
    IM_LOG("dw3-items-menu: equip-stays slot=%d after=equip frames=%d bag=%d worn=%d", slot, frames,
           IM_ITEMS_TOTAL(), IM_EQUIPPED_TOTAL());
}

void dw3_port_items_stays_list_open(int slot, int row, int cursor, int scroll, int item, int count) {
    IM_LOG("dw3-items-menu: equip-stays list-open slot=%d row=%d cursor=%d scroll=%d item=%d count=%d", slot, row,
           cursor, scroll, item, count);
}

/* ---- P6.34 phase 3: the whole bag in the equip list (patch 0192) --------- */

int dw3_port_items_bag_view_enabled(void) {
    return im_flag("DW3_ITEMS_BAG_VIEW", DW3_ITEMS_BAG_VIEW_KEY);
}

int dw3_port_items_bag_next(int filter) {
    if (filter < 0 || filter >= DW3_BAG_FILTERS - 1) {
        return 0;
    }
    return filter + 1;
}

int dw3_port_items_bag_fits(int slot, int kind) {
    switch (slot) {
    case 0:
        return kind == DW3_EQUIP_HEAD;
    case 1:
        return kind == DW3_EQUIP_BODY;
    case 2:
    case 3:
        return kind == DW3_EQUIP_RIGHT_HAND || kind == DW3_EQUIP_LEFT_HAND || kind == DW3_EQUIP_EITHER_HAND ||
               kind == DW3_EQUIP_BOTH_HANDS;
    case 4:
    case 5:
        return kind == DW3_EQUIP_ACCESSORY || kind == DW3_EQUIP_GROUP_ACCESSORY;
    default:
        return 0;
    }
}

/* Drafts for FR, IT, DE, ES: native review needed before a non-preview release.
 * Same glyph rules as k_words (letters, digits, spaces, the icons of
 * docs/P6.34_MENU_FLOW.md section 12). */
typedef struct ImBagWords {
    const char *title[3]; /* weapons, armor and accessories, worn by others */
    const char *worn_by;  /* "Worn by " */
} ImBagWords;

static const ImBagWords k_bag_words[5] = {
    {{"All weapons", "Armor and gear", "Worn by others"}, "Worn by "},                              /* EN */
    {{"Toutes les armes", "Armure et objets", "Port\303\251s ailleurs"}, "Port\303\251 par "},       /* FR */
    {{"Tutte le armi", "Armature e altro", "Indossati altrove"}, "Indossato da "},                  /* IT */
    {{"Alle Waffen", "Alle R\303\274stungen", "Bei anderen"}, "Getragen von "},                     /* DE */
    {{"Todas las armas", "Armadura y extras", "Llevado por otros"}, "Equipado por "},               /* ES */
};

int dw3_port_items_bag_title(char *out, int cap, int language, int filter, int rows) {
    const ImBagWords *w = &k_bag_words[language >= 3 && language <= 6 ? language - 2 : 0];
    char dg[12];
    ImOut o;
    int n = 0;

    if (cap < 1) {
        return 0;
    }
    if (filter < 1 || filter > 3) {
        out[0] = '\0';
        return 0;
    }
    o.p = out;
    o.end = out + cap - 1;
    im_text(&o, w->title[filter - 1]);
    im_text(&o, " ");
    if (rows < 0) {
        rows = 0;
    }
    do {
        dg[n++] = (char)(rows % 10);
        rows /= 10;
    } while (rows > 0 && n < 10);
    while (n > 0) {
        im_put(&o, (char)(4 + dg[--n]));
    }
    *o.p = '\0';
    return (int)(o.p - out);
}

int dw3_port_items_bag_note(char *out, int cap, int language, const char *wearer) {
    const ImBagWords *w = &k_bag_words[language >= 3 && language <= 6 ? language - 2 : 0];
    ImOut o;

    if (cap < 1) {
        return 0;
    }
    o.p = out;
    o.end = out + cap - 1;
    im_icon(&o, 1);
    im_text(&o, w->worn_by);
    im_name(&o, wearer);
    *o.p = '\0';
    return (int)(o.p - out);
}

static const char *const k_bag_names[DW3_BAG_FILTERS] = {"slot", "weapons", "gear", "worn-by-others"};

void dw3_port_items_bag_viewed(int filter, int slot, int rows, int worn_elsewhere) {
    IM_LOG("dw3-items-menu: bag-view filter=%s slot=%d rows=%d worn-elsewhere=%d",
           k_bag_names[filter >= 0 && filter < DW3_BAG_FILTERS ? filter : 0], slot, rows, worn_elsewhere);
}

static int g_im_move_bag0;
static int g_im_move_worn0;

void dw3_port_items_bag_move_begin(void) {
    g_im_move_bag0 = IM_ITEMS_TOTAL();
    g_im_move_worn0 = IM_EQUIPPED_TOTAL();
}

void dw3_port_items_bag_moved(int item, int from, int from_slot, int to, int slot) {
    IM_LOG("dw3-items-menu: move item=%d from=%d from-slot=%d to=%d slot=%d bag=%d->%d worn=%d->%d", item, from,
           from_slot, to, slot, g_im_move_bag0, IM_ITEMS_TOTAL(), g_im_move_worn0, IM_EQUIPPED_TOTAL());
}
