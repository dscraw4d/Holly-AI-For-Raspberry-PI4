#ifndef HOLLY_ANIMATION_H
#define HOLLY_ANIMATION_H
#include <stdint.h>
#include "holly.h"
struct holly_animation {
    uint64_t origin_ms,last_ms;
    enum holly_expression expression;
    unsigned level,initialized;
};
/* Cosmetic mouth motion and periodic blink; no audio signal is implied.
 * Returns 1 only when a new face frame is due, 0 otherwise. */
int holly_animation_step(struct holly_animation *state,uint64_t now_ms,
                         enum holly_expression requested);
/* Text-derived visual approximation; no TTS/audio or phoneme alignment. */
#define HOLLY_MOUTH_EVENTS 128u
struct holly_mouth {
 uint8_t frames[HOLLY_MOUTH_EVENTS],durations[HOLLY_MOUTH_EVENTS];
 unsigned count,index,frame,started,manual;
 uint64_t next_ms,last_ms;
};
void holly_mouth_reply(struct holly_mouth *,const char *);
int holly_mouth_step(struct holly_mouth *,uint64_t now_ms);
int holly_mouth_frame(struct holly_mouth *,unsigned frame);
#endif
