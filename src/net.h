#ifndef VIPER_NET_H
#define VIPER_NET_H
#include <stdint.h>
struct viper_packet {
    uint8_t source_mac[6], target_mac[6];
    uint8_t source_ip[4], target_ip[4];
    uint16_t source_port, target_port;
    const uint8_t *payload;
    uint16_t payload_len;
    uint8_t protocol;
};
/* 1 = valid IPv4 TCP/UDP, 0 = unsupported, -1 = malformed */
int viper_parse_packet(const uint8_t *data, unsigned size, struct viper_packet *out);
uint16_t viper_checksum(const uint8_t *data, unsigned size);
#endif
