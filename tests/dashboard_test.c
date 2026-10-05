#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dashboard.h"
static uint32_t pixels[642*480+1],snapshot[642*480+1];
static uint16_t short_pixels[642*480+1];
int main(void){
 for(unsigned i=0;i<sizeof pixels/sizeof pixels[0];i++)pixels[i]=0xdeadbeef;
 uint8_t ip[]={192,168,50,148};holly_dashboard_status(1,2,ip,1,1);
 assert(!holly_dashboard_render(pixels,640,480,642,642*480,HOLLY_IDLE,0));
 assert(pixels[641]==0xdeadbeef&&pixels[642*480]==0xdeadbeef);
 for(unsigned y=0;y<480;y++)assert(pixels[y*642+640]==0xdeadbeef&&pixels[y*642+641]==0xdeadbeef);
 memcpy(snapshot,pixels,sizeof pixels);
 holly_dashboard_status(-1,0,0,0,0);
 assert(!holly_dashboard_render(pixels,640,480,642,642*480,HOLLY_THINKING,0));
 assert(!memcmp(snapshot,pixels,sizeof pixels)); /* no status panels/text */
 for(unsigned frame=1;frame<7;frame++){
  assert(!holly_dashboard_render(pixels,640,480,642,642*480,HOLLY_SPEAKING,frame));
  assert(memcmp(snapshot,pixels,sizeof pixels));
 }
 memcpy(snapshot,pixels,sizeof pixels);
 assert(holly_dashboard_render(pixels,640,480,642,642*480-1,HOLLY_IDLE,0)==-1);
 assert(holly_dashboard_render(pixels,640,480,642,642*480,HOLLY_IDLE,256)==-1);
 assert(!memcmp(snapshot,pixels,sizeof pixels));
 for(unsigned i=0;i<642*480+1;i++)short_pixels[i]=0xbeef;
 assert(!holly_dashboard_render16(short_pixels,640,480,642,642*480,HOLLY_SPEAKING,6,1));
 for(unsigned y=0;y<480;y++)for(unsigned x=0;x<640;x++){
  uint32_t v=pixels[y*642+x];
  assert(short_pixels[y*642+x]==(((v>>19)<<11)|(((v>>10)&63)<<5)|((v>>3)&31)));
 }
 for(unsigned y=0;y<480;y++)assert(short_pixels[y*642+640]==0xbeef&&short_pixels[y*642+641]==0xbeef);
 assert(short_pixels[642*480]==0xbeef);
 unsigned original=short_pixels[240*642+320];
 assert(!holly_dashboard_render16(short_pixels,640,480,642,642*480,HOLLY_SPEAKING,6,0));
 assert(short_pixels[240*642+320]==((original&0x7e0)|((original&0xf800)>>11)|((original&31)<<11)));
 assert(!holly_dashboard_render(pixels,320,240,320,320*240,HOLLY_IDLE,0));
 assert(!holly_dashboard_render(pixels,64,64,64,64*64,HOLLY_IDLE,0));
 memcpy(snapshot,pixels,sizeof pixels);holly_dashboard_avatar(1);
 assert(!holly_dashboard_render(pixels,64,64,64,64*64,HOLLY_IDLE,0));assert(memcmp(snapshot,pixels,sizeof pixels));
 for(unsigned frame=0;frame<7;frame++)assert(!holly_dashboard_render16(short_pixels,640,480,642,642*480,HOLLY_SPEAKING,frame,1));
 holly_dashboard_avatar(2);
 assert(!holly_dashboard_render(pixels,640,480,642,642*480,HOLLY_IDLE,0));memcpy(snapshot,pixels,sizeof pixels);
 for(unsigned frame=1;frame<7;frame++){assert(!holly_dashboard_render(pixels,640,480,642,642*480,HOLLY_SPEAKING,frame));assert(memcmp(snapshot,pixels,sizeof pixels));}
 holly_dashboard_avatar(0);
 puts("Face-only HDMI: seven portraits, status independence, RGB565/RGB888 agreement, channel order and stride/buffer guards passed");
}
