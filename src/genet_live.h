#ifndef HOLLY_GENET_LIVE_H
#define HOLLY_GENET_LIVE_H
#include "ethernet.h"
/* Pi 4 GENET v5 only. Buffers must be CPU-physical, identity mapped, coherent,
 * 64-byte aligned, and remain allocated until the device is stopped.
 * Kernel integration currently requires MMU and D-cache OFF. */
struct holly_genet {
    struct ether_device device;
    void (*delay_us)(unsigned,void *);void (*barrier)(void *);void *context;
    uint8_t *rx,*tx;uint64_t rx_physical,tx_physical;
    uint16_t consumer,producer;unsigned phy,active,link,speed;
};
int holly_genet_start(struct holly_genet *,const uint8_t mac[6]);
int holly_genet_link(struct holly_genet *);
/* Copy one validated frame. 0 empty/discarded, positive bytes, -1 fault. */
int holly_genet_receive(struct holly_genet *,uint8_t *,unsigned);
/* Queue a private copy before handing its descriptor to DMA. */
int holly_genet_transmit(struct holly_genet *,const uint8_t *,unsigned);
void holly_genet_stop(struct holly_genet *);
#endif
