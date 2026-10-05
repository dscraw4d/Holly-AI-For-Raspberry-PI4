#ifndef HOLLY_SHA256_H
#define HOLLY_SHA256_H
#include <stddef.h>
#include <stdint.h>
#define HOLLY_SHA256_BYTES 32u
struct holly_sha256 {
    uint32_t state[8];
    uint64_t bytes;
    uint8_t block[64];
    unsigned used,finalized;
};
/* Context must be initialized before use. Input/output buffers must not
 * overlap the context; each finish consumes it. No heap or libc is needed. */
void holly_sha256_init(struct holly_sha256 *context);
/* Reject invalid pointers, finalized contexts and length overflow. */
int holly_sha256_update(struct holly_sha256 *context,const void *data,size_t bytes);
int holly_sha256_finish(struct holly_sha256 *context,uint8_t digest[32]);
int holly_sha256_hash(const void *data,size_t bytes,uint8_t digest[32]);
int holly_hmac_sha256(const void *key,size_t key_bytes,const void *data,
                      size_t bytes,uint8_t digest[32]);
/* Fixed-length comparison for authentication tags; call only with valid
 * buffers. Returns 1 for equality. No early exit based on tag contents. */
int holly_tag_equal(const uint8_t *left,const uint8_t *right,size_t bytes);
#endif
