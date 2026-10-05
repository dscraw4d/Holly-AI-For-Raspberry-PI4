#include <assert.h>
#include <stdio.h>
#include "clock.h"
#include "animation.h"
struct registers {unsigned index;};
static uint32_t rollover(unsigned offset,void *context){
    struct registers *r=context;
    const unsigned offsets[]={8,4,8,8,4,8};
    const uint32_t values[]={1,0xFFFFFFFFu,2,2,4,2};
    assert(r->index<6 && offset==offsets[r->index]);
    return values[r->index++];
}
static uint32_t unstable(unsigned offset,void *context){
    (void)offset;struct registers *r=context;return r->index++;
}
int main(void){
    struct registers r={0};struct holly_clock c={rollover,&r};
    uint64_t ticks=0;
    assert(holly_clock_read(&c,&ticks)==0 && ticks==0x200000004ULL && r.index==6);
    c.read=unstable;r.index=0;ticks=99;
    assert(holly_clock_read(&c,&ticks)==-2 && ticks==99 && r.index==12);
    assert(holly_clock_read(0,&ticks)==-1);
    struct holly_animation a={0};
    assert(holly_animation_step(&a,100,HOLLY_LISTENING)==1);
    assert(holly_animation_step(&a,200,HOLLY_LISTENING)==0);
    assert(holly_animation_step(&a,4950,HOLLY_LISTENING)==1 && a.expression==HOLLY_BLINKING);
    assert(holly_animation_step(&a,5099,HOLLY_LISTENING)==0);
    assert(holly_animation_step(&a,5100,HOLLY_LISTENING)==1 && a.expression==HOLLY_LISTENING);
    assert(holly_animation_step(&a,5100,HOLLY_SPEAKING)==1 && a.level<=255);
    unsigned before=a.level;
    assert(holly_animation_step(&a,5180,HOLLY_SPEAKING)==1 && a.level!=before);
    assert(holly_animation_step(&a,5181,HOLLY_SPEAKING)==0);
    assert(holly_animation_step(&a,1,HOLLY_IDLE)==1 && a.origin_ms==1);
    assert(holly_animation_step(&a,2,HOLLY_BLINKING)==-1);
    puts("Timer rollover, bounded retries and animation timing tests passed");
}
