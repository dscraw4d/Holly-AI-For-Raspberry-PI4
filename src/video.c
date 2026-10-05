#include "video.h"
#define TAG_SELECT 0x00048013u
#define TAG_COUNT 0x00040013u
#define TAG_OFFSET 0x00048009u
#define TAG_BLANK 0x00040002u
#define TAG_LAYER 0x0004800cu
#define TAG_EDID 0x00030020u
/* Firmware display selection belongs in the same transaction as its target
 * operation. Never rely on the selector remaining global across messages.
 * Persistent staging survives mailbox timeouts; only the primary CPU uses it. */
static int targeted_exchange(uint32_t *request,size_t bytes,void *context){
 struct holly_video *v=context;
 if(!v||v->poisoned||!v->ops.exchange||bytes<12||bytes>sizeof(v->target_request)-16||bytes%4)return -1;
 uint32_t *w=v->target_request;unsigned words=(unsigned)(bytes/4);
 for(unsigned i=0;i<64;i++)w[i]=0;
 w[0]=sizeof(v->target_request);w[2]=TAG_SELECT;w[3]=4;w[5]=v->request_display;
 for(unsigned i=2;i<words;i++)w[i+4]=request[i];
 if(v->ops.exchange(w,sizeof(v->target_request),v->ops.context)){
  v->poisoned=1;return -1;
 }
 v->select_result=(w[0]==sizeof(v->target_request)&&w[1]==0x80000000u&&w[2]==TAG_SELECT&&w[3]==4&&(w[4]&0x80000000u))?0:-3;
 if(v->select_result&&v->request_display)return -1;
 request[1]=w[1];for(unsigned i=2;i<words;i++)request[i]=w[i+4];
 return 0;
}
static int targeted_map(const struct holly_framebuffer *fb,uint32_t **p,size_t *bytes,void *context){
 struct holly_video *v=context;return v->ops.map(fb,p,bytes,v->ops.context);
}
static int property(struct holly_video *v,uint32_t tag,uint32_t a,uint32_t b,unsigned words,unsigned reply){
    if(!v||!v->initialized||v->poisoned||!v->ops.exchange||words<1||words>2)return -1;
    uint32_t *w=v->control;
    for(unsigned i=0;i<16;i++)w[i]=0;
    w[0]=sizeof(v->control);w[2]=tag;w[3]=words*4;w[5]=a;if(words==2)w[6]=b;
    v->last_tag=tag;
    if((tag==TAG_COUNT||tag==TAG_SELECT?
        v->ops.exchange(w,sizeof(v->control),v->ops.context):
        targeted_exchange(w,sizeof(v->control),v))){
        v->poisoned=1;return -2; /* retain request; firmware may still own it */
    }
    v->last_header=w[1];v->last_tag_reply=w[4];
    if(w[0]!=sizeof(v->control)||w[1]!=0x80000000u||w[2]!=tag||w[3]!=words*4)return -4;
    if(!(w[4]&0x80000000u))return -3;
    if((w[4]&0x7fffffffu)<reply)return -4;
    return 0;
}
int holly_video_unblank(struct holly_video *v){
    if(!v||!v->initialized)return -1;
    v->request_display=v->active;
    v->unblank_result=property(v,TAG_BLANK,0,0,1,4);
    if(!v->unblank_result&&(v->control[5]&1u))v->unblank_result=-5;
    return v->unblank_result;
}
int holly_video_select(struct holly_video *v,unsigned index){
    if(!v||!v->initialized||v->poisoned||index>1)return -1;
    /* Do not assume secondary display support when enumeration is unavailable. */
    if(index && (v->count_result||index>=v->count))return -6;
    if(!v->count_result && index>=v->count)return -6;
    v->select_result=property(v,TAG_SELECT,index,0,1,0);
    /* Only primary/default display may use the legacy no-selection path. */
    if(v->select_result && !(index==0&&v->select_result==-3))return v->select_result;
    v->request_display=index;
    struct holly_display *d=&v->displays[index];
    const struct holly_display_ops target_ops={targeted_exchange,v->ops.map,v};
    /* Mapping belongs to the original transport context, not this wrapper. */
    struct holly_display_ops mapped_ops=target_ops;
    mapped_ops.map=targeted_map;
    if(holly_display_start(d,640,480,&mapped_ops)){
        if(d->status==HOLLY_DISPLAY_TRANSPORT_FAILED)v->poisoned=1;
        return -7;
    }
    v->offset_result=property(v,TAG_OFFSET,0,0,2,8);
    if(!v->offset_result&&(v->control[5]||v->control[6]))v->offset_result=-5;
    if(v->poisoned)return -2;
    v->active=index;
    v->layer_result=property(v,TAG_LAYER,0,0,1,4);
    if(v->poisoned)return -2;
    (void)holly_video_unblank(v);
    if(v->poisoned)return -2;
    return holly_video_pattern(v,v->mode);
}
int holly_video_start(struct holly_video *v,const struct holly_display_ops *ops){
    if(!v||!ops||!ops->exchange||!ops->map||v->initialized)return -1;
    v->ops=*ops;v->initialized=1;
    v->select_result=v->offset_result=v->unblank_result=v->layer_result=v->edid_result=-99;
    v->splash_result[0]=v->splash_result[1]=-99;
    v->count_result=property(v,TAG_COUNT,0,0,1,4);
    if(!v->count_result){v->count=v->control[5];if(!v->count||v->count>16)v->count_result=-5;}
    return holly_video_select(v,0);
}
int holly_video_pattern(struct holly_video *v,unsigned mode){
    if(!v||!v->initialized||mode>5)return -1;
    struct holly_display *d=&v->displays[v->active];
    if(d->status!=HOLLY_DISPLAY_READY)return -1;
    v->mode=mode;
    if(!mode)return holly_display_draw(d,HOLLY_IDLE,0);
    int result=holly_fb_pattern(&d->framebuffer,d->pixels,d->mapped_bytes,mode);
    if(!result)d->frames++;
    return result;
}
int holly_video_splash(struct holly_video *v){
 if(!v||!v->initialized||v->poisoned||v->displays[0].status!=HOLLY_DISPLAY_READY)return -1;
 /* Recovery baseline: secondary allocation is opt-in via display 1. */
 unsigned seen=1;
 for(unsigned i=0;i<seen;i++){
  if(i&&holly_video_select(v,i)){v->splash_result[i]=-2;continue;}
  struct holly_display *d=&v->displays[i];
  v->splash_result[i]=holly_fb_splash(&d->framebuffer,d->pixels,d->mapped_bytes);
  if(!v->splash_result[i])d->frames++;
 }
 if(v->active&& !v->poisoned){
  if(holly_video_select(v,0))return -1;
  struct holly_display *d=&v->displays[0];
  v->splash_result[0]=holly_fb_splash(&d->framebuffer,d->pixels,d->mapped_bytes);
  if(!v->splash_result[0])d->frames++;
 }
 return 0;
}

