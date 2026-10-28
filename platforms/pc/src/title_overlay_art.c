/* Native-resolution PC title plaques. Geometry is authored in 960x720 pixels. */
#include "title_overlay_art.h"
#include "port_options_groups.h"
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
/* P6.31: the title menu has the extra Load row (an auto-save file exists). */
static int load_row;
static unsigned art_generation;
static unsigned options_generation;
static unsigned dialog_language_generation;
static int option_cursor;
static int option_values[DW3_TITLE_OPTION_ROWS];
enum { OPTIONS_LAYER_FULL, OPTIONS_LAYER_STATIC, OPTIONS_LAYER_FOCUS,
       OPTIONS_LAYER_VALUE, OPTIONS_LAYER_DESCRIPTION };
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
void dw3_pc_title_overlay_set_options_locked(unsigned mask) {
    mask&=((1u<<DW3_TITLE_OPTION_ROWS)-1u)|DW3_TITLE_LOCK_CHEATS;
    if(mask!=options_locked){options_locked=mask;options_layout++;options_generation++;}
}
unsigned dw3_pc_title_overlay_options_locked(void) { return options_locked; }
unsigned dw3_pc_title_overlay_options_layout(void) { return options_layout; }
void dw3_pc_title_overlay_set_options_page(int page,int cheats_on) {
    int group,scroll;
#ifdef DW3_CHEATS
    const int cheats_page=page==DW3_TITLE_OPTIONS_PAGE_CHEATS||page==DW3_TITLE_OPTIONS_PAGE_CHEATS2;
#else
    const int cheats_page=0;
#endif
    if(!cheats_page&&!dw3_optlist_decode(page,&group,&scroll))page=DW3_TITLE_OPTIONS_PAGE_OPTIONS;
    cheats_on=cheats_on?1:0;
    if(page!=options_page||cheats_on!=options_cheats_on){
        options_page=page;options_cheats_on=cheats_on;
        options_layout++;options_generation++;
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
/* A button cap: Switch letters, or the PlayStation Cross / Triangle marks. */
static void button(int cx,int cy,int back) {
    disc(cx,cy,14,214,222,240);
#if defined(__SWITCH__)
    const Word b_key=WORD(opt_key_b),x_key=WORD(opt_key_x);
    const Word *k=back?&x_key:&b_key;
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
       ROW_BATTLE, ROW_AUTOSAVE };
static int row_kind(int row) {
    return row == 1 ? ROW_FAST_FORWARD : row == 3 || row == 4 ? ROW_VOLUME :
           row == 5 ? ROW_ENCOUNTER : row == 6 ? ROW_TEXT :
           row == 7 ? ROW_BATTLE : row == 8 ? ROW_AUTOSAVE : ROW_SWITCH;
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
    Word rows[DW3_TITLE_CHEAT_PAGES*DW3_TITLE_CHEAT_ROWS],desc[DW3_TITLE_CHEAT_PAGES*DW3_TITLE_CHEAT_ROWS];
    Word warp[DW3_TITLE_CHEAT_WARPS];
} CheatWords;
static const CheatWords cheat_words[5]={CHEAT_WORDS(en),CHEAT_WORDS(fr),CHEAT_WORDS(de),
                                        CHEAT_WORDS(es),CHEAT_WORDS(it)};
static const CheatWords *cheat_lang(void) {
    return &cheat_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
}
/* The Cheats page: the Options window, its own title, five rows (master,
 * Fill bag, Instant win, Battle now, marked saves), the same focus box,
 * message line and hints, in the language of the disc. While the master
 * switch is off rows 1..4 are greyed. Same layers as options_screen. */
/* A number of the Give item rows: the digits (fixed advance), right edge at x. */
static void number(int v,int x,int y,int r,int g,int b) {
    static const Word digits[10]={WORD(opt_cht_d0),WORD(opt_cht_d1),WORD(opt_cht_d2),WORD(opt_cht_d3),
                                  WORD(opt_cht_d4),WORD(opt_cht_d5),WORD(opt_cht_d6),WORD(opt_cht_d7),
                                  WORD(opt_cht_d8),WORD(opt_cht_d9)};
    int n=v>=100?3:v>=10?2:1,adv=digits[0].w;
    if(v<0)v=0;
    for(int k=0,div=n==3?100:n==2?10:1;k<n;k++,div/=10)
        word(&digits[(v/div)%10],x-(n-k)*adv,y,0,r,g,b);
}
static void cheats_screen(void) {
    static const Word values[]={WORD(opt_off),WORD(opt_on)};
    static const Word money[]={WORD(opt_off),WORD(opt_cht_m10k),WORD(opt_cht_m100k),WORD(opt_cht_mmax)};
    static const Word steps[]={WORD(opt_n1),WORD(opt_n2),WORD(opt_n3),WORD(opt_n4),WORD(opt_n5),
                               WORD(opt_n6),WORD(opt_n7),WORD(opt_n8),WORD(opt_n9),WORD(opt_n10)};
    const CheatWords *t=cheat_lang();
    const int x0=OPT_X0,y0=OPT_Y0,w=OPT_W,h=OPT_H,x1=x0+w,y1=y0+h;
    const int full=options_layer==OPTIONS_LAYER_FULL;
    const int draw_static=full||options_layer==OPTIONS_LAYER_STATIC;
    const int page2=EPAGE==DW3_TITLE_OPTIONS_PAGE_CHEATS2;

    if(draw_static) {
    box(0,0,960,720,0,0,16,150);
    box(x0+6,y0+8,w,h,0,0,8,150);
    box(x0,y0,w,h,255,255,255,255);
    box(x0+w-3,y0+3,3,h-3,181,181,181,255);box(x0+3,y0+h-3,w-3,3,181,181,181,255);
    box(x0+9,y0+9,w-18,h-18,49,49,49,255);
    box(x0+15,y0+15,w-30,h-30,58,58,148,255);
    box(x0+18,y0+18,w-36,h-36,0,0,49,255);
    {
        const int a=y0+36,xs=x0+500;
        box(x0+30,a,xs-x0-30,3,0,66,156,255);box(x0+30,a+6,xs-x0-24,3,0,66,156,255);
        segment(xs,a+1.5f,xs+30,a+31.5f,1.6f,0,66,156);
        segment(xs+6,a+7.5f,xs+30,a+31.5f,1.6f,0,66,156);
        box(xs+30,a+30,x1-30-xs-30,3,0,66,156,255);
        word(page2?&t->title2:&t->title,xs+48,a+2-t->title.top,0,255,255,255);
    }
    }
    for(int i=0;i<DW3_TITLE_CHEAT_ROWS;i++) {
        const int row=page2*DW3_TITLE_CHEAT_ROWS+i;
        const int ry=OPT_ROW_Y+i*OPT_ROW_STEP,focus=i==option_cursor;
        const int grey=row>0&&!ECHEATS;
        int v=option_values[i];
        const Word *value;
        int dim;
        if(focus&&(full||options_layer==OPTIONS_LAYER_FOCUS)){
            box(x0+48,ry-6,w-96,OPT_ROW_H,16,74,156,255);
            box(x0+51,ry-3,w-102,OPT_ROW_H-6,0,8,74,255);
            pointer(x0+66,ry+1);
        }
        if(draw_static||(options_layer==OPTIONS_LAYER_FOCUS&&i==options_layer_row))
            word(&t->rows[row],x0+106,ry-2,0,grey?150:255,grey?158:255,grey?196:255);
        if(v<0)v=0;
        if(row==4){
            if(v>10)v=10;
            value=v?&steps[v-1]:&t->none;dim=v==0;
        }else if(row==1||row==3||row==9){
            value=v?&t->ready:&values[0];dim=v==0;
        }else if(row==5){
            if(v>3)v=3;
            value=&money[v];dim=v==0;
        }else if(row==6){
            if(v>DW3_TITLE_CHEAT_WARPS)v=DW3_TITLE_CHEAT_WARPS;
            value=v?&t->warp[v-1]:&values[0];dim=v==0;
        }else if(row==7||row==8){
            value=NULL;dim=0;
        }else{
            value=&values[v?1:0];dim=v==0;
        }
        if(grey)dim=1;
        if(full||(options_layer==OPTIONS_LAYER_VALUE&&i==options_layer_row)){
            if(value)word(value,x1-96,ry-2,1,dim?150:255,dim?158:255,dim?196:255);
            else number(v,x1-96,ry-2,grey?150:255,grey?158:255,grey?196:255);
        }
    }
    if(draw_static)box(x0+30,y1-152,w-60,3,16,156,247,255);
    if((full||options_layer==OPTIONS_LAYER_DESCRIPTION)&&
       option_cursor>=0&&option_cursor<DW3_TITLE_CHEAT_ROWS)
        word(&t->desc[page2*DW3_TITLE_CHEAT_ROWS+option_cursor],x0+48,y1-128,0,255,255,255);
    if(draw_static) {
        const int hy=y1-74,bx=x1-48-t->back.w,cx=bx-36-t->change.w-60;
        button(cx,hy+12,0);word(&t->change,cx+22,hy,0,255,255,255);
        button(bx-22,hy+12,1);word(&t->back,bx,hy,0,255,255,255);
        box(x0+30,y1-36,w-60,3,0,66,156,255);
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
enum { DIM_TITLE = 150, DIM_IN_GAME = 208 };
enum { L_ROW_X = 152, L_ROW_W = 656, L_ROW_Y0 = 116, L_ROW_PITCH = 66,
       L_PLQ_H = 50, L_PLQ_DY = 8, L_GRP_X = 230, L_GRP_W = 500,
       L_ROWPLQ_X = 160, L_ROWPLQ_W = 640, L_VAL_X = 500, L_VAL_W = 260,
       L_DESC_X = 120, L_DESC_Y = 532, L_DESC_W = 720, L_DESC_H = 60,
       L_HEAD_Y = 40 };
/* The cheats pages keep the Options window of P9.2-P9.7 (its geometry). */
enum { C_ROW_X = 132, C_ROW_Y = 150, C_ROW_W = 696, C_ROW_H = 34, C_ROW_STEP = 38,
       C_VAL_X = 560, C_VAL_W = 268, C_DESC_Y = 524, C_DESC_H = 52 };

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
    {WORD(opt_grp_cheats_##l),WORD(opt_grpdesc_cheats_##l)}}
typedef struct GroupWords { Word name,desc; } GroupWords;
enum { LISTED_GROUPS = 5 }; /* the groups with lettering (the built ones) */
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
    for(int k=0;k<dw3_optgroups[group].nrows;k++)m|=1u<<dw3_optgroups[group].rows[k];
    return m;
}
static int group_locked(int group,unsigned locked) {
    if(group==DW3_OPTGRP_CHEATS)return (locked&DW3_TITLE_LOCK_CHEATS)!=0;
    return dw3_optgroups[group].nrows>0&&(locked&group_row_mask(group))==group_row_mask(group);
}
/* The geometry of slot s (0 = the first visible plaque). */
static int slot_y(int s) { return L_ROW_Y0+s*L_ROW_PITCH; }

static const Word v_onoff[]={WORD(opt_off),WORD(opt_on),WORD(opt_2x),WORD(opt_3x),WORD(opt_4x)};
static const Word v_steps[]={WORD(opt_n1),WORD(opt_n2),WORD(opt_n3),WORD(opt_n4),WORD(opt_n5),
                             WORD(opt_n6),WORD(opt_n7),WORD(opt_n8),WORD(opt_n9),WORD(opt_n10)};
/* P6.18: Encounter rate, indexed by the stored value (0 1x, 1 0.5x, 2 OFF,
 * 3 1.5x, 4 2x; the screen steps them OFF, 0.5x, 1x, 1.5x, 2x). */
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
    default:
        if(v>1)v=1;
        *dim=v==0;
        return &v_onoff[v];
    }
}
/* Items of a list: the visible groups (ids[]) or the rows of `group`. */
static void options_list_screen(int group,int scroll) {
    const ListWords *t=list_lang();
    const int full=options_layer==OPTIONS_LAYER_FULL;
    const int draw_static=full||options_layer==OPTIONS_LAYER_STATIC;
    const int on_groups=group<0;
    int ids[DW3_OPTGRP_COUNT];
    const int n=on_groups?dw3_optgroups_visible(ECHEATS,ids):dw3_optgroups[group].nrows;
    const unsigned locked=ELOCKED;
    const int cursor=options_layer==OPTIONS_LAYER_STATIC?-1:option_cursor;

    if(draw_static) {
        /* The title backdrop stays, dimmed; the logo and the menu are gone.
         * Over the field or a battle (a locked mask means in game) the dim is
         * deeper: the menu behind must not read through the plaques. */
        box(0,0,960,720,0,0,16,locked?DIM_IN_GAME:DIM_TITLE);
        /* Header plaque: OPTIONS, or the name of the open group. */
        options_plaque(L_GRP_X,L_HEAD_Y,L_GRP_W,0,0);
        {
            const Word *name=on_groups?&t->title:&group_lang(group)->name;
            plaque_word(name,480,L_HEAD_Y+L_PLQ_H/2-name->top-name->h/2-1,255,198,98);
        }
        if(scroll>0)arrow(480,L_ROW_Y0-10,1);
        if(scroll+DW3_OPTLIST_VISIBLE<n)arrow(480,L_ROW_Y0+DW3_OPTLIST_VISIBLE*L_ROW_PITCH-8,0);
        /* Message strip frame, then the button hints. */
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
        lock=on_groups?group_locked(ids[a],locked):(int)((locked>>reg)&1u);
        label=on_groups?&group_lang(ids[a])->name
              :row_kind(reg)==ROW_AUTOSAVE?&autosave_lang()->row:&t->row[reg];
        /* STATIC: every plaque idle. FULL: the idle plaque, and the lit one over
         * it for the focused row. FOCUS: the lit plaque and its label again
         * over the restored idle one. The label comes after the plaque. */
        const int lit=focus&&options_layer!=OPTIONS_LAYER_STATIC&&
                      (full||options_layer==OPTIONS_LAYER_FOCUS);
        if(draw_static)options_plaque(px,py,pw,!on_groups,0);
        if(lit)options_plaque(px,py,pw,!on_groups,1);
        if(draw_static||lit){
            const int cr=lock?(lit?150:110):(lit?236:181);
            const int cg=lock?(lit?158:118):(lit?247:219);
            const int cb=lock?(lit?190:160):255;
            if(on_groups)plaque_word(label,480,py+L_PLQ_H/2-label->top-label->h/2-1,cr,cg,cb);
            else wordsh(label,px+58,py+9,0,cr,cg,cb);
        }
        if(on_groups)continue;
        if(full||(options_layer==OPTIONS_LAYER_VALUE&&a==options_layer_row)) {
            value=list_value(t,reg,option_values[a],&dim);
            if(lock)dim=2; /* P9.7: greyer than an OFF value */
            wordsh(value,px+pw-58,py+9,1,dim==2?120:dim?150:255,dim==2?128:dim?158:255,dim==2?170:dim?196:255);
        }
    }
    /* The message line of the focused item: a locked one says why. */
    if((full||options_layer==OPTIONS_LAYER_DESCRIPTION)&&cursor>=0&&cursor<n) {
        const Word *d;
        if(on_groups)d=group_locked(ids[cursor],locked)?lock_lang():&group_lang(ids[cursor])->desc;
        else {
            const int reg=dw3_optgroups[group].rows[cursor];
            d=(locked>>reg)&1u?lock_lang():row_kind(reg)==ROW_AUTOSAVE?&autosave_lang()->desc:&t->desc[reg];
        }
        wordsh(d,L_DESC_X+28,L_DESC_Y+14,0,255,255,255);
    }
}
static void options_screen(void) {
    int group=-1,scroll=0;
#ifdef DW3_CHEATS
    if(EPAGE==DW3_TITLE_OPTIONS_PAGE_CHEATS||EPAGE==DW3_TITLE_OPTIONS_PAGE_CHEATS2){cheats_screen();return;}
#endif
    if(!dw3_optlist_decode(EPAGE,&group,&scroll)){group=-1;scroll=0;}
    options_list_screen(group,scroll);
}
/* --- P6.27 / P6.28 Yes / No window ------------------------------------------
 * The Options window's look (white bezel, navy body, blue rules, the focus box
 * and pointer of a chosen row, the two button hints) in its own small texture.
 * Local coordinates: the window is DW3_DIALOG_W-8 x DW3_DIALOG_H-8 at (0, 0),
 * its shadow 6 px right and 8 px down. Rows: Yes above No, like the game's own
 * questions; focus 0 puts the pointer on No. */
#define DLG_WORDS(l) {{WORD(opt_dlg_quit_title_##l),WORD(opt_dlg_ret_title_##l)}, \
    {WORD(opt_dlg_quit_q_##l),WORD(opt_dlg_ret_q_##l)},WORD(opt_dlg_warn_##l), \
    WORD(opt_dlg_yes_##l),WORD(opt_dlg_no_##l),WORD(opt_dlg_select_##l),WORD(opt_dlg_cancel_##l)}
typedef struct DialogWords { Word title[2],question[2],warn,yes,no,select,cancel; } DialogWords;
static const DialogWords dialog_words[5]={DLG_WORDS(en),DLG_WORDS(fr),DLG_WORDS(de),
                                          DLG_WORDS(es),DLG_WORDS(it)};
void dw3_pc_dialog_art(uint8_t *rgba,int pitch,int kind,int focus) {
    const DialogWords *t=&dialog_words[options_lang>=DW3_TITLE_LANG_EN&&options_lang<=DW3_TITLE_LANG_IT?options_lang:0];
    const int w=DW3_DIALOG_W-8,h=DW3_DIALOG_H-8,x1=w,y1=h;
    const int k=kind==DW3_DIALOG_RETURN?1:0;
    canvas=rgba;stride=pitch;clip_x0=clip_y0=0;clip_x1=DW3_DIALOG_W;clip_y1=DW3_DIALOG_H;
    for(int j=0;j<DW3_DIALOG_H;j++)memset(rgba+(size_t)j*pitch,0,(size_t)DW3_DIALOG_W*4);
    box(6,8,w,h,0,0,8,150);
    box(0,0,w,h,255,255,255,255);
    box(w-3,3,3,h-3,181,181,181,255);box(3,h-3,w-3,3,181,181,181,255);
    box(9,9,w-18,h-18,49,49,49,255);
    box(15,15,w-30,h-30,58,58,148,255);
    box(18,18,w-36,h-36,0,0,49,255);
    /* Title, then the stepped rule of the Options window under it. */
    word(&t->title[k],40,34-t->title[k].top,0,255,255,255);
    box(30,74,w-60,3,0,66,156,255);box(30,80,w-60,3,0,66,156,255);
    word(&t->question[k],48,94,0,255,255,255);
    word(&t->warn,48,130,0,255,224,140);
    for(int i=0;i<2;i++) {
        const int ry=176+i*42,choice_focus=i==(focus?0:1);
        const Word *label=i==0?&t->yes:&t->no;
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
       layer>OPTIONS_LAYER_DESCRIPTION||x<0||y<0||w<=0||h<=0||
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
void dw3_pc_title_overlay_options_static_band(uint8_t *rgba,int pitch,
        unsigned locked,int page,int cheats_on,int y,int h) {
    if(!rgba||pitch<960*4||y<0||h<=0||y+h>720)return;
    ovr_on=1;ovr_locked=locked;ovr_page=page;ovr_cheats=cheats_on;
    canvas=rgba;stride=pitch;
    clip_x0=0;clip_y0=y;clip_x1=960;clip_y1=y+h;
    for(int j=y;j<y+h;j++)memset(rgba+(size_t)j*pitch,0,960*4);
    options_layer=OPTIONS_LAYER_STATIC;
    options_layer_row=-1;
    options_screen();
    options_layer=OPTIONS_LAYER_FULL;
    options_layer_row=0;
    clip_x0=clip_y0=0;clip_x1=960;clip_y1=720;
    ovr_on=0;
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
    if(dw3_optlist_decode(page,&group,&sc)) {
        *scroll=sc;
        *items=dw3_optlist_count(group,cheats_on);
        *slots=DW3_OPTLIST_VISIBLE;
    } else { /* a Cheats page: the Options window's five rows */
        *scroll=0;
        *items=*slots=5;
    }
}
void dw3_pc_title_overlay_options_rect(int page,int kind,int slot,int *x,int *y,int *w,int *h) {
    int group,sc;
    if(dw3_optlist_decode(page,&group,&sc)) {
        const int ry=slot_y(slot);
        switch(kind) {
        case DW3_OPTRECT_ROW:*x=L_ROW_X;*y=ry;*w=L_ROW_W;*h=L_ROW_PITCH;break;
        case DW3_OPTRECT_VALUE:*x=L_VAL_X;*y=ry+L_PLQ_DY;*w=L_VAL_W;*h=L_PLQ_H;break;
        default:*x=L_DESC_X;*y=L_DESC_Y;*w=L_DESC_W;*h=L_DESC_H;break;
        }
    } else {
        const int ry=C_ROW_Y+C_ROW_STEP*slot;
        switch(kind) {
        case DW3_OPTRECT_ROW:*x=C_ROW_X;*y=ry;*w=C_ROW_W;*h=C_ROW_H;break;
        case DW3_OPTRECT_VALUE:*x=C_VAL_X;*y=ry;*w=C_VAL_W;*h=C_ROW_H;break;
        default:*x=C_ROW_X;*y=C_DESC_Y;*w=C_ROW_W;*h=C_DESC_H;break;
        }
    }
}
/* The part of the locked mask that changes the static layer of `page`, so a
 * layout that cannot differ (Sound in game and at the title) shares one layer. */
unsigned dw3_pc_title_overlay_options_lock_key(int page,unsigned locked) {
    int group,sc;
    /* In game (a locked mask) the backdrop is dimmed deeper: bit 31 keeps those layers apart. */
    const unsigned in_game=locked?1u<<31:0;
    if(!dw3_optlist_decode(page,&group,&sc))return 0;
    if(group<0)return in_game|(locked&(DW3_TITLE_LOCK_CHEATS|group_row_mask(DW3_OPTGRP_DISPLAY)|
                                       group_row_mask(DW3_OPTGRP_SOUND)|group_row_mask(DW3_OPTGRP_GAMEPLAY)|
                                       group_row_mask(DW3_OPTGRP_SAVES)));
    return in_game|(locked&group_row_mask(group));
}
