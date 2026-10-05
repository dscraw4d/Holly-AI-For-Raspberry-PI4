#include <assert.h>
#include <stdio.h>
#include "display.h"
struct fake {
    unsigned exchanges,maps;
    int timeout,bad_response,bad_mapping;
    uint32_t pixels[64*64+1];
};
static int exchange(uint32_t *w,size_t bytes,void *context){
    struct fake *f=context;f->exchanges++;
    assert(bytes==144 && ((uintptr_t)w&15u)==0);
    if(f->timeout)return -1;
    w[1]=0x80000000u;
    const unsigned tags[]={4,9,14,18,22,26,31};
    const unsigned lengths[]={8,8,4,4,4,8,4};
    for(unsigned i=0;i<7;i++)w[tags[i]]=0x80000000u|lengths[i];
    w[27]=0xC1000000u;w[28]=64*64*4;w[32]=256;
    if(f->bad_response)w[19]=2;
    return 0;
}
static int map(const struct holly_framebuffer *fb,uint32_t **p,size_t *bytes,void *context){
    struct fake *f=context;f->maps++;
    assert(fb->bus_address==0xC1000000u);
    *p=f->pixels;*bytes=f->bad_mapping ? 4 : 64*64*4;
    return 0;
}
int main(void){
    struct fake f={0};struct holly_display d={0};
    const struct holly_display_ops ops={exchange,map,&f};
    f.pixels[64*64]=0xDEADBEEF;
    assert(holly_display_draw(&d,HOLLY_IDLE,0)==-1);
    assert(holly_display_start(&d,64,64,&ops)==0);
    assert(d.status==HOLLY_DISPLAY_READY && d.frames==1);
    assert(f.exchanges==1 && f.maps==1 && f.pixels[64*64]==0xDEADBEEF);
    assert(holly_display_draw(&d,HOLLY_SPEAKING,200)==0 && d.frames==2);
    assert(holly_display_start(&d,64,64,&ops)==0 && f.exchanges==1);
    struct holly_display failed={0};f.timeout=1;
    assert(holly_display_start(&failed,64,64,&ops)==-1);
    assert(failed.status==HOLLY_DISPLAY_TRANSPORT_FAILED && f.maps==1);
    unsigned exchanges=f.exchanges;
    assert(holly_display_start(&failed,64,64,&ops)==-1 && f.exchanges==exchanges);
    f.timeout=0;f.bad_response=1;struct holly_display invalid={0};
    assert(holly_display_start(&invalid,64,64,&ops)==-1 && f.maps==1);
    assert(invalid.status==HOLLY_DISPLAY_RESPONSE_FAILED);
    f.bad_response=0;f.bad_mapping=1;struct holly_display short_map={0};
    assert(holly_display_start(&short_map,64,64,&ops)==-1);
    assert(short_map.status==HOLLY_DISPLAY_MAPPING_FAILED && !short_map.pixels);
    assert(holly_display_draw(&short_map,HOLLY_IDLE,0)==-1);
    struct holly_display bad_request={0};
    assert(holly_display_start(&bad_request,1,1,&ops)==-1);
    assert(bad_request.status==HOLLY_DISPLAY_REQUEST_FAILED);
    puts("Display service lifecycle, timeout retention and mapping guards passed");
}
