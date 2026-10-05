#ifndef HOLLY_CLIENT_H
#define HOLLY_CLIENT_H
#include <stddef.h>
#include <stdint.h>
#define HOLLY_CLIENT_RX 65536u
#define HOLLY_CLIENT_TX 32768u
enum holly_client_phase {
  HC_IDLE,
  HC_ARP_DNS,
  HC_DNS,
  HC_ARP_TCP,
  HC_SYN,
  HC_OPEN,
  HC_FAILED
};
typedef int (*holly_client_send_fn)(const uint8_t *, unsigned, void *);
typedef int (*holly_client_random_fn)(uint8_t *, size_t, void *);
struct holly_client {
  enum holly_client_phase phase;
  uint8_t mac[6], ip[4], mask[4], router[4], dns[4], peer[4], hop[4],
      hop_mac[6];
  uint16_t local_port, port, dns_id, window, mss;
  uint32_t una, next, remote, elapsed, retry;
  unsigned retries, flight, eof;
  unsigned rx_head, rx_used, tx_head, tx_used, question_size;
  uint8_t rx[HOLLY_CLIENT_RX], tx[HOLLY_CLIENT_TX], question[260], frame[1514];
  holly_client_send_fn send;
  holly_client_random_fn random;
  void *context;
};
int holly_client_start(struct holly_client *, const uint8_t mac[6],
                       const uint8_t ip[4], const uint8_t mask[4],
                       const uint8_t router[4], const uint8_t dns[4],
                       const char *host, uint16_t port, holly_client_send_fn,
                       holly_client_random_fn, void *);
int holly_client_receive(struct holly_client *, const uint8_t *, unsigned);
int holly_client_tick(struct holly_client *, unsigned ms);
unsigned holly_client_write(struct holly_client *, const uint8_t *, unsigned);
unsigned holly_client_read(struct holly_client *, uint8_t *, unsigned);
void holly_client_abort(struct holly_client *);
#endif
