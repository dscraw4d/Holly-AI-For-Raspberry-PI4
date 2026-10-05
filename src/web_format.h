#ifndef HOLLY_WEB_FORMAT_H
#define HOLLY_WEB_FORMAT_H
#include <stddef.h>
#define HOLLY_HTTP_LIMIT 40000u
struct holly_http {
  char raw[HOLLY_HTTP_LIMIT + 1], body[32769];
  unsigned used, body_size, accept_xml, accept_ddg;
};
/* 1 complete, 0 incomplete, -1 malformed/unsupported. No partial imports. */
int holly_http_feed(struct holly_http *, const void *, unsigned);
int holly_wiki_excerpt(const char *, unsigned, char excerpt[176]);
#endif
