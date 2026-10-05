#ifndef VIPER_DMA_RING_H
#define VIPER_DMA_RING_H
#include <stdint.h>
#define VIPER_RING_SLOTS 16
#define VIPER_FRAME_CAP 1536
struct viper_dma_slot {
    uint8_t bytes[VIPER_FRAME_CAP];
    uint16_t length;
};
struct viper_dma_ring {
    struct viper_dma_slot slots[VIPER_RING_SLOTS];
    unsigned producer;
    unsigned consumer;
    unsigned used;
};
void viper_ring_init(struct viper_dma_ring *ring);
int viper_ring_push(struct viper_dma_ring *ring,const uint8_t *frame,unsigned length);
int viper_ring_pop(struct viper_dma_ring *ring,uint8_t *frame,unsigned capacity,unsigned *length);
/* GENET descriptor status and address words for the hardware handoff. */
int viper_genet_descriptor(uint64_t physical,unsigned length,int transmit,uint32_t words[3]);
#endif
