#ifndef HOLLY_LORE_H
#define HOLLY_LORE_H
#include <stddef.h>
struct holly_lore { unsigned enabled; int topic; unsigned page; };
void holly_lore_init(struct holly_lore *);
void holly_lore_clear(struct holly_lore *);
unsigned holly_lore_count(void);
/* Returns 1 for a handled reply, 0 outside this domain, -1 for invalid/bounded input.
 * Every output is NUL-terminated; caller supplies at least 257 bytes.
 * Only explicit dwarf commands change enabled. All state is per session. */
int holly_lore_reply(struct holly_lore *,const char *,char *,size_t);
const char *holly_lore_topic(const struct holly_lore *);
#endif
/* Read-only passages for question retrieval. Pointers remain valid for kernel lifetime. */
int holly_lore_passage(unsigned topic,unsigned page,const char **title,const char **aliases,const char **text,const char **source);
