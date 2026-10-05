#include "dma_ring.h"
void viper_ring_init(struct viper_dma_ring *r) {
    if(!r) return;
    r->producer=0;r->consumer=0;r->used=0;
    for(unsigned i=0;i<VIPER_RING_SLOTS;i++) r->slots[i].length=0;
}
int viper_ring_push(struct viper_dma_ring *r,const uint8_t *frame,unsigned length) {
    if(!r || !frame || length<14 || length>VIPER_FRAME_CAP) return -1;
    if(r->used==VIPER_RING_SLOTS) return -2;
    struct viper_dma_slot *slot=&r->slots[r->producer];
    for(unsigned i=0;i<length;i++) slot->bytes[i]=frame[i];
    slot->length=(uint16_t)length;
    r->producer=(r->producer+1)%VIPER_RING_SLOTS;
    r->used++;
    return 0;
}
int viper_ring_pop(struct viper_dma_ring *r,uint8_t *frame,unsigned capacity,unsigned *length) {
    if(!r || !frame || !length) return -1;
    if(!r->used) return -2;
    struct viper_dma_slot *slot=&r->slots[r->consumer];
    if(capacity<slot->length) return -3;
    for(unsigned i=0;i<slot->length;i++) frame[i]=slot->bytes[i];
    *length=slot->length;
    slot->length=0;
    r->consumer=(r->consumer+1)%VIPER_RING_SLOTS;
    r->used--;
    return 0;
}
int viper_genet_descriptor(uint64_t physical,unsigned length,int transmit,uint32_t words[3]) {
    if(!words || !physical || length<14 || length>VIPER_FRAME_CAP || (physical>>40) ||
       (transmit!=0 && transmit!=1)) return -1;
    /* GENET v5: [31:16] byte count, [15:0] flags; 40-bit DMA address. */
    uint32_t flags=transmit ? (0x4000u|0x2000u|(0x3fu<<7)|0x40u) : 0x8000u;
    words[0]=(length<<16)|flags;
    words[1]=(uint32_t)physical;
    words[2]=(uint32_t)(physical>>32);
    return 0;
}
