#include "splash.h"
/* A compact original wordmark: a rust-coloured Jovian disc and industrial type. */
static const uint8_t glyphs[][8]={
 {'J',0x07,0x02,0x02,0x02,0x12,0x12,0x0c},
 {'M',0x11,0x1b,0x15,0x15,0x11,0x11,0x11},
 {'C',0x0e,0x11,0x10,0x10,0x10,0x11,0x0e},
 {'U',0x11,0x11,0x11,0x11,0x11,0x11,0x0e},
 {'P',0x1e,0x11,0x11,0x1e,0x10,0x10,0x10},
 {'I',0x1f,0x04,0x04,0x04,0x04,0x04,0x1f},
 {'T',0x1f,0x04,0x04,0x04,0x04,0x04,0x04},
 {'E',0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f},
 {'R',0x1e,0x11,0x11,0x1e,0x14,0x12,0x11},
 {'N',0x11,0x19,0x15,0x15,0x13,0x11,0x11},
 {'G',0x0e,0x11,0x10,0x17,0x11,0x11,0x0e},
 {'O',0x0e,0x11,0x11,0x11,0x11,0x11,0x0e},
 {'A',0x0e,0x11,0x11,0x1f,0x11,0x11,0x11},
 {'S',0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e},
 {'Y',0x11,0x11,0x0a,0x04,0x04,0x04,0x04},
 {'B',0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e},
 {'L',0x10,0x10,0x10,0x10,0x10,0x10,0x1f},
 {'D',0x1e,0x11,0x11,0x11,0x11,0x11,0x1e},
 {'H',0x11,0x11,0x11,0x1f,0x11,0x11,0x11},
 {' ',0,0,0,0,0,0,0}
};
static void rect(uint32_t *p,unsigned w,unsigned h,unsigned stride,
                 int x,int y,int width,int height,uint32_t c){
 if(x<0||y<0||width<=0||height<=0||(unsigned)x>=w||(unsigned)y>=h)return;
 unsigned endx=(unsigned)x+(unsigned)width,endy=(unsigned)y+(unsigned)height;
 if(endx>w)endx=w;
 if(endy>h)endy=h;
 for(unsigned yy=(unsigned)y;yy<endy;yy++)for(unsigned xx=(unsigned)x;xx<endx;xx++)p[(size_t)yy*stride+xx]=c;
}
static void type(uint32_t *p,unsigned w,unsigned h,unsigned stride,
                 const char *text,int x,int y,unsigned scale,uint32_t color){
 while(*text){const uint8_t *g=0;
  for(unsigned i=0;i<sizeof glyphs/sizeof glyphs[0];i++){
   if(glyphs[i][0]==(uint8_t)*text){g=glyphs[i]+1;break;}
  }
  if(g)for(unsigned row=0;row<7;row++)for(unsigned col=0;col<5;col++)
   if(g[row]&(1u<<(4u-col)))rect(p,w,h,stride,x+(int)(col*scale),y+(int)(row*scale),(int)scale,(int)scale,color);
  x+=(int)(6u*scale);text++;
 }
}
int holly_splash_render(uint32_t *p,unsigned w,unsigned h,unsigned stride,size_t capacity){
 if(!p||w<320||h<240||w>4096||h>4096||stride<w||(size_t)stride*h>capacity)return -1;
 for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)p[(size_t)y*stride+x]=0x07121eu;
 /* Design in a 640x480 reference grid, scaled to the selected framebuffer. */
 for(unsigned y=0;y<h;y++){
  int fy=(int)((uint64_t)y*480u/h);
  for(unsigned x=0;x<w;x++){
   int fx=(int)((uint64_t)x*640u/w),dx=fx-143,dy=fy-209;
   uint32_t c=0;
   int d=dx*dx+dy*dy;
   if(d<=85*85){c=(fy%17<5)?0xdca66cu:(fy%37<11)?0x9b5038u:0xc6764eu;
    if(d>79*79)c=0xefcf9cu;
    if(fy>213&&fy<224)c=0xf0c697u;
    p[(size_t)y*stride+x]=c;
   }
   if(fy==88||fy==336){p[(size_t)y*stride+x]=0x9a563bu;}
  }
 }
 unsigned big=w/640u? (w*13u/640u):13u;if(big<5)big=5;
 /* At 640 px, JMC occupies 234 px and the full corporate name 468 px. */
 type(p,w,h,stride,"JMC",(int)(w*290u/640u),(int)(h*163u/480u),big,0xf4ddbau);
 unsigned small=w*3u/640u;if(small<2)small=2;
 type(p,w,h,stride,"JUPITER MINING CORPORATION",(int)(w*84u/640u),(int)(h*355u/480u),small,0xd6b897u);
 type(p,w,h,stride,"HOLLY SYSTEM BOOT",(int)(w*218u/640u),(int)(h*411u/480u),small>2?small-1:2,0x8baebbu);
 return 0;
}
