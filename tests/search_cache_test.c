#include "search.h"
#include "search_cache.h"
#include "vault.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SECTORS 4096u
static uint8_t disk[SECTORS*512];static struct holly_vault vault;static int fail_after=-1,writes,calls;static char output[4096];static unsigned used;
static int read_disk(uint32_t lba,uint8_t*b,void*c){(void)c;if(lba>=SECTORS)return -1;memcpy(b,disk+lba*512,512);return 0;}
static int write_disk(uint32_t lba,const uint8_t*b,void*c){(void)c;if(lba>=SECTORS||(fail_after>=0&&writes++>=fail_after))return -1;memcpy(disk+lba*512,b,512);return 0;}
static int add(const char*t,const char*s,unsigned conf,uint32_t*id,void*c){return holly_vault_memory_add(c,t,s,conf,id);}
static unsigned count(void*c){return holly_vault_memory_count(c);}
static int get(uint32_t id,struct holly_memory_item*i,void*c){return holly_vault_memory_get(c,id,i);}
static int get_at(unsigned n,struct holly_memory_item*i,void*c){return holly_vault_memory_get_at(c,n,i);}
static int update(uint32_t id,const char*t,const char*s,unsigned conf,void*c){return holly_vault_memory_update(c,id,t,s,conf);}
static int forget(uint32_t id,void*c){return holly_vault_memory_forget(c,id);}
static const struct holly_memory_ops ops={add,count,get,get_at,update,forget};
static const struct holly_block_ops io={read_disk,write_disk,0};
static void reboot(void){memset(&vault,0,sizeof vault);assert(!holly_vault_mount(&vault,&io,8,SECTORS-8));struct lesson saved[MIND_SLOTS];unsigned n;assert(!holly_vault_load(&vault,saved,&n));assert(!holly_vault_memory_start(&vault,saved,n));holly_set_memory(&ops,&vault);holly_search_bind(0);}
static void emit(const char*p,void*c){(void)c;while(*p&&used+1<sizeof output)output[used++]=*p++;output[used]=0;}
static int fetch(uint32_t t,const char*q,unsigned provider){(void)provider;assert(t&&*q);calls++;return 0;}
static void ask(struct holly_session*s,const char*q,const char*e,int guest){used=0;output[0]=0;assert(!(guest?holly_conversation_only(s,q,emit,0):holly_turn(s,q,emit,0)));if(!strstr(output,e)){fprintf(stderr,"%s expected %s got %s\n",q,e,output);assert(0);}assert(!strstr(output,"unverified")&&!strstr(output,"From my saved web references"));if(strstr(output,"DuckDuckGo")){fprintf(stderr,"Provider leak: %s: %s\n",q,output);assert(0);}}
static const char*article="{\"Type\":\"A\",\"AbstractText\":\"A fictional summary for the cache integration test.\",\"AbstractURL\":\"https://example.org/cache\"}";
int main(void){
 memset(disk,0,sizeof disk);reboot();assert(holly_search_cache_available());assert(!holly_search_cache_count());
 uint32_t personal;assert(!add("My name is Darren","user",100,&personal,&vault));
 char text[1201],source[513];uint32_t date;char longtext[1201],url[513];memset(longtext,'a',1200);longtext[1200]=0;memset(url,'b',512);memcpy(url,"https://",8);url[512]=0;
 assert(!holly_search_cache_save("jupiter",longtext,url,1791176400));assert(holly_search_cache_get("jupiter",text,source,&date));assert(!strcmp(text,longtext)&&!strcmp(source,url)&&date==1791176400);unsigned before=count(&vault);assert(before==9); // 7 chunks + manifest + user
 reboot();assert(holly_search_cache_count()==1&&holly_search_cache_get("jupiter",text,source,&date));assert(!strcmp(text,longtext));
 struct holly_memory_item item;assert(!get(personal,&item,&vault)&&!strcmp(item.text,"My name is Darren"));
 struct holly_session a,b;holly_session_init(&a);holly_session_init(&b);
 assert(!holly_search_cache_save("legacy wiki","Article overview: A planet reference.","https://en.wikipedia.org/wiki/Planet",1791176400));
 ask(&b,"legacy wiki","A planet reference",1);assert(!strcmp(output,"Holly: A planet reference.\n"));assert(!holly_search_cache_forget("legacy wiki"));
 ask(&a,"what is Jupiter?","aaaa",1);assert(!calls);ask(&a,"search source","2026-10-05 05:00 UTC",1);ask(&a,"search status","aaaa",1);
 ask(&b,"search forget jupiter","requires SSH",1);assert(holly_search_cache_count()==1);
 ask(&a,"search off","network lookup off",0);ask(&a,"Tell me about JUPITER!","aaaa",0);assert(!calls);
 // A failed refresh must retain the old complete entry across remount.
 for(int point=0;point<8;point++){
  fail_after=point;writes=0;assert(holly_search_cache_save("jupiter",longtext,url,1791176500)==-1);fail_after=-1;reboot();assert(holly_search_cache_get("jupiter",text,source,&date));assert(!strcmp(text,longtext)&&date==1791176400);
 }
 assert(!holly_search_cache_save("jupiter","Replacement reference","https://example.org/new",1791176500));assert(holly_search_cache_count()==1);reboot();assert(holly_search_cache_get("jupiter",text,source,&date));assert(!strcmp(text,"Replacement reference")&&date==1791176500);
 holly_search_bind(fetch);assert(!holly_clock_seed(1791176400,0,1));ask(&a,"search refresh jupiter","searching the Junior Encyclopedia of Space",0);assert(calls==1);assert(!holly_search_finish(article,strlen(article)));ask(&a,"search status","fictional summary",0);
 reboot();holly_session_init(&a);holly_session_init(&b);ask(&b,"explain Jupiter","fictional summary",1);assert(calls==1);
 ask(&b,"what do you know about Jupiter?","fictional summary",1);ask(&b,"can you tell me about Jupiter?","fictional summary",1);assert(calls==1);ask(&b,"search source","https://example.org/cache",1);ask(&b,"tell me about rimmer","Arnold Rimmer",1);ask(&b,"search status","fictional summary",1);
 // Cache hits must not overwrite another session's pending search.
 holly_search_bind(fetch);ask(&a,"search another subject","searching the Junior Encyclopedia of Space",1);uint32_t job=a.search_job;
 ask(&b,"search Jupiter","fictional summary",1);assert(holly_search_pending()&&a.search_job==job&&calls==2);assert(!holly_search_finish(article,strlen(article)));
 ask(&a,"search status","fictional summary",1);assert(holly_search_cache_count()==2);
 ask(&a,"search forget Jupiter","forgotten",0);assert(!holly_search_cache_get("jupiter",text,source,&date));reboot();assert(!holly_search_cache_get("jupiter",text,source,&date));assert(!get(personal,&item,&vault));
 // Fill the bounded bank. Never silently evict a saved topic.
 for(unsigned n=0;holly_search_cache_count()<HOLLY_SEARCH_CACHE_LIMIT;n++){char q[40];snprintf(q,sizeof q,"test topic %u",n);assert(!holly_search_cache_save(q,"Reference body","https://example.org/test",1791176400+n));}
 assert(holly_search_cache_save("one too many","No eviction","https://example.org/test",1791176400)==-1);assert(holly_search_cache_count()==64);reboot();assert(holly_search_cache_count()==64);assert(!get(personal,&item,&vault));
 // Content mutation invalidates a manifest instead of yielding mixed answers.
 unsigned chunk=0;for(unsigned n=0;n<count(&vault);n++){assert(!get_at(n,&item,&vault));if(!strcmp(item.source,"ddg chunk")){chunk=item.id;break;}}
 assert(chunk);assert(!update(chunk,"Corrupted content","ddg chunk",0,&vault));reboot();assert(holly_search_cache_count()==63);
 holly_set_memory(0,0);assert(!holly_search_cache_available());assert(!holly_search_cache_get("test topic 0",text,source,&date));
 puts("PASS: real Vault-backed full summary/source/date reboot recall, offline guest recall, local priority, refresh, partial-write rejection, user-record preservation, bounded no-eviction capacity, corruption refusal, pending-session isolation and SSH-only forget");return 0;
}
