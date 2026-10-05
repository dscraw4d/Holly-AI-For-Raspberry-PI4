#include "display.h"
int holly_display_draw(struct holly_display *d,enum holly_expression expression,unsigned level){
    if(!d||d->status!=HOLLY_DISPLAY_READY)return -1;
    if(holly_fb_draw(&d->framebuffer,d->pixels,d->mapped_bytes,expression,level)){
        d->status=HOLLY_DISPLAY_RENDER_FAILED;return -1;
    }
    d->frames++;
    return 0;
}
int holly_display_start(struct holly_display *d,unsigned width,unsigned height,
                        const struct holly_display_ops *ops){
    if(!d)return -1;
    if(d->status!=HOLLY_DISPLAY_OFF)return d->status==HOLLY_DISPLAY_READY ? 0 : -1;
    d->pixels=0;d->mapped_bytes=0;d->frames=0;
    d->status=HOLLY_DISPLAY_REQUEST_FAILED;
    if(!ops||!ops->exchange||!ops->map||holly_fb_request_depth(d->request,width,height,16))return -1;
    d->status=HOLLY_DISPLAY_TRANSPORT_FAILED;
    if(ops->exchange(d->request,sizeof(d->request),ops->context))return -1;
    d->status=HOLLY_DISPLAY_RESPONSE_FAILED;
    if(holly_fb_response(d->request,&d->framebuffer))return -1;
    d->status=HOLLY_DISPLAY_MAPPING_FAILED;
    if(ops->map(&d->framebuffer,&d->pixels,&d->mapped_bytes,ops->context)||
       !d->pixels||((uintptr_t)d->pixels&3u)||d->mapped_bytes<d->framebuffer.byte_size){
        d->pixels=0;d->mapped_bytes=0;return -1;
    }
    d->status=HOLLY_DISPLAY_READY;
    return holly_display_draw(d,HOLLY_IDLE,0);
}
const char *holly_display_status_text(enum holly_display_status status){
    switch(status){
    case HOLLY_DISPLAY_OFF:return "not attempted";
    case HOLLY_DISPLAY_READY:return "framebuffer ready";
    case HOLLY_DISPLAY_REQUEST_FAILED:return "invalid display request";
    case HOLLY_DISPLAY_TRANSPORT_FAILED:return "mailbox exchange failed; request retained";
    case HOLLY_DISPLAY_RESPONSE_FAILED:return "firmware response rejected";
    case HOLLY_DISPLAY_MAPPING_FAILED:return "unsafe framebuffer mapping rejected";
    case HOLLY_DISPLAY_RENDER_FAILED:return "face rendering failed";
    default:return "unknown display state";
    }
}
