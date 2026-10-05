#ifndef HOLLY_REFERENCE_H
#define HOLLY_REFERENCE_H
#include <stddef.h>
#define HOLLY_REFERENCE_REPLY 1800
struct holly_reference { unsigned enabled,dry_voice,banter_state,banter_last; int topic; unsigned last_key,seen_count,seen[16]; char subject[81]; char answer[HOLLY_REFERENCE_REPLY]; char sources[512]; };
typedef void (*holly_reference_offer_fn)(const char *text,unsigned size,const char *title,const char *source,unsigned key,void *ctx);
typedef void (*holly_reference_documents_fn)(holly_reference_offer_fn,void *visitor,void *storage,const char *query);
void holly_reference_set_documents(holly_reference_documents_fn,void *);
void holly_reference_banter(struct holly_reference *,const char *,char *,size_t);
void holly_reference_init(struct holly_reference *);
void holly_reference_clear(struct holly_reference *);
/* 1 handled, 2 unanswered reference query, 0 outside domain, -1 invalid. Extractive, not neural. */
int holly_reference_reply(struct holly_reference *,const char *,char *,size_t);
#endif
