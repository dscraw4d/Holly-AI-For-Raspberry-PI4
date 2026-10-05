#ifndef HOLLY_VIDEO_H
#define HOLLY_VIDEO_H
#include "display.h"
struct holly_video {
    struct holly_display displays[2];
    struct holly_display_ops ops;
    _Alignas(16) uint32_t control[16];
    _Alignas(16) uint32_t target_request[64];
    _Alignas(16) uint32_t monitor_request[48];
    unsigned request_display;
    int layer_result,edid_result;
    unsigned edid_valid,edid_width,edid_height;
    uint32_t edid_manufacturer;
    unsigned initialized,active,mode,poisoned,count;
    int count_result,select_result,offset_result,unblank_result;
    int splash_result[2];
    uint32_t last_tag,last_header,last_tag_reply;
};
/* Persistent object: mailbox timeouts prohibit all further control requests. */
int holly_video_start(struct holly_video *,const struct holly_display_ops *);
int holly_video_select(struct holly_video *,unsigned index);
int holly_video_unblank(struct holly_video *);
/* Read EDID block zero for the active display; no framebuffer allocation. */
int holly_video_monitor(struct holly_video *);
/* mode 0 face, 1 bars, 2 white, 3 red, 4 green, 5 blue */
int holly_video_pattern(struct holly_video *,unsigned mode);
/* Draw the boot mark on primary framebuffer only; secondary is opt-in. */
int holly_video_splash(struct holly_video *);
#endif
