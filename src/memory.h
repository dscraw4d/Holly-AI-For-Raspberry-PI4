#ifndef HOLLY_MEMORY_H
#define HOLLY_MEMORY_H
#include <stdint.h>

#define HOLLY_MEMORY_TEXT_SIZE 257u
#define HOLLY_MEMORY_SOURCE_SIZE 97u

/* A reviewable memory carries its provenance and an explicit confidence score. */
struct holly_memory_item {
    uint32_t id;
    uint32_t revision;
    unsigned confidence;
    char text[HOLLY_MEMORY_TEXT_SIZE];
    char source[HOLLY_MEMORY_SOURCE_SIZE];
};

#endif