int holly_video_monitor(struct holly_video *v){
 if(!v||!v->initialized||v->poisoned)return -1;
 v->edid_valid=v->edid_width=v->edid_height=v->edid_manufacturer=0;
 uint32_t *w=v->monitor_request;for(unsigned i=0;i<48;i++)w[i]=0;
 w[0]=sizeof(v->monitor_request);w[2]=TAG_EDID;w[3]=136;w[5]=0;
 v->request_display=v->active;
 if(targeted_exchange(w,sizeof(v->monitor_request),v)){v->edid_result=-2;return -2;}
 v->edid_result=-3;
 if(w[1]!=0x80000000u||w[2]!=TAG_EDID||w[3]!=136||w[4]!=0x80000088u)return -3;
 if(w[5]||w[6]){v->edid_result=-4;return -4;}
 const uint8_t *b=(const uint8_t *)(w+7);unsigned checksum=0;
 for(unsigned i=0;i<128;i++)checksum+=b[i];
 static const uint8_t magic[8]={0,255,255,255,255,255,255,0};
 for(unsigned i=0;i<8;i++)if(b[i]!=magic[i]){v->edid_result=-5;return -5;}
 if(checksum&255u){v->edid_result=-5;return -5;}
 v->edid_manufacturer=((unsigned)b[8]<<8)|b[9];
 /* First detailed timing, if present; a monitor need not advertise one. */
 if(b[54]||b[55]){
  v->edid_width=(unsigned)b[56]|((unsigned)(b[58]&0xf0u)<<4);
  v->edid_height=(unsigned)b[59]|((unsigned)(b[61]&0xf0u)<<4);
 }
 v->edid_valid=1;v->edid_result=0;return 0;
}
