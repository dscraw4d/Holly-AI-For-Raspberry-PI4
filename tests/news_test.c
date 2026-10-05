#include "news.h"
#include "http_server.h"
#include "web_format.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char output[65536];static unsigned used,serial;static int fetch_result,calls;
static void emit(const char*p,void*c){(void)c;while(*p&&used+1<sizeof output)output[used++]=*p++;output[used]=0;}
static int write_out(const uint8_t*p,size_t n,void*c){(void)c;assert(used+n<sizeof output);memcpy(output+used,p,n);used+=(unsigned)n;output[used]=0;return 0;}
static int random_bytes(uint8_t*p,size_t n,void*c){(void)c;for(size_t i=0;i<n;i++)p[i]=(uint8_t)++serial;return 0;}
static int fetch(uint32_t now){assert(now);calls++;return fetch_result;}
static void ask(struct holly_session*s,const char*q,const char*expected,int guest){used=0;output[0]=0;assert(!(guest?holly_conversation_only(s,q,emit,0):holly_turn(s,q,emit,0)));if(!strstr(output,expected)){fprintf(stderr,"%s: %s\n",q,output);assert(0);}}
static void http(struct holly_web_server*h,const char*q,const char*expected){used=0;output[0]=0;assert(!holly_web_start(write_out,0,h));assert(holly_web_feed((const uint8_t*)q,strlen(q),h)==1);assert(strstr(output,expected));}
static const char*feed="<?xml version=\"1.0\"?><rss version=\"2.0\"><channel><title>Fixture</title><item><title><![CDATA[Scientists test a new telescope]]></title><pubDate>Mon, 05 Oct 2026 05:00:00 GMT</pubDate></item><item><title>New ferry &amp; rail connection opens</title><pubDate>Mon, 05 Oct 2026 05:01:00 GMT</pubDate></item><item><title>Old item</title><pubDate>Fri, 02 Oct 2026 05:00:00 GMT</pubDate></item></channel></rss>";
int main(int argc,char**argv){
 if(argc==3){FILE*f=fopen(argv[1],"rb");assert(f);char body[32769],text[1400];unsigned n=(unsigned)fread(body,1,sizeof body,f);fclose(f);int count=holly_news_parse(body,n,(uint32_t)strtoul(argv[2],0,10),text,sizeof text);assert(count>0);printf("Live feed: %u bytes; %d fresh headlines parsed (content omitted).\n",n,count);return 0;}
 struct holly_session a,b;holly_session_init(&a);holly_session_init(&b);holly_news_bind(fetch);holly_news_tick(1000000);
 ask(&a,"Holly read me todays world news","Synchronise my clock",0);assert(!calls);
 assert(!holly_clock_seed(1791176400,0,1)); // Oct 5 2026 05:00 UTC
 ask(&a,"NEWS","Fetching current",1);assert(calls==1&&holly_news_pending());
 ask(&b,"Holly news","Fetching current",0);assert(calls==1);
 char text[1450];holly_news_poll(text,sizeof text);assert(!strcmp(text,"pending|"));
 assert(!holly_news_finish(feed,(unsigned)strlen(feed)));holly_news_poll(text,sizeof text);assert(strstr(text,"ready|Latest world headlines"));assert(strstr(text,"ferry & rail"));assert(!strstr(text,"Old item"));
 ask(&a,"news status","Scientists test",0);
 fetch_result=-2;ask(&b,"world news","reader is busy",1);fetch_result=-3;ask(&b,"news","30 seconds",1);fetch_result=-1;ask(&b,"world news","DHCP",1);fetch_result=0;
 ask(&b,"world news","Fetching current",1);holly_news_tick(61000000);assert(!holly_news_pending());holly_news_poll(text,sizeof text);assert(strstr(text,"error|I couldn't fetch"));
 assert(holly_news_parse(feed,strlen(feed),1791435600,text,sizeof text)==-1);assert(holly_news_parse("<!DOCTYPE rss><rss><channel></channel></rss>",45,1791176400,text,sizeof text)==-1);
 assert(holly_news_parse("<rss><channel><item><title>broken",31,1791176400,text,sizeof text)==-1);
 struct holly_http h={0};char response[8192];snprintf(response,sizeof response,"HTTP/1.1 200 OK\r\nContent-Type: text/xml; charset=UTF-8\r\nContent-Length: %zu\r\n\r\n%s",strlen(feed),feed);assert(holly_http_feed(&h,response,strlen(response))==-1);memset(&h,0,sizeof h);h.accept_xml=1;for(unsigned i=0;response[i];i++)assert(holly_http_feed(&h,response+i,1)==(response[i+1]?0:1));assert(!strcmp(h.body,feed));
 memset(&h,0,sizeof h);h.accept_xml=1;snprintf(response,sizeof response,"HTTP/1.1 200 OK\r\nContent-Type: application/rss+xml\r\nTransfer-Encoding: chunked\r\n\r\n%zx\r\n%s\r\n0\r\n\r\n",strlen(feed),feed);assert(holly_http_feed(&h,response,strlen(response))==1);
 struct holly_web_server server={0};server.random=random_bytes;http(&server,"GET /session HTTP/1.1\r\nHost: 192.168.1.2\r\n\r\n","200 OK");char token[33];strcpy(token,server.sessions[0].token);char q[1024];
 ask(&a,"world news","Fetching current",0);assert(!holly_news_finish(feed,strlen(feed)));
 snprintf(q,sizeof q,"POST /news HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nContent-Length: 4\r\nX-Holly-Session: %s\r\n\r\npoll",token);http(&server,q,"ready|Latest world headlines");
 http(&server,"POST /news HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nContent-Length: 4\r\nX-Holly-Session: 00000000000000000000000000000000\r\n\r\npoll","410 Gone");
 assert(holly_news_parse(feed,strlen(feed),1791176400,text,sizeof text)==2);
 const char*bad_day="<rss><channel><item><title>Fixture</title><pubDate>Mon, 999999999999999 Oct 2026 05:00:00 GMT</pubDate></item></channel></rss>";
 assert(holly_news_parse(bad_day,strlen(bad_day),1791176400,text,sizeof text)==-1);
 assert(!holly_clock_seed(1791176400+901,0,1));ask(&a,"news status","out of date",0);ask(&a,"news source","feeds.bbci.co.uk",1);
 ask(&b,"Hillary read me todays world news","Fetching current",1);
 holly_news_bind(0);ask(&a,"news","unavailable",0);
 puts("News commands, fresh-dated RSS/CDATAs/entities, stale rejection, pending/failure, XML HTTP framing and token-protected polling passed");return 0;
}
