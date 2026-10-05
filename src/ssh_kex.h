#ifndef HOLLY_SSH_KEX_H
#define HOLLY_SSH_KEX_H
#include <stddef.h>
#include <stdint.h>
#define HOLLY_SSH_NAME_LIST_MAX 1024u
#define HOLLY_SSH_KEX_LISTS 10u
struct holly_ssh_names { const uint8_t *data; size_t bytes; };
enum holly_ssh_kex_field { HOLLY_KEX_METHOD, HOLLY_KEX_HOST_KEY,
    HOLLY_KEX_CIPHER_C2S,HOLLY_KEX_CIPHER_S2C,HOLLY_KEX_MAC_C2S,HOLLY_KEX_MAC_S2C,
    HOLLY_KEX_COMPRESSION_C2S,HOLLY_KEX_COMPRESSION_S2C,
    HOLLY_KEX_LANGUAGE_C2S,HOLLY_KEX_LANGUAGE_S2C };
struct holly_ssh_kexinit {
    uint8_t cookie[16];
    struct holly_ssh_names lists[HOLLY_SSH_KEX_LISTS];
    unsigned first_packet_follows;
};
/* Parse an SSH_MSG_KEXINIT payload, not a packet envelope. Views borrow input
 * until the backing buffer is changed. Output is unchanged on failure. */
int holly_ssh_kex_parse(const uint8_t *data,size_t bytes,struct holly_ssh_kexinit *out);
/* Cookie must come from a validated random source in a real transport.
 * No default algorithms are advertised. Output must not alias input views. */
int holly_ssh_kex_encode(const struct holly_ssh_kexinit *proposal,
                         uint8_t *output,size_t capacity,size_t *written);
/* 1 first mutual name in client preference order, 0 no match, -1 invalid.
 * This is name matching only: caller must check key capabilities, algorithm
 * availability and policy before choosing a complete crypto suite. */
int holly_ssh_name_select(struct holly_ssh_names client,struct holly_ssh_names server,
                          struct holly_ssh_names *selected);
#endif
