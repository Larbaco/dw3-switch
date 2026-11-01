/* Native-resolution PC title plaques. Geometry is authored in 960x720 pixels. */
#include "title_overlay_art.h"
#include "menu_bg_tile.h"
#include "port_options_groups.h"
#include "port_map_buttons.h"
#include "port_atlas.h"
#include "port_help.h"
#include "port_pages.h"
#include "port_version.h"
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include "title_overlay_labels.inc"
#include "title_overlay_lettering.inc"
static _Thread_local uint8_t *canvas;
static _Thread_local int stride;
static _Thread_local int clip_x0,clip_y0,clip_x1=960,clip_y1=720;
enum { LABEL_MAX_W = 440, LABEL_MAX_H = 48 };
/* Runtime label masks (raw coverage), the Options language and the footer. */
static uint8_t *label_mask[DW3_TITLE_LABEL_COUNT];
static int label_w[DW3_TITLE_LABEL_COUNT], label_h[DW3_TITLE_LABEL_COUNT];
static int options_lang = DW3_TITLE_LANG_EN;
static int footer_shown = 1;
/* P6.37: the port version in a corner of the title canvas (show_version=0 in
 * port_options.cfg hides it; the Options footer is always drawn). */
static int version_shown = 1;
/* P6.31: the title menu has the extra Load row (an auto-save file exists). */
static int load_row;
static unsigned art_generation;
static unsigned options_generation;
static unsigned dialog_language_generation;
static int option_cursor;
static int option_values[DW3_TITLE_OPTION_ROWS];
enum { OPTIONS_LAYER_FULL, OPTIONS_LAYER_STATIC, OPTIONS_LAYER_FOCUS,
       OPTIONS_LAYER_VALUE, OPTIONS_LAYER_DESCRIPTION, OPTIONS_LAYER_LABEL };
static _Thread_local int options_layer = OPTIONS_LAYER_FULL;
static _Thread_local int options_layer_row;
/* The static layer's version: bumped when the page, the cheats master switch
 * or (P9.7) the locked rows change, so the retained canvas rebuilds it. */
static unsigned options_layout;
/* P9.7: rows greyed in game (bit i = row i, bit 16 = the Cheats entry). */
static unsigned options_locked;
/* Which page the Options screen shows (the group list, a group's sub-screen,
 * P6.20 the Cheats pages) and whether the cheats master switch is on. */
static int options_page = DW3_TITLE_OPTIONS_PAGE_OPTIONS;
static int options_cheats_on;
/* The worker thread that composes the options window's static layer ahead of
 * time (title_options_cache.c) draws with its own canvas, clip and layout:
 * everything the renderer writes is thread-local, and the layout the worker
 * draws (locked rows, page, cheats switch) overrides the live one for that
 * thread only, so the main thread's view is never touched. */
static _Thread_local int ovr_on;
static _Thread_local unsigned ovr_locked;
static _Thread_local int ovr_page,ovr_cheats;
/* Which part of a sub-screen's static layer is drawn: 0 all, 1 the part shared by
 * every group (backdrop, message strip, hints), 2 the part of the open group
 * (header, plaques, labels) over a canvas that holds the shared part. */
static _Thread_local int static_part;
#define ELOCKED (ovr_on?ovr_locked:options_locked)
#define EPAGE (ovr_on?ovr_page:options_page)
#define ECHEATS (ovr_on?ovr_cheats:options_cheats_on)
static void pixel(int x,int y,int r,int g,int b,int alpha) {
    if(x<clip_x0||x>=clip_x1||y<clip_y0||y>=clip_y1||
       x<0||x>=960||y<0||y>=720||alpha<=0)return;
    uint8_t *p=canvas+y*stride+x*4;
    /* Opaque, or over a transparent pixel: the blend below gives exactly r,g,b. */
    if(alpha>=255||!p[3]){p[0]=r;p[1]=g;p[2]=b;p[3]=alpha>255?255:alpha;return;}
    int a=alpha+p[3]*(255-alpha)/255;
    if(!a)return;
    p[0]=(r*alpha+p[0]*p[3]*(255-alpha)/255)/a;
    p[1]=(g*alpha+p[1]*p[3]*(255-alpha)/255)/a;
    p[2]=(b*alpha+p[2]*p[3]*(255-alpha)/255)/a;p[3]=a;
}
static void box(int x,int y,int w,int h,int r,int g,int b,int a) {
    /* Clamp to the clip first: same pixels as testing every one, but a band
     * or a dirty rect no longer walks the whole box (title_options_cache.c
     * renders the static layer a band at a time). */
    int x0=x<clip_x0?clip_x0:x,y0=y<clip_y0?clip_y0:y;
    int x1=x+w>clip_x1?clip_x1:x+w,y1=y+h>clip_y1?clip_y1:y+h;
    if(x0<0){x0=0;}
    if(y0<0){y0=0;}
    if(x1>960){x1=960;}
    if(y1>720){y1=720;}
    if(a<=0)return;
    for(int j=y0;j<y1;j++) {
        uint8_t *p=canvas+(size_t)j*stride+(size_t)x0*4;
        if(a>=255){for(int i=x0;i<x1;i++,p+=4){p[0]=r;p[1]=g;p[2]=b;p[3]=255;}continue;}
        for(int i=x0;i<x1;i++,p+=4) {
            if(!p[3]){p[0]=r;p[1]=g;p[2]=b;p[3]=a;continue;}
            const int na=a+p[3]*(255-a)/255;
            p[0]=(r*a+p[0]*p[3]*(255-a)/255)/na;
            p[1]=(g*a+p[1]*p[3]*(255-a)/255)/na;
            p[2]=(b*a+p[2]*p[3]*(255-a)/255)/na;p[3]=na;
        }
    }
}
/* Clipped octagonal hardware silhouette, with a vertical metallic ramp. */
static void plaque(int x,int y,int w,int h,int cut,int r1,int g1,int b1,int r2,int g2,int b2,int a) {
    for(int j=0;j<h;j++) {
        int inset=j<cut?cut-j:(j>=h-cut?j-(h-cut)+1:0);
        float t=(float)j/(h-1);
        int r=r1+(r2-r1)*t,g=g1+(g2-g1)*t,b=b1+(b2-b1)*t;
        for(int i=inset;i<w-inset;i++)pixel(x+i,y+j,r,g,b,a);
    }
}
static void lamp(int x,int y,int focused) {
    int r=focused?255:204,g=focused?186:128,b=focused?66:37;
    if(focused)for(int j=-5;j<=5;j++)for(int i=-5;i<=5;i++){
        float d=sqrtf(i*i+j*j);if(d<5)pixel(x+i,y+j,255,121,25,(int)((5-d)*13));
    }
    box(x-4,y-4,9,9,6,16,32,255);box(x-3,y-3,7,7,52,83,116,255);
    box(x-2,y-2,5,5,r,g,b,255);box(x-1,y-2,3,1,255,focused?245:205,147,255);
}
static void text_mask(int center,int y,const unsigned char *runs,int w,int h,int focused) {
    int total=w*h,k=0,n=0;
    while(k<total){int count=runs[n++],a=runs[n++];for(int j=0;j<count;j++,k++){
        int x=center-w/2+k%w,yy=y+k/w;
        pixel(x+1,yy+2,2,7,19,a);pixel(x,yy,focused?236:181,focused?247:219,255,a);
    }}
}
/* Coverage masks are rasterized at their native display size, not upscaled. */
static void lettering(int cx,int y,const unsigned char *runs,int w,int h,int style,int ox,int oy) {
    int k=0,n=0;
    while(k<w*h) {
        int count=runs[n++],a=runs[n++];
        for(int j=0;j<count;j++,k++) {
            int yy=k/w,r,g,b;
            float t=(float)yy/(h?h:1);
            if(style==0) { r=5;g=12;b=27; }
            else if(style==1) { r=191-100*t;g=229-60*t;b=248-25*t; }
            else if(style==2) { r=255;g=216-91*t;b=119-81*t; }
            else { r=126;g=163;b=191; }
            pixel(cx-w/2+k%w+ox,y+yy+oy,r,g,b,a);
        }
    }
}
/* Raw (not run-length) coverage, as captured from the EU title bank. */
static void text_raw(int center,int y,const uint8_t *mask,int w,int h,int focused) {
    for(int k=0;k<w*h;k++){
        int a=mask[k],x=center-w/2+k%w,yy=y+k/w;
        if(!a)continue;
        pixel(x+1,yy+2,2,7,19,a);pixel(x,yy,focused?236:181,focused?247:219,255,a);
    }
}
static void lettering_raw(int cx,int y,const uint8_t *mask,int w,int h) {
    for(int k=0;k<w*h;k++){
        int a=mask[k],yy=k/w;
        float t=(float)yy/(h?h:1);
        if(a)pixel(cx-w/2+k%w,y+yy,191-100*t,229-60*t,248-25*t,a);
    }
}
int dw3_pc_title_overlay_set_label(int slot,const uint8_t *coverage,int w,int h) {
    uint8_t *copy=NULL;
    if(slot<0||slot>=DW3_TITLE_LABEL_COUNT)return -1;
    if(coverage) {
        if(w<=0||h<=0||w>LABEL_MAX_W||h>LABEL_MAX_H)return -1;
        copy=(uint8_t*)malloc((size_t)w*h);
        if(!copy)return -1;
        memcpy(copy,coverage,(size_t)w*h);
    }
    free(label_mask[slot]);
    label_mask[slot]=copy;label_w[slot]=copy?w:0;label_h[slot]=copy?h:0;
    art_generation++;
    return 0;
}
int dw3_pc_title_overlay_get_options_language(void) { return options_lang; }
void dw3_pc_title_overlay_set_options_language(int lang) {
    if(lang<DW3_TITLE_LANG_EN||lang>DW3_TITLE_LANG_IT)lang=DW3_TITLE_LANG_EN;
    if(lang!=options_lang){options_lang=lang;art_generation++;options_generation++;dialog_language_generation++;}
}
unsigned dw3_pc_dialog_language_generation(void) { return dialog_language_generation; }
void dw3_pc_title_overlay_set_footer(int show) {
    show=show?1:0;
    if(show!=footer_shown){footer_shown=show;art_generation++;}
}
int dw3_pc_title_overlay_footer_shown(void) { return footer_shown; }
void dw3_pc_title_overlay_set_version(int show) {
    show=show?1:0;
    version_shown=show; /* read by the hosts when they draw the label */
}
void dw3_pc_title_overlay_set_load_row(int on) {
    on=on?1:0;
    if(on!=load_row){load_row=on;art_generation++;}
}
void dw3_pc_title_overlay_set_options_view(int cursor,const int *values,int count) {
    int changed=cursor!=option_cursor;
    if(count>DW3_TITLE_OPTION_ROWS)count=DW3_TITLE_OPTION_ROWS;
    for(int i=0;i<count;i++)if(values[i]!=option_values[i]){option_values[i]=values[i];changed=1;}
    option_cursor=cursor;
    if(changed)options_generation++;
}
unsigned dw3_pc_title_overlay_generation(void) { return art_generation; }
unsigned dw3_pc_title_overlay_options_generation(void) { return options_generation; }
int dw3_pc_title_overlay_options_cursor(void) { return option_cursor; }
int dw3_pc_title_overlay_options_value(int row) {
    return row>=0&&row<DW3_TITLE_OPTION_ROWS?option_values[row]:0;
}
enum { OPT_LOCK_ROWS = 30 }; /* registry rows 0..29: the lock mask's width (D1, patch 0200, D5, P6.7); bit 30 is the Cheats entry */
void dw3_pc_title_overlay_set_options_locked(unsigned mask) {
    mask&=((1u<<OPT_LOCK_ROWS)-1u)|DW3_TITLE_LOCK_CHEATS;
    if(mask!=options_locked){options_locked=mask;options_layout++;options_generation++;}
}
unsigned dw3_pc_title_overlay_options_locked(void) { return options_locked; }
unsigned dw3_pc_title_overlay_options_layout(void) { return options_layout; }
int dw3_pc_title_overlay_options_desc_follows_value(int page) {
    int group,scroll;
    return dw3_optlist_decode(page,&group,&scroll)&&group==DW3_OPTGRP_CUSTOM;
}
int dw3_pc_title_overlay_options_dynamic_labels(int page) {
#ifdef DW3_CHEATS
    int group,scroll;
    return dw3_optlist_decode(page,&group,&scroll)&&group==DW3_OPTGRP_CHEATS;
#else
    (void)page;
    return 0;
#endif
}
/* The page whose static layer `page` shows: the Cheats sub-screen scrolls over
 * one layer (its plaques are the same at every offset; labels and arrows are
 * drawn per update), the other pages have a layer per scroll offset. */
