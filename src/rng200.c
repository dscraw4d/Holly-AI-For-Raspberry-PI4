#include "rng200.h"
#include "aes.h"
static int next(struct holly_rng200 *s,uint32_t *out){
    for(unsigned i=0;i<1000000;i++){
        if(s->read(0x18,s->context)&0x80000020u)break;
        if(s->read(0x24,s->context)&255u){
            uint32_t v=s->read(0x20,s->context);
            if(s->have_previous&&v==s->previous)break;
            s->have_previous=1;s->previous=v;*out=v;return 0;
        }
    }
    s->failed=1;s->ready=0;return -1;
}
int holly_rng200_start(struct holly_rng200 *s){
    if(!s||!s->read||!s->write||s->failed)return -1;
    s->ready=0;s->have_previous=0;
    uint32_t control=s->read(0,s->context);s->write(0,(control&~0x1fffu)|1u,s->context);
    /* Discard initial FIFO words; no software timing seed or fallback. */
    for(unsigned i=0;i<32;i++){uint32_t discard;if(next(s,&discard))return -1;}
    s->ready=1;return 0;
}
int holly_rng200_bytes(struct holly_rng200 *s,uint8_t *out,size_t n){
    if(!s||(!out&&n)||!s->ready||s->failed||n>35000)return -1;
    size_t used=0;
    while(used<n){uint32_t v;if(next(s,&v)){holly_secret_wipe(out,n);return -1;}
        for(unsigned j=0;j<4&&used<n;j++)out[used++]=(uint8_t)(v>>(8*j));}
    return 0;
}
#ifndef HOST_TEST
static uint32_t rd(unsigned at,void *context){(void)context;uint32_t v=*(volatile uint32_t *)(uintptr_t)(0xfe104000ul+at);__asm__ volatile("dmb sy":::"memory");return v;}
static void wr(unsigned at,uint32_t v,void *context){(void)context;__asm__ volatile("dmb sy":::"memory");*(volatile uint32_t *)(uintptr_t)(0xfe104000ul+at)=v;}
struct holly_rng200 holly_pi4_rng200(void){struct holly_rng200 s={rd,wr,0,0,0,0,0};return s;}
#else
struct holly_rng200 holly_pi4_rng200(void){struct holly_rng200 s={0};return s;}
#endif
