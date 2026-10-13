#include "title_logo_resample.h"
#include <math.h>
static const uint8_t *at(const uint8_t *p,int w,int h,int pitch,int x,int y) {
    if(x<0)x=0;
    if(x>=w)x=w-1;
    if(y<0)y=0;
    if(y>=h)y=h-1;
    return p+y*pitch+x*4;
}
/* Mitchell-Netravali B=C=1/3: conservative sharpness with limited ringing. */
static double weight(double x,int filter) {
    x=fabs(x);
    if(filter==DW3_PC_LOGO_LANCZOS2) {
        if(x<1e-8)return 1;
        if(x>=2)return 0;
        const double pi=3.14159265358979323846;
        return sin(pi*x)*sin(pi*x/2)/(pi*pi*x*x/2);
    }
    if(x<1.0)return (7.0*x*x*x-12.0*x*x+16.0/3.0)/6.0;
    if(x<2.0)return (-7.0/3.0*x*x*x+12.0*x*x-20.0*x+32.0/3.0)/6.0;
    return 0;
}
static double clamp(double v,double lo,double hi) {return v<lo?lo:(v>hi?hi:v);}
int dw3_pc_title_logo_resample(const uint8_t *src,int sw,int sh,int spitch,
    uint8_t *dst,int dw,int dh,int dpitch,int filter) {
    if(!src||!dst||sw<=0||sh<=0||dw<=0||dh<=0||sw>4096||sh>4096||dw>4096||dh>4096||spitch<sw*4||dpitch<dw*4)return -1;
    for(int y=0;y<dh;y++)for(int x=0;x<dw;x++) {
        double sx=(x+0.5)*sw/dw-0.5,sy=(y+0.5)*sh/dh-0.5;
        int ix=(int)floor(sx),iy=(int)floor(sy);
        double sum[4]={0,0,0,0},total=0;
        double lo[3]={255,255,255},hi[3]={0,0,0};
        int visible=0;
        for(int j=-1;j<=2;j++)for(int i=-1;i<=2;i++) {
            double w=weight(sx-(ix+i),filter)*weight(sy-(iy+j),filter);
            const uint8_t *p=at(src,sw,sh,spitch,ix+i,iy+j);
            double a=p[3]/255.0;
            if(p[3]) {
                visible=1;
                for(int c=0;c<3;c++) {
                    if(p[c]<lo[c])lo[c]=p[c];
                    if(p[c]>hi[c])hi[c]=p[c];
                }
            }
            for(int c=0;c<3;c++)sum[c]+=p[c]*a*w;
            sum[3]+=p[3]*w;total+=w;
        }
        uint8_t *p=dst+y*dpitch+x*4;
        double a=clamp(sum[3]/total,0,255);
        p[3]=(uint8_t)(a+0.5);
        for(int c=0;c<3;c++)p[c]=a>0.5&&visible?(uint8_t)(clamp(sum[c]/total*255/a,lo[c],hi[c])+0.5):0;
    }
    return 0;
}
