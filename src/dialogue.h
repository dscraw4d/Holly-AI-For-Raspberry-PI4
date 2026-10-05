#ifndef HOLLY_DIALOGUE_H
#define HOLLY_DIALOGUE_H
#include <stdint.h>
#define HOLLY_DIALOGUE_OUTPUT 256
#define HOLLY_DIALOGUE_WORDS 8
#define HOLLY_DIALOGUE_USER_WORDS 32
#define HOLLY_DIALOGUE_PREVIOUS_WORDS 48
unsigned holly_dialogue_parameters(void);
unsigned holly_dialogue_vocabulary(void);
/* 0 complete reply; 1 unsupported input/incomplete generation; -1 bad arguments.
 * Printable ASCII, bounded buffers; previous contains one recent exchange.
 * This is independent of persistent Seed-1 training weights. */
int holly_dialogue_generate(const char *user,const char *previous,char *out,unsigned capacity);
/* Exposed for parity verification; arrays contain bounded token IDs. */
void holly_dialogue_logits(const uint16_t *user,unsigned count,
                          const uint16_t *previous,unsigned previous_count,
                          const uint16_t words[HOLLY_DIALOGUE_WORDS],int32_t *scores);
#endif
