#include "search.h"
#include "http_server.h"
#include "web_format.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char output[65536],query[577];static unsigned used,serial;static int calls,fetch_result;
static void emit(const char*s,void*c){(void)c;while(*s&&used+1<sizeof output)output[used++]=*s++;output[used]=0;}
static int write_out(const uint8_t*p,size_t n,void*c){(void)c;assert(used+n<sizeof output);memcpy(output+used,p,n);used+=(unsigned)n;output[used]=0;return 0;}
static int random_bytes(uint8_t*p,size_t n,void*c){(void)c;for(size_t i=0;i<n;i++)p[i]=(uint8_t)++serial;return 0;}
static int fetch(uint32_t t,const char*q,unsigned provider){(void)provider;assert(t&&strlen(q)<=576);calls++;strcpy(query,q);return fetch_result;}
static void ask(struct holly_session*s,const char*q,const char*expect,int guest){used=0;output[0]=0;assert(!(guest?holly_conversation_only(s,q,emit,0):holly_turn(s,q,emit,0)));if(!strstr(output,expect)){fprintf(stderr,"%s: %s\n",q,output);assert(0);}}
static void http(struct holly_web_server*h,const char*q,const char*expected){used=0;output[0]=0;assert(!holly_web_start(write_out,0,h));assert(holly_web_feed((const uint8_t*)q,strlen(q),h)==1);assert(strstr(output,expected));}
static const char*article="{\"AbstractText\":\"A fictional test summary about a test subject.\",\"AbstractURL\":\"https://example.org/subject\",\"Type\":\"A\",\"RelatedTopics\":[{\"Text\":\"Ignore this related-topic guess\"}]}";
int main(int argc,char**argv){
 char text[1450],source[513];
 if(argc==2){FILE*f=fopen(argv[1],"rb");assert(f);char body[32769];unsigned n=(unsigned)fread(body,1,sizeof body,f);fclose(f);assert(holly_search_parse(body,n,text,sizeof text,source,sizeof source)==1);assert(strlen(text)>100&&strncmp(source,"https://",8)==0);printf("Live DuckDuckGo JSON: %u bytes; bounded topic summary and HTTPS source parsed.\n",n);return 0;}
 assert(holly_search_parse(article,strlen(article),text,sizeof text,source,sizeof source)==1);assert(strstr(text,"fictional")&&!strstr(text,"guess"));assert(!strcmp(source,"https://example.org/subject"));
 const char*bad[]={"{\"Type\":\"A\",\"AbstractText\":\"bad\",\"AbstractText\":\"duplicate\"}","{\"Type\":\"A\",}","{\"Type\":null}","{\"Type\":\"A\",\"AbstractText\":\"<script>bad</script>\",\"AbstractURL\":\"https://example.org\"}","{\"Type\":\"A\",\"AbstractText\":\"plain\",\"AbstractURL\":\"https://example.org/\\nheader\"}"};
 for(unsigned i=0;i<sizeof bad/sizeof bad[0];i++)assert(holly_search_parse(bad[i],strlen(bad[i]),text,sizeof text,source,sizeof source)==-1);
 assert(holly_search_parse(article,strlen(article)-1,text,sizeof text,source,sizeof source)==-1);
 const char*empty="{\"Type\":\"D\",\"AbstractText\":\"Ambiguous name\",\"AbstractURL\":\"https://example.org\",\"RelatedTopics\":[{\"Text\":\"Wrong answer\"}]}";assert(!holly_search_parse(empty,strlen(empty),text,sizeof text,source,sizeof source));
 assert(!holly_search_parse("{}",2,text,sizeof text,source,sizeof source));
 char response[4096];snprintf(response,sizeof response,"HTTP/1.1 202 Accepted\r\nContent-Type: application/x-javascript\r\nTransfer-Encoding: chunked\r\n\r\n%zx\r\n%s\r\n0\r\n\r\n",strlen(article),article);struct holly_http framing={0};assert(holly_http_feed(&framing,response,strlen(response))==-1);memset(&framing,0,sizeof framing);framing.accept_ddg=1;for(unsigned i=0;response[i];i++)assert(holly_http_feed(&framing,response+i,1)==(response[i+1]?0:1));assert(!strcmp(framing.body,article));
 memset(&framing,0,sizeof framing);framing.accept_xml=1;assert(holly_http_feed(&framing,response,strlen(response))==-1);
 struct holly_session a,b;holly_session_init(&a);holly_session_init(&b);holly_search_bind(fetch);holly_search_tick(1000000);
 ask(&a,"what is a mitochondrion","synchronise",0);assert(!calls);assert(!holly_clock_seed(1791176400,0,1));
 ask(&a,"Holly what is a mitochondrion","searching the Junior Encyclopedia of Space",0);assert(calls==1&&!strcmp(query,"mitochondrion")&&holly_search_pending());
 holly_search_poll(&b,text,sizeof text);assert(!strcmp(text,"idle|"));ask(&b,"search source","No source",1);
 ask(&b,"what is a mitochondrion","busy",1);assert(calls==1);
 assert(!holly_search_finish(article,strlen(article)));holly_search_poll(&a,text,sizeof text);assert(strstr(text,"ready|A fictional test summary"));assert(!strstr(text,"https:"));assert(strstr(a.last_answer,"fictional"));
 ask(&a,"search status","fictional",0);ask(&a,"search source","https://example.org/subject",0);
 int previous=calls;ask(&a,"tell me about rimmer","Arnold Rimmer",0);assert(calls==previous);
 ask(&a,"search off","network lookup off",0);ask(&a,"what is a mitochondrion","more detail",0);assert(calls==previous);
 ask(&a,"search on","online reference service",0);ask(&a,"what is my unknown password","more detail",0);assert(calls==previous);
 ask(&a,"train status","Training phase",0);assert(calls==previous);
 ask(&a,"search alpha & beta?","searching the Junior Encyclopedia of Space",0);assert(!strcmp(query,"alpha%20%26%20beta"));assert(holly_search_finish(empty,strlen(empty))==1);assert(!holly_search_continue());assert(!holly_search_finish("{}",2));ask(&a,"search status","didn't turn up",0);
 fetch_result=-2;ask(&a,"web search jupiter","busy",1);fetch_result=-3;ask(&a,"search jupiter","10 seconds",1);fetch_result=-1;ask(&a,"search jupiter","DHCP",1);fetch_result=0;
 ask(&a,"search jupiter","searching the Junior Encyclopedia of Space",1);holly_search_tick(61000000);assert(!holly_search_pending());ask(&a,"search status","couldn't retrieve",0);
 struct holly_web_server h={0};h.random=random_bytes;http(&h,"GET /session HTTP/1.1\r\nHost: 192.168.1.2\r\n\r\n","200 OK");char token[33],req[1024];strcpy(token,h.sessions[0].token);
 snprintf(req,sizeof req,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nContent-Length: 14\r\nX-Holly-Session: %s\r\n\r\nsearch jupiter",token);http(&h,req,"searching the Junior Encyclopedia of Space");assert(!holly_search_finish(article,strlen(article)));
 snprintf(req,sizeof req,"POST /search HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nContent-Length: 4\r\nX-Holly-Session: %s\r\n\r\npoll",token);http(&h,req,"ready|A fictional test summary");
 http(&h,"POST /search HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nContent-Length: 4\r\nX-Holly-Session: 00000000000000000000000000000000\r\n\r\npoll","410 Gone");
 http(&h,"GET /session HTTP/1.1\r\nHost: 192.168.1.2\r\n\r\n","200 OK");snprintf(req,sizeof req,"POST /search HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nContent-Length: 4\r\nX-Holly-Session: %s\r\n\r\npoll",h.sessions[1].token);http(&h,req,"idle|");
 holly_search_bind(0);ask(&a,"search jupiter","unavailable",0);
 puts("PASS: DuckDuckGo fallback, local priority, personal-query guard, encoding, opt-out, session isolation, no-answer/timeout/errors, strict JSON/202 framing and HTTP polling");return 0;
}
