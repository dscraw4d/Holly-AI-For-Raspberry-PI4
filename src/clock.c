#include "clock.h"
int holly_clock_read(struct holly_clock *clock,uint64_t *ticks){
    if(!clock||!clock->read||!ticks)return -1;
    for(unsigned retry=0;retry<4;retry++){
        uint32_t high=clock->read(8,clock->context);
        uint32_t low=clock->read(4,clock->context);
        if(high==clock->read(8,clock->context)){
            *ticks=((uint64_t)high<<32)|low;return 0;
        }
    }
    return -2;
}
#ifndef HOST_TEST
static uint32_t pi_read(unsigned offset,void *context){
    (void)context;
    __asm__ volatile("dmb sy" ::: "memory");
    uint32_t value=*(volatile uint32_t *)(uintptr_t)(0xFE003000UL+offset);
    __asm__ volatile("dmb sy" ::: "memory");
    return value;
}
struct holly_clock holly_pi4_clock(void){struct holly_clock c={pi_read,0};return c;}
#else
struct holly_clock holly_pi4_clock(void){struct holly_clock c={0,0};return c;}
#endif
