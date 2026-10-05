#include <assert.h>
#include <stdio.h>
#include "dma_ring.h"
int main(void) {
 struct viper_dma_ring ring;viper_ring_init(&ring);
 uint8_t frame[32]={0},out[32]={0};frame[0]=0xAB;
 for(unsigned i=0;i<VIPER_RING_SLOTS;i++) assert(viper_ring_push(&ring,frame,32)==0);
 assert(viper_ring_push(&ring,frame,32)==-2);
 unsigned size=0;
 assert(viper_ring_pop(&ring,out,1,&size)==-3 && ring.used==VIPER_RING_SLOTS);
 for(unsigned i=0;i<VIPER_RING_SLOTS;i++) {
  assert(viper_ring_pop(&ring,out,sizeof(out),&size)==0);
  assert(out[0]==0xAB && size==32);
 }
 assert(viper_ring_pop(&ring,out,sizeof(out),&size)==-2);
 assert(viper_ring_push(&ring,frame,32)==0);
 uint32_t desc[3]={0};
 assert(viper_genet_descriptor(0x12ABCDEF12ULL,32,1,desc)==0);
 assert(desc[0]==(32u<<16|0x7fc0u));
 assert(desc[1]==0xABCDEF12u && desc[2]==0x12u);
 assert(viper_genet_descriptor(0x12ABCDEF12ULL,1536,0,desc)==0);
 assert(desc[0]==(1536u<<16|0x8000u));
 assert(viper_genet_descriptor(0x12ABCDEF12ULL,32,2,desc)==-1);
 assert(viper_genet_descriptor(1ULL<<40,32,1,desc)==-1);
 puts("DMA ring and descriptor tests passed");
}
