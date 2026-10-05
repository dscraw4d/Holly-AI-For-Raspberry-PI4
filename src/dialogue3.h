#ifndef HOLLY_DIALOGUE3_H
#define HOLLY_DIALOGUE3_H
#include <stdint.h>
#define HOLLY_DIALOGUE3_OUTPUT 256
#define HOLLY_DIALOGUE3_WORDS 12
#define HOLLY_DIALOGUE3_USER_WORDS 48
#define HOLLY_DIALOGUE3_PREVIOUS_WORDS 64
unsigned holly_dialogue3_parameters(void);
unsigned holly_dialogue3_vocabulary(void);
/* 0 complete reply; 1 unsupported input/incomplete generation; -1 bad arguments.
 * Printable ASCII, bounded buffers; previous contains one recent exchange.
 * This is independent of persistent Seed-1 training weights. */
int holly_dialogue3_generate(const char *user,const char *previous,char *out,unsigned capacity);
/* Exposed for parity verification; arrays contain bounded token IDs. */
void holly_dialogue3_logits(const uint16_t *user,unsigned count,
                          const uint16_t *previous,unsigned previous_count,
                          const uint16_t words[HOLLY_DIALOGUE3_WORDS],int32_t *scores);
#endif