int dw3_pc_title_overlay_options_is_group_page(int page) {
    int group,scroll;
    return dw3_optlist_decode(page,&group,&scroll)&&group>=0;
}
int dw3_pc_title_overlay_options_static_page(int page) {
    return dw3_pc_title_overlay_options_dynamic_labels(page)?dw3_optlist_page(DW3_OPTGRP_CHEATS,0):page;
}
void dw3_pc_title_overlay_set_options_page(int page,int cheats_on) {
    int group,scroll;
    if(!dw3_optlist_decode(page,&group,&scroll))page=DW3_TITLE_OPTIONS_PAGE_OPTIONS;
    cheats_on=cheats_on?1:0;
    if(page!=options_page){
        const int relayout=dw3_pc_title_overlay_options_static_page(page)!=
                           dw3_pc_title_overlay_options_static_page(options_page);
        options_page=page;options_cheats_on=cheats_on;
        if(relayout)options_layout++;
        options_generation++;
    }else if(cheats_on!=options_cheats_on){
        /* The master switch on the Cheats sub-screen only greys labels, which
         * are drawn per row: a dirty-row update, not a new static layer. */
        options_cheats_on=cheats_on;
        if(!dw3_pc_title_overlay_options_dynamic_labels(page))options_layout++;
        options_generation++;
    }
}

/* --- Port options screen ---------------------------------------------------
 * Modelled on the game's own windows (the memory card screen): a white bezel
 * with a dark rim around a navy body, a stepped blue rule with the window
 * title at its right, plain white text, a white pointer on the focused choice,
 * a cyan rule above a message line that explains the focused choice. */
typedef struct Word { const unsigned char *runs; int w, h, top; } Word;
#define WORD(n) {title_##n, title_##n##_width, title_##n##_height, title_##n##_top}
/* x is the left edge (align 0) or the right edge (align 1); y is the text
 * origin, so words drawn at one y share a baseline. */
static void word(const Word *t,int x,int y,int align,int r,int g,int b) {
    int k=0,n=0,left=align?x-t->w:x;
    while(k<t->w*t->h){int count=t->runs[n++],a=t->runs[n++];
        for(int j=0;j<count;j++,k++)pixel(left+k%t->w,y+t->top+k/t->w,r,g,b,a);}
}
/* Antialiased thick segment (distance to the segment, half-width rad). */
static void segment(float x0,float y0,float x1,float y1,float rad,int r,int g,int b) {
    float dx=x1-x0,dy=y1-y0,len2=dx*dx+dy*dy;
    int bx0=(int)fminf(x0,x1)-(int)rad-2,bx1=(int)fmaxf(x0,x1)+(int)rad+2;
    int by0=(int)fminf(y0,y1)-(int)rad-2,by1=(int)fmaxf(y0,y1)+(int)rad+2;
    for(int y=by0;y<=by1;y++)for(int x=bx0;x<=bx1;x++){
        float px=x+0.5f-x0,py=y+0.5f-y0,t=len2>0?(px*dx+py*dy)/len2:0;
        t=t<0?0:(t>1?1:t);
        float ex=px-t*dx,ey=py-t*dy,d=sqrtf(ex*ex+ey*ey),c=rad+0.5f-d;
        if(c>0)pixel(x,y,r,g,b,(int)(255*(c>1?1:c)));
    }
}
static void disc(float cx,float cy,float rad,int r,int g,int b) {
    for(int y=(int)(cy-rad)-1;y<=(int)(cy+rad)+1;y++)for(int x=(int)(cx-rad)-1;x<=(int)(cx+rad)+1;x++){
        float dx=x+0.5f-cx,dy=y+0.5f-cy,c=rad+0.5f-sqrtf(dx*dx+dy*dy);
        if(c>0)pixel(x,y,r,g,b,(int)(255*(c>1?1:c)));
    }
}
/* The game's pointer: a white triangle, shaded grey on its back edge. */
static void pointer(int x,int y) {
    for(int j=0;j<26;j++){int span=(j<13?j:25-j)*20/13;
        for(int i=0;i<=span;i++)pixel(x+i,y+j,i<4?190:255,i<4?190:255,i<4?198:255,255);}
}
/* The source positions (0..9, port_button_map.h) the confirm and back caps
 * name; the button map sets them (dw3_pc_title_overlay_set_hint_caps). */
static int cap_confirm_src=0,cap_back_src=2;
void dw3_pc_title_overlay_set_hint_caps(int confirm_src,int back_src) {
    if(confirm_src<0||confirm_src>9||back_src<0||back_src>9)return;
    if(confirm_src==cap_confirm_src&&back_src==cap_back_src)return;
    cap_confirm_src=confirm_src;cap_back_src=back_src;
#if defined(__SWITCH__)
    options_layout++;options_generation++; /* the retained static layer draws the caps */
#endif
}
/* A button cap: Switch letters, or the PlayStation Cross / Triangle marks. */
static void button(int cx,int cy,int back) {
    disc(cx,cy,14,214,222,240);
#if defined(__SWITCH__)
    /* P10.6 fix-now: the cap is the Switch button that produces the confirm
     * (Cross) or back (Triangle) PS button under the button map in force, so
     * Nintendo reads A / B and a custom map reads its own sources. Indexed by
     * source position: Cross B, Circle A, Triangle X, Square Y, L1 L, R1 R,
     * L2 ZL, R2 ZR, L3, R3. */
    const Word keys[10]={WORD(opt_key_b),WORD(opt_key_a),WORD(opt_key_x),WORD(opt_key_y),
                                    WORD(opt_key_l),WORD(opt_key_r),WORD(opt_key_zl),WORD(opt_key_zr),
                                    WORD(opt_key_l3),WORD(opt_key_r3)};
    const Word *k=&keys[back?cap_back_src:cap_confirm_src];
    word(k,cx-k->w/2,cy-k->top-k->h/2,0,0,0,49);
#else
    if(back){segment(cx,cy-8,cx-8,cy+6,1.4f,0,0,49);segment(cx-8,cy+6,cx+8,cy+6,1.4f,0,0,49);
             segment(cx+8,cy+6,cx,cy-8,1.4f,0,0,49);}
    else{segment(cx-6,cy-6,cx+6,cy+6,1.5f,0,0,49);segment(cx-6,cy+6,cx+6,cy-6,1.5f,0,0,49);}
#endif
}
/* Geometry of the Port options window and its rows, in canvas pixels. The
 * retained cache (title_options_cache.c) repeats the row numbers; the unit
 * test proves they agree. Nine rows share the window (P6.31 added Auto-save),
 * so a row is 34 px high on a 38 px pitch (it was 38 on 42 with eight rows, 42 on
 * 48 with seven, 52 on 60 with five, P6.17): the first row starts at y 150 and
 * the last ends at y 488, 20 px above the message rule. The Cheats pages share it; the
 * Yes / No window keeps the old 38 px row on a 42 px pitch (CHT_ROW_*). */
enum { OPT_X0 = 84, OPT_Y0 = 60, OPT_W = 792, OPT_H = 600,
       OPT_ROW_Y = OPT_Y0 + 96, OPT_ROW_STEP = 38, OPT_ROW_H = 34,
       CHT_ROW_STEP = 42, CHT_ROW_H = 38 };
/* The rows' kinds: how a registry value is written. */
enum { ROW_SWITCH, ROW_FAST_FORWARD, ROW_VOLUME, ROW_ENCOUNTER, ROW_TEXT,
       ROW_BATTLE, ROW_AUTOSAVE, ROW_LAYOUT, ROW_REGION, ROW_CUSTOMIZE, ROW_RESET,
       ROW_TURBO_MUSIC, ROW_GLYPHS, ROW_MAP_SCREEN,
       ROW_EXTRA /* D1: registry rows 16..28, words in extra_words */ };
static int row_kind(int row) {
    return row == 1 ? ROW_FAST_FORWARD : row == 3 || row == 4 ? ROW_VOLUME :
           row == 5 ? ROW_ENCOUNTER : row == 6 ? ROW_TEXT :
           row == 7 ? ROW_BATTLE : row == 8 ? ROW_AUTOSAVE :
           row == 9 ? ROW_LAYOUT : row == 10 ? ROW_REGION :
           row == 11 ? ROW_CUSTOMIZE : row == 12 ? ROW_RESET :
           row == 13 ? ROW_TURBO_MUSIC : row == 14 ? ROW_GLYPHS :
           row == 15 ? ROW_MAP_SCREEN : row >= 16 && row <= 29 ? ROW_EXTRA : ROW_SWITCH;
}
/* P6.31: the Auto-save row's lettering, in the language of the disc (en, fr,
 * de, es, it: the DW3_TITLE_LANG_* order). OFF is the Options screen's word. */
#define AUTOSAVE_WORDS(l) {WORD(opt_as_row_##l),WORD(opt_as_area_##l),WORD(opt_as_desc_##l)}
typedef struct AutosaveWords { Word row,area,desc; } AutosaveWords;
static const AutosaveWords autosave_words[5]={AUTOSAVE_WORDS(en),AUTOSAVE_WORDS(fr),AUTOSAVE_WORDS(de),
                                              AUTOSAVE_WORDS(es),AUTOSAVE_WORDS(it)};
