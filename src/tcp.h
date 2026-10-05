#ifndef HOLLY_TCP_H
#define HOLLY_TCP_H
#include <stdint.h>
#include "ssh_ident.h"
/* A single, bounded diagnostic connection on port 2222. No SSH data or shell. */
enum holly_tcp_phase { HOLLY_TCP_CLOSED, HOLLY_TCP_SYN_RECEIVED, HOLLY_TCP_ESTABLISHED };
struct holly_tcp {
    enum holly_tcp_phase phase;
    uint8_t peer_mac[6], peer_ip[4];
    uint16_t peer_port;
    uint32_t next_local, next_remote;
    uint32_t elapsed_ms;
    unsigned retries;
    struct holly_ssh_ident client_ident;
};
void holly_tcp_init(struct holly_tcp *connection);
/* 0 ignored, positive reply length, -1 malformed. */
int holly_tcp_receive(struct holly_tcp *connection,const uint8_t *frame,unsigned length,
    const uint8_t mac[6],const uint8_t ip[4],uint8_t *reply,unsigned capacity);
/* Call with elapsed monotonic milliseconds. Emits at most one SYN-ACK retry per call. */
int holly_tcp_tick(struct holly_tcp *connection,uint32_t elapsed_ms,
    const uint8_t mac[6],const uint8_t ip[4],uint8_t *reply,unsigned capacity);
#endif
