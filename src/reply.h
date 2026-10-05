#ifndef VIPER_REPLY_H
#define VIPER_REPLY_H
#include <stdint.h>
/* Generates an ARP reply or IPv4 ICMP echo reply for one configured address.
   Returns reply bytes, zero when no reply is needed, or -1 for bad input. */
int viper_network_reply(const uint8_t *incoming,unsigned input_size,
                        const uint8_t mac[6],const uint8_t ip[4],
                        uint8_t *out,unsigned capacity);
#endif
