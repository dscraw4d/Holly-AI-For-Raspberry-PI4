#ifndef HOLLY_SEARCH_DISK_H
#define HOLLY_SEARCH_DISK_H
#include "vault.h"
#define HOLLY_SEARCH_DISK_LIMIT 256u
#define HOLLY_SEARCH_DISK_SECTORS 2050u
int holly_search_disk_bind(const struct holly_block_ops *,uint32_t,uint32_t);
int holly_search_disk_ready(void);
unsigned holly_search_disk_count(void);
int holly_search_disk_get(const char *,char *,char *,uint32_t *);
int holly_search_disk_save(const char *,const char *,const char *,uint32_t);
int holly_search_disk_forget(const char *);
int holly_search_disk_known(const char *);
#endif
