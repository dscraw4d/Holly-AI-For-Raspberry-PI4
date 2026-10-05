#ifndef HOLLY_NEWS_H
#define HOLLY_NEWS_H
#include <stdint.h>
#include "holly.h"
typedef int (*holly_news_fetch_fn)(uint32_t);
void holly_news_bind(holly_news_fetch_fn);
int holly_news_command(const char *,holly_emit_fn,void *);
int holly_news_pending(void);
void holly_news_tick(uint64_t);
void holly_news_fail(void);
int holly_news_finish(const char *,unsigned);
/* poll output: pending|, ready|bulletin, error|message or idle| */
void holly_news_poll(char *,unsigned);
int holly_news_parse(const char *,unsigned,uint32_t,char *,unsigned);
#endif
