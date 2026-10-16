/* Native-resolution PC title plaques. Geometry is authored in 960x720 pixels. */
#include "title_overlay_art.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "title_overlay_labels.inc"
#include "title_overlay_lettering.inc"
static uint8_t *canvas;
static int stride;
static int clip_x0,clip_y0,clip_x1=960,clip_y1=720;
enum { LABEL_MAX_W = 440, LABEL_MAX_H = 48 };
/* Runtime label masks (raw coverage), the Options language and the footer. */
static uint8_t *label_mask[DW3_TITLE_LABEL_COUNT];
static int label_w[DW3_TITLE_LABEL_COUNT], label_h[DW3_TITLE_LABEL_COUNT];
static int options_lang = DW3_TITLE_LANG_EN;
static int footer_shown = 1;
static unsigned art_generation;
static unsigned options_generation;
static int option_cursor;
static int option_values[DW3_TITLE_OPTION_ROWS];
enum { OPTIONS_LAYER_FULL, OPTIONS_LAYER_STATIC, OPTIONS_LAYER_FOCUS,
       OPTIONS_LAYER_VALUE, OPTIONS_LAYER_DESCRIPTION };
static int options_layer = OPTIONS_LAYER_FULL;
static int options_layer_row;
static void pixel(int x,int y,int r,int g,int b,int alpha) {
    if(x<clip_x0||x>=clip_x1||y<clip_y0||y>=clip_y1||
       x<0||x>=960||y<0||y>=720||alpha<=0)return;
    uint8_t *p=canvas+y*stride+x*4;
    int a=alpha+p[3]*(255-alpha)/255;
    if(!a)return;
    p[0]=(r*alpha+p[0]*p[3]*(255-alpha)/255)/a;
    p[1]=(g*alpha+p[1]*p[3]*(255-alpha)/255)/a;
    p[2]=(b*alpha+p[2]*p[3]*(255-alpha)/255)/a;p[3]=a;
}
static void box(int x,int y,int w,int h,int r,int g,int b,int a) {
    for(int j=0;j<h;j++)for(int i=0;i<w;i++)pixel(x+i,y+j,r,g,b,a);
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
    if(lang!=options_lang){options_lang=lang;art_generation++;options_generation++;}
}
void dw3_pc_title_overlay_set_footer(int show) {
    show=show?1:0;
    if(show!=footer_shown){footer_shown=show;art_generation++;}
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
static void options_screen(void) {
    static const Word rows[DW3_TITLE_OPTION_ROWS]={WORD(opt_row0),WORD(opt_row1),WORD(opt_row2)};
    static const Word values[]={WORD(opt_off),WORD(opt_on),WORD(opt_2x),WORD(opt_3x),WORD(opt_4x)};
#if defined(__SWITCH__)
    static const Word desc[DW3_TITLE_OPTION_ROWS]={WORD(opt_desc0),WORD(opt_desc1_nx),WORD(opt_desc2_nx)};
#else
    static const Word desc[DW3_TITLE_OPTION_ROWS]={WORD(opt_desc0),WORD(opt_desc1_pc),WORD(opt_desc2_pc)};
#endif
    static const Word titles[]={WORD(opt_title_en),WORD(opt_title_de),WORD(opt_title_es),WORD(opt_title_it)};
    const Word change=WORD(opt_change),back=WORD(opt_back);
    const Word *title=&titles[options_lang==DW3_TITLE_LANG_DE?1:options_lang==DW3_TITLE_LANG_ES?2:
                              options_lang==DW3_TITLE_LANG_IT?3:0];
    const int x0=84,y0=108,w=792,h=528,x1=x0+w,y1=y0+h;
    const int full=options_layer==OPTIONS_LAYER_FULL;
    const int draw_static=full||options_layer==OPTIONS_LAYER_STATIC;

    /* The title backdrop stays, dimmed; the logo and the menu are gone. */
    if(draw_static) {
    box(0,0,960,720,0,0,16,150);
    /* Window: white bezel shaded on its lower right, dark rim, blue-violet
     * inner line, navy body (the memory card window's colors). */
    box(x0+6,y0+8,w,h,0,0,8,150);
    box(x0,y0,w,h,255,255,255,255);
    box(x0+w-3,y0+3,3,h-3,181,181,181,255);box(x0+3,y0+h-3,w-3,3,181,181,181,255);
    box(x0+9,y0+9,w-18,h-18,49,49,49,255);
    box(x0+15,y0+15,w-30,h-30,58,58,148,255);
    box(x0+18,y0+18,w-36,h-36,0,0,49,255);
    /* Stepped title rule, the window title right of the step. */
    {
        const int a=y0+36,xs=x0+500;
        box(x0+30,a,xs-x0-30,3,0,66,156,255);box(x0+30,a+6,xs-x0-24,3,0,66,156,255);
        segment(xs,a+1.5f,xs+30,a+31.5f,1.6f,0,66,156);
        segment(xs+6,a+7.5f,xs+30,a+31.5f,1.6f,0,66,156);
        box(xs+30,a+30,x1-30-xs-30,3,0,66,156,255);
        word(title,xs+48,a+2-title->top,0,255,255,255);
    }
    }
    /* Choices: label left, value right; the focused one sits in the game's
     * choice box with the pointer. */
    for(int i=0;i<DW3_TITLE_OPTION_ROWS;i++) {
        const int ry=y0+112+i*68,focus=i==option_cursor;
        int v=option_values[i];
        if(focus&&(full||options_layer==OPTIONS_LAYER_FOCUS)){
            box(x0+48,ry-8,w-96,52,16,74,156,255);
            box(x0+51,ry-5,w-102,46,0,8,74,255);
            pointer(x0+66,ry+5);
        }
        if(draw_static||(options_layer==OPTIONS_LAYER_FOCUS&&
                       i==options_layer_row))
            word(&rows[i],x0+106,ry,0,255,255,255);
        if(v<0)v=0;
        if(v>4)v=4;
        if(i!=1&&v>1)v=1;
        if(full||(options_layer==OPTIONS_LAYER_VALUE&&i==options_layer_row))
            word(&values[v],x1-96,ry,1,v?255:150,v?255:158,v?255:196);
    }
    /* Message line for the focused choice, then the button hints. */
    if(draw_static)box(x0+30,y1-152,w-60,3,16,156,247,255);
    if((full||options_layer==OPTIONS_LAYER_DESCRIPTION)&&
       option_cursor>=0&&option_cursor<DW3_TITLE_OPTION_ROWS)
        word(&desc[option_cursor],x0+48,y1-128,0,255,255,255);
    if(draw_static) {
        const int hy=y1-74,bx=x1-48-back.w,cx=bx-36-change.w-60;
        button(cx,hy+12,0);word(&change,cx+22,hy,0,255,255,255);
        button(bx-22,hy+12,1);word(&back,bx,hy,0,255,255,255);
        box(x0+30,y1-36,w-60,3,0,66,156,255);
    }
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
    for(int row=0;row<3;row++) {
        int x=230,y=400+row*60,w=500,h=54,focus=row==selected;
        /* Soft optical orange halo only on the focused hardware silhouette. */
        if(focus)for(int spread=8;spread>=1;spread--)
            plaque(x-spread,y-spread,w+spread*2,h+spread*2,10+spread,255,103,17,255,103,17,5);
        plaque(x+1,y+4,w,h,10,0,3,10,0,3,10,190);
        plaque(x,y,w,h,10,focus?255:99,focus?180:149,focus?60:196,focus?207:17,focus?100:40,focus?26:77,255);
        plaque(x+1,y+1,w-2,h-2,9,99,149,196,17,40,77,255);
        plaque(x+2,y+2,w-4,h-4,9,25,57,98,49,90,141,255);
        plaque(x+5,y+5,w-10,h-10,7,5,16,39,13,29,57,255);
        /* Reflective top and lower lips. */
        box(x+36,y+2,w-72,1,143,198,232,220);
        box(x+36,y+h-3,w-72,1,78,120,173,255);
        int rr=focus?255:138,gg=focus?172:98,bb=focus?42:43;
        box(x+37,y+8,w-74,2,rr,gg,bb,255);
        box(x+37,y+h-10,w-74,2,rr,gg,bb,255);
        if(focus){box(x+39,y+7,w-78,1,255,236,167,235);box(x+39,y+h-8,w-78,1,255,221,136,210);}
        /* Cast-metal lateral clamps, bright chamfer, black inset channels. */
        for(int side=0;side<2;side++) {
            int cx=side?x+w-35:x+1;
            plaque(cx,y+1,34,h-2,7,100,160,206,17,47,95,255);
            plaque(cx+3,y+4,28,h-8,5,32,73,124,12,30,63,255);
            box(cx+5,y+7,2,h-14,106,166,209,215);
            box(cx+25,y+7,3,h-14,3,10,28,255);
            for(int l=0;l<3;l++)lamp(cx+15,y+12+l*15,focus);
            int railx=side?cx-5:cx+36;
            for(int l=0;l<3;l++)box(railx,y+18+l*6,2,3,rr,gg,bb,255);
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
        text_mask(480,y+(h-heights[row])/2-1,labels[row],widths[row],heights[row],focus);
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
