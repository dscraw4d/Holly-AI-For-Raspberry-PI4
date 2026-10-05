#ifndef HOLLY_CLOCK_H
#define HOLLY_CLOCK_H
#include <stdint.h>
struct holly_clock { uint32_t (*read)(unsigned offset,void *context); void *context; };
/* Bounded high/low/high snapshot; no timer compare registers are written. */
int holly_clock_read(struct holly_clock *clock,uint64_t *ticks);
struct holly_clock holly_pi4_clock(void);
#endif
