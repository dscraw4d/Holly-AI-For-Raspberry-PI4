#ifndef HOLLY_SEARCH_H
#define HOLLY_SEARCH_H
#include "holly.h"
typedef int (*holly_search_fetch_fn)(uint32_t,const char *,unsigned);
void holly_search_bind(holly_search_fetch_fn);
int holly_search_command(struct holly_session *,const char *,unsigned,holly_emit_fn,void *);
int holly_search_fallback(struct holly_session *,const char *,holly_emit_fn,void *);
int holly_search_pending(void);
void holly_search_tick(uint64_t);
void holly_search_fail(void);
int holly_search_continue(void);
int holly_search_wiki_parse(const char *,unsigned,char *,unsigned,char *,unsigned);
int holly_search_finish(const char *,unsigned);
void holly_search_poll(struct holly_session *,char *,unsigned);
/* Plain bounded topic summary only; no HTML, related-topic guess or training. */
int holly_search_parse(const char *,unsigned,char *,unsigned,char *,unsigned);
#endif
