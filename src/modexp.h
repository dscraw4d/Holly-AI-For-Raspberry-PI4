#ifndef HOLLY_MODEXP_H
#define HOLLY_MODEXP_H
#include <stddef.h>
#include <stdint.h>
/* Fixed 2048-bit odd, high-bit-set modulus. Big-endian byte interface.
 * Uses 64-bit limbs and fixed four-bit windows with full-table selection. This has
 * not had a cryptographic side-channel audit. No heap or external library. */
int holly_modexp2048(const uint8_t base[256],const uint8_t *exponent,size_t exponent_bytes,
                    const uint8_t modulus[256],uint8_t output[256]);
extern const uint8_t holly_dh14_prime[256];
#endif
