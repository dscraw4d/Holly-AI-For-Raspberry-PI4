#include "web_format.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct holly_http h;
int main(void) {
  const char *body =
      "{\"type\":\"standard\",\"extract\":\"Red Dwarf is a British sitcom. "
      "More here.\",\"nested\":{\"extract\":\"Ignore this\"}}";
  char response[1024];
  snprintf(response, sizeof(response),
           "HTTP/1.1 200 OK\r\nContent-Type: application/json; "
           "charset=utf-8\r\nContent-Length: %zu\r\n\r\n%s",
           strlen(body), body);
  memset(&h, 0, sizeof(h));
  for (unsigned i = 0; response[i]; i++)
    assert(holly_http_feed(&h, response + i, 1) == (response[i + 1] ? 0 : 1));
  char fact[176];
  assert(!holly_wiki_excerpt(h.body, h.body_size, fact));
  assert(!strcmp(fact, "Red Dwarf is a British sitcom."));
  memset(&h, 0, sizeof(h));
  snprintf(
      response, sizeof(response),
      "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nTransfer-Encoding: "
      "chunked\r\n\r\n%zx\r\n%s\r\n0\r\n\r\n",
      strlen(body), body);
  assert(holly_http_feed(&h, response, (unsigned)strlen(response)) == 1);
  assert(!strcmp(h.body, body));
  const char *bad[] = {
      "HTTP/1.1 301 Moved\r\nContent-Length: 0\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
      "1\r\nTransfer-Encoding: chunked\r\n\r\nx",
      "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
      "40000\r\n\r\n",
      "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "
      "0\r\n\r\n"};
  for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
    memset(&h, 0, sizeof(h));
    assert(holly_http_feed(&h, bad[i], (unsigned)strlen(bad[i])) == -1);
  }
  const char *bad_json[] = {
      "{\"type\":\"standard\",\"extract\":\"one\",}",
      "{\"type\":\"standard\",\"extract\":\"one\",\"x\":garbage}",
      "{\"type\":\"disambiguation\",\"extract\":\"Choice.\"}",
      "{\"type\":\"standard\",\"extract\":\"one\",\"extract\":\"two\"}",
      "{\"type\":\"standard\",\"extract\":\"one\"} trailing",
      "{\"type\":\"standard\",\"extract\":\"bad | pipe\"}"};
  for (unsigned i = 0; i < sizeof(bad_json) / sizeof(bad_json[0]); i++)
    assert(holly_wiki_excerpt(bad_json[i], (unsigned)strlen(bad_json[i]),
                              fact) == -1);
  const char *unicode = "{\"type\":\"standard\",\"extract\":\"The caf\xc3\xa9 "
                        "is open. More.\",\"x\":[true,null,-12.5e+2]}";
  assert(!holly_wiki_excerpt(unicode, (unsigned)strlen(unicode), fact));
  assert(!strcmp(fact, "The caf? is open."));
  puts("HTTP fragmentation/chunking, response limits, JSON source selection "
       "and rejection checks passed");
}
