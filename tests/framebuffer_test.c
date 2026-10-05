#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "framebuffer.h"
static uint32_t sent;
static uint32_t read_reg(uint32_t offset,void *ctx){
 (void)ctx;
 if(offset==0x18||offset==0x38)return 0;
 assert(offset==0);return sent;
}
static void write_reg(uint32_t offset,uint32_t v,void *ctx){
 (void)ctx;assert(offset==0x20);sent=v;
}
int main(void){
 uint32_t bus;uintptr_t physical;
 assert(holly_pi4_ram_to_bus(0x80000,144,&bus)==0 && bus==0xC0080000);
 assert(holly_pi4_ram_to_bus(0x3FFFFF00,256,&bus)==0 && bus==0xFFFFFF00);
 assert(holly_pi4_ram_to_bus(0x3FFFFF00,257,&bus)==-1);
 assert(holly_pi4_ram_to_bus(0x40000000,144,&bus)==-1);
 assert(holly_pi4_ram_to_bus(0x80001,144,&bus)==-1);
 assert(holly_pi4_fb_bus_to_arm(0xC1000000,16384,&physical)==0 && physical==0x1000000);
 assert(holly_pi4_fb_bus_to_arm(0x01000000,16384,&physical)==0 && physical==0x1000000);
 assert(holly_pi4_fb_bus_to_arm(0x81000000,16384,&physical)==-1);
 assert(holly_pi4_fb_bus_to_arm(0xC0000000,16384,&physical)==-1);
 assert(holly_pi4_fb_bus_to_arm(0xFFFFF000,0x1001,&physical)==-1);
 assert(holly_pi4_fb_bus_to_arm(0xC1000001,16384,&physical)==-1);
 _Alignas(16) uint32_t w[HOLLY_FB_WORDS];
 assert(holly_fb_request(w,640,480)==0);
 assert(w[0]==144 && w[1]==0 && w[2]==0x48003 && w[5]==640 && w[6]==480);
 assert(w[23]==2); /* Request alpha ignored, but use returned convention. */
 assert(w[24]==0x40001 && w[27]==16 && w[29]==0x40008 && w[33]==0);
 for(unsigned i=0;i<7;i++){const unsigned at[]={4,9,14,18,22,26,31};assert(w[at[i]]==0);}
 assert(holly_fb_request(w,32,480)==-1);
 assert(holly_fb_request(w,64,64)==0);
 w[1]=0x80000000;
 const unsigned tags[]={4,9,14,18,22,26,31};
 const unsigned sizes[]={8,8,4,4,4,8,4};
 for(unsigned i=0;i<7;i++)w[tags[i]]=0x80000000u|sizes[i];
 w[27]=0xc1000000;w[28]=64u*64u*4u;w[32]=64u*4u;
 struct holly_framebuffer fb;
 assert(holly_fb_response(w,&fb)==0);
 assert(fb.width==64 && fb.height==64 && fb.pitch_bytes==256);
 static uint32_t pixels[64*64+1];pixels[64*64]=0xdeadbeef;
 assert(holly_fb_draw(&fb,pixels,sizeof(pixels),HOLLY_SPEAKING,150)==0);
 assert(pixels[32*64+32]!=0 && pixels[64*64]==0xdeadbeef);
 assert(holly_fb_draw(&fb,pixels,100,HOLLY_IDLE,0)==-1);
 w[19]=0;assert(holly_fb_response(w,&fb)==0);
 w[19]=2;assert(holly_fb_response(w,&fb)==-2);w[19]=1;
 w[32]=100;assert(holly_fb_response(w,&fb)==-2);w[32]=256;
 w[26]=4;assert(holly_fb_response(w,&fb)==-1);w[26]=0x80000008;
 w[25]=4;assert(holly_fb_response(w,&fb)==-1);w[25]=8;
 static uint32_t padded[66*64+1];
 for(unsigned i=0;i<66*64+1;i++)padded[i]=0xdeadbeef;
 fb.width=64;fb.height=64;fb.pitch_bytes=264;fb.byte_size=66*64*4;
 for(unsigned order=0;order<2;order++)for(unsigned alpha=0;alpha<3;alpha++){
  fb.pixel_order=order;fb.alpha_mode=alpha;
  assert(holly_fb_pattern(&fb,padded,sizeof(padded),3)==0);
  uint32_t red=(order?0x000000ffu:0x00ff0000u)|(alpha?0xff000000u:0);
  for(unsigned y=0;y<64;y++){
   assert(padded[y*66]==red && padded[y*66+63]==red);
   assert(padded[y*66+64]==0xdeadbeef && padded[y*66+65]==0xdeadbeef);
  }
 }
 assert(padded[66*64]==0xdeadbeef);
 assert(holly_fb_pattern(&fb,padded,100,1)==-1);
 assert(holly_fb_pattern(&fb,padded,sizeof(padded),6)==-1);
 /* RGB565 recovery path: both channel orders, every alpha response, padding
    and exact pattern pixels. Sixteen-bit scanout has no per-pixel alpha. */
 assert(holly_fb_request_depth(w,64,64,16)==0&&w[15]==16);
 assert(holly_fb_request_depth(w,64,64,24)==-1);
 assert(holly_fb_request_depth(w,64,64,16)==0);
 w[1]=0x80000000u;
 for(unsigned i=0;i<7;i++)w[tags[i]]=0x80000000u|sizes[i];
 w[27]=0xc1000000;w[28]=66u*64u*2u;w[32]=66u*2u;
 assert(holly_fb_response(w,&fb)==0&&fb.depth==16&&fb.pitch_bytes==132);
 _Alignas(16) static uint16_t short_pixels[66u*64u+2u];
 for(unsigned i=0;i<66u*64u+2u;i++)short_pixels[i]=0xbeefu;
 for(unsigned order=0;order<2;order++)for(unsigned alpha=0;alpha<3;alpha++){
  fb.pixel_order=order;fb.alpha_mode=alpha;
  assert(holly_fb_pattern(&fb,(uint32_t *)short_pixels,sizeof short_pixels,3)==0);
  for(unsigned y=0;y<64;y++){
   assert(short_pixels[y*66]==(order?0xf800u:0x001fu));
   assert(short_pixels[y*66+63]==(order?0xf800u:0x001fu));
   assert(short_pixels[y*66+64]==0xbeefu&&short_pixels[y*66+65]==0xbeefu);
  }
  assert(holly_fb_pattern(&fb,(uint32_t *)short_pixels,sizeof short_pixels,2)==0);
  assert(short_pixels[0]==0xffffu&&short_pixels[32*66+32]==0xffffu);
  assert(holly_fb_draw(&fb,(uint32_t *)short_pixels,sizeof short_pixels,HOLLY_IDLE,0)==0);
 }
 assert(short_pixels[66*64]==0xbeefu&&short_pixels[66*64+1]==0xbeefu);
 assert(holly_fb_draw(&fb,(uint32_t *)short_pixels,fb.byte_size-1,HOLLY_IDLE,0)==-1);
 fb.width=641;fb.height=64;fb.pitch_bytes=1284;fb.byte_size=1284*64;
 /* Scratch bounds checked before writes; no large fixed-buffer overflow. */
 static uint32_t wider[1284*64/4];wider[0]=0xabcdef01;
 assert(holly_fb_draw(&fb,wider,sizeof wider,HOLLY_IDLE,0)==-1&&wider[0]==0xabcdef01);
 struct holly_mailbox d={read_reg,write_reg,0};
 assert(holly_mailbox_property(&d,0x1000)==0 && sent==0x1008);
 assert(holly_mailbox_property(&d,0x1001)==-1);
 puts("Framebuffer address, request, validation and mailbox tests passed");
}
