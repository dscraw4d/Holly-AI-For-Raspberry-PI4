#include "https.h"
static struct holly_https h;
int https_start(const char *host, uint64_t epoch, holly_https_rng_fn random,
                holly_https_io_fn write, holly_https_io_fn read) {
  return holly_https_start(&h, host,
                           "GET /test HTTP/1.1\r\nHost: "
                           "en.wikipedia.org\r\nConnection: close\r\n\r\n",
                           epoch, random, write, read, 0);
}
int https_start_ddg(const char*host,uint64_t now,holly_https_rng_fn random,holly_https_io_fn write,holly_https_io_fn read){int r=holly_https_start(&h,host,"GET /?q=Jupiter&format=json&no_html=1&skip_disambig=1&no_redirect=1&t=hollyos HTTP/1.1\r\nHost: api.duckduckgo.com\r\nAccept: application/json, application/x-javascript\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n",now,random,write,read,0);if(!r)h.http.accept_ddg=1;return r;}
int https_poll(void) { return holly_https_poll(&h); }
unsigned https_verified(void) { return h.verified; }
unsigned https_error(void) { return h.error; }
const char *https_body(void) { return h.http.body; }
void https_destroy(void) { holly_https_destroy(&h); }

int https_start_xml(const char *host,uint64_t epoch,holly_https_rng_fn random,holly_https_io_fn write,holly_https_io_fn read){
 int r=holly_https_start(&h,host,"GET /news/world/rss.xml HTTP/1.1\r\nHost: en.wikipedia.org\r\nConnection: close\r\n\r\n",epoch,random,write,read,0);if(!r)h.http.accept_xml=1;return r;
}
