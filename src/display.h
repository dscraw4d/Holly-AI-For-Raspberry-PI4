#ifndef HOLLY_DISPLAY_H
#define HOLLY_DISPLAY_H
#include "framebuffer.h"
enum holly_display_status {
    HOLLY_DISPLAY_OFF, HOLLY_DISPLAY_READY, HOLLY_DISPLAY_REQUEST_FAILED,
    HOLLY_DISPLAY_TRANSPORT_FAILED, HOLLY_DISPLAY_RESPONSE_FAILED,
    HOLLY_DISPLAY_MAPPING_FAILED, HOLLY_DISPLAY_RENDER_FAILED
};
struct holly_display {
    _Alignas(16) uint32_t request[HOLLY_FB_WORDS];
    struct holly_framebuffer framebuffer;
    uint32_t *pixels;
    size_t mapped_bytes;
    enum holly_display_status status;
    unsigned frames;
};
struct holly_display_ops {
    int (*exchange)(uint32_t *request,size_t bytes,void *context);
    /* Return a proven writable mapping; reject protected OS memory. */
    int (*map)(const struct holly_framebuffer *fb,uint32_t **pixels,
               size_t *bytes,void *context);
    void *context;
};
/* Zero-initialize once. Keep this object alive after a mailbox timeout:
 * firmware may still own the request. Start makes only one attempt. */
int holly_display_start(struct holly_display *display,unsigned width,unsigned height,
                        const struct holly_display_ops *ops);
int holly_display_draw(struct holly_display *display,enum holly_expression expression,
                       unsigned voice_level);
const char *holly_display_status_text(enum holly_display_status status);
#endif
