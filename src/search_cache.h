#ifndef HOLLY_SEARCH_CACHE_H
#define HOLLY_SEARCH_CACHE_H
#include "holly.h"
#include "vault.h"
#define HOLLY_SEARCH_CACHE_LIMIT 64u
void holly_search_cache_bind(const struct holly_memory_ops *,void *);
int holly_search_cache_available(void);
unsigned holly_search_cache_count(void);
unsigned holly_search_cache_capacity(void);
int holly_search_cache_bind_disk(const struct holly_block_ops *,uint32_t,uint32_t);
int holly_search_cache_get(const char *,char [1201],char [513],uint32_t *);
int holly_search_cache_save(const char *,const char *,const char *,uint32_t);
int holly_search_cache_forget(const char *);
void holly_search_cache_date(uint32_t,char out[21]);
#endif
