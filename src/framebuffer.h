#ifndef HOLLY_FRAMEBUFFER_H
#define HOLLY_FRAMEBUFFER_H
#include <stddef.h>
#include <stdint.h>
#include "holly.h"
#define HOLLY_FB_WORDS 36u
struct holly_framebuffer {
    uint32_t bus_address,byte_size,width,height,pitch_bytes,pixel_order,alpha_mode,depth;
};
int holly_fb_request_depth(uint32_t words[HOLLY_FB_WORDS],unsigned width,unsigned height,unsigned depth);
/* Construct a 16-byte-aligned property request for 32-bit RGB pixels. */
int holly_fb_request(uint32_t words[HOLLY_FB_WORDS],unsigned width,unsigned height);
/* Validate all required responses before exposing a framebuffer address. */
int holly_fb_response(const uint32_t words[HOLLY_FB_WORDS],struct holly_framebuffer *out);
/* Draw after the caller has mapped the returned bus address for the ARM and
 * arranged appropriate cache maintenance. No address translation is guessed. */
int holly_fb_draw(const struct holly_framebuffer *fb,uint32_t *mapped,
                  size_t mapped_bytes,enum holly_expression expression,unsigned voice_level);
int holly_fb_pattern(const struct holly_framebuffer *,uint32_t *,size_t,unsigned mode);
int holly_fb_splash(const struct holly_framebuffer *,uint32_t *,size_t);
/* Pi 4 legacy bus window for the first 1 GiB of physical SDRAM. These
 * helpers require an identity-mapped ARM address and reject crossing it. */
int holly_pi4_ram_to_bus(uintptr_t physical,size_t bytes,uint32_t *bus);
int holly_pi4_fb_bus_to_arm(uint32_t bus,size_t bytes,uintptr_t *physical);
struct holly_mailbox {
    uint32_t (*read)(uint32_t offset,void *context);
    void (*write)(uint32_t offset,uint32_t value,void *context);
    void *context;
};
/* Channel 8 transaction. bus_address is the caller-proven GPU address of the
 * 16-byte-aligned request, visible to both processors. */
int holly_mailbox_property(struct holly_mailbox *device,uint32_t bus_address);
struct holly_mailbox holly_pi4_mailbox(void);
#endif
