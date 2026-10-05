#ifndef VIPER_ETHERNET_H
#define VIPER_ETHERNET_H
#include <stdint.h>
/* GENET/MDIO register access, with DMA activation still deliberately absent. */
typedef uint32_t (*ether_read_fn)(uint32_t offset, void *context);
typedef void (*ether_write_fn)(uint32_t offset, uint32_t value, void *context);
struct ether_device {
    ether_read_fn read;
    ether_write_fn write;
    void *context;
};
/* GENET v5 ring 16, which uses the descriptor RAM. Read-only snapshot. */
struct ether_dma_snapshot {
    uint32_t configuration, control, status;
    uint16_t producer, consumer;
    uint32_t buffer_size;
    uint64_t descriptor_start, descriptor_end;
};
uint32_t ether_revision(struct ether_device *device);
int ether_dma_snapshot(struct ether_device *device, int transmit,
                       struct ether_dma_snapshot *result);
/* Stage ring 16 with 256 descriptors / 2048-byte buffers. Never starts DMA.
 * Caller must allocate and map coherent frame buffers separately. */
int ether_dma_prepare_ring(struct ether_device *device,int transmit);
/* Stage RX descriptor RAM from a proven device-visible, coherent DMA region.
 * Does not allocate buffers, enable the queue, or advance indices. */
int ether_dma_stage_rx(struct ether_device *device,uint64_t dma_base);
/* Stage one TX descriptor without posting it to the producer index. */
int ether_dma_stage_tx(struct ether_device *device,unsigned index,
                       uint64_t dma_address,unsigned length);
struct ether_rx_descriptor {
    uint64_t dma_address;
    uint16_t length, flags;
};
/* 0 = no new descriptor, 1 = raw metadata ready, negative = invalid/overrun.
 * The caller owns consumer-index updates and DMA buffer cache invalidation. */
int ether_dma_rx_peek(struct ether_device *device,uint16_t consumer,
                      struct ether_rx_descriptor *result);
int ether_phy_read(struct ether_device *device, unsigned phy, unsigned reg, uint16_t *value);
int ether_phy_probe(struct ether_device *device, unsigned *address, uint32_t *identifier);
struct ether_device ether_pi4_device(void);
#endif
