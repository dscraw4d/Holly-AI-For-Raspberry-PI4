#ifndef HOLLY_SPLASH_H
#define HOLLY_SPLASH_H
#include <stddef.h>
#include <stdint.h>
/* Original JMC boot mark in 32-bit RGB; no firmware calls or dynamic memory. */
int holly_splash_render(uint32_t *pixels,unsigned width,unsigned height,
                        unsigned stride,size_t capacity);
#endif
