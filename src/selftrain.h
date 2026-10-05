#ifndef HOLLY_SELFTRAIN_H
#define HOLLY_SELFTRAIN_H
#include "holly.h"
#include "model_store.h"
void holly_training_init(void);
int holly_training_mount(const struct holly_block_ops *io);
void holly_training_tick(uint64_t now);
void holly_training_activity(void);
int holly_training_command(char *line,holly_emit_fn emit,void *context,
                          const struct holly_memory_ops *memory,void *memory_context);
#endif
