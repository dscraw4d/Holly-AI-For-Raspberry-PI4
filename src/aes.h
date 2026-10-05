#ifndef HOLLY_AES_H
#define HOLLY_AES_H
#include <stddef.h>
#include <stdint.h>
/* Original AES-128 implementation of FIPS 197. No secret-indexed tables.
 * CTR state is directional and must never be reused with the same key/IV. */
struct holly_aes_ctr { uint8_t keys[176], counter[16], stream[16]; unsigned used, exhausted; };
void holly_aes128_keys(const uint8_t key[16],uint8_t expanded[176]);
void holly_aes128_block(const uint8_t expanded[176],const uint8_t input[16],uint8_t output[16]);
void holly_aes_ctr_init(struct holly_aes_ctr *s,const uint8_t key[16],const uint8_t iv[16]);
int holly_aes_ctr_xor(struct holly_aes_ctr *s,uint8_t *data,size_t bytes);
void holly_secret_wipe(void *data,size_t bytes);
#endif
