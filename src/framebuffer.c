#include "framebuffer.h"
#include "face.h"
#include "dashboard.h"
#include "splash.h"
#define PI4_LEGACY_LIMIT 0x40000000u
int holly_pi4_ram_to_bus(uintptr_t physical,size_t bytes,uint32_t *bus){
    if(!bus||!physical||!bytes||physical>=PI4_LEGACY_LIMIT||
       bytes>PI4_LEGACY_LIMIT-physical||(physical&15u))return -1;
    *bus=0xC0000000u|(uint32_t)physical;
    return 0;
}
int holly_pi4_fb_bus_to_arm(uint32_t bus,size_t bytes,uintptr_t *physical){
    uint32_t alias=bus&0xC0000000u;
    uint32_t offset=bus&0x3FFFFFFFu;
    if(!physical||!offset||!bus||!bytes||(alias!=0xC0000000u && alias!=0)||
       (bus&15u)||bytes>PI4_LEGACY_LIMIT-offset)return -1;
    *physical=(uintptr_t)offset;
    return 0;
}
static int tag(const uint32_t *w,unsigned at,uint32_t id,unsigned length){
    return w[at]==id && w[at+1]==length && w[at+2]==(0x80000000u|length);
}
int holly_fb_request_depth(uint32_t w[HOLLY_FB_WORDS],unsigned width,unsigned height,unsigned depth){
    if((depth!=16&&depth!=32)||!w||((uintptr_t)w&15u)||width<64||height<64||width>4096||height>4096)return -1;
    for(unsigned i=0;i<HOLLY_FB_WORDS;i++)w[i]=0;
    w[0]=HOLLY_FB_WORDS*4u;
    w[2]=0x00048003u;w[3]=8;w[4]=0;w[5]=width;w[6]=height;
    w[7]=0x00048004u;w[8]=8;w[9]=0;w[10]=width;w[11]=height;
    w[12]=0x00048005u;w[13]=4;w[14]=0;w[15]=depth;
    w[16]=0x00048006u;w[17]=4;w[18]=0;w[19]=1; /* RGB */
    w[20]=0x00048007u;w[21]=4;w[22]=0;w[23]=2; /* ignore alpha where supported; honor returned mode */
    w[24]=0x00040001u;w[25]=8;w[26]=0;w[27]=16;
    w[29]=0x00040008u;w[30]=4;w[31]=0;
    return 0;
}
int holly_fb_request(uint32_t w[HOLLY_FB_WORDS],unsigned width,unsigned height){
 return holly_fb_request_depth(w,width,height,32);
}
int holly_fb_response(const uint32_t w[HOLLY_FB_WORDS],struct holly_framebuffer *fb){
    if(!w||!fb||w[0]!=HOLLY_FB_WORDS*4u||w[1]!=0x80000000u||w[33]!=0||
       !tag(w,2,0x00048003u,8)||!tag(w,7,0x00048004u,8)||
       !tag(w,12,0x00048005u,4)||!tag(w,16,0x00048006u,4)||
       !tag(w,20,0x00048007u,4)||!tag(w,24,0x00040001u,8)||
       !tag(w,29,0x00040008u,4))return -1;
    unsigned width=w[5],height=w[6],pitch=w[32];
    if(width<64||height<64||width>4096||height>4096||w[10]!=width||w[11]!=height||
       (w[15]!=16&&w[15]!=32)||w[19]>1||w[23]>2||!w[27]||w[28]==0||
       (w[27]&15u)||(pitch&3u)||pitch<width*(w[15]/8u)||
       (uint64_t)pitch*height>w[28])return -2;
    fb->bus_address=w[27];fb->byte_size=w[28];
    fb->width=width;fb->height=height;fb->pitch_bytes=pitch;
    fb->pixel_order=w[19];fb->alpha_mode=w[23];fb->depth=w[15];
    return 0;
}
static int writable(const struct holly_framebuffer *fb,uint32_t *pixels,size_t available){
    return fb&&pixels&&!(fb->pitch_bytes&3u)&&fb->width>=64&&fb->width<=4096&&
        fb->height>=64&&fb->height<=4096&&(fb->depth==16||fb->depth==32)&&fb->pitch_bytes>=fb->width*(fb->depth/8u)&&
        fb->pixel_order<=1&&fb->alpha_mode<=2&&available>=fb->byte_size&&
        (uint64_t)fb->pitch_bytes*fb->height<=fb->byte_size;
}
static uint32_t pixel(const struct holly_framebuffer *fb,uint32_t rgb){
    uint32_t value=rgb&0xffffffu;
    if(fb->pixel_order==1)value=((value&255u)<<16)|(value&0xff00u)|((value>>16)&255u);
    return value|(fb->alpha_mode==0?0:0xff000000u);
}
static uint32_t scratch[640u*480u];
static uint16_t pixel16(const struct holly_framebuffer *fb,uint32_t rgb){
 unsigned r=(rgb>>16)&255u,g=(rgb>>8)&255u,b=rgb&255u;
 if(!fb->pixel_order){unsigned t=r;r=b;b=t;}
 return (uint16_t)(((r>>3)<<11)|((g>>2)<<5)|(b>>3));
}
static int draw16(const struct holly_framebuffer *fb,uint32_t *pixels,size_t available,
 enum holly_expression expression,unsigned level,int splash){
 if(fb->width>640||fb->height>480||available<fb->byte_size)return -1;
 if(!splash)return holly_dashboard_render16((uint16_t *)pixels,fb->width,fb->height,fb->pitch_bytes/2u,available/2u,expression,level,fb->pixel_order);
 int result=splash?holly_splash_render(scratch,fb->width,fb->height,fb->width,640u*480u):
  holly_dashboard_render(scratch,fb->width,fb->height,fb->width,640u*480u,expression,level);
 if(result)return -1;
 uint16_t *out=(uint16_t *)pixels;unsigned stride=fb->pitch_bytes/2u;
 for(unsigned y=0;y<fb->height;y++)for(unsigned x=0;x<fb->width;x++)
  out[(size_t)y*stride+x]=pixel16(fb,scratch[(size_t)y*fb->width+x]);
 return 0;
}
static void publish(void){
#if defined(__aarch64__) && !defined(HOST_TEST)
    __asm__ volatile("dsb sy" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}
int holly_fb_draw(const struct holly_framebuffer *fb,uint32_t *pixels,
                  size_t available,enum holly_expression expression,unsigned voice_level){
    if(!writable(fb,pixels,available))return -1;
    if(fb->depth==16){if(draw16(fb,pixels,available,expression,voice_level,0))return -1;publish();return 0;}
    if(holly_dashboard_render(pixels,fb->width,fb->height,fb->pitch_bytes/4u,
                         available/4u,expression,voice_level))return -1;
    unsigned stride=fb->pitch_bytes/4u;
    for(unsigned y=0;y<fb->height;y++)for(unsigned x=0;x<fb->width;x++){
        size_t at=(size_t)y*stride+x;pixels[at]=pixel(fb,pixels[at]);
    }
    publish();return 0;
}
int holly_fb_pattern(const struct holly_framebuffer *fb,uint32_t *pixels,size_t available,unsigned mode){
    static const uint32_t bars[]={0xffffff,0xffff00,0x00ffff,0x00ff00,0xff00ff,0xff0000,0x0000ff,0};
    static const uint32_t solid[]={0,0,0xffffff,0xff0000,0x00ff00,0x0000ff};
    if(!writable(fb,pixels,available)||mode<1||mode>5)return -1;
    unsigned stride=fb->pitch_bytes/4u;
    for(unsigned y=0;y<fb->height;y++)for(unsigned x=0;x<fb->width;x++){
        uint32_t rgb=mode==1?bars[(uint64_t)x*8/fb->width]:solid[mode];
        if(mode==1&&(y<4||y+4>=fb->height))rgb=0xffffff;
        if(fb->depth==16)((uint16_t *)pixels)[(size_t)y*(fb->pitch_bytes/2u)+x]=pixel16(fb,rgb);
        else pixels[(size_t)y*stride+x]=pixel(fb,rgb);
    }
    publish();return 0;
}
int holly_fb_splash(const struct holly_framebuffer *fb,uint32_t *pixels,size_t available){
 if(!writable(fb,pixels,available))return -1;
 if(fb->depth==16){if(draw16(fb,pixels,available,HOLLY_IDLE,0,1))return -1;publish();return 0;}
 if(holly_splash_render(pixels,fb->width,fb->height,fb->pitch_bytes/4u,available/4u))return -1;
 unsigned stride=fb->pitch_bytes/4u;
 for(unsigned y=0;y<fb->height;y++)for(unsigned x=0;x<fb->width;x++){
  size_t at=(size_t)y*stride+x;pixels[at]=pixel(fb,pixels[at]);
 }
 publish();return 0;
}
int holly_mailbox_property(struct holly_mailbox *d,uint32_t address){
    if(!d||!d->read||!d->write||!address||(address&15u))return -1;
    unsigned polls=0;
    while(d->read(0x38u,d->context)&0x80000000u)if(++polls>=1000000u)return -2;
#if defined(__aarch64__) && !defined(HOST_TEST)
    __asm__ volatile("dmb sy" ::: "memory");
#endif
    d->write(0x20u,address|8u,d->context);
    for(polls=0;polls<1000000u;polls++){
        if(d->read(0x18u,d->context)&0x40000000u)continue;
        uint32_t message=d->read(0,d->context);
        if(message==(address|8u)){
#if defined(__aarch64__) && !defined(HOST_TEST)
            __asm__ volatile("dmb sy" ::: "memory");
#endif
            return 0;
        }
    }
    return -2;
}
#ifndef HOST_TEST
static uint32_t pi_read(uint32_t offset,void *unused){
    (void)unused;return *(volatile uint32_t *)(uintptr_t)(0xFE00B880UL+offset);
}
static void pi_write(uint32_t offset,uint32_t value,void *unused){
    (void)unused;*(volatile uint32_t *)(uintptr_t)(0xFE00B880UL+offset)=value;
}
struct holly_mailbox holly_pi4_mailbox(void){struct holly_mailbox d={pi_read,pi_write,0};return d;}
#else
struct holly_mailbox holly_pi4_mailbox(void){struct holly_mailbox d={0,0,0};return d;}
#endif
