#include "ethernet.h"
#include "dma_ring.h"
/* BCM2711 GENET v5 UMAC MDIO controller. Register values are hardware facts. */
#define MDIO_COMMAND (0x800u + 0x614u)
#define MDIO_BUSY (1u<<29)
#define MDIO_FAILURE (1u<<28)
#define MDIO_READ (2u<<26)
#define MAX_POLLS 100000u
/* GENET v5 descriptor RAM is 256 * 12 bytes, followed by 17 * 64 byte rings.
 * Ring 16 is the descriptor-based queue. These offsets do not enable DMA. */
#define RX_DMA 0x2000u
#define TX_DMA 0x4000u
#define DESC_RAM_BYTES (256u * 12u)
#define RING_BYTES 0x40u
#define DESC_RING (DESC_RAM_BYTES + 16u * RING_BYTES)
#define DMA_COMMON (DESC_RAM_BYTES + 17u * RING_BYTES)
static int v5_ready(struct ether_device *d,uint32_t base) {
    unsigned revision=(d->read(0,d->context)>>24)&15u;
    if(revision!=6u&&revision!=7u)return -2;
    if(d->read(base+DMA_COMMON+4u,d->context)&1u)return -3;
    if(d->read(base+DMA_COMMON+8u,d->context)&2u)return -4;
    return 0;
}
int ether_dma_stage_tx(struct ether_device *d,unsigned index,uint64_t dma_address,unsigned length) {
    uint32_t descriptor[3];
    if(!d||!d->read||!d->write||index>=256u||(dma_address&63u)||
       !dma_address||length<14u||length>VIPER_FRAME_CAP||
       dma_address>((1ULL<<40)-1u)-(length-1u)||
       viper_genet_descriptor(dma_address,length,1,descriptor))return -1;
    int ready=v5_ready(d,TX_DMA);
    if(ready)return ready;
    uint32_t at=TX_DMA+index*12u;
    d->write(at+4u,descriptor[1],d->context);
    d->write(at+8u,descriptor[2],d->context);
    d->write(at,descriptor[0],d->context);
    return 0;
}
int ether_dma_rx_peek(struct ether_device *d,uint16_t consumer,struct ether_rx_descriptor *result) {
    if(!d||!d->read||!result)return -1;
    unsigned revision=(d->read(0,d->context)>>24)&15u;
    if(revision!=6u&&revision!=7u)return -2;
    uint16_t producer=(uint16_t)d->read(RX_DMA+DESC_RING+0x08u,d->context);
    uint16_t pending=(uint16_t)(producer-consumer);
    if(!pending)return 0;
    if(pending>256u)return -3;
    uint32_t at=RX_DMA+((unsigned)consumer&255u)*12u;
    uint32_t status=d->read(at,d->context);
    uint16_t length=(uint16_t)(status>>16);
    if(length<14u||length>2048u)return -4;
    uint64_t low=d->read(at+4u,d->context);
    uint64_t high=d->read(at+8u,d->context);
    if(high>255u||!(low|high))return -4;
    result->dma_address=low|(high<<32);
    result->length=length;
    result->flags=(uint16_t)status;
    return 1;
}
int ether_dma_stage_rx(struct ether_device *d,uint64_t dma_base) {
    const uint64_t last_byte=256u*2048u-1u;
    if(!d||!d->read||!d->write||!dma_base||(dma_base&63u)||
       dma_base>((1ULL<<40)-1u)-last_byte)return -1;
    int ready=v5_ready(d,RX_DMA);
    if(ready)return ready;
    for(unsigned i=0;i<256u;i++) {
        uint32_t at=RX_DMA+i*12u;
        uint64_t addr=dma_base+(uint64_t)i*2048u;
        /* Address words precede OWN so the device never sees a partial slot. */
        d->write(at+4u,(uint32_t)addr,d->context);
        d->write(at+8u,(uint32_t)(addr>>32),d->context);
        d->write(at,(2048u<<16)|0x8000u,d->context);
    }
    return 0;
}
int ether_dma_prepare_ring(struct ether_device *d,int transmit) {
    if(!d||!d->read||!d->write||(transmit!=0&&transmit!=1))return -1;
    /* The Pi 4's v5 controller reports 6 or 7 in the revision major nibble. */
    unsigned revision=(d->read(0,d->context)>>24)&15u;
    if(revision!=6u&&revision!=7u)return -2;
    uint32_t base=transmit ? TX_DMA : RX_DMA;
    uint32_t ring=base+DESC_RING,common=base+DMA_COMMON;
    /* Refuse to rewrite a live DMA queue. The DMA enable bit is bit zero. */
    if(d->read(common+4u,d->context)&1u)return -3;
    d->write(ring+0x08u,0,d->context); /* consumer on TX, producer on RX */
    d->write(ring+0x0cu,0,d->context); /* producer on TX, consumer on RX */
    d->write(ring+0x00u,0,d->context);
    d->write(ring+0x04u,0,d->context);
    d->write(ring+0x2cu,0,d->context);
    d->write(ring+0x30u,0,d->context);
    d->write(ring+0x14u,0,d->context);
    d->write(ring+0x18u,0,d->context);
    /* GENET ring boundaries count 32-bit descriptor words, not byte addresses. */
    d->write(ring+0x1cu,256u*3u-1u,d->context);
    d->write(ring+0x20u,0,d->context);
    d->write(ring+0x10u,(256u<<16)|2048u,d->context);
    return 0;
}
int ether_dma_snapshot(struct ether_device *d,int transmit,struct ether_dma_snapshot *s) {
    if(!d || !d->read || !s || (transmit!=0 && transmit!=1)) return -1;
    uint32_t base=transmit ? TX_DMA : RX_DMA;
    uint32_t ring=base+DESC_RING, common=base+DMA_COMMON;
    s->configuration=d->read(common,d->context);
    s->control=d->read(common+4u,d->context);
    s->status=d->read(common+8u,d->context);
    /* The producer and consumer index offsets exchange roles on RX. */
    s->producer=(uint16_t)d->read(ring+(transmit ? 0x0cu : 0x08u),d->context);
    s->consumer=(uint16_t)d->read(ring+(transmit ? 0x08u : 0x0cu),d->context);
    s->buffer_size=d->read(ring+0x10u,d->context);
    uint64_t lo=d->read(ring+0x14u,d->context);
    uint64_t hi=d->read(ring+0x18u,d->context);
    s->descriptor_start=lo|(hi<<32);
    lo=d->read(ring+0x1cu,d->context);
    hi=d->read(ring+0x20u,d->context);
    s->descriptor_end=lo|(hi<<32);
    return 0;
}
uint32_t ether_revision(struct ether_device *d) { return d->read(0, d->context); }
int ether_phy_read(struct ether_device *d,unsigned phy,unsigned reg,uint16_t *value) {
    if(!d || !d->read || !d->write || !value || phy>31 || reg>31) return -1;
    for(unsigned i=0;i<MAX_POLLS;i++) if(!(d->read(MDIO_COMMAND,d->context)&MDIO_BUSY)) {
        d->write(MDIO_COMMAND,MDIO_BUSY|MDIO_READ|(phy<<21)|(reg<<16),d->context);
        for(unsigned j=0;j<MAX_POLLS;j++) {
            uint32_t result=d->read(MDIO_COMMAND,d->context);
            if(!(result&MDIO_BUSY)) {
                if(result&MDIO_FAILURE) return -2;
                *value=(uint16_t)result;
                return 0;
            }
        }
        return -3;
    }
    return -3;
}
int ether_phy_probe(struct ether_device *d,unsigned *address,uint32_t *identifier) {
    if(!d || !address || !identifier) return -1;
    for(unsigned phy=0;phy<32;phy++) {
        uint16_t high=0,low=0;
        if(ether_phy_read(d,phy,2,&high) || ether_phy_read(d,phy,3,&low)) continue;
        if(high==0 || high==0xFFFF || (high==0xFFFF && low==0xFFFF)) continue;
        *address=phy;*identifier=((uint32_t)high<<16)|low;
        return 0;
    }
    return -2;
}
#ifndef HOST_TEST
static uint32_t pi_read(uint32_t offset,void *unused) {
    (void)unused; return *(volatile uint32_t *)(uintptr_t)(0xFD580000UL+offset);
}
static void pi_write(uint32_t offset,uint32_t value,void *unused) {
    (void)unused; *(volatile uint32_t *)(uintptr_t)(0xFD580000UL+offset)=value;
}
struct ether_device ether_pi4_device(void) {
    struct ether_device d={pi_read,pi_write,0}; return d;
}
#else
struct ether_device ether_pi4_device(void) {
    struct ether_device d={0,0,0};return d;
}
#endif
