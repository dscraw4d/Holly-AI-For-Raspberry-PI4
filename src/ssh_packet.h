#ifndef HOLLY_SSH_PACKET_H
#define HOLLY_SSH_PACKET_H
#include <stddef.h>
#include <stdint.h>
#define HOLLY_SSH_PACKET_MAX 35000u
#define HOLLY_SSH_PAYLOAD_MAX 32768u
/* Initial SSH transport only: no encryption, compression or MAC. Do not use
 * after NEWKEYS. Initialize before first use; do not alter state fields. Keep this 35 KB object out of a small interrupt stack. */
struct holly_ssh_plain_packet {
    uint8_t bytes[HOLLY_SSH_PACKET_MAX];
    unsigned used,total,payload_bytes;
    int complete,failed;
};
void holly_ssh_plain_init(struct holly_ssh_plain_packet *packet);
/* 0 incomplete, 1 complete, -1 rejected. consumed identifies the exact prefix
 * accepted, allowing TCP callers to retain subsequent coalesced packets.
 * A failure is sticky until init. Complete data remains valid until init. */
int holly_ssh_plain_feed(struct holly_ssh_plain_packet *packet,
                         const uint8_t *data,size_t bytes,size_t *consumed);
const uint8_t *holly_ssh_plain_payload(const struct holly_ssh_plain_packet *packet,
                                      size_t *bytes);
int holly_ssh_plain_size(size_t payload_bytes,size_t *total,unsigned *padding);
/* Supply at least the padding count from size(), preferably random bytes.
 * No deterministic padding fallback or random generator is provided.
 * Output must not overlap payload or padding. No output writes on rejection. */
int holly_ssh_plain_encode(const uint8_t *payload,size_t payload_bytes,
                           const uint8_t *padding,size_t padding_bytes,
                           uint8_t *output,size_t capacity,size_t *written);
#endif
