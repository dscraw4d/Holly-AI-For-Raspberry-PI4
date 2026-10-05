#ifndef HOLLY_LANGUAGE_MODEL_H
#define HOLLY_LANGUAGE_MODEL_H
#include <stdint.h>
#define HOLLY_LM_CONTEXT 64
#define HOLLY_LM_VOCAB 98
#define HOLLY_LM_LIMIT 96
/* Bounded, integer-only, stateless inference. Returns -1 for invalid input. */
int holly_lm_generate(const char *prompt, char *output, unsigned capacity);
void holly_lm_logits(const uint8_t context[HOLLY_LM_CONTEXT], int32_t logits[HOLLY_LM_VOCAB]);
#define HOLLY_LM_PARAMETERS 39986
#define HOLLY_LM_OUTPUT_OFFSET 33616
#define HOLLY_LM_BIAS_OFFSET 39888
void holly_lm_default(int16_t *weights);
void holly_lm_use(const int16_t *weights);
void holly_lm_forward(const int16_t *weights,const uint8_t *context,int32_t *hidden,int32_t *logits);
#endif
