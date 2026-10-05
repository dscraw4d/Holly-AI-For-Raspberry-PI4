#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "video.h"
struct fake {
 unsigned calls, allocations, selected, unsupported, timeout_tag, bad_edid;
 uint32_t pixels[2][640*480];
};
/* Model a firmware selector that does NOT persist between transactions. */
static int exchange(uint32_t *w,size_t bytes,void *ctx){
 struct fake *f=ctx;f->calls++;f->selected=0;
 assert(!((uintptr_t)w&15u)&&w[1]==0&&w[0]==bytes);
 unsigned pos=2;
 while(w[pos]){
  uint32_t tag=w[pos],len=w[pos+1];
  assert(w[pos+2]==0 && pos+3+len/4<=bytes/4);
  if(tag==f->timeout_tag)return -1;
  if(f->unsupported&&(tag==0x40013u||tag==0x48013u)){pos+=3+len/4;continue;}
  w[pos+2]=0x80000000u|len;
  switch(tag){
  case 0x40013:w[pos+3]=2;break;
  case 0x48013:assert(w[pos+3]<2);f->selected=w[pos+3];w[pos+2]=0x80000000u;break;
  case 0x48003:case 0x48004:case 0x48005:case 0x48006:case 0x48007:break;
  case 0x40001:
   assert(bytes==256&&w[2]==0x48013u);f->allocations++;
   w[pos+3]=0xc1000000u+f->selected*0x200000u;w[pos+4]=640*480*2;break;
  case 0x40008:w[pos+3]=640*2;break;
  case 0x48009:assert(w[pos+3]==0&&w[pos+4]==0);break;
  case 0x40002:case 0x4800c:assert(w[pos+3]==0);break;
  case 0x30020:{
   assert(bytes==256&&w[2]==0x48013u&&len==136);
   w[pos+3]=0;w[pos+4]=0;
   uint8_t *e=(uint8_t *)(w+pos+5);
   memset(e,0,128);for(unsigned i=1;i<7;i++)e[i]=255;
   e[8]=0x12;e[9]=0x34;e[54]=1;e[56]=128;e[58]=0x70;e[59]=56;e[61]=0x40;
   unsigned sum=0;for(unsigned i=0;i<127;i++)sum+=e[i];e[127]=(uint8_t)(0-sum);if(f->bad_edid)e[127]^=1;
   break;}
  default:assert(0);
  }
  pos+=3+len/4;
 }
 w[1]=0x80000000u;return 0;
}
static int map(const struct holly_framebuffer *fb,uint32_t **p,size_t *bytes,void *ctx){
 struct fake *f=ctx;assert(fb->bus_address==0xc1000000u+f->selected*0x200000u);
 *p=f->pixels[f->selected];*bytes=sizeof(f->pixels[0]);return 0;
}
int main(void){
 static struct fake f;struct holly_video v={0};struct holly_display_ops ops={exchange,map,&f};
 assert(holly_video_start(&v,&ops)==0 && v.count==2 && !v.poisoned);
 assert(f.allocations==1&&v.displays[1].status==HOLLY_DISPLAY_OFF);
 assert(v.select_result==0&&v.offset_result==0&&v.unblank_result==0);
 assert(holly_video_pattern(&v,2)==0 && f.pixels[0][0]==0xffffffffu);
 assert(holly_video_select(&v,1)==0 && v.active==1 && f.allocations==2);
 assert(f.pixels[1][0]==0xffffffffu);
 assert(holly_video_monitor(&v)==0&&v.edid_valid&&v.edid_width==1920&&v.edid_height==1080);
 assert(v.edid_manufacturer==0x1234&&f.selected==1);
 f.bad_edid=1;assert(holly_video_monitor(&v)==-5&&!v.edid_valid);f.bad_edid=0;
 assert(holly_video_pattern(&v,3)==0 && f.pixels[1][100]==0xf800f800u);
 assert(f.pixels[0][100]==0xffffffffu);
 assert(holly_video_select(&v,0)==0 && f.allocations==2);
 assert(holly_video_splash(&v)==0 && v.active==0);
 assert(v.splash_result[0]==0 && v.splash_result[1]==-99);
 assert(((uint16_t *)f.pixels[0])[0]==0x0083u);
 assert(((uint16_t *)f.pixels[0])[209*640+143]!=((uint16_t *)f.pixels[0])[0]);
 /* Secondary remains independently selected and rendered by display 1. */
 assert(holly_video_select(&v,2)<0 && holly_video_pattern(&v,6)<0);
 assert(holly_video_start(&v,&ops)<0);
 struct holly_video legacy={0};f.unsupported=1;
 assert(holly_video_start(&legacy,&ops)==0 && legacy.count_result==-3);
 unsigned calls=f.calls;assert(holly_video_select(&legacy,1)==-6 && f.calls==calls);
 assert(holly_video_splash(&legacy)==0 && legacy.active==0);
 f.unsupported=0;f.timeout_tag=0x40002u;
 assert(holly_video_unblank(&v)==-2 && v.poisoned);
 uint32_t retained[16];memcpy(retained,v.target_request,sizeof retained);calls=f.calls;
 assert(holly_video_select(&v,1)<0 && holly_video_unblank(&v)<0);
 assert(f.calls==calls&&!memcmp(retained,v.target_request,sizeof retained));
 struct holly_video dead={0};f.timeout_tag=0x40013u;
 assert(holly_video_start(&dead,&ops)<0 && dead.poisoned);
 puts("Video atomic targeting, EDID, switching, legacy fallback and timeout retention passed");
}
