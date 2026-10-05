#ifndef HOLLY_TCP_STREAM_H
#define HOLLY_TCP_STREAM_H
#include "ssh_server.h"
#define HOLLY_TCP_QUEUE 524288u /* bounded queue fits portrait atlas and HTTP responses */
typedef int (*holly_frame_write_fn)(const uint8_t *,unsigned,void *);
enum holly_stream_phase { HOLLY_STREAM_LISTEN,HOLLY_STREAM_SYN,HOLLY_STREAM_OPEN,HOLLY_STREAM_FIN };
struct holly_tcp_stream {
    enum holly_stream_phase phase;
    uint16_t local_port;
    int (*app_start)(holly_ssh_write_fn,void *,void *);
    int (*app_feed)(const uint8_t *,size_t,void *);
    void (*app_close)(void *);
    void *app_context;
    struct holly_ssh_server ssh;
    const struct holly_ssh_credentials *credentials;
    holly_ssh_random_fn random;
    holly_frame_write_fn transmit;
    void *context;
    uint8_t mac[6],ip[4],peer_mac[6],peer_ip[4];uint16_t peer_port,peer_window,mss;
    uint32_t una,next,remote,idle_ms,retry_ms;
    unsigned retries,flight,fin_requested,fin_sent,peer_fin;
    uint8_t queue[HOLLY_TCP_QUEUE];unsigned head,queued;
    uint8_t frame[1514];
};
void holly_tcp_stream_init(struct holly_tcp_stream *,const uint8_t mac[6],const uint8_t ip[4],
    const struct holly_ssh_credentials *,holly_ssh_random_fn,holly_frame_write_fn,void *);
int holly_tcp_stream_receive(struct holly_tcp_stream *,const uint8_t *,unsigned);
int holly_tcp_stream_tick(struct holly_tcp_stream *,uint32_t milliseconds);
/* Link loss: discard old TCP/SSH state so a fresh wired connection can log in. */
void holly_tcp_stream_link_lost(struct holly_tcp_stream *);
#endif
