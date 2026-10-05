#include "search.h"
#include "holly.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned provider,calls;static char encoded[577],out[1600];
static int fetch(uint32_t epoch,const char*q,unsigned p){assert(epoch);provider=p;calls++;strcpy(encoded,q);return 0;}
static void emit(const char*p,void*c){(void)c;(void)p;}
static const char*wiki="{\"batchcomplete\":true,\"query\":{\"pages\":[{\"ns\":0,\"title\":\"Black hole\",\"extract\":\"A region of spacetime.\\nLight cannot escape.\",\"fullurl\":\"https://en.wikipedia.org/wiki/Black_hole\"}]}}";
int main(int argc,char**argv){char text[1201],url[513];
 if(argc==2){FILE*f=fopen(argv[1],"rb");assert(f);char b[32769];unsigned n=(unsigned)fread(b,1,sizeof b,f);fclose(f);assert(holly_search_wiki_parse(b,n,text,sizeof text,url,sizeof url)==1);assert(strlen(text)>100);puts("Live Wikipedia API response parsed with bounded article overview and source.");return 0;}
 assert(holly_search_wiki_parse(wiki,strlen(wiki),text,sizeof text,url,sizeof url)==1&&!strstr(text,"Article overview: ")&&strstr(text,"spacetime. Light"));
 const char*bad[]={"{\"query\":{\"pages\":[{\"ns\":0,\"title\":\"x\",\"extract\":\"x\",\"fullurl\":\"https://evil.org/wiki/X\"}]}}","{\"query\":{\"pages\":[{\"ns\":0,\"title\":\"x\",\"extract\":\"<script>bad</script>\",\"fullurl\":\"https://en.wikipedia.org/wiki/X\"}]}}","{\"query\":{\"pages\":[{\"ns\":0,\"extract\":\"x\",\"extract\":\"dup\"}]}}","{\"query\":{\"pages\":[{},{}]}}"};for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++)assert(holly_search_wiki_parse(bad[i],strlen(bad[i]),text,sizeof text,url,sizeof url)<0);
 const char*disamb="{\"query\":{\"pages\":[{\"ns\":0,\"title\":\"Cat\",\"extract\":\"Ambiguous\",\"fullurl\":\"https://en.wikipedia.org/wiki/Cat\",\"pageprops\":{\"disambiguation\":\"\"}}]}}";assert(!holly_search_wiki_parse(disamb,strlen(disamb),text,sizeof text,url,sizeof url));assert(!holly_search_wiki_parse("{}",2,text,sizeof text,url,sizeof url));assert(holly_search_wiki_parse(wiki,strlen(wiki)-1,text,sizeof text,url,sizeof url)<0);
 struct holly_session s;holly_session_init(&s);holly_search_bind(fetch);assert(!holly_clock_seed(1791176400,0,1));holly_search_tick(1000000);assert(holly_search_command(&s,"what is an asteroid",1,emit,0)==0);assert(holly_search_fallback(&s,"what is an asteroid?",emit,0));assert(calls==1&&provider==0&&!strcmp(encoded,"asteroid"));assert(holly_search_finish("{}",2)==1&&holly_search_pending());assert(!holly_search_continue()&&calls==2&&provider==1);assert(!holly_search_finish(wiki,strlen(wiki))&&!holly_search_pending());holly_search_poll(&s,out,sizeof out);assert(strstr(out,"ready|")&&strstr(out,"spacetime"));assert(!strstr(out,"DuckDuckGo")&&!strstr(out,"https://"));
 puts("Free lookup: article cleanup, two-provider pending-to-ready lifecycle, strict JSON, safe source, disambiguation and malformed response rejection passed.");}
