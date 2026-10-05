#ifndef HOLLY_HTTPS_H
#define HOLLY_HTTPS_H
#include "bearssl.h"
#include "web_format.h"
#include <stdint.h>
typedef int (*holly_https_io_fn)(uint8_t *, unsigned, void *);
typedef int (*holly_https_rng_fn)(uint8_t *, size_t, void *);
struct holly_https {
  br_ssl_client_context tls;
  br_x509_minimal_context x509;
  uint8_t buffer[BR_SSL_BUFSIZE_BIDI];
  struct holly_http http;
  char request[1280];
  unsigned sent, request_size, verified, finished, error;
  holly_https_io_fn write, read;
  void *context;
};
int holly_https_start(struct holly_https *, const char *host,
                      const char *request, uint64_t unix_time,
                      holly_https_rng_fn, holly_https_io_fn write,
                      holly_https_io_fn read, void *);
/* 1 complete authenticated HTTP response, 0 pending, -1 error. */
int holly_https_poll(struct holly_https *);
void holly_https_destroy(struct holly_https *);
#endif
