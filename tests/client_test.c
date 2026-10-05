#include "client.h"
#include "net.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct holly_client c;
static uint8_t sent[1514];
static unsigned sent_size, sends;
static const uint8_t mac[6] = {2, 1, 2, 3, 4, 5},
                     gateway_mac[6] = {2, 6, 7, 8, 9, 10};
static const uint8_t ip[4] = {192, 168, 1, 20}, mask[4] = {255, 255, 255, 0},
                     gateway[4] = {192, 168, 1, 1}, dns[4] = {8, 8, 8, 8},
                     peer[4] = {203, 0, 113, 20};
static void w16(uint8_t *p, unsigned n) {
  p[0] = (uint8_t)(n >> 8);
  p[1] = (uint8_t)n;
}
static void w32(uint8_t *p, uint32_t n) {
  p[0] = (uint8_t)(n >> 24);
  p[1] = (uint8_t)(n >> 16);
  p[2] = (uint8_t)(n >> 8);
  p[3] = (uint8_t)n;
}
static unsigned r16(const uint8_t *p) { return (unsigned)p[0] * 256 + p[1]; }
static uint16_t checksum(const uint8_t *ip4, const uint8_t *p, unsigned n,
                         unsigned proto) {
  uint32_t sum = proto + n;
  for (unsigned i = 12; i < 20; i += 2)
    sum += r16(ip4 + i);
  while (n > 1) {
    sum += r16(p);
    p += 2;
    n -= 2;
  }
  if (n)
    sum += (unsigned)*p << 8;
  while (sum >> 16)
    sum = (sum & 65535) + (sum >> 16);
  return (uint16_t)~sum;
}
static int transmit(const uint8_t *f, unsigned n, void *ctx) {
  (void)ctx;
  assert(n <= sizeof(sent));
  memcpy(sent, f, n);
  sent_size = n;
  sends++;
  return 0;
}
static int random_bytes(uint8_t *p, size_t n, void *ctx) {
  (void)ctx;
  for (size_t i = 0; i < n; i++)
    p[i] = (uint8_t)(i + 1);
  return 0;
}
static void arp_reply(void) {
  uint8_t f[42] = {0};
  memcpy(f, mac, 6);
  memcpy(f + 6, gateway_mac, 6);
  w16(f + 12, 0x806);
  w16(f + 14, 1);
  w16(f + 16, 0x800);
  f[18] = 6;
  f[19] = 4;
  w16(f + 20, 2);
  memcpy(f + 22, gateway_mac, 6);
  memcpy(f + 28, gateway, 4);
  memcpy(f + 32, mac, 6);
  memcpy(f + 38, ip, 4);
  assert(!holly_client_receive(&c, f, sizeof(f)));
}
static void packet(uint8_t *f, unsigned proto, unsigned n,
                   const uint8_t source[4]) {
  memset(f, 0, 34 + n);
  memcpy(f, mac, 6);
  memcpy(f + 6, gateway_mac, 6);
  w16(f + 12, 0x800);
  f[14] = 0x45;
  w16(f + 16, 20 + n);
  f[22] = 64;
  f[23] = (uint8_t)proto;
  memcpy(f + 26, source, 4);
  memcpy(f + 30, ip, 4);
  w16(f + 24, viper_checksum(f + 14, 20));
}
static void tcp(unsigned flags, uint32_t seq, uint32_t ack, unsigned window,
                const char *payload) {
  uint8_t f[1514];
  unsigned n = payload ? (unsigned)strlen(payload) : 0;
  packet(f, 6, 20 + n, peer);
  uint8_t *t = f + 34;
  w16(t, 443);
  w16(t + 2, c.local_port);
  w32(t + 4, seq);
  w32(t + 8, ack);
  t[12] = 80;
  t[13] = (uint8_t)flags;
  w16(t + 14, window);
  if (n)
    memcpy(t + 20, payload, n);
  w16(t + 16, checksum(f + 14, t, 20 + n, 6));
  assert(!holly_client_receive(&c, f, 54 + n));
}
int main(void) {
  assert(!holly_client_start(&c, mac, ip, mask, gateway, dns,
                             "en.wikipedia.org", 443, transmit, random_bytes,
                             0));
  assert(c.phase == HC_ARP_DNS && sent_size == 42);
  unsigned before = sends;
  assert(!holly_client_tick(&c, 1000) && sends == before + 1);
  arp_reply();
  assert(c.phase == HC_DNS && sent[23] == 17);
  uint8_t f[600];
  unsigned n = 12 + c.question_size + 16;
  packet(f, 17, 8 + n, dns);
  uint8_t *u = f + 34, *d = u + 8;
  w16(u, 53);
  w16(u + 2, c.local_port);
  w16(u + 4, 8 + n);
  w16(d, c.dns_id);
  w16(d + 2, 0x8180);
  w16(d + 4, 1);
  w16(d + 6, 1);
  memcpy(d + 12, c.question, c.question_size);
  unsigned at = 12 + c.question_size;
  d[at] = 192;
  d[at + 1] = 12;
  w16(d + at + 2, 1);
  w16(d + at + 4, 1);
  w32(d + at + 6, 60);
  w16(d + at + 10, 4);
  memcpy(d + at + 12, peer, 4);
  w16(u + 6, checksum(f + 14, u, 8 + n, 17));
  d[0] ^= 1;
  assert(!holly_client_receive(&c, f, 42 + n) && c.phase == HC_DNS);
  d[0] ^= 1;
  w16(u + 6, 0);
  w16(u + 6, checksum(f + 14, u, 8 + n, 17));
  assert(!holly_client_receive(&c, f, 42 + n) && c.phase == HC_ARP_TCP);
  arp_reply();
  assert(c.phase == HC_SYN);
  tcp(0x12, 1000, c.next, 4096, 0);
  assert(c.phase == HC_OPEN && c.remote == 1001);
  assert(holly_client_write(&c, (const uint8_t *)"abcdefghij", 10) == 10 &&
         c.flight == 10);
  uint32_t old = c.una;
  tcp(16, c.remote, old + 3, 4096, 0);
  assert(c.flight == 7 && c.tx_used == 7);
  before = sends;
  assert(!holly_client_tick(&c, 1000) && sends == before + 1);
  assert(!memcmp(sent + 54, "defghij", 7));
  tcp(0x18, c.remote, c.next, 4096, "hello");
  assert(c.rx_used == 5 && !c.tx_used);
  tcp(0x18, c.remote - 5, c.next, 4096, "hello");
  assert(c.rx_used == 5);
  tcp(4, c.remote + 100, c.next, 4096, 0);
  assert(c.phase == HC_OPEN);
  char bytes[8] = {0};
  assert(holly_client_read(&c, (uint8_t *)bytes, 5) == 5 &&
         !strcmp(bytes, "hello"));
  tcp(16, c.remote, c.next, 0, 0);
  assert(holly_client_write(&c, (const uint8_t *)"x", 1) == 1 && !c.flight);
  assert(!holly_client_tick(&c, 1000) && c.flight == 1);
  tcp(16, c.remote, c.next, 4096, 0);
  assert(!c.tx_used);
  tcp(0x11, c.remote, c.next, 4096, 0);
  assert(c.eof);
  holly_client_abort(&c);
  assert(c.phase == HC_IDLE);
  assert(!holly_client_start(&c, mac, ip, mask, gateway, dns,
                             "en.wikipedia.org", 443, transmit, random_bytes,
                             0));
  assert(holly_client_tick(&c, 120001) == -1 && c.phase == HC_FAILED);
  puts("Outbound ARP/DNS/TCP: routing, DNS ID/checksum, partial ACK, loss, "
       "duplicate, reset, window and timeout checks passed");
}
