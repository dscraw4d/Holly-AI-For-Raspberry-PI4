#include "https.h"
#include <string.h>
#ifdef HOLLY_HTTPS_TEST_ANCHORS
#include "https_test_anchors.h"
#else
#include "https_anchors.h"
#endif
static const uint16_t suites[] = {
    BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
    BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
    BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
    BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256};
int holly_https_start(struct holly_https *h, const char *host,
                      const char *request, uint64_t unix_time,
                      holly_https_rng_fn random, holly_https_io_fn write,
                      holly_https_io_fn read, void *context) {
  if (!h || !host || !request || !random || !write || !read ||
      unix_time < 1577836800ull || unix_time > 4102444800ull)
    return -1;
  unsigned size = 0;
  while (request[size] && size < sizeof(h->request))
    size++;
  if (size == sizeof(h->request))
    return -1;
  memset(h, 0, sizeof(*h));
  memcpy(h->request, request, size + 1);
  h->request_size = size;
  h->write = write;
  h->read = read;
  h->context = context;
  br_ssl_client_init_full(&h->tls, &h->x509, TAs, TAs_NUM);
  br_ssl_engine_set_versions(&h->tls.eng, BR_TLS12, BR_TLS12);
  br_ssl_engine_set_suites(&h->tls.eng, suites,
                           sizeof(suites) / sizeof(suites[0]));
  br_ssl_engine_add_flags(&h->tls.eng, BR_OPT_NO_RENEGOTIATION);
  br_x509_minimal_set_minrsa(&h->x509, 256);
  br_x509_minimal_set_time(&h->x509, (uint32_t)(unix_time / 86400 + 719528),
                           (uint32_t)(unix_time % 86400));
  br_ssl_engine_set_buffer(&h->tls.eng, h->buffer, sizeof(h->buffer), 1);
  uint8_t entropy[64];
  if (random(entropy, sizeof(entropy), context)) {
    volatile uint8_t *wipe = entropy;
    for (unsigned i = 0; i < sizeof(entropy); i++)
      wipe[i] = 0;
    return -1;
  }
  br_ssl_engine_inject_entropy(&h->tls.eng, entropy, sizeof(entropy));
  volatile uint8_t *wipe = entropy;
  for (unsigned i = 0; i < sizeof(entropy); i++)
    wipe[i] = 0;
  if (!br_ssl_client_reset(&h->tls, host, 0))
    return -1;
  return 0;
}
int holly_https_poll(struct holly_https *h) {
  if (!h || h->error)
    return -1;
  if (h->finished)
    return 1;
  for (unsigned step = 0; step < 32; step++) {
    unsigned state = br_ssl_engine_current_state(&h->tls.eng), progress = 0;
    size_t size = 0;
    unsigned char *p;
    if (state & BR_SSL_CLOSED) {
      h->error = (unsigned)br_ssl_engine_last_error(&h->tls.eng);
      if (!h->error)
        h->error = 1000;
      return -1;
    }
    if (state & BR_SSL_SENDREC) {
      p = br_ssl_engine_sendrec_buf(&h->tls.eng, &size);
      int count = h->write(p, (unsigned)size, h->context);
      if (count < 0 || (unsigned)count > size) {
        h->error = 1001;
        return -1;
      }
      if (count) {
        br_ssl_engine_sendrec_ack(&h->tls.eng, (size_t)count);
        progress = 1;
      }
    }
    state = br_ssl_engine_current_state(&h->tls.eng);
    if (state & BR_SSL_RECVREC) {
      p = br_ssl_engine_recvrec_buf(&h->tls.eng, &size);
      int count = h->read(p, (unsigned)size, h->context);
      if (count < 0 || (unsigned)count > size) {
        h->error = 1002;
        return -1;
      }
      if (count) {
        br_ssl_engine_recvrec_ack(&h->tls.eng, (size_t)count);
        progress = 1;
      }
    }
    state = br_ssl_engine_current_state(&h->tls.eng);
    if (state & BR_SSL_SENDAPP) {
      h->verified = 1;
      if (h->sent < h->request_size) {
        p = br_ssl_engine_sendapp_buf(&h->tls.eng, &size);
        unsigned remain = h->request_size - h->sent;
        if (size > remain)
          size = remain;
        memcpy(p, h->request + h->sent, size);
        h->sent += (unsigned)size;
        br_ssl_engine_sendapp_ack(&h->tls.eng, size);
        progress = 1;
        if (h->sent == h->request_size)
          br_ssl_engine_flush(&h->tls.eng, 0);
      }
    }
    state = br_ssl_engine_current_state(&h->tls.eng);
    if (state & BR_SSL_RECVAPP) {
      p = br_ssl_engine_recvapp_buf(&h->tls.eng, &size);
      int result = holly_http_feed(&h->http, p, (unsigned)size);
      br_ssl_engine_recvapp_ack(&h->tls.eng, size);
      progress = 1;
      if (result < 0 || !h->verified) {
        h->error = 1003;
        return -1;
      }
      if (result == 1) {
        h->finished = 1;
        return 1;
      }
    }
    if (!progress)
      break;
  }
  return 0;
}
void holly_https_destroy(struct holly_https *h) {
  if (!h)
    return;
  volatile uint8_t *p = (volatile uint8_t *)h;
  for (size_t i = 0; i < sizeof(*h); i++)
    p[i] = 0;
}
