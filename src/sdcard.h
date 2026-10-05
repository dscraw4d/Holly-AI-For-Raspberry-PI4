#ifndef HOLLY_SDCARD_H
#define HOLLY_SDCARD_H
#include <stdint.h>

/* BCM2711's Pi 4 microSD socket is attached to the eMMC2 SDHCI host. */
#define HOLLY_PI4_EMMC2_BASE 0xFE340000UL
struct holly_sdcard {
    uintptr_t base;
    uint16_t rca;
    uint32_t sectors;
    unsigned high_capacity;
    unsigned ready;
    int last_error;
    unsigned stage, last_command;
    uint32_t capabilities, version, state, control1, interrupt, response;

};
struct holly_sdcard holly_pi4_sdcard(void);
/* Decode a normalized 128-bit SD CSD response (most significant word first). */
int holly_sdcard_capacity_sectors(const uint32_t csd[4],uint32_t *sectors);
int holly_sdcard_start(struct holly_sdcard *card);
int holly_sdcard_read_sector(struct holly_sdcard *card,uint32_t lba,uint8_t *sector);
int holly_sdcard_write_sector(struct holly_sdcard *card,uint32_t lba,const uint8_t *sector);
const char *holly_sdcard_status(const struct holly_sdcard *card);

#endif
