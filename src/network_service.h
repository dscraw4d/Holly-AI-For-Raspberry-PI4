#ifndef HOLLY_NETWORK_SERVICE_H
#define HOLLY_NETWORK_SERVICE_H
#include <stdint.h>
#include "dma_ring.h"
#include "tcp.h"
struct holly_network {
    struct viper_dma_ring rx;
    struct viper_dma_ring tx;
    struct holly_tcp diagnostic_tcp;
    uint8_t mac[6];
    uint8_t ip[4];
    unsigned received;
    unsigned answered;
    unsigned dropped;
};
void holly_network_init(struct holly_network *network,const uint8_t mac[6],const uint8_t ip[4]);
int holly_network_receive(struct holly_network *network,const uint8_t *frame,unsigned length);
unsigned holly_network_process(struct holly_network *network,unsigned budget);
/* Advance diagnostic TCP timers and queue any retransmission. */
int holly_network_tick(struct holly_network *network,uint32_t elapsed_ms);
int holly_network_transmit(struct holly_network *network,uint8_t *frame,unsigned capacity,unsigned *length);
#endif
