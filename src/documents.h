#ifndef HOLLY_DOCUMENTS_H
#define HOLLY_DOCUMENTS_H
#include "holly.h"
#include "vault.h"
#define HOLLY_DOC_MAX 4096u
#define HOLLY_DOC_BYTES (32u*1024u*1024u)
#define HOLLY_DOC_CHUNK 4096u
#define HOLLY_DOC_CATALOG 4097u
struct holly_document {unsigned state,size,received,reading_page,reading_lines,reading_qa,reading_kind,reading_match;char title[81],episode[81];uint8_t hash[32],bloom[64],tail[128];unsigned tail_size;};
struct holly_doc_index {uint32_t lba;uint8_t bloom[256];};
struct holly_doc_text_cache {unsigned valid,id,page,n;char text[4097],episode[81];};
struct holly_documents {struct holly_block_ops io;uint32_t first,sectors,pages,slots;unsigned reading_paused,reading_cursor,reading_error,ready,cache_id,cache_page,cache_valid,index_reads,data_reads,index_hits,skipped,limited,text_hits;char cache[4097],episode[81],work[4225];struct holly_document doc[HOLLY_DOC_MAX];struct holly_doc_index index[1024];struct holly_doc_text_cache text_cache[8];};
int holly_documents_mount(struct holly_documents *,const struct holly_block_ops *,uint32_t);
int holly_documents_mount_span(struct holly_documents *,const struct holly_block_ops *,uint32_t,uint32_t);
int holly_documents_format(struct holly_documents *,const struct holly_block_ops *,uint32_t,uint32_t);
int holly_documents_command(struct holly_documents *,const char *,holly_emit_fn,void *);
int holly_documents_search(struct holly_documents *,const char *,holly_emit_fn,void *);
int holly_documents_script_ask(struct holly_documents *,const char *,holly_emit_fn,void *);
void holly_documents_visit(holly_reference_offer_fn,void *,void *);
void holly_documents_visit_query(holly_reference_offer_fn,void *,void *,const char *);
void holly_documents_read_step(struct holly_documents *);
int holly_documents_chat(struct holly_documents *,struct holly_session *,const char *,unsigned,holly_emit_fn,void *);
int holly_documents_web_area(struct holly_documents *,struct holly_block_ops *,uint32_t *,uint32_t *);
#endif
