#include "client.h"
#include "net.h"
#include <string.h>
static unsigned r16(const uint8_t *p) { return (unsigned)p[0] * 256u + p[1]; }
static uint32_t r32(const uint8_t *p) {
  return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 |
         p[3];
}
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
static uint16_t sum(const uint8_t *ip, const uint8_t *p, unsigned n,
                    unsigned proto) {
  uint32_t v = proto + n;
  for (unsigned i = 12; i < 20; i += 2)
    v += r16(ip + i);
  while (n > 1) {
    v += r16(p);
    p += 2;
    n -= 2;
  }
  if (n)
    v += (unsigned)*p << 8;
  while (v >> 16)
    v = (v & 65535) + (v >> 16);
  return (uint16_t)~v;
}
static int failed(struct holly_client *c) {
  c->phase = HC_FAILED;
  return -1;
}
static void ip_header(struct holly_client *c, const uint8_t target[4],
                      unsigned proto, unsigned n) {
  uint8_t *f = c->frame, *ip = f + 14;
  memset(f, 0, 34 + n);
  memcpy(f, c->hop_mac, 6);
  memcpy(f + 6, c->mac, 6);
  w16(f + 12, 0x800);
  ip[0] = 0x45;
  w16(ip + 2, 20 + n);
  w16(ip + 6, 0x4000);
  ip[8] = 64;
  ip[9] = (uint8_t)proto;
  memcpy(ip + 12, c->ip, 4);
  memcpy(ip + 16, target, 4);
  w16(ip + 10, viper_checksum(ip, 20));
}
static int arp(struct holly_client *c) {
  uint8_t *f = c->frame;
  memset(f, 0, 42);
  memset(f, 255, 6);
  memcpy(f + 6, c->mac, 6);
  w16(f + 12, 0x806);
  w16(f + 14, 1);
  w16(f + 16, 0x800);
  f[18] = 6;
  f[19] = 4;
  w16(f + 20, 1);
  memcpy(f + 22, c->mac, 6);
  memcpy(f + 28, c->ip, 4);
  memcpy(f + 38, c->hop, 4);
  return c->send(f, 42, c->context);
}
static int route(struct holly_client *c, const uint8_t destination[4],
                 enum holly_client_phase phase) {
  unsigned local = 1;
  for (unsigned i = 0; i < 4; i++)
    if ((destination[i] & c->mask[i]) != (c->ip[i] & c->mask[i]))
      local = 0;
  memcpy(c->hop, local ? destination : c->router, 4);
  c->phase = phase;
  c->retry = 0;
  c->retries = 0;
  if (!c->hop[0] && !c->hop[1] && !c->hop[2] && !c->hop[3])
    return failed(c);
  return arp(c);
}
static int dns_send(struct holly_client *c) {
  unsigned n = 12 + c->question_size;
  ip_header(c, c->dns, 17, 8 + n);
  uint8_t *u = c->frame + 34, *d = u + 8;
  w16(u, c->local_port);
  w16(u + 2, 53);
  w16(u + 4, 8 + n);
  w16(d, c->dns_id);
  w16(d + 2, 0x100);
  w16(d + 4, 1);
  memcpy(d + 12, c->question, c->question_size);
  unsigned checksum = sum(c->frame + 14, u, 8 + n, 17);
  w16(u + 6, checksum ? checksum : 65535);
  return c->send(c->frame, 42 + n, c->context);
}
static int tcp_send(struct holly_client *c, unsigned flags, uint32_t seq,
                    unsigned bytes) {
  unsigned th = (flags & 2) ? 24 : 20;
  if (bytes > 1460 || bytes > c->tx_used)
    return -1;
  ip_header(c, c->peer, 6, th + bytes);
  uint8_t *t = c->frame + 34;
  w16(t, c->local_port);
  w16(t + 2, c->port);
  w32(t + 4, seq);
  w32(t + 8, c->remote);
  t[12] = (uint8_t)(th / 4 * 16);
  t[13] = (uint8_t)flags;
  unsigned space = HOLLY_CLIENT_RX - c->rx_used;
  w16(t + 14, space > 32768 ? 32768 : space);
  if (th == 24) {
    t[20] = 2;
    t[21] = 4;
    w16(t + 22, 1200);
  }
  for (unsigned i = 0; i < bytes; i++)
    t[th + i] = c->tx[(c->tx_head + i) & (HOLLY_CLIENT_TX - 1)];
  w16(t + 16, sum(c->frame + 14, t, th + bytes, 6));
  return c->send(c->frame, 34 + th + bytes, c->context);
}
static int pump(struct holly_client *c) {
  if (c->phase != HC_OPEN || c->flight || !c->tx_used || !c->window)
    return 0;
  unsigned n = c->tx_used;
  if (n > c->mss)
    n = c->mss;
  if (n > c->window)
    n = c->window;
  if (tcp_send(c, 0x18, c->next, n))
    return failed(c);
  c->flight = n;
  c->next += n;
  c->retry = 0;
  c->retries = 0;
  return 0;
}
int holly_client_start(struct holly_client *c, const uint8_t mac[6],
                       const uint8_t ip[4], const uint8_t mask[4],
                       const uint8_t router[4], const uint8_t dns[4],
                       const char *host, uint16_t port,
                       holly_client_send_fn send, holly_client_random_fn random,
                       void *context) {
  if (!c || !mac || !ip || !mask || !router || !dns || !host || !send ||
      !random || !port)
    return -1;
  memset(c, 0, sizeof(*c));
  memcpy(c->mac, mac, 6);
  memcpy(c->ip, ip, 4);
  memcpy(c->mask, mask, 4);
  memcpy(c->router, router, 4);
  memcpy(c->dns, dns, 4);
  c->send = send;
  c->random = random;
  c->context = context;
  c->port = port;
  c->mss = 536;
  uint8_t rnd[8];
  if (random(rnd, sizeof(rnd), context))
    return failed(c);
  c->local_port = (uint16_t)(49152 + (r16(rnd) & 16383));
  c->dns_id = (uint16_t)r16(rnd + 2);
  c->una = r32(rnd + 4);
  c->next = c->una + 1;
  unsigned out = 0;
  const char *p = host;
  while (*p) {
    unsigned n = 0;
    while (p[n] && p[n] != '.')
      n++;
    if (!n || n > 63 || out + n + 6 > sizeof(c->question))
      return failed(c);
    c->question[out++] = (uint8_t)n;
    for (unsigned i = 0; i < n; i++) {
      char x = p[i];
      if (!((x >= 'a' && x <= 'z') || (x >= '0' && x <= '9') || x == '-'))
        return failed(c);
      c->question[out++] = (uint8_t)x;
    }
    p += n;
    if (*p == '.')
      p++;
  }
  c->question[out++] = 0;
  w16(c->question + out, 1);
  w16(c->question + out + 2, 1);
  c->question_size = out + 4;
  return route(c, c->dns, HC_ARP_DNS);
}
static int skip_name(const uint8_t *d, unsigned n, unsigned *at) {
  for (unsigned loops = 0; loops < 128; loops++) {
    if (*at >= n)
      return -1;
    unsigned size = d[(*at)++];
    if (!size)
      return 0;
    if ((size & 192) == 192) {
      if (*at >= n)
        return -1;
      unsigned target = ((size & 63) << 8) | d[(*at)++];
      return target < n ? 0 : -1;
    }
    if (size > 63 || size > n - *at)
      return -1;
    *at += size;
  }
  return -1;
}
int holly_client_receive(struct holly_client *c, const uint8_t *f, unsigned n) {
  if (!c || !f || n < 14 || c->phase == HC_IDLE || c->phase == HC_FAILED)
    return 0;
  if (r16(f + 12) == 0x806) {
    if (c->phase != HC_ARP_DNS && c->phase != HC_ARP_TCP)
      return 0;
    if (n < 42 || memcmp(f, c->mac, 6) || r16(f + 14) != 1 ||
        r16(f + 16) != 0x800 || f[18] != 6 || f[19] != 4 || r16(f + 20) != 2 ||
        memcmp(f + 28, c->hop, 4) || memcmp(f + 38, c->ip, 4) ||
        memcmp(f + 32, c->mac, 6) || memcmp(f + 6, f + 22, 6))
      return 0;
    memcpy(c->hop_mac, f + 22, 6);
    c->retry = 0;
    c->retries = 0;
    if (c->phase == HC_ARP_DNS) {
      c->phase = HC_DNS;
      return dns_send(c);
    }
    c->phase = HC_SYN;
    c->flight = 1;
    return tcp_send(c, 2, c->una, 0);
  }
  struct viper_packet packet;
  if (viper_parse_packet(f, n, &packet) != 1 ||
      memcmp(packet.target_ip, c->ip, 4) || memcmp(f, c->mac, 6))
    return 0;
  const uint8_t *ip = f + 14;
  unsigned ihl = (ip[0] & 15) * 4u, total = r16(ip + 2);
  const uint8_t *t = ip + ihl;
  unsigned len = total - ihl;
  if (packet.protocol == 17 && c->phase == HC_DNS) {
    unsigned ulen = r16(t + 4);
    if (ulen != len || (r16(t + 6) && sum(ip, t, len, 17)) ||
        memcmp(packet.source_ip, c->dns, 4) || packet.source_port != 53 ||
        packet.target_port != c->local_port)
      return 0;
    const uint8_t *d = packet.payload;
    unsigned bytes = packet.payload_len;
    if (bytes < 12 + c->question_size || r16(d) != c->dns_id ||
        (r16(d + 2) & 0x820f) != 0x8000 || r16(d + 4) != 1 ||
        memcmp(d + 12, c->question, c->question_size))
      return 0;
    unsigned at = 12 + c->question_size, answers = r16(d + 6);
    if (answers > 64)
      return failed(c);
    for (unsigned i = 0; i < answers; i++) {
      if (skip_name(d, bytes, &at) || bytes - at < 10)
        return failed(c);
      unsigned type = r16(d + at), cl = r16(d + at + 2), size = r16(d + at + 8);
      at += 10;
      if (size > bytes - at)
        return failed(c);
      if (type == 1 && cl == 1 && size == 4) {
        memcpy(c->peer, d + at, 4);
        return route(c, c->peer, HC_ARP_TCP);
      }
      at += size;
    }
    return failed(c);
  }
  if (packet.protocol != 6 || (c->phase != HC_SYN && c->phase != HC_OPEN) ||
      memcmp(packet.source_ip, c->peer, 4) || packet.source_port != c->port ||
      packet.target_port != c->local_port || sum(ip, t, len, 6))
    return 0;
  unsigned th = (t[12] >> 4) * 4u, flags = t[13], payload = len - th;
  uint32_t seq = r32(t + 4), ack = r32(t + 8);
  if (flags & 4) {
    if ((c->phase == HC_SYN && (flags & 16) && ack == c->next) ||
        (c->phase == HC_OPEN && seq == c->remote))
      return failed(c);
    return 0;
  }
  if (c->phase == HC_SYN) {
    if ((flags & 0x17) != 0x12 || ack != c->next || payload)
      return 0;
    for (unsigned i = 20; i < th;) {
      unsigned kind = t[i++];
      if (!kind)
        break;
      if (kind == 1)
        continue;
      if (i >= th || t[i] < 2 || t[i] > th - i + 1)
        return failed(c);
      unsigned size = t[i++];
      if (kind == 2 && size == 4) {
        unsigned m = r16(t + i);
        if (!m)
          return failed(c);
        c->mss = (uint16_t)(m > 1200 ? 1200 : m);
      }
      i += size - 2;
    }
    c->remote = seq + 1;
    c->una = ack;
    c->flight = 0;
    c->window = (uint16_t)r16(t + 14);
    c->phase = HC_OPEN;
    c->retry = 0;
    c->retries = 0;
    return tcp_send(c, 16, c->next, 0);
  }
  if (!(flags & 16))
    return 0;
  if (flags & 2) {
    if (seq + 1 == c->remote && ack == c->una)
      return tcp_send(c, 16, c->next, 0);
    return 0;
  }
  unsigned advance = ack - c->una;
  if (advance > c->flight)
    return tcp_send(c, 16, c->next, 0);
  if (advance) {
    c->una = ack;
    c->flight -= advance;
    c->tx_head = (c->tx_head + advance) & (HOLLY_CLIENT_TX - 1);
    c->tx_used -= advance;
    c->retry = 0;
    c->retries = 0;
  }
  if (seq != c->remote)
    return tcp_send(c, 16, c->next, 0);
  c->window = (uint16_t)r16(t + 14);
  if (payload > HOLLY_CLIENT_RX - c->rx_used)
    return tcp_send(c, 16, c->next, 0);
  if (!c->eof)
    for (unsigned i = 0; i < payload; i++)
      c->rx[(c->rx_head + c->rx_used + i) & (HOLLY_CLIENT_RX - 1)] = t[th + i];
  if (!c->eof) {
    c->rx_used += payload;
    c->remote += payload;
  }
  if ((flags & 1) && !c->eof) {
    c->remote++;
    c->eof = 1;
  }
  if (payload || (flags & 1))
    if (tcp_send(c, 16, c->next, 0))
      return failed(c);
  return pump(c);
}
int holly_client_tick(struct holly_client *c, unsigned ms) {
  if (!c || c->phase == HC_IDLE || c->phase == HC_FAILED)
    return 0;
  if (ms > 120000u - c->elapsed)
    return failed(c);
  c->elapsed += ms;
  if (ms > 8000u - c->retry)
    c->retry = 8000;
  else
    c->retry += ms;
  unsigned delay = 1000u << (c->retries > 3 ? 3 : c->retries);
  if (c->retry >= delay) {
    if (c->phase != HC_OPEN || c->flight) {
      if (c->retries >= 6)
        return failed(c);
      c->retry = 0;
      c->retries++;
      if (c->phase == HC_ARP_DNS || c->phase == HC_ARP_TCP)
        return arp(c);
      if (c->phase == HC_DNS)
        return dns_send(c);
      if (c->phase == HC_SYN)
        return tcp_send(c, 2, c->una, 0);
      return tcp_send(c, 0x18, c->una, c->flight);
    }
    if (c->tx_used && !c->window) {
      if (tcp_send(c, 0x18, c->next, 1))
        return failed(c);
      c->flight = 1;
      c->next++;
      c->retry = 0;
    }
  }
  return pump(c);
}
unsigned holly_client_write(struct holly_client *c, const uint8_t *p,
                            unsigned n) {
  if (!c || !p || c->phase != HC_OPEN || c->eof)
    return 0;
  if (n > HOLLY_CLIENT_TX - c->tx_used)
    n = HOLLY_CLIENT_TX - c->tx_used;
  for (unsigned i = 0; i < n; i++)
    c->tx[(c->tx_head + c->tx_used + i) & (HOLLY_CLIENT_TX - 1)] = p[i];
  c->tx_used += n;
  (void)pump(c);
  return n;
}
unsigned holly_client_read(struct holly_client *c, uint8_t *p, unsigned n) {
  if (!c || !p || c->phase != HC_OPEN)
    return 0;
  if (n > c->rx_used)
    n = c->rx_used;
  for (unsigned i = 0; i < n; i++)
    p[i] = c->rx[(c->rx_head + i) & (HOLLY_CLIENT_RX - 1)];
  c->rx_head = (c->rx_head + n) & (HOLLY_CLIENT_RX - 1);
  c->rx_used -= n;
  if (n)
    (void)tcp_send(c, 16, c->next, 0);
  return n;
}
void holly_client_abort(struct holly_client *c) {
  if (!c)
    return;
  if (c->phase == HC_OPEN)
    (void)tcp_send(c, 0x14, c->next, 0);
  c->phase = HC_IDLE;
  c->rx_used = 0;
  c->tx_used = 0;
}
