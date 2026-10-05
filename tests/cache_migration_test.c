#include "search_cache.h"
#include "search_disk.h"
#include "documents.h"
#include "vault.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SECTORS 262144u
static FILE*media;static int fail=-1,writes;static struct holly_vault vault;static struct holly_documents docs;
static int rd(uint32_t n,uint8_t*b,void*c){(void)c;assert(n<SECTORS);if(fseek(media,(long)n*512,SEEK_SET))return -1;unsigned got=(unsigned)fread(b,1,512,media);if(ferror(media))return -1;memset(b+got,0,512-got);clearerr(media);return 0;}
static int wr(uint32_t n,const uint8_t*b,void*c){(void)c;assert(n<SECTORS);if(fail>=0&&writes++>=fail)return -1;if(fseek(media,(long)n*512,SEEK_SET))return -1;return fwrite(b,1,512,media)==512?0:-1;}
static int add(const char*t,const char*s,unsigned c,uint32_t*id,void*v){return holly_vault_memory_add(v,t,s,c,id);}
static unsigned count(void*v){return holly_vault_memory_count(v);}
static int get(uint32_t id,struct holly_memory_item*i,void*v){return holly_vault_memory_get(v,id,i);}
static int at(unsigned n,struct holly_memory_item*i,void*v){return holly_vault_memory_get_at(v,n,i);}
static int update(uint32_t id,const char*t,const char*s,unsigned c,void*v){return holly_vault_memory_update(v,id,t,s,c);}
static int forget(uint32_t id,void*v){return holly_vault_memory_forget(v,id);}
static struct holly_memory_ops ops={add,count,get,at,update,forget};static struct holly_block_ops io={rd,wr,0};static char output[2048];
static void emit(const char*p,void*c){(void)c;strncat(output,p,sizeof output-strlen(output)-1);}
int main(void){media=tmpfile();assert(media);assert(!holly_vault_mount(&vault,&io,0,SECTORS));struct lesson saved[MIND_SLOTS];unsigned n;assert(!holly_vault_load(&vault,saved,&n));assert(!holly_vault_memory_start(&vault,saved,n));holly_search_cache_bind(&ops,&vault);uint32_t user;assert(!add("My name is Darren","user",100,&user,&vault));assert(!holly_search_cache_save("jupiter","A saved planet summary.","https://example.org/jupiter",1791176400));assert(count(&vault)>1);
 assert(!holly_documents_format(&docs,&io,8192,200000));assert(docs.slots==2);/* Simulate a live uploaded document catalogue in slot one. */
 const char*cmd="doc begin 1 559aead08264d5795d3909718cdd05abd49572e84fe55590eef31a88a08fdffd User notes";output[0]=0;assert(!holly_documents_command(&docs,cmd,emit,0));assert(docs.doc[0].state==1);
 struct holly_document original=docs.doc[0];struct holly_block_ops disk_io;uint32_t first,span;assert(!holly_documents_web_area(&docs,&disk_io,&first,&span));assert(!memcmp(&original,&docs.doc[0],sizeof original));assert(first>=8192+4097+8192*9);assert(!holly_search_disk_bind(&disk_io,first,span));
 fail=0;writes=0;assert(holly_search_cache_bind_disk(&disk_io,first,span)<0);fail=-1;char text[1201],source[513];uint32_t epoch;assert(holly_search_cache_get("jupiter",text,source,&epoch)&&strstr(text,"planet"));assert(count(&vault)>1);assert(!holly_search_cache_bind_disk(&disk_io,first,span));assert(holly_search_cache_capacity()==256&&holly_search_cache_count()==1&&count(&vault)==1);
 struct holly_memory_item item;assert(!get(user,&item,&vault)&&strstr(item.text,"Darren"));assert(!holly_documents_mount_span(&docs,&io,8192,200000));assert(!memcmp(&original,&docs.doc[0],sizeof original));uint32_t again,again_span;assert(!holly_documents_web_area(&docs,&disk_io,&again,&again_span)&&again==first&&again_span==span);
 output[0]=0;assert(!holly_documents_command(&docs,"doc delete 2",emit,0)&&strstr(output,"ERR"));assert(docs.ready&&docs.doc[1].state==2);output[0]=0;assert(!holly_documents_command(&docs,"doc show 2 0",emit,0)&&strstr(output,"ERR"));assert(docs.ready);output[0]=0;assert(!holly_documents_command(&docs,"doc list",emit,0));assert(strstr(output,"User notes")&&!strstr(output,"Internal"));
 assert(!holly_search_cache_forget("jupiter"));holly_search_cache_bind(&ops,&vault);assert(!holly_search_cache_bind_disk(&disk_io,first,span));assert(!holly_search_cache_get("jupiter",text,source,&epoch));assert(!get(user,&item,&vault));fclose(media);puts("Migration: reserved free document slot, upload/user preservation, failed-copy retention, verified migration, legacy cleanup, internal guards and durable forget passed.");}
