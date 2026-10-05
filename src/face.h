#ifndef HOLLY_FACE_H
#define HOLLY_FACE_H
#include <stddef.h>
#include <stdint.h>
#include "holly.h"
/* XRGB8888 pixels. voice_level 0..255 controls mouth opening while speaking.
 * Caller supplies a display buffer; no VideoCore or HDMI dependency. */
int holly_face_render(uint32_t *pixels,unsigned width,unsigned height,
                      unsigned stride,size_t capacity,
                      enum holly_expression expression,unsigned voice_level);
#endif
