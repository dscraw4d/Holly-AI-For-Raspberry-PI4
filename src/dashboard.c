/* HDMI portrait renderer. The browser UI has its own independent artwork. */
#include "dashboard.h"
#include "hdmi_face_art.h"
#include "hilly_face_art.h"
#include "queeg_face_art.h"
static unsigned revision,avatar;
void holly_dashboard_avatar(unsigned choice){if(choice>2)choice=0;if(avatar!=choice){avatar=choice;revision++;}}
void holly_dashboard_status(int network,unsigned lease,const uint8_t ip[4],unsigned vault,unsigned documents){
 (void)network;(void)lease;(void)ip;(void)vault;(void)documents;
}
unsigned holly_dashboard_revision(void){return revision;}
unsigned holly_dashboard_frame(enum holly_expression expression,unsigned level){
 return expression==HOLLY_SPEAKING&&level<HOLLY_PORTRAIT_FRAMES?level:0;
}
static int render(void *pixels,unsigned w,unsigned h,unsigned stride,size_t capacity,
 enum holly_expression expression,unsigned level,unsigned depth,unsigned order){
 if(!pixels||w<64||h<64||w>4096||h>4096||stride<w||(size_t)stride*h>capacity||expression>HOLLY_BLINKING||level>255||order>1)return -1;
 /* Centre-cover preserves head proportions and crops only background on 4:3.
  * No console, dashboard lettering, status lamps or transcript is drawn. */
 unsigned art_width=avatar==2?QUEEG_PORTRAIT_WIDTH:avatar==1?HILLY_PORTRAIT_WIDTH:HOLLY_PORTRAIT_WIDTH,art_height=avatar==2?QUEEG_PORTRAIT_HEIGHT:avatar==1?HILLY_PORTRAIT_HEIGHT:HOLLY_PORTRAIT_HEIGHT;
 unsigned scaled_w=w,scaled_h=w*art_height/art_width;
 if(scaled_h<h){scaled_h=h;scaled_w=(h*art_width+art_height-1)/art_height;}
 if(!scaled_h)return -1;
 unsigned crop_x=(scaled_w-w)/2,crop_y=(scaled_h-h)/2;
 unsigned xmap[4096];
 for(unsigned x=0;x<w;x++)xmap[x]=(x+crop_x)*art_width/scaled_w;
 const uint16_t *runs=avatar==2?queeg_runs:avatar==1?hilly_runs:holly_runs;
 const uint32_t *rows=avatar==2?queeg_rows:avatar==1?hilly_rows:holly_rows;
 unsigned frame=holly_dashboard_frame(expression,level),previous=~0u;uint16_t row[512];
 for(unsigned y=0;y<h;y++){
  unsigned sy=(y+crop_y)*art_height/scaled_h;
  if(sy!=previous){unsigned at=rows[frame*art_height+sy],x=0;while(x<art_width){unsigned count=runs[at++];uint16_t value=runs[at++];if(!count||count>art_width-x)return -1;while(count--)row[x++]=value;}previous=sy;}
  for(unsigned x=0;x<w;x++){
   uint16_t v=row[xmap[x]];
   if(!order)v=(uint16_t)((v&0x07e0u)|((v&0xf800u)>>11)|((v&0x001fu)<<11));
   size_t at=(size_t)y*stride+x;
   if(depth==16)((uint16_t *)pixels)[at]=v;
   else {unsigned r=(v>>11)&31u,g=(v>>5)&63u,b=v&31u;
    ((uint32_t *)pixels)[at]=((r*255u/31u)<<16)|((g*255u/63u)<<8)|(b*255u/31u);
   }
  }
 }
 return 0;
}
int holly_dashboard_render(uint32_t *p,unsigned w,unsigned h,unsigned stride,size_t capacity,
 enum holly_expression expression,unsigned level){return render(p,w,h,stride,capacity,expression,level,32,1);}
int holly_dashboard_render16(uint16_t *p,unsigned w,unsigned h,unsigned stride,size_t capacity,
 enum holly_expression expression,unsigned level,unsigned order){return render(p,w,h,stride,capacity,expression,level,16,order);}