static const AutosaveWords *autosave_lang(void) {
    return &autosave_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
/* P9.8: the Button layout row's lettering, in the language of the disc. The two
 * values are product names, the same in every language. */
#define LAYOUT_WORDS(l) {WORD(opt_row_bl_##l),WORD(opt_val_bl_ps_##l),WORD(opt_val_bl_nin_##l),WORD(opt_desc_bl_##l)}
typedef struct LayoutWords { Word row,ps,nintendo,desc; } LayoutWords;
static const LayoutWords layout_words[5]={LAYOUT_WORDS(en),LAYOUT_WORDS(fr),LAYOUT_WORDS(de),
                                          LAYOUT_WORDS(es),LAYOUT_WORDS(it)};
static const LayoutWords *layout_lang(void) {
    return &layout_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
/* P6.12: the Customize buttons and Reset to default rows of Controls, the Custom
 * value of Button layout and the prompt of the Customize screen, in the language
 * of the disc. The PlayStation and Switch button names are the same everywhere. */
#define CUSTOM_WORDS(l) {WORD(opt_row_cz_##l),WORD(opt_desc_cz_##l),WORD(opt_row_rs_##l),WORD(opt_desc_rs_##l), \
    WORD(opt_val_cp_##l),WORD(opt_desc_cp_##l),WORD(opt_val_bl_cus_##l),WORD(opt_grp_custom_##l), \
    WORD(opt_grpdesc_custom_##l)}
typedef struct CustomWords { Word row,rowdesc,reset,resetdesc,prompt,promptdesc,custom,name,desc; } CustomWords;
static const CustomWords custom_words[5]={CUSTOM_WORDS(en),CUSTOM_WORDS(fr),CUSTOM_WORDS(de),
                                          CUSTOM_WORDS(es),CUSTOM_WORDS(it)};
static const CustomWords *custom_lang(void) {
    return &custom_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
/* The Customize screen's rows (the PS buttons, map order) and their values (the
 * Switch button that makes each one). */
static const Word ps_words[10]={WORD(opt_row_ps_cross),WORD(opt_row_ps_circle),WORD(opt_row_ps_triangle),
    WORD(opt_row_ps_square),WORD(opt_row_ps_l1),WORD(opt_row_ps_r1),WORD(opt_row_ps_l2),WORD(opt_row_ps_r2),
    WORD(opt_row_ps_l3),WORD(opt_row_ps_r3)};
static const Word btn_words[10]={WORD(opt_val_btn_b),WORD(opt_val_btn_a),WORD(opt_val_btn_x),WORD(opt_val_btn_y),
    WORD(opt_val_btn_l),WORD(opt_val_btn_r),WORD(opt_val_btn_zl),WORD(opt_val_btn_zr),WORD(opt_val_btn_lstick),
    WORD(opt_val_btn_rstick)};
/* P51 item 4: the Region at start row (combined image): Ask in the language
 * of the disc, US and EU the same everywhere. */
#define REGION_WORDS(l) {WORD(opt_row_rg_##l),WORD(opt_val_rg_ask_##l),WORD(opt_desc_rg_##l)}
typedef struct RegionWords { Word row,ask,desc; } RegionWords;
static const RegionWords region_words[5]={REGION_WORDS(en),REGION_WORDS(fr),REGION_WORDS(de),
                                          REGION_WORDS(es),REGION_WORDS(it)};
static const Word v_region_us=WORD(opt_val_rg_us),v_region_eu=WORD(opt_val_rg_eu);
static const RegionWords *region_lang(void) {
    return &region_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
/* P6.36: the Turbo music row (Sound group): label and message line in the
 * language of the disc; the values are the Options screen's 1x and Fast. */
#define TMUSIC_WORDS(l) {WORD(opt_row_tm_##l),WORD(opt_desc_tm_##l)}
typedef struct TmusicWords { Word row,desc; } TmusicWords;
static const TmusicWords tmusic_words[5]={TMUSIC_WORDS(en),TMUSIC_WORDS(fr),TMUSIC_WORDS(de),
                                          TMUSIC_WORDS(es),TMUSIC_WORDS(it)};
static const TmusicWords *tmusic_lang(void) {
    return &tmusic_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
/* P10.6: the Switch glyphs row (Controls group, Switch only): label and message
 * line in the language of the disc; the values are the Options screen's OFF / ON. */
#define GLYPHS_WORDS(l) {WORD(opt_row_sg_##l),WORD(opt_desc_sg_##l)}
typedef struct GlyphsWords { Word row,desc; } GlyphsWords;
static const GlyphsWords glyphs_words[5]={GLYPHS_WORDS(en),GLYPHS_WORDS(fr),GLYPHS_WORDS(de),
                                          GLYPHS_WORDS(es),GLYPHS_WORDS(it)};
static const GlyphsWords *glyphs_lang(void) {
    return &glyphs_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
/* P6.32 phase 4: the Full-screen map row (Display group): label and message
 * line in the language of the disc; the values are the Options screen's OFF / ON. */
#define MAPSCR_WORDS(l) {WORD(opt_row_ms_##l),WORD(opt_desc_ms_##l)}
typedef struct MapscrWords { Word row,desc; } MapscrWords;
static const MapscrWords mapscr_words[5]={MAPSCR_WORDS(en),MAPSCR_WORDS(fr),MAPSCR_WORDS(de),
                                          MAPSCR_WORDS(es),MAPSCR_WORDS(it)};
static const MapscrWords *mapscr_lang(void) {
    return &mapscr_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
/* D1: the rows that were file-only keys (registry 16..28: the five Menus switches,
 * Version label, Map mod pack, Shared saves, Stage sweep, Sweep frames, Map
 * buttons), their Stage sweep values, and the Map buttons screen (eight rows,
 * three message lines, the Select / Minus values). */
#define XW(k,l) {WORD(opt_row_x_##k##_##l),WORD(opt_desc_x_##k##_##l)}
#define EXTRA_WORDS(l) {{XW(eq,l),XW(st,l),XW(bg,l),XW(us,l),XW(ps,l),XW(vr,l),XW(mm,l),XW(ss,l), \
    XW(sw,l),XW(sf,l),XW(mb,l),XW(mx,l),XW(sc,l),XW(tl,l)}, WORD(opt_val_sw_run_##l),WORD(opt_val_sw_over_##l), \
    {WORD(opt_row_mb0_##l),WORD(opt_row_mb1_##l),WORD(opt_row_mb2_##l),WORD(opt_row_mb3_##l), \
     WORD(opt_row_mb4_##l),WORD(opt_row_mb5_##l),WORD(opt_row_mb6_##l),WORD(opt_row_mb7_##l)}, \
    {WORD(opt_desc_mb_bind_##l),WORD(opt_desc_mb_field_##l),WORD(opt_desc_mb_reset_##l)}, \
    WORD(opt_desc_tl_nopack_##l),WORD(opt_val_tl_orig_##l)}
typedef struct ExtraWords { Word row,desc; } ExtraWords;
typedef struct ExtraSet {
    ExtraWords reg[14];       /* registry rows 16..29 (27 = Instant menu exit, 28 = D5 Map scaler, 29 = P6.7 Text language) */
    Word sweep_run,sweep_over;
    Word mb_row[8];
    Word mb_desc[3];          /* bind, field, reset */
    Word tl_nopack,tl_orig;   /* P6.7 Text language: the message line with no pack, the Original value */
} ExtraSet;
static const ExtraSet extra_words[5]={EXTRA_WORDS(en),EXTRA_WORDS(fr),EXTRA_WORDS(de),
                                      EXTRA_WORDS(es),EXTRA_WORDS(it)};
static const ExtraSet *extra_lang(void) {
    return &extra_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
#if defined(__SWITCH__)
static const Word v_btn_minus=WORD(opt_val_btn_minus);
#else
static const Word v_btn_select=WORD(opt_val_btn_select);
#endif
/* P9.7: the message line of a row that is locked in game, in the language of
 * the disc (en, fr, de, es, it: the DW3_TITLE_LANG_* order). */
static const Word lock_words[5]={WORD(opt_lock_en),WORD(opt_lock_fr),WORD(opt_lock_de),
                                 WORD(opt_lock_es),WORD(opt_lock_it)};
static const Word *lock_lang(void) {
    return &lock_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
#ifdef DW3_CHEATS
/* The cheat lettering of one language (en, fr, de, es, it: the
 * DW3_TITLE_LANG_* order). */
#define CHEAT_WARP(l,i) WORD(opt_cht_warp##i##_##l)
#define CHEAT_WORDS(l) {WORD(opt_cht_title_##l),WORD(opt_cht_hint_##l),WORD(opt_cht_change_##l), \
    WORD(opt_cht_back_##l),WORD(opt_cht_ready_##l),WORD(opt_cht_none_##l),WORD(opt_cht_title2_##l), \
    {WORD(opt_cht_row0_##l),WORD(opt_cht_row1_##l),WORD(opt_cht_row2_##l),WORD(opt_cht_row3_##l), \
     WORD(opt_cht_row4_##l),WORD(opt_cht_row5_##l),WORD(opt_cht_row6_##l),WORD(opt_cht_row7_##l), \
     WORD(opt_cht_row8_##l),WORD(opt_cht_row9_##l)}, \
    {WORD(opt_cht_desc0_##l),WORD(opt_cht_desc1_##l),WORD(opt_cht_desc2_##l),WORD(opt_cht_desc3_##l), \
     WORD(opt_cht_desc4_##l),WORD(opt_cht_desc5_##l),WORD(opt_cht_desc6_##l),WORD(opt_cht_desc7_##l), \
     WORD(opt_cht_desc8_##l),WORD(opt_cht_desc9_##l)}, \
    {CHEAT_WARP(l,0),CHEAT_WARP(l,1),CHEAT_WARP(l,2),CHEAT_WARP(l,3),CHEAT_WARP(l,4),CHEAT_WARP(l,5), \
     CHEAT_WARP(l,6),CHEAT_WARP(l,7),CHEAT_WARP(l,8),CHEAT_WARP(l,9),CHEAT_WARP(l,10),CHEAT_WARP(l,11), \
     CHEAT_WARP(l,12)}}
typedef struct CheatWords {
    Word title,hint,change,back,ready,none,title2;
    Word rows[10],desc[10];
    Word warp[DW3_TITLE_CHEAT_WARPS];
} CheatWords;
static const CheatWords cheat_words[5]={CHEAT_WORDS(en),CHEAT_WORDS(fr),CHEAT_WORDS(de),
                                        CHEAT_WORDS(es),CHEAT_WORDS(it)};
static const CheatWords *cheat_lang(void) {
    return &cheat_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
/* A number of the Give item rows: the digits (fixed advance), right edge at x. */
static void number_plain(int v,int x,int y,int r,int g,int b) {
    static const Word digits[10]={WORD(opt_cht_d0),WORD(opt_cht_d1),WORD(opt_cht_d2),WORD(opt_cht_d3),
                                  WORD(opt_cht_d4),WORD(opt_cht_d5),WORD(opt_cht_d6),WORD(opt_cht_d7),
                                  WORD(opt_cht_d8),WORD(opt_cht_d9)};
    int n=v>=1000?4:v>=100?3:v>=10?2:1,adv=digits[0].w;
    if(v<0)v=0;
    for(int k=0,div=n==4?1000:n==3?100:n==2?10:1;k<n;k++,div/=10)
        word(&digits[(v/div)%10],x-(n-k)*adv,y,0,r,g,b);
}
/* The same with the title menu's one pixel dark shadow. */
static void number(int v,int x,int y,int r,int g,int b) {
    number_plain(v,x+1,y+2,2,7,19);
    number_plain(v,x,y,r,g,b);
}
/* The value of cheat row `row` (port_cheats.h): a word, or NULL for the two
 * numbers (item id, count). *dim = 1 for an OFF-like value. */
static const Word *cheat_value(const CheatWords *t,int row,int v,int *dim) {
    static const Word values[]={WORD(opt_off),WORD(opt_on)};
    static const Word money[]={WORD(opt_off),WORD(opt_cht_m10k),WORD(opt_cht_m100k),WORD(opt_cht_mmax)};
    static const Word steps[]={WORD(opt_n1),WORD(opt_n2),WORD(opt_n3),WORD(opt_n4),WORD(opt_n5),
                               WORD(opt_n6),WORD(opt_n7),WORD(opt_n8),WORD(opt_n9),WORD(opt_n10)};
    if(v<0)v=0;
    *dim=v==0;
    switch(row){
    case 4:if(v>10)v=10;return v?&steps[v-1]:&t->none;
    case 1:case 3:case 9:return v?&t->ready:&values[0];
    case 5:if(v>3)v=3;*dim=v==0;return &money[v];
    case 6:if(v>DW3_TITLE_CHEAT_WARPS)v=DW3_TITLE_CHEAT_WARPS;*dim=v==0;return v?&t->warp[v-1]:&values[0];
    case 7:case 8:*dim=0;return NULL;
    default:return &values[v?1:0];
    }
}
#endif
/* --- P9.9 Options list screens ------------------------------------------------
 * The Options screen is a list of groups; a group opens a sub-screen with its
 * rows (port_options_groups.h). Both are drawn in the title menu's identity:
 * the dimmed title backdrop, the same navy hardware plaques with steel bevels,
 * amber rails and fasteners (title_plaque), a lit rail, lit fasteners and a
 * short orange halo as the focus cue, and the same warm lettering with a dark
 * one pixel shadow. Six plaques are on screen at a time (DW3_OPTLIST_VISIBLE);
 * a longer list scrolls and shows an arrow at the end that has more.
 *
 * Layers (the retained canvas, title_options_cache.c): STATIC = backdrop, header,
 * idle plaques and their labels, arrows, message strip, hints; FOCUS = the lit
 * plaque of the focused item and its label; VALUE = a row's value; DESCRIPTION
 * = the message line for the focused item. The cache asks for the rectangles
 * with dw3_pc_title_overlay_options_rect. */
enum { DIM_TITLE = 150, DIM_IN_GAME = 252, DIM_TILE = 90 };

/* P51 item 6: the game's field-menu background (sprite 0x1D of the status
 * sprites, one 96 x 96 period) under the title's Options, Cheats and Help
 * screens. D4 / A18: it scrolls, as in the Items menu (down and right, one game
 * pixel every other frame, back to 0 at 96). The retained canvas is not
 * recomposed for a scroll, so the tile is not drawn into it: the canvas keeps a
 * transparent backdrop (alpha 0, the tile's dimmed average colour, so a linear
 * sample of an edge does not darken) and the hosts draw the dimmed, 3x expanded
 * texture below it as a tiled quad with a UV offset (platforms/pc/src/
 * title_menu_overlay.c, platforms/switch/psyz_backend/nx_title_overlay.c).
 * In game the near-opaque dim stays (the field menu behind must not read through
 * and the tile would only show at 2 %): an opaque canvas backdrop, no quad.
 * Untried until the first prewarm / update asks (a frame-thread call: the disc
 * read), then 1 ready or -1 not available (the dimmed title backdrop stays). */
static uint8_t menu_bg_tex[DW3_MENU_BG_TEX*DW3_MENU_BG_TEX*4];
static uint8_t menu_bg_avg[3];
static atomic_int menu_tile_state;
void dw3_pc_title_overlay_menu_tile_prepare(void) {
    static uint8_t tile[MENU_BG_TILE*MENU_BG_TILE*4];
    double ms=0.0;
    char why[96];
    if(atomic_load(&menu_tile_state)!=0)return;
    why[0]=0;
    if(!dw3_menu_bg_tile_load) {
        atomic_store(&menu_tile_state,-1);
        return;
    }
    if(dw3_menu_bg_tile_load(tile,&ms,why,sizeof(why))==0) {
        unsigned sum[3]={0,0,0};
        for(int i=0;i<MENU_BG_TILE*MENU_BG_TILE;i++)
            for(int c=0;c<3;c++)sum[c]+=tile[i*4+c];
        for(int c=0;c<3;c++)
            menu_bg_avg[c]=(uint8_t)(sum[c]/(MENU_BG_TILE*MENU_BG_TILE)*(255-DIM_TILE)/255);
        for(int j=0;j<DW3_MENU_BG_TEX;j++) {
            const uint8_t *row=tile+(size_t)(j/DW3_MENU_BG_SCALE)*MENU_BG_TILE*4;
            uint8_t *d=menu_bg_tex+(size_t)j*DW3_MENU_BG_TEX*4;
            for(int i=0;i<DW3_MENU_BG_TEX;i++,d+=4) {
                const uint8_t *s=row+(size_t)(i/DW3_MENU_BG_SCALE)*4;
                d[0]=(uint8_t)(s[0]*(255-DIM_TILE)/255);
                d[1]=(uint8_t)(s[1]*(255-DIM_TILE)/255);
                d[2]=(uint8_t)(s[2]*(255-DIM_TILE)/255);
                d[3]=255;
            }
        }
        fprintf(stderr,"dw3-options: menu background tile %dx%d from the status sprites "
                "(sprite 0x1D) ready in %.0f us (scrolls: 1 px every 2 ticks, period %d)\n",
                MENU_BG_TILE,MENU_BG_TILE,ms*1000.0,DW3_MENU_BG_PERIOD);
        atomic_store(&menu_tile_state,1);
    } else {
        fprintf(stderr,"dw3-options: menu background tile not available (%s), using the "
                "title backdrop (%.0f us)\n",why,ms*1000.0);
        atomic_store(&menu_tile_state,-1);
    }
}
const uint8_t *dw3_pc_title_overlay_menu_bg_texture(void) {
    return atomic_load(&menu_tile_state)==1?menu_bg_tex:NULL;
}
/* The screen's backdrop: at the title the scrolling tile's place (transparent,
 * see above), else the dimmed title picture (or the deeper in-game dim). */
static void backdrop(int in_game) {
    if(in_game||atomic_load(&menu_tile_state)!=1) {
        box(0,0,960,720,0,0,16,in_game?DIM_IN_GAME:DIM_TITLE);
        return;
    }
    int x0=clip_x0<0?0:clip_x0,y0=clip_y0<0?0:clip_y0;
    int x1=clip_x1>960?960:clip_x1,y1=clip_y1>720?720:clip_y1;
    for(int j=y0;j<y1;j++) {
        uint8_t *p=canvas+(size_t)j*stride+(size_t)x0*4;
        for(int i=x0;i<x1;i++,p+=4) {
            p[0]=menu_bg_avg[0];p[1]=menu_bg_avg[1];p[2]=menu_bg_avg[2];p[3]=0;
        }
    }
}
enum { L_ROW_X = 152, L_ROW_W = 656, L_ROW_Y0 = 116, L_ROW_PITCH = 66,
       L_PLQ_H = 50, L_PLQ_DY = 8, L_GRP_X = 230, L_GRP_W = 500,
       L_ROWPLQ_X = 160, L_ROWPLQ_W = 640, L_VAL_X = 500, L_VAL_W = 260,
       L_DESC_X = 120, L_DESC_Y = 532, L_DESC_W = 720, L_DESC_H = 60,
       L_HEAD_Y = 40,
       /* P6.37: baseline of the version line, under the strip (ends at 669) */
       L_VERSION_BASE = 702 };
/* Does the clipped area touch this rectangle? (a draw skips work it would clip) */
static int clip_hits(int x,int y,int w,int h) {
    return x<clip_x1&&x+w>clip_x0&&y<clip_y1&&y+h>clip_y0;
}
/* The title menu's plaque (the loop of dw3_pc_title_overlay_art draws the same):
 * halo, shadow, bevels, lips, rails, and the lateral clamps with their lamps.
 * `compact` is the 40 px high, five plaque variant. */
enum { PLQ_TITLE = 0, PLQ_LIST = 1, PLQ_LIST_FOCUS = 2 };
/* mode: PLQ_TITLE draws the halo, then the shadow (the title menu's order);
 * PLQ_LIST draws the shadow, then the halo, so that the retained canvas, which
 * has the idle plaque and its shadow already, gets the same pixels from
 * PLQ_LIST_FOCUS (halo and lit plaque only) as a full render gets from
 * PLQ_LIST. */
static void title_plaque_ex(int x,int y,int w,int h,int focus,int compact,int mode) {
    const int cut=compact?8:10;
    if(mode==PLQ_LIST)plaque(x+1,y+4,w,h,cut,0,3,10,0,3,10,190);
    /* Soft optical orange halo only on the focused hardware silhouette. */
    if(focus)for(int spread=8;spread>=1;spread--)
        plaque(x-spread,y-spread,w+spread*2,h+spread*2,cut+spread,255,103,17,255,103,17,5);
    if(mode==PLQ_TITLE)plaque(x+1,y+4,w,h,cut,0,3,10,0,3,10,190);
    plaque(x,y,w,h,cut,focus?255:99,focus?180:149,focus?60:196,focus?207:17,focus?100:40,focus?26:77,255);
    plaque(x+1,y+1,w-2,h-2,cut-1,99,149,196,17,40,77,255);
    plaque(x+2,y+2,w-4,h-4,cut-1,25,57,98,49,90,141,255);
    plaque(x+5,y+5,w-10,h-10,cut-3,5,16,39,13,29,57,255);
    /* Reflective top and lower lips. */
    box(x+36,y+2,w-72,1,143,198,232,220);
    box(x+36,y+h-3,w-72,1,78,120,173,255);
    int rr=focus?255:138,gg=focus?172:98,bb=focus?42:43;
    box(x+37,y+6,w-74,2,rr,gg,bb,255);
    box(x+37,y+h-8,w-74,2,rr,gg,bb,255);
    if(focus){box(x+39,y+5,w-78,1,255,236,167,235);box(x+39,y+h-6,w-78,1,255,221,136,210);}
    /* Cast-metal lateral clamps, bright chamfer, black inset channels. */
    for(int side=0;side<2;side++) {
        int cx=side?x+w-35:x+1;
        plaque(cx,y+1,34,h-2,7,100,160,206,17,47,95,255);
        plaque(cx+3,y+4,28,h-8,5,32,73,124,12,30,63,255);
        box(cx+5,y+7,2,h-14,106,166,209,215);
        box(cx+25,y+7,3,h-14,3,10,28,255);
        for(int l=0;l<3;l++)lamp(cx+15,compact?y+9+l*11:y+10+l*14,focus);
        int railx=side?cx-5:cx+36;
        for(int l=0;l<3;l++)box(railx,compact?y+13+l*5:y+16+l*6,2,3,rr,gg,bb,255);
    }
}
static void title_plaque(int x,int y,int w,int h,int focus,int compact) {
    title_plaque_ex(x,y,w,h,focus,compact,PLQ_TITLE);
}
/* The Options plaques are rasterized once per size and state into small
 * sprites (the halo and lit plaque alone for the lit one, the shadow and idle
 * plaque for the idle one) and blitted: a focus change on the retained canvas
 * is then a blit, not ~1M blended pixels (it matters on the Switch's CPU).
 * The sprites are shared by the frame thread and the prewarm worker; they are
 * built once, under a spin lock, and never freed. A sprite has PLQ_PAD px
 * around the plaque for the halo and the shadow. */
enum { PLQ_PAD = 8, PLQ_H = 50 };
typedef struct PlaqueSprite { int w,h; uint8_t *px; } PlaqueSprite;
static PlaqueSprite plq_sprites[2][2]; /* [0 narrow: group, 1 wide: row][idle, lit] */
static atomic_flag plq_lock=ATOMIC_FLAG_INIT;
static const PlaqueSprite *plaque_sprite(int pw,int wide,int lit) {
    PlaqueSprite *sp=&plq_sprites[wide][lit];
    while(atomic_flag_test_and_set_explicit(&plq_lock,memory_order_acquire)){}
    if(!sp->px) {
        const int w=pw+2*PLQ_PAD,h=PLQ_H+2*PLQ_PAD;
        uint8_t *buf=(uint8_t*)calloc((size_t)w*h,4);
        if(buf) {
            /* This thread's drawing state, swapped for the sprite and back. */
            uint8_t *sc=canvas;const int ss=stride,cx0=clip_x0,cy0=clip_y0,cx1=clip_x1,cy1=clip_y1;
            canvas=buf;stride=w*4;clip_x0=clip_y0=0;clip_x1=w;clip_y1=h;
            title_plaque_ex(PLQ_PAD,PLQ_PAD,pw,PLQ_H,lit,0,lit?PLQ_LIST_FOCUS:PLQ_LIST);
            canvas=sc;stride=ss;clip_x0=cx0;clip_y0=cy0;clip_x1=cx1;clip_y1=cy1;
            sp->w=w;sp->h=h;sp->px=buf;
        }
    }
    atomic_flag_clear_explicit(&plq_lock,memory_order_release);
    return sp->px?sp:NULL;
}
/* Blit a sprite so that its plaque is at (x, y), inside the clip. */
static void plaque_blit(const PlaqueSprite *sp,int x,int y) {
    x-=PLQ_PAD;y-=PLQ_PAD;
    for(int j=0;j<sp->h;j++) {
        const int yy=y+j;
        if(yy<clip_y0||yy>=clip_y1)continue;
        const uint8_t *q=sp->px+(size_t)j*sp->w*4;
        for(int i=0;i<sp->w;i++,q+=4)if(q[3])pixel(x+i,yy,q[0],q[1],q[2],q[3]);
    }
}
static void options_plaque(int x,int y,int pw,int wide,int lit) {
    const PlaqueSprite *sp=plaque_sprite(pw,wide,lit);
    if(sp)plaque_blit(sp,x,y);
    else title_plaque_ex(x,y,pw,50,lit,0,lit?PLQ_LIST_FOCUS:PLQ_LIST);
}
/* A Word with the title menu's one pixel dark shadow. */
static void wordsh(const Word *t,int x,int y,int align,int r,int g,int b) {
    word(t,x+1,y+2,align,2,7,19);
    word(t,x,y,align,r,g,b);
}
/* Centered lettering of a plaque. */
static void plaque_word(const Word *t,int cx,int y,int r,int g,int b) {
    wordsh(t,cx-t->w/2,y,0,r,g,b);
}
/* Scroll arrow: a small solid triangle, up or down, centered at cx. */
static void arrow(int cx,int y,int up) {
    for(int j=0;j<9;j++){
        const int span=up?j:8-j;
        box(cx-span,y+j,span*2+1,1,243,158,45,255);
    }
}

#define GROUP_WORDS(l) {{WORD(opt_grp_display_##l),WORD(opt_grpdesc_display_##l)}, \
    {WORD(opt_grp_sound_##l),WORD(opt_grpdesc_sound_##l)}, \
    {WORD(opt_grp_gameplay_##l),WORD(opt_grpdesc_gameplay_##l)}, \
    {WORD(opt_grp_saves_##l),WORD(opt_grpdesc_saves_##l)}, \
    {WORD(opt_grp_cheats_##l),WORD(opt_grpdesc_cheats_##l)}, \
    {WORD(opt_grp_controls_##l),WORD(opt_grpdesc_controls_##l)}, \
    {WORD(opt_grp_help_##l),WORD(opt_grpdesc_help_##l)}, \
    {WORD(opt_grp_walkthrough_##l),WORD(opt_grpdesc_walkthrough_##l)}, \
    {WORD(opt_grp_custom_##l),WORD(opt_grpdesc_custom_##l)}, \
    {WORD(opt_grp_menus_##l),WORD(opt_grpdesc_menus_##l)}, \
    {WORD(opt_grp_advanced_##l),WORD(opt_grpdesc_advanced_##l)}, \
    {WORD(opt_grp_mapbtn_##l),WORD(opt_grpdesc_mapbtn_##l)}}
typedef struct GroupWords { Word name,desc; } GroupWords;
enum { LISTED_GROUPS = 12 }; /* the groups with lettering: indexed by group id (D1: up to Map buttons) */
static const GroupWords group_words[5][LISTED_GROUPS]={GROUP_WORDS(en),GROUP_WORDS(fr),GROUP_WORDS(de),
                                                       GROUP_WORDS(es),GROUP_WORDS(it)};
#if defined(__SWITCH__)
#define DESC_P(n,l) WORD(opt_desc##n##_nx_##l)
#else
#define DESC_P(n,l) WORD(opt_desc##n##_pc_##l)
#endif
#define DESC_N(n,l) WORD(opt_desc##n##_##l)
#define ROW_WORDS(l) {{WORD(opt_row0_##l),WORD(opt_row1_##l),WORD(opt_row2_##l),WORD(opt_row3_##l), \
    WORD(opt_row4_##l),WORD(opt_row5_##l),WORD(opt_row6_##l),WORD(opt_row7_##l)}, \
    {DESC_N(0,l),DESC_P(1,l),DESC_P(2,l),DESC_N(3,l),DESC_N(4,l),DESC_N(5,l),DESC_N(6,l),DESC_N(7,l)}, \
    {WORD(opt_val_normal_##l),WORD(opt_val_fast_##l),WORD(opt_val_instant_##l),WORD(opt_val_command_##l), \
     WORD(opt_val_command_tech_##l)}, \
    WORD(opt_title_##l),WORD(opt_open_##l),WORD(opt_change_##l),WORD(opt_back_##l)}
typedef struct ListWords {
    Word row[8],desc[8],value[5],title,open,change,back;
} ListWords;
static const ListWords list_words[5]={ROW_WORDS(en),ROW_WORDS(fr),ROW_WORDS(de),ROW_WORDS(es),ROW_WORDS(it)};
static const ListWords *list_lang(void) {
    return &list_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
static const GroupWords *group_lang(int group) {
    return &group_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0]
                       [group>=0&&group<LISTED_GROUPS?group:0];
}
/* The registry rows of every locked bit of a group: a group whose rows are all
 * locked in game (Display: Widescreen) greys its plaque. */
static unsigned group_row_mask(int group) {
    unsigned m=0;
    if(group==DW3_OPTGRP_CHEATS||group==DW3_OPTGRP_CUSTOM||group==DW3_OPTGRP_MAPBTN)return 0; /* not registry rows */
    for(int k=0;k<dw3_optgroups[group].nrows;k++)m|=1u<<dw3_optgroups[group].rows[k];
    return m;
}
static int group_locked(int group,unsigned locked) {
    if(group==DW3_OPTGRP_CHEATS)return (locked&DW3_TITLE_LOCK_CHEATS)!=0;
    return dw3_optgroups[group].nrows>0&&group_row_mask(group)!=0&&
           (locked&group_row_mask(group))==group_row_mask(group);
}
/* The geometry of slot s (0 = the first visible plaque). */
static int slot_y(int s) { return L_ROW_Y0+s*L_ROW_PITCH; }

static const Word v_onoff[]={WORD(opt_off),WORD(opt_on),WORD(opt_2x),WORD(opt_3x),WORD(opt_4x)};
static const Word v_steps[]={WORD(opt_n1),WORD(opt_n2),WORD(opt_n3),WORD(opt_n4),WORD(opt_n5),
                             WORD(opt_n6),WORD(opt_n7),WORD(opt_n8),WORD(opt_n9),WORD(opt_n10)};
/* P6.18: Encounter rate, indexed by the stored value (0 1x, 1 0.5x, 2 OFF,
 * 3 1.5x, 4 2x; the screen steps them OFF, 0.5x, 1x, 1.5x, 2x). */
static const Word v_1x=WORD(opt_1x);
static const Word v_encounter[]={WORD(opt_1x),WORD(opt_05x),WORD(opt_off),WORD(opt_15x),WORD(opt_2x)};
/* The word of registry row `reg` holding `v`; *dim = 1 for an OFF-like value. */
static const Word *list_value(const ListWords *t,int reg,int v,int *dim) {
    if(v<0)v=0;
    *dim=0;
    switch(row_kind(reg)) {
    case ROW_VOLUME:
        if(v>10)v=10;
        *dim=v==0;
        return v?&v_steps[v-1]:&v_onoff[0];
    case ROW_FAST_FORWARD:
        if(v>4)v=4;
        *dim=v==0;
        return &v_onoff[v];
    case ROW_ENCOUNTER:
        if(v>4)v=4;
        *dim=v==2;
        return &v_encounter[v];
    case ROW_TEXT:
        if(v>2)v=2;
        return &t->value[v];
    case ROW_BATTLE:
        if(v>2)v=2;
        *dim=v==0;
        return v==0?&v_onoff[0]:&t->value[v==1?3:4];
    case ROW_AUTOSAVE:
        if(v>1)v=1;
        *dim=v==0;
        return v?&autosave_lang()->area:&v_onoff[0];
    case ROW_LAYOUT:
        if(v>2)v=2;
        return v==2?&custom_lang()->custom:v?&layout_lang()->nintendo:&layout_lang()->ps;
    case ROW_TURBO_MUSIC:
        if(v>1)v=1;
        return v?&t->value[1]:&v_1x;
    case ROW_CUSTOMIZE:case ROW_RESET:
        return NULL; /* an action: no value */
    case ROW_EXTRA:
        if(reg==24) {       /* Stage sweep: OFF, Run, Start over */
            if(v>2)v=2;
            *dim=v==0;
            return v==0?&v_onoff[0]:v==1?&extra_lang()->sweep_run:&extra_lang()->sweep_over;
        }
        if(reg==25||reg==26)return NULL; /* Sweep frames: a number; Map buttons: an action */
        if(reg==28) {       /* D5 Map scaler: Nearest, Sharp bilinear, Lanczos2, FSR 1, Bilinear (product names, the same in every language) */
            static const Word v_scaler[]={WORD(opt_val_sc_nearest),WORD(opt_val_sc_sharp),WORD(opt_val_sc_lanczos2),
                                          WORD(opt_val_sc_fsr1),WORD(opt_val_sc_bilinear)};
            if(v>4)v=4;
            return &v_scaler[v];
        }
        if(reg==29) {       /* P6.7 Text language: Original, Português (BR); 2 = Original with no usable pack (greyed) */
            static const Word v_pt=WORD(opt_val_tl_pt);
            if(v>2)v=2;
            if(v==1)return &v_pt;
            *dim=v==2?2:0;
            return &extra_lang()->tl_orig;
        }
        if(v>1)v=1;
        *dim=v==0;
        return &v_onoff[v];
    case ROW_REGION:
        if(v>2)v=2;
        *dim=v==0;
        return v==0?&region_lang()->ask:v==1?&v_region_us:&v_region_eu;
    default:
        if(v>1)v=1;
        *dim=v==0;
        return &v_onoff[v];
    }
}
/* Items of a list: the visible groups (ids[]) or the rows of `group`. */
static int atlas_width(int face,const char *s);
static void atlas_shadow(int face,int x,int base,const char *s,int r,int g,int b);
/* P6.37: the port version, one small dim line (the help atlas, 24 px) in the
 * free band below the plaques. align: -1 left, 0 centred, 1 right of `x`. */
static void version_line(int x,int align,int base) {
    char text[160];
    int w;
    dw3_version_text(text,sizeof(text));
    w=atlas_width(DW3_ATLAS_BODY,text);
    atlas_shadow(DW3_ATLAS_BODY,align<0?x:align>0?x-w:x-w/2,base,text,143,168,199);
}
static void options_list_screen(int group,int scroll) {
    const ListWords *t=list_lang();
    const int full=options_layer==OPTIONS_LAYER_FULL;
    const int draw_static=full||options_layer==OPTIONS_LAYER_STATIC;
    const int on_groups=group<0;
#ifdef DW3_CHEATS
    const int cht=group==DW3_OPTGRP_CHEATS; /* P6.20 rows: their own words and values */
#else
    const int cht=0;
#endif
    const int cus=group==DW3_OPTGRP_CUSTOM; /* P6.12 Customize: the PS buttons and their sources */
    const int mbt=group==DW3_OPTGRP_MAPBTN; /* D1 Map buttons: the seven bindings and Reset */
    int ids[DW3_OPTGRP_COUNT];
    const int n=on_groups?dw3_optgroups_visible(DW3_OPTGRP_CHEATS_LISTED,ids):dw3_optgroups[group].nrows;
    const unsigned locked=ELOCKED;
    const int cursor=options_layer==OPTIONS_LAYER_STATIC?-1:option_cursor;
    const int shared=static_part!=2,specific=static_part!=1;

    if(draw_static) {
        /* The title backdrop stays, dimmed; the logo and the menu are gone.
         * Over the field or a battle (a locked mask means in game) the dim is
         * deeper: the menu behind must not read through the plaques. */
        if(shared)backdrop(locked!=0);
        /* Header plaque: OPTIONS, or the name of the open group. */
        if(specific)options_plaque(L_GRP_X,L_HEAD_Y,L_GRP_W,0,0);
        if(specific){
            const Word *name=on_groups?&t->title:cus?&custom_lang()->name:&group_lang(group)->name;
            plaque_word(name,480,L_HEAD_Y+(on_groups?8:9),255,198,98); /* one baseline for every word */
        }
        if(specific&&!cht&&scroll>0)arrow(480,L_ROW_Y0-10,1);
        if(specific&&!cht&&scroll+DW3_OPTLIST_VISIBLE<n)arrow(480,L_ROW_Y0+DW3_OPTLIST_VISIBLE*L_ROW_PITCH-8,0);
        /* Message strip frame, then the button hints. */
        if(shared){
        plaque(L_DESC_X+1,L_DESC_Y+4,L_DESC_W,L_DESC_H,6,0,3,10,0,3,10,190);
        plaque(L_DESC_X,L_DESC_Y,L_DESC_W,L_DESC_H,6,99,149,196,17,40,77,255);
        plaque(L_DESC_X+2,L_DESC_Y+2,L_DESC_W-4,L_DESC_H-4,5,5,16,39,13,29,57,255);
        box(L_DESC_X+24,L_DESC_Y+5,L_DESC_W-48,2,138,98,43,255);
        {
            const Word *act=on_groups?&t->open:&t->change;
            const int hy=618,bx=L_DESC_X+L_DESC_W-t->back.w,cx=bx-36-act->w-60;
            button(cx,hy+12,0);wordsh(act,cx+22,hy,0,255,255,255);
            button(bx-22,hy+12,1);wordsh(&t->back,bx,hy,0,255,255,255);
            box(L_DESC_X,666,L_DESC_W,3,0,66,156,255);
        }
        /* P6.37: the footer, centred under the strip; always shown. */
        version_line(480,0,L_VERSION_BASE);
        }
    }
    for(int s=0;s<DW3_OPTLIST_VISIBLE;s++) {
        const int a=scroll+s;
        const int py=slot_y(s)+L_PLQ_DY,px=on_groups?L_GRP_X:L_ROWPLQ_X,pw=on_groups?L_GRP_W:L_ROWPLQ_W;
        const int focus=a==cursor;
        int reg,lock,dim;
        const Word *label,*value;

        if(a>=n)break;
        if(!clip_hits(L_ROW_X,slot_y(s),L_ROW_W,L_ROW_PITCH))continue;
        reg=on_groups?-1:dw3_optgroups[group].rows[a];
        lock=on_groups?group_locked(ids[a],locked):cht||cus||mbt?0:(int)((locked>>reg)&1u);
        label=on_groups?&group_lang(ids[a])->name
              :cus?&ps_words[reg]
              :mbt?&extra_lang()->mb_row[reg]
              :row_kind(reg)==ROW_EXTRA?&extra_lang()->reg[reg-16].row
              :row_kind(reg)==ROW_CUSTOMIZE?&custom_lang()->row
              :row_kind(reg)==ROW_RESET?&custom_lang()->reset
              :row_kind(reg)==ROW_AUTOSAVE?&autosave_lang()->row
              :row_kind(reg)==ROW_LAYOUT?&layout_lang()->row
              :row_kind(reg)==ROW_TURBO_MUSIC?&tmusic_lang()->row
              :row_kind(reg)==ROW_GLYPHS?&glyphs_lang()->row
              :row_kind(reg)==ROW_MAP_SCREEN?&mapscr_lang()->row
              :row_kind(reg)==ROW_REGION?&region_lang()->row:&t->row[reg];
#ifdef DW3_CHEATS
        if(cht){
            /* Rows 1..9 are inert (greyed) while the master switch is off. */
            label=&cheat_lang()->rows[reg];
            lock=reg>0&&!ECHEATS;
        }
#endif
        /* STATIC: every plaque idle. FULL: the idle plaque, and the lit one over
         * it for the focused row. FOCUS: the lit plaque and its label again
         * over the restored idle one. The label comes after the plaque. */
        const int lit=focus&&options_layer!=OPTIONS_LAYER_STATIC&&
                      (full||options_layer==OPTIONS_LAYER_FOCUS);
        if(draw_static&&specific)options_plaque(px,py,pw,!on_groups,0);
        if(lit)options_plaque(px,py,pw,!on_groups,1);
        /* Cheats labels follow the master switch, so they are not in the static
         * layer: FULL, FOCUS and LABEL draw them (title_options_cache.c). */
        if((draw_static&&specific&&(!cht||full))||lit||
           (cht&&options_layer==OPTIONS_LAYER_LABEL&&a==options_layer_row)){
            const int cr=lock?(lit?150:110):(lit?236:181);
            const int cg=lock?(lit?158:118):(lit?247:219);
            const int cb=lock?(lit?190:160):255;
            if(on_groups)plaque_word(label,480,py+9,cr,cg,cb); /* origin, not ink: same baseline on every plaque */
            else wordsh(label,px+58,py+9,0,cr,cg,cb);
        }
        if(on_groups)continue;
        if(full||(options_layer==OPTIONS_LAYER_VALUE&&a==options_layer_row)) {
#ifdef DW3_CHEATS
            if(cht)value=cheat_value(cheat_lang(),reg,option_values[a],&dim);
            else
#endif
            if(cus){
                /* the source that makes this PS button, or 10: waiting for a press */
                const int v=option_values[a];
                dim=0;
                value=v>=10?&custom_lang()->prompt:&btn_words[v<0?0:v];
            }else if(mbt){
                /* D1: PS button in the low four bits (10 = Select), the source of the
                 * button that makes it (+1) above. The Switch names its own button,
                 * the PC the PS one. */
                const int v=option_values[a],ps=v&15;
                dim=0;
                if(reg==DW3_MAPBTN_RESET)value=NULL;
#if defined(__SWITCH__)
                else {const int src=(v>>4)-1;value=ps>=10||src<0||src>9?&v_btn_minus:&btn_words[src];}
#else
                else value=ps>=10?&v_btn_select:&ps_words[ps>9?9:ps];
#endif
            }else value=list_value(t,reg,option_values[a],&dim);
            if(!on_groups&&!cus&&!cht&&!mbt&&reg==25)dim=option_values[0]==0; /* Sweep frames: grey while the sweep is OFF */
            if(lock)dim=2; /* P9.7: greyer than an OFF value */
            if(value)wordsh(value,px+pw-58,py+9,1,dim==2?120:dim?150:255,dim==2?128:dim?158:255,dim==2?170:dim?196:255);
#ifdef DW3_CHEATS
            else if(cht)number(option_values[a],px+pw-58,py+9,dim==2?120:255,dim==2?128:255,dim==2?170:255);
            else if(!cus&&!mbt&&reg==25)number(option_values[a],px+pw-58,py+9,dim==2?120:dim?150:255,dim==2?128:dim?158:255,dim==2?170:dim?196:255);
#endif
        }
    }
    /* The Cheats sub-screen scrolls, and its arrows are drawn last (per update,
     * not in the static layer): FULL, and LABEL with no row. */
    if(cht&&(full||(options_layer==OPTIONS_LAYER_LABEL&&options_layer_row<0))) {
        if(scroll>0)arrow(480,L_ROW_Y0-10,1);
        if(scroll+DW3_OPTLIST_VISIBLE<n)arrow(480,L_ROW_Y0+DW3_OPTLIST_VISIBLE*L_ROW_PITCH-8,0);
    }
    /* The message line of the focused item: a locked one says why. */
    if((full||options_layer==OPTIONS_LAYER_DESCRIPTION)&&cursor>=0&&cursor<n) {
        const Word *d;
        if(on_groups)d=group_locked(ids[cursor],locked)?lock_lang():&group_lang(ids[cursor])->desc;
        else {
            const int reg=dw3_optgroups[group].rows[cursor];
#ifdef DW3_CHEATS
            if(cht)d=&cheat_lang()->desc[reg];
            else
#endif
            d=cus?(option_values[cursor]>=10?&custom_lang()->promptdesc:&custom_lang()->desc)
              :mbt?&extra_lang()->mb_desc[reg==DW3_MAPBTN_RESET?2:reg==6?1:0]
              :(locked>>reg)&1u?lock_lang():reg==29&&option_values[cursor]==2?&extra_lang()->tl_nopack
              :row_kind(reg)==ROW_EXTRA?&extra_lang()->reg[reg-16].desc
              :row_kind(reg)==ROW_CUSTOMIZE?&custom_lang()->rowdesc
              :row_kind(reg)==ROW_RESET?&custom_lang()->resetdesc
              :row_kind(reg)==ROW_AUTOSAVE?&autosave_lang()->desc
              :row_kind(reg)==ROW_LAYOUT?&layout_lang()->desc
              :row_kind(reg)==ROW_TURBO_MUSIC?&tmusic_lang()->desc
              :row_kind(reg)==ROW_GLYPHS?&glyphs_lang()->desc
              :row_kind(reg)==ROW_MAP_SCREEN?&mapscr_lang()->desc
              :row_kind(reg)==ROW_REGION?&region_lang()->desc:&t->desc[reg];
        }
        wordsh(d,L_DESC_X+28,L_DESC_Y+14,0,255,255,255);
    }
}
static void options_screen(void) {
    int group=-1,scroll=0;
    if(!dw3_optlist_decode(EPAGE,&group,&scroll)){group=-1;scroll=0;}
    options_list_screen(group,scroll);
}
/* --- P6.27 / P6.28 Yes / No window ------------------------------------------
 * The Options window's look (white bezel, navy body, blue rules, the focus box
 * and pointer of a chosen row, the two button hints) in its own small texture.
 * Local coordinates: the window is DW3_DIALOG_W-8 x DW3_DIALOG_H-8 at (0, 0),
 * its shadow 6 px right and 8 px down. Rows: Yes above No, like the game's own
 * questions; focus 0 puts the pointer on No.
 * P6.38 adds the restart question of the Region at start row (title / in game,
 * US / EU) and a one-button notice, in the same window. */
#define DLG_WORDS(l) {{WORD(opt_dlg_quit_title_##l),WORD(opt_dlg_ret_title_##l)}, \
    {WORD(opt_dlg_quit_q_##l),WORD(opt_dlg_ret_q_##l)},WORD(opt_dlg_warn_##l), \
    WORD(opt_dlg_yes_##l),WORD(opt_dlg_no_##l),WORD(opt_dlg_select_##l),WORD(opt_dlg_cancel_##l), \
    WORD(opt_dlg_rst_title_##l),{WORD(opt_dlg_rst_q_us_##l),WORD(opt_dlg_rst_q_eu_##l)}, \
    WORD(opt_dlg_rst_note_##l), \
    {WORD(opt_dlg_ask_title_ss_##l),WORD(opt_dlg_ask_title_sw_##l)}, \
    {WORD(opt_dlg_ask_q_ss_##l),WORD(opt_dlg_ask_q_sw_##l)}, \
    {WORD(opt_dlg_ask_warn_ss_##l),WORD(opt_dlg_ask_warn_sw_##l)}}
typedef struct DialogWords { Word title[2],question[2],warn,yes,no,select,cancel,rst_title,rst_q[2],rst_note,
                             ask_title[2],ask_q[2],ask_warn[2]; } DialogWords;
static const DialogWords dialog_words[5]={DLG_WORDS(en),DLG_WORDS(fr),DLG_WORDS(de),
                                          DLG_WORDS(es),DLG_WORDS(it)};
static const Word dialog_ok=WORD(opt_dlg_ok);
void dw3_pc_dialog_art(uint8_t *rgba,int pitch,int kind,int focus) {
    const DialogWords *t=&dialog_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
    const int w=DW3_DIALOG_W-8,h=DW3_DIALOG_H-8,x1=w,y1=h;
    const int restart=kind>=DW3_DIALOG_RESTART_US&&kind<=DW3_DIALOG_RESTART_EU_GAME;
    const int notice=kind==DW3_DIALOG_NOTICE;
    const int ask=kind==DW3_DIALOG_SHARED_SAVES||kind==DW3_DIALOG_SWEEP; /* D1 */
    const int ak=kind==DW3_DIALOG_SWEEP?1:0;
    const int k=kind==DW3_DIALOG_RETURN?1:0;
    const Word *title=ask?&t->ask_title[ak]:restart||notice?&t->rst_title:&t->title[k];
    const Word *question=ask?&t->ask_q[ak]:restart?&t->rst_q[(kind-DW3_DIALOG_RESTART_US)&1]:notice?&t->rst_note:&t->question[k];
    /* The progress warning: the two question windows of the first slice always had it;
     * the restart question has it in game only (on the title there is nothing to lose). */
    const int warn=ask?1:restart?kind>=DW3_DIALOG_RESTART_US_GAME:!notice;
    canvas=rgba;stride=pitch;clip_x0=clip_y0=0;clip_x1=DW3_DIALOG_W;clip_y1=DW3_DIALOG_H;
    for(int j=0;j<DW3_DIALOG_H;j++)memset(rgba+(size_t)j*pitch,0,(size_t)DW3_DIALOG_W*4);
    box(6,8,w,h,0,0,8,150);
    box(0,0,w,h,255,255,255,255);
    box(w-3,3,3,h-3,181,181,181,255);box(3,h-3,w-3,3,181,181,181,255);
    box(9,9,w-18,h-18,49,49,49,255);
    box(15,15,w-30,h-30,58,58,148,255);
    box(18,18,w-36,h-36,0,0,49,255);
    /* Title, then the stepped rule of the Options window under it. */
    word(title,40,34-title->top,0,255,255,255);
    box(30,74,w-60,3,0,66,156,255);box(30,80,w-60,3,0,66,156,255);
    word(question,48,94,0,255,255,255);
    if(warn)word(ask?&t->ask_warn[ak]:&t->warn,48,130,0,255,224,140);
    for(int i=0;i<(notice?1:2);i++) {
        const int ry=176+i*42,choice_focus=notice||i==(focus?0:1);
        const Word *label=notice?&dialog_ok:i==0?&t->yes:&t->no;
        if(choice_focus){
            box(48,ry-6,w-96,CHT_ROW_H,16,74,156,255);
            box(51,ry-3,w-102,CHT_ROW_H-6,0,8,74,255);
            pointer(66,ry+3);
        }
        word(label,106,ry,0,255,255,255);
    }
    {
        const int hy=y1-74,bx=x1-48-t->cancel.w,cx=bx-36-t->select.w-60;
        button(cx,hy+12,0);word(&t->select,cx+22,hy,0,255,255,255);
        button(bx-22,hy+12,1);word(&t->cancel,bx,hy,0,255,255,255);
        box(30,y1-36,w-60,3,0,66,156,255);
    }
    clip_x0=clip_y0=0;clip_x1=960;clip_y1=720;
}

static void title_screen(void) {
    /* The original game logo is retained and framed by the PC integrator.
     * This layer owns only the native menu and discreet legal footer. */
    box(390,375,180,1,79,120,151,140);
    /* P6.37: the port version is not in this canvas any more (owner OLED p57,
     * A20): it is a label of its own at the real right edge of the picture,
     * drawn by the hosts (dw3_pc_title_version_art). */
    if(!footer_shown)return;
    plaque(252,610,456,59,8,5,16,30,5,16,30,145);
    lettering(480,619,title_credit1,title_credit1_width,title_credit1_height,3,0,0);
    lettering(480,635,title_credit2,title_credit2_width,title_credit2_height,3,0,0);
    lettering(480,651,title_credit3,title_credit3_width,title_credit3_height,3,0,0);
    box(360,600,240,1,53,91,121,140);
}
void dw3_pc_title_overlay_art(uint8_t *rgba,int pitch,int selected) {
    canvas=rgba;stride=pitch;clip_x0=clip_y0=0;clip_x1=960;clip_y1=720;
    for(int j=0;j<720;j++)memset(rgba+j*pitch,0,960*4);
    if(selected==DW3_TITLE_ART_OPTIONS){options_screen();return;}
    title_screen();
    if(selected<0) {
        plaque(320,445,320,58,10,99,149,196,17,40,77,255);
        plaque(322,447,316,54,9,12,29,57,5,16,39,235);
        box(337,451,286,2,243,158,45,255);box(337,495,286,2,243,158,45,255);
        if(label_mask[DW3_TITLE_LABEL_START])
            lettering_raw(480,471-label_h[DW3_TITLE_LABEL_START]/2,label_mask[DW3_TITLE_LABEL_START],
                          label_w[DW3_TITLE_LABEL_START],label_h[DW3_TITLE_LABEL_START]);
        else
            lettering(480,462,title_start,title_start_width,title_start_height,1,0,0);
        return;
    }
    for(int row=0;row<(load_row?DW3_TITLE_MENU_ROWS:DW3_TITLE_MENU_ROWS-1);row++) {
        /* P6.28: four plaques, 48 px high on a 52 px pitch (they were 54 on
         * 60) so Quit game fits above the credits. P6.31: with an auto-save
         * file there are five (Continue, then Load Game): 40 px high on a
         * 44 px pitch. The pitch is the same for every row, Quit game too
         * (it was set apart by 8-10 px more, which read as uneven spacing on
         * the Switch's screen); the last bottom edge is 588 / 600, above the
         * credits. */
        const int compact=load_row;
        const int quit_row=load_row?4:3;
        int x=230,w=500,h=compact?40:48,focus=row==selected;
        int y=384+row*(compact?44:52);
        title_plaque(x,y,w,h,focus,compact);
        if(row==quit_row) {
            const int li=options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0;
            const unsigned char *runs[]={title_quit_en,title_quit_fr,title_quit_de,title_quit_es,title_quit_it};
            const int ws[]={title_quit_en_width,title_quit_fr_width,title_quit_de_width,title_quit_es_width,title_quit_it_width};
            const int hs[]={title_quit_en_height,title_quit_fr_height,title_quit_de_height,title_quit_es_height,title_quit_it_height};
            text_mask(480,y+(h-hs[li])/2-1,runs[li],ws[li],hs[li],focus);
            continue;
        }
        if(load_row&&row==2) {
            /* P6.31: Continue (loads the auto-save); the game's own label
             * moves to the Load row below it. */
            const int li=options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0;
            const unsigned char *runs[]={title_continue_en,title_continue_fr,title_continue_de,title_continue_es,title_continue_it};
            const int ws[]={title_continue_en_width,title_continue_fr_width,title_continue_de_width,title_continue_es_width,title_continue_it_width};
            const int hs[]={title_continue_en_height,title_continue_fr_height,title_continue_de_height,title_continue_es_height,title_continue_it_height};
            text_mask(480,y+(h-hs[li])/2-1,runs[li],ws[li],hs[li],focus);
            continue;
        }
        if(load_row&&row==3) {
            /* P6.31: Load Game (the slot list) in the port's own lettering:
             * the EU disc's label for this row says "Continue". */
            const int li=options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0;
            const unsigned char *runs[]={title_loadgame_en,title_loadgame_fr,title_loadgame_de,title_loadgame_es,title_loadgame_it};
            const int ws[]={title_loadgame_en_width,title_loadgame_fr_width,title_loadgame_de_width,title_loadgame_es_width,title_loadgame_it_width};
            const int hs[]={title_loadgame_en_height,title_loadgame_fr_height,title_loadgame_de_height,title_loadgame_es_height,title_loadgame_it_height};
            text_mask(480,y+(h-hs[li])/2-1,runs[li],ws[li],hs[li],focus);
            continue;
        }
        if((row==0&&label_mask[DW3_TITLE_LABEL_NEW_GAME])||
           (row==2&&label_mask[DW3_TITLE_LABEL_LOAD_GAME])) {
            /* EU: the disc's own words, in the lettering of the other
             * plaques. Its sprites (a white bubble round a dark word, a
             * different look) are not drawn. The third plaque with no
             * auto-save file is Load Game (it opens the slot list); the
             * disc's sprite for it says "Continue". */
            const int li=options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0;
            const unsigned char *const *runs=row==0?
                (const unsigned char *const[]){title_newgame_en,title_newgame_fr,title_newgame_de,title_newgame_es,title_newgame_it}:
                (const unsigned char *const[]){title_loadgame_en,title_loadgame_fr,title_loadgame_de,title_loadgame_es,title_loadgame_it};
            const int ws[]={row==0?title_newgame_en_width:title_loadgame_en_width,row==0?title_newgame_fr_width:title_loadgame_fr_width,
                row==0?title_newgame_de_width:title_loadgame_de_width,row==0?title_newgame_es_width:title_loadgame_es_width,
                row==0?title_newgame_it_width:title_loadgame_it_width};
            const int hs[]={row==0?title_newgame_en_height:title_loadgame_en_height,row==0?title_newgame_fr_height:title_loadgame_fr_height,
                row==0?title_newgame_de_height:title_loadgame_de_height,row==0?title_newgame_es_height:title_loadgame_es_height,
                row==0?title_newgame_it_height:title_loadgame_it_height};
            text_mask(480,y+(h-hs[li])/2-1,runs[li],ws[li],hs[li],focus);
            continue;
        }
        const int slot=row==0?DW3_TITLE_LABEL_NEW_GAME:row==1?DW3_TITLE_LABEL_OPTIONS:DW3_TITLE_LABEL_LOAD_GAME;
        if(label_mask[slot]) {
            text_raw(480,y+(h-label_h[slot])/2-1,label_mask[slot],label_w[slot],label_h[slot],focus);
            continue;
        }
        const unsigned char *labels[]={label_0,label_1,label_2};
        int widths[]={label_0_width,label_1_width,label_2_width},heights[]={label_0_height,label_1_height,label_2_height};
        if(row==1&&options_lang==DW3_TITLE_LANG_DE){labels[1]=title_options_de;widths[1]=title_options_de_width;heights[1]=title_options_de_height;}
        if(row==1&&options_lang==DW3_TITLE_LANG_ES){labels[1]=title_options_es;widths[1]=title_options_es_width;heights[1]=title_options_es_height;}
        if(row==1&&options_lang==DW3_TITLE_LANG_IT){labels[1]=title_options_it;widths[1]=title_options_it_width;heights[1]=title_options_it_height;}
        text_mask(480,y+(h-heights[row>2?2:row])/2-1,labels[row>2?2:row],widths[row>2?2:row],heights[row>2?2:row],focus);
    }
}

void dw3_pc_title_overlay_options_component(uint8_t *rgba,int pitch,
        int layer,int row,int value,int x,int y,int w,int h,int clear_region) {
    const int old_cursor=option_cursor;
    const int old_value=(row>=0&&row<DW3_TITLE_OPTION_ROWS)?option_values[row]:0;
    if(!rgba||pitch<960*4||layer<OPTIONS_LAYER_STATIC||
       layer>OPTIONS_LAYER_LABEL||x<0||y<0||w<=0||h<=0||
       x+w>960||y+h>720)return;
    canvas=rgba;stride=pitch;
    clip_x0=x;clip_y0=y;clip_x1=x+w;clip_y1=y+h;
    if(clear_region)
        for(int j=y;j<y+h;j++)memset(rgba+j*pitch+x*4,0,(size_t)w*4);
    options_layer=layer;
    options_layer_row=row;
    if(row>=0&&row<DW3_TITLE_OPTION_ROWS)option_cursor=row;
    else option_cursor=-1;
    if(layer==OPTIONS_LAYER_VALUE&&row>=0&&row<DW3_TITLE_OPTION_ROWS)
        option_values[row]=value;
    options_screen();
    option_cursor=old_cursor;
    if(row>=0&&row<DW3_TITLE_OPTION_ROWS)option_values[row]=old_value;
    options_layer=OPTIONS_LAYER_FULL;
    options_layer_row=0;
    clip_x0=clip_y0=0;clip_x1=960;clip_y1=720;
}

/* The static layer of one layout (locked rows, page, cheats switch), rows
 * y..y+h-1, cleared first. Same pixels as options_component(STATIC) under
 * that layout; safe to call from the worker thread while the main thread
 * draws (only thread-local state is written). */
void dw3_pc_title_overlay_options_static_part(uint8_t *rgba,int pitch,
        unsigned locked,int page,int cheats_on,int part,int y,int h,int clear) {
    if(!rgba||pitch<960*4||y<0||h<=0||y+h>720)return;
    if(page==DW3_TITLE_OPTIONS_PAGE_SUB_BASE){page=dw3_optlist_page(DW3_OPTGRP_DISPLAY,0);part=1;}
    ovr_on=1;ovr_locked=locked;ovr_page=page;ovr_cheats=cheats_on;
    static_part=part;
    canvas=rgba;stride=pitch;
    clip_x0=0;clip_y0=y;clip_x1=960;clip_y1=y+h;
    if(clear)for(int j=y;j<y+h;j++)memset(rgba+(size_t)j*pitch,0,960*4);
    options_layer=OPTIONS_LAYER_STATIC;
    options_layer_row=-1;
    options_screen();
    options_layer=OPTIONS_LAYER_FULL;
    options_layer_row=0;
    clip_x0=clip_y0=0;clip_x1=960;clip_y1=720;
    static_part=0;
    ovr_on=0;
}
void dw3_pc_title_overlay_options_static_band(uint8_t *rgba,int pitch,
        unsigned locked,int page,int cheats_on,int y,int h) {
    dw3_pc_title_overlay_options_static_part(rgba,pitch,locked,page,cheats_on,0,y,h,1);
}
int dw3_pc_title_overlay_options_cheats_on(void) {
    return options_cheats_on;
}
int dw3_pc_title_overlay_options_page(void) {
    return options_page;
}

/* What the cache needs to lay a page out (title_options_cache.c). Slots are
 * the plaques on screen (a list scrolls: slot s shows item scroll + s). */
void dw3_pc_title_overlay_options_page_info(int page,int cheats_on,int *scroll,int *items,int *slots) {
    int group,sc;
    if(!dw3_optlist_decode(page,&group,&sc)){group=-1;sc=0;}
    *scroll=sc;
    *items=dw3_optlist_count(group,DW3_OPTGRP_CHEATS_LISTED);
    (void)cheats_on; /* P51: the group list no longer follows the master switch */
    *slots=DW3_OPTLIST_VISIBLE;
}
void dw3_pc_title_overlay_options_rect(int page,int kind,int slot,int *x,int *y,int *w,int *h) {
    const int ry=slot_y(slot);
    (void)page;
    switch(kind) {
    case DW3_OPTRECT_ROW:*x=L_ROW_X;*y=ry;*w=L_ROW_W;*h=L_ROW_PITCH;break;
    case DW3_OPTRECT_VALUE:*x=L_VAL_X;*y=ry+L_PLQ_DY;*w=L_VAL_W;*h=L_PLQ_H;break;
    case DW3_OPTRECT_ARROW_UP:*x=464;*y=L_ROW_Y0-12;*w=32;*h=12;break;
    case DW3_OPTRECT_ARROW_DOWN:*x=464;*y=L_ROW_Y0+DW3_OPTLIST_VISIBLE*L_ROW_PITCH-8;*w=32;*h=10;break;
    default:*x=L_DESC_X;*y=L_DESC_Y;*w=L_DESC_W;*h=L_DESC_H;break;
    }
}
/* The part of the locked mask that changes the static layer of `page`, so a
 * layout that cannot differ (Sound in game and at the title) shares one layer. */
unsigned dw3_pc_title_overlay_options_lock_key(int page,unsigned locked) {
    int group,sc;
    /* In game (a locked mask) the backdrop is dimmed deeper: bit 31 keeps those layers apart. */
    const unsigned in_game=locked?1u<<31:0;
    if(page==DW3_TITLE_OPTIONS_PAGE_SUB_BASE)return in_game;
    if(!dw3_optlist_decode(page,&group,&sc))return 0;
    if(group<0)return in_game|(locked&(DW3_TITLE_LOCK_CHEATS|group_row_mask(DW3_OPTGRP_DISPLAY)|
                                       group_row_mask(DW3_OPTGRP_SOUND)|group_row_mask(DW3_OPTGRP_GAMEPLAY)|
                                       group_row_mask(DW3_OPTGRP_MENUS)|group_row_mask(DW3_OPTGRP_CONTROLS)|
                                       group_row_mask(DW3_OPTGRP_SAVES)|group_row_mask(DW3_OPTGRP_ADVANCED)));
    return in_game|(locked&group_row_mask(group));
}

/* --- P6.30 technical help ----------------------------------------------------
 * The Help sub-screen of the Options screen (docs/P6.30_HELP.md), in the title
 * menu's identity: the dimmed backdrop, the header plaque, navy plaques with
 * steel bevels and amber rails, the message strip and the button hints. It is
 * its own full 960x720 layer, drawn by the host over (instead of) the Options
 * layer while the help is open, so it never touches the Options cache. The
 * text is the page engine's lines drawn from the baked glyph atlas
 * (port_atlas.h): the page was laid out at open, only the visible lines are
 * blitted. The canvas is allocated when the help opens and freed when it
 * closes. */
static const Dw3HelpView *help_view;
static uint8_t *help_canvas;
static unsigned help_drawn_generation, help_version, help_shown_serial;

static double help_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (double)ts.tv_sec*1000.0+(double)ts.tv_nsec/1.0e6;
}

/* UTF-8 text from the atlas: x is the pen, base the baseline. Returns the width. */
static int atlas_text(int face,int x,int base,const char *s,int len,int r,int g,int b) {
    int i=0,pen=x;
    while(i<len) {
        unsigned cp;
        const int u=dw3_pages_utf8(s+i,len-i,&cp);
        if(u<=0)break;
        {
            const Dw3AtlasGlyph *gl=dw3_atlas_glyph(face,cp);
            if(gl->w&&gl->h) {
                const unsigned char *cv=dw3_atlas_coverage(gl);
                const int gx=pen+gl->left,gy=base-gl->top;
                for(int j=0;j<gl->h;j++)for(int k=0;k<gl->w;k++) {
                    const int a=cv[j*gl->w+k];
                    if(a)pixel(gx+k,gy+j,r,g,b,a);
                }
            }
            pen+=gl->adv;
        }
        i+=u;
    }
    return pen-x;
}
static int atlas_width(int face,const char *s) {
    return dw3_atlas_measure(face,s,(int)strlen(s));
}
/* The title menu's lettering: a one pixel dark shadow under warm or cool text. */
static void atlas_shadow(int face,int x,int base,const char *s,int r,int g,int b) {
    const int n=(int)strlen(s);
    atlas_text(face,x+1,base+2,s,n,2,7,19);
    atlas_text(face,x,base,s,n,r,g,b);
}
static void atlas_centered(int face,int cx,int base,const char *s,int r,int g,int b) {
    atlas_shadow(face,cx-atlas_width(face,s)/2,base,s,r,g,b);
}
int dw3_pc_title_overlay_version_shown(void) { return version_shown; }
/* The title's version label: a DW3_TITLE_VERSION_W x DW3_TITLE_VERSION_H RGBA of
 * its own, the short text right-aligned in it, the same small dim line as the
 * Options footer. The hosts place it with its right edge DW3_TITLE_VERSION_MARGIN
 * canvas pixels inside the right edge of the picture, so that in widescreen it
 * sits at the 16:9 edge and in 4:3 at the canvas corner. */
static void version_short(char *text,size_t cap) {
    dw3_version_short_text(text,cap);
}
unsigned dw3_pc_title_version_generation(void) {
    char text[160];
    unsigned h=5381u;
    version_short(text,sizeof(text));
    for(const char *c=text;*c;c++)h=h*33u+(unsigned char)*c;
    return h|1u;
}
void dw3_pc_title_version_art(uint8_t *rgba,int pitch) {
    char text[160];
    uint8_t *sc=canvas;const int ss=stride,cx0=clip_x0,cy0=clip_y0,cx1=clip_x1,cy1=clip_y1;
    canvas=rgba;stride=pitch;clip_x0=clip_y0=0;clip_x1=DW3_TITLE_VERSION_W;clip_y1=DW3_TITLE_VERSION_H;
    for(int j=0;j<DW3_TITLE_VERSION_H;j++)memset(rgba+(size_t)j*pitch,0,(size_t)DW3_TITLE_VERSION_W*4);
    version_short(text,sizeof(text));
    fprintf(stderr,"dw3-title-overlay: version label \"%s\" (short form, at the right edge of the picture; the Options footer and dw3-version keep the full text)\n",text);
    atlas_shadow(DW3_ATLAS_BODY,DW3_TITLE_VERSION_W-4-atlas_width(DW3_ATLAS_BODY,text),
                 DW3_TITLE_VERSION_BASE,text,143,168,199);
    canvas=sc;stride=ss;clip_x0=cx0;clip_y0=cy0;clip_x1=cx1;clip_y1=cy1;
}

/* A text plaque: bevel, shadow, navy body with a vertical ramp, amber rail. */
static void text_plaque(int x,int y,int w,int h) {
    plaque(x+1,y+4,w,h,6,0,3,10,0,3,10,190);
    plaque(x,y,w,h,6,99,149,196,17,40,77,255);
    plaque(x+2,y+2,w-4,h-4,5,5,16,39,13,29,57,255);
    box(x+24,y+5,w-48,2,138,98,43,255);
}

static void help_line(const Dw3PagesLine *ln,int x0,int top) {
    static const int col[][3]={{214,230,250},{214,230,250},{255,198,98},{214,230,250},
                               {214,230,250},{181,219,255},{255,186,132},{236,247,255},
                               {214,230,250}};
    const int st=ln->style<=DW3_PAGES_S_A?ln->style:DW3_PAGES_S_P;
    const int base=top+dw3_atlas_ascent(0)+1;
    for(int r=0;r<2;r++) {
        if(!ln->s[r]||ln->len[r]<=0)continue;
        if(st==DW3_PAGES_S_ROW&&r==1)
            atlas_text(ln->face[r],x0+ln->x[r],base,ln->s[r],ln->len[r],255,216,119);
        else
            atlas_text(ln->face[r],x0+ln->x[r],base,ln->s[r],ln->len[r],col[st][0],col[st][1],col[st][2]);
    }
    if(ln->flags&DW3_PAGES_LF_BULLET)atlas_text(DW3_ATLAS_BOLD,x0+6,base,"\xE2\x80\xA2",3,243,158,45);
    if(ln->flags&DW3_PAGES_LF_BAR) {
        /* Never colour alone: a note is one solid bar, a warning two thin ones. */
        if(st==DW3_PAGES_S_WARN){box(x0+2,top,2,DW3_HELP_LINE_PITCH,255,138,43,255);
                                 box(x0+7,top,2,DW3_HELP_LINE_PITCH,255,138,43,255);}
        else box(x0+3,top,3,DW3_HELP_LINE_PITCH,99,149,196,255);
    }
}

/* The hints, right aligned in the strip's width, the last one first. */
static void help_hints(const Dw3HelpView *v) {
    const int hy=618;
    int xr=L_DESC_X+L_DESC_W;
    for(int i=v->nhints-1;i>=0;i--) {
        const char *s=v->hint[i];
        if(!s[0])continue;
        {
            const int w=atlas_width(DW3_ATLAS_BODY,s),xt=xr-w;
            if(v->hint_cap[i])button(xt-22,hy+12,v->hint_cap[i]==2);
            atlas_shadow(DW3_ATLAS_BODY,xt,hy+22,s,255,255,255);
            xr=xt-(v->hint_cap[i]?36:0)-44;
        }
    }
}

static void help_draw(const Dw3HelpView *v) {
    const int on_list=v->screen==DW3_HELP_SCREEN_LIST;
    canvas=help_canvas;stride=960*4;clip_x0=clip_y0=0;clip_x1=960;clip_y1=720;
    memset(help_canvas,0,(size_t)960*720*4);
    backdrop(v->in_game!=0);
    options_plaque(L_GRP_X,L_HEAD_Y,L_GRP_W,0,0);
    if(on_list) {
        const int lang=v->lang>=DW3_TITLE_LANG_EN&&v->lang<=DW3_TITLE_LANG_IT?v->lang:DW3_TITLE_LANG_EN;
        if(v->header[0])atlas_centered(DW3_ATLAS_TITLE,480,L_HEAD_Y+35,v->header,255,198,98); /* P6.35: the walkthrough's group name */
        else plaque_word(&group_words[lang][DW3_OPTGRP_HELP].name,480,L_HEAD_Y+9,255,198,98);
    } else {
        atlas_centered(DW3_ATLAS_TITLE,480,L_HEAD_Y+35,v->header,255,198,98);
    }
    /* Message strip frame, text, hints. */
    plaque(L_DESC_X+1,L_DESC_Y+4,L_DESC_W,L_DESC_H,6,0,3,10,0,3,10,190);
    plaque(L_DESC_X,L_DESC_Y,L_DESC_W,L_DESC_H,6,99,149,196,17,40,77,255);
    plaque(L_DESC_X+2,L_DESC_Y+2,L_DESC_W-4,L_DESC_H-4,5,5,16,39,13,29,57,255);
    box(L_DESC_X+24,L_DESC_Y+5,L_DESC_W-48,2,138,98,43,255);
    box(L_DESC_X,666,L_DESC_W,3,0,66,156,255);
    atlas_shadow(DW3_ATLAS_BODY,L_DESC_X+28,L_DESC_Y+37,v->strip,255,255,255);
    help_hints(v);
    if(on_list) {
        if(v->scroll>0)arrow(480,L_ROW_Y0-10,1);
        if(v->scroll+DW3_OPTLIST_VISIBLE<v->npages)arrow(480,L_ROW_Y0+DW3_OPTLIST_VISIBLE*L_ROW_PITCH-8,0);
        for(int s=0;s<DW3_OPTLIST_VISIBLE;s++) {
            const int a=v->scroll+s,py=slot_y(s)+L_PLQ_DY;
            if(a>=v->npages)break;
            options_plaque(L_GRP_X,py,L_GRP_W,0,0);
            if(a==v->cursor)options_plaque(L_GRP_X,py,L_GRP_W,0,1);
            atlas_centered(DW3_ATLAS_TITLE,480,py+35,v->titles[a],a==v->cursor?236:181,a==v->cursor?247:219,255);
        }
        return;
    }
    text_plaque(DW3_HELP_PANEL_X,DW3_HELP_PANEL_Y,DW3_HELP_PANEL_W,DW3_HELP_PANEL_H);
    if(v->arrow_up)arrow(480,DW3_HELP_PANEL_Y-12,1);
    if(v->arrow_down)arrow(480,DW3_HELP_PANEL_Y+DW3_HELP_PANEL_H+4,0);
    for(int k=0;k<v->panel&&v->top+k<v->nlines;k++)
        help_line(&v->lines[v->top+k],DW3_HELP_PANEL_X+DW3_HELP_PAD_X,
                  DW3_HELP_PANEL_Y+DW3_HELP_PAD_Y+k*DW3_HELP_LINE_PITCH);
}

void dw3_pc_help_set_view(const Dw3HelpView *view) {
    help_view=view;
    if(!view) {
        if(help_canvas) {
            free(help_canvas);help_canvas=NULL;
            fprintf(stderr,"dw3-help: panel canvas released\n");
        }
        help_drawn_generation=0;
        return;
    }
    if(!help_canvas) {
        help_canvas=(uint8_t *)malloc((size_t)960*720*4);
        if(!help_canvas) {
            fprintf(stderr,"dw3-help: panel canvas of %d bytes cannot be allocated\n",960*720*4);
            help_view=NULL;
            return;
        }
        fprintf(stderr,"dw3-help: panel canvas allocated, %d bytes (960x720 RGBA)\n",960*720*4);
        help_drawn_generation=0;
    }
}
int dw3_pc_help_active(void) {
    return help_view!=NULL&&help_canvas!=NULL;
}
const uint8_t *dw3_pc_help_render(unsigned *version) {
    static unsigned composes;
    if(!dw3_pc_help_active())return NULL;
    if(help_view->generation!=help_drawn_generation||!help_version) {
        const double t0=help_now_ms();
        double ms;
        help_draw(help_view);
        help_drawn_generation=help_view->generation;
        help_version++;
        composes++;
        ms=help_now_ms()-t0;
        if(help_view->open_serial!=help_shown_serial) {
            help_shown_serial=help_view->open_serial;
            fprintf(stderr,"%s: first frame composed %.1f ms after open\n",
                    help_view->log_tag[0]?help_view->log_tag:"dw3-help",
                    help_now_ms()-help_view->open_ms);
        }
        if(composes<=2||ms>20.0)
            fprintf(stderr,"dw3-help: panel composed in %.0f us (%s, compose %u)\n",ms*1000.0,
                    help_view->screen==DW3_HELP_SCREEN_LIST?"list":"page",composes);
    }
    if(version)*version=help_version;
    return help_canvas;
}
