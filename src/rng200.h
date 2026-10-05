#ifndef HOLLY_RNG200_H
#define HOLLY_RNG200_H
#include <stddef.h>
#include <stdint.h>
struct holly_rng200 {
    uint32_t (*read)(unsigned,void *);
    void (*write)(unsigned,uint32_t,void *);
    void *context;
    uint32_t previous;unsigned have_previous,ready,failed;
};
/* Bounded FIFO polling, hardware fault checks and continuous word check.
 * This is not a certification of the hardware's entropy quality. */
int holly_rng200_start(struct holly_rng200 *);
int holly_rng200_bytes(struct holly_rng200 *,uint8_t *,size_t);
struct holly_rng200 holly_pi4_rng200(void);
#endif
