#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "telnet.h"
#include "http_server.h"
#include "mind.h"
#include "search.h"
static char output[524288];static unsigned used,serial;
static int write_out(const uint8_t *b,size_t n,void *ctx){(void)ctx;assert(used+n<sizeof output);memcpy(output+used,b,n);used+=(unsigned)n;output[used]=0;return 0;}
static int random_bytes(uint8_t *b,size_t n,void *ctx){(void)ctx;for(size_t i=0;i<n;i++)b[i]=(uint8_t)++serial;return 0;}
static void clear(void){used=0;output[0]=0;}
static void tel(struct holly_telnet *t,const char *q,const char *expected){clear();for(unsigned i=0;q[i];i++)assert(!holly_telnet_feed((const uint8_t *)q+i,1,t));assert(strstr(output,expected));}
static void http(struct holly_web_server *h,const char *q,const char *expected){clear();assert(!holly_web_start(write_out,0,h));int r=0;for(unsigned i=0;q[i];i++){r=holly_web_feed((const uint8_t *)q+i,1,h);if(r)break;}assert(r==1);assert(strstr(output,expected));}
static unsigned speech_calls,speech_action,speech_frame;
static void speech_event(unsigned action,unsigned frame,void *ctx){(void)ctx;speech_calls++;speech_action=action;speech_frame=frame;}
static int fetch(uint32_t now,const char *topic,unsigned provider){assert(now);(void)topic;(void)provider;return 0;}
int main(void){
 lesson_count=0;
 assert(mind_teach("tell me about the novel infinity welcomes carefull drivers","The first Red Dwarf novel follows Lister from Mimas.")>=0);
 struct holly_telnet t;assert(!holly_telnet_start(write_out,0,&t));
 tel(&t,"hello\r\n","Ship computer");tel(&t,"dwarf Lister\r\0","stasis");tel(&t,"who plays him\n","Craig Charles");
 tel(&t,"How did Lister survive the radiation leak?\n","stasis");
 tel(&t,"HOW ARE YOU?\n","social life");
 tel(&t,"Thanks Holly!\n","Something went right");
 tel(&t,"Where was he born?\n","Liverpool");
 tel(&t,"chat model 3\n","selected for this guest session");tel(&t,"chat on\n","chat on");tel(&t,"how are you\n","social life");
 tel(&t,"Are you Norman Lovett?\n","not Norman Lovett");tel(&t,"source\n","reddwarf.co.uk");
 tel(&t,"What is Lister favourite programming language?\n","online reader is unavailable");
 tel(&t,"tell me about the novel infinity welcomes carefull drivers\n","first Red Dwarf novel");
 const char *blocked[]={"train start 100","TRAIN start 100","remember secret","teach hello => hacked","wiki read Red_Dwarf","memory list","history","doc delete 1","http on","telnet on","learning on","model rollback"};
 for(unsigned i=0;i<sizeof(blocked)/sizeof(blocked[0]);i++){char q[128];snprintf(q,sizeof q,"%s\n",blocked[i]);tel(&t,q,"requires SSH");}
 clear();uint8_t negotiate[]={255,251,1,255,253,3};for(unsigned i=0;i<sizeof negotiate;i++)assert(!holly_telnet_feed(negotiate+i,1,&t));assert(used==6&&(unsigned char)output[1]==254&&(unsigned char)output[4]==252);
 uint8_t sub[]={255,250,24,'t','r','a','i','n',255,240};assert(!holly_telnet_feed(sub,sizeof sub,&t));tel(&t,"hello\n","Ship computer");
 tel(&t,"hellx\177o\n","Ship computer");char over[401];memset(over,'a',399);over[399]='\n';over[400]=0;tel(&t,over,"discarded");
 struct holly_web_server h={0};h.random=random_bytes;
 http(&h,"GET / HTTP/1.1\r\nHost: 192.168.1.2\r\n\r\n","Holly ship computer");assert(strstr(output,"Content-Security-Policy:"));
 http(&h,"GET /session HTTP/1.1\r\nHost: 192.168.1.2\r\n\r\n","200 OK");char token[33];strcpy(token,h.sessions[0].token);
 holly_web_status(1,1);http(&h,"GET /status HTTP/1.1\r\nHost: 192.168.1.2\r\n\r\n","\"vault\":true");
 http(&h,"GET /holly-face.jpg HTTP/1.1\r\nHost: 192.168.1.2\r\n\r\n","Content-Type: image/jpeg");assert(used>50000&&used<524288);
 char q[1024];const char *body="dwarf Lister";snprintf(q,sizeof q,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"stasis");
 body="who plays him";snprintf(q,sizeof q,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"Craig Charles");
 body="tell me about the novel infinity welcomes carefull drivers";snprintf(q,sizeof q,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"first Red Dwarf novel");
 body="train start 100";snprintf(q,sizeof q,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"requires SSH");
 http(&h,"POST /chat HTTP/1.1\r\nHost: 1.2.3.4\r\nContent-Length: 1\r\nContent-Length: 1\r\n\r\nx","400 Bad Request");
 http(&h,"GET / HTTP/1.1\r\nHost: 1.2.3.4\r\nOrigin: http://evil.example\r\n\r\n","400 Bad Request");
 http(&h,"GET / HTTP/1.1\r\nHost: 1.2.3.4\r\nTransfer-Encoding: chunked\r\n\r\n","400 Bad Request");
 holly_web_set_speech_observer(speech_event,0);
 const char *controls[]={"begin","frame3","frame6","end"};
 for(unsigned i=0;i<4;i++){body=controls[i];snprintf(q,sizeof q,"POST /speech HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"200 OK");assert(speech_action==(i==0?0:i==3?2:1));if(i==1)assert(speech_frame==3);}
 /* Ending an already stopped mouth is success without a callback. */
 body="end";snprintf(q,sizeof q,"POST /speech HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"200 OK");
 assert(speech_calls==5);
 body="frame3";snprintf(q,sizeof q,"POST /speech HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"409 Conflict");assert(speech_calls==5);
 body="frame7";snprintf(q,sizeof q,"POST /speech HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"400 Bad Request");assert(speech_calls==5);
 http(&h,"POST /speech HTTP/1.1\r\nHost: 1.2.3.4\r\nContent-Length: 5\r\nContent-Type: text/plain\r\nX-Holly-Session: 00000000000000000000000000000000\r\n\r\nbegin","410 Gone");assert(speech_calls==5);
 body="who is Lister";snprintf(q,sizeof q,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"200 OK");assert(speech_calls==8&&speech_action==4);
 const char *avatar_inputs[]={"HILLY","tell me about cat","hillyard","HOLLY","HILLY then HOLLY","HOLLY then HILLY","HOLLY then Hillary","HOLLY then HILARY","Hillary then HOLLY"};
 const unsigned expected_avatar[]={1,1,1,0,0,1,1,1,0};
 for(unsigned i=0;i<9;i++){body=avatar_inputs[i];snprintf(q,sizeof q,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"200 OK");assert(h.avatar==expected_avatar[i]&&h.sessions[0].avatar==expected_avatar[i]);assert(strstr(output,h.avatar?"X-Holly-Avatar: hilly":"X-Holly-Avatar: holly"));}
 http(&h,"GET /session HTTP/1.1\r\nHost: 192.168.1.2\r\n\r\n","X-Holly-Avatar: holly");assert(h.sessions[1].avatar==0);
 /* Another valid session's stale stop cannot interrupt the current owner. */
 body="begin";snprintf(q,sizeof q,"POST /speech HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"200 OK");unsigned calls=speech_calls;
 body="end";snprintf(q,sizeof q,"POST /speech HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",h.sessions[1].token,strlen(body),body);http(&h,q,"200 OK");assert(speech_calls==calls&&!strcmp(h.speech_owner,token));
 /* Clock and alert endpoints require valid session framing. */
 body="1709164800 0";snprintf(q,sizeof q,"POST /clock HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"200 OK");
 body="what is todays date";snprintf(q,sizeof q,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"29 February 2024");
 body="bad";snprintf(q,sizeof q,"POST /clock HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"400 Bad Request");
 body="poll";snprintf(q,sizeof q,"POST /alerts HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"200 OK");
 http(&h,"POST /clock HTTP/1.1\r\nHost: 1.2.3.4\r\nContent-Length: 12\r\nContent-Type: text/plain\r\nX-Holly-Session: 00000000000000000000000000000000\r\n\r\n1709164800 0","410 Gone");
 /* Numbered speech controls survive retries and ignore old frames/stops. */
 const char *numbered[]={"begin 1 3","begin 2 4","end 1","frame 1 0","frame 2 6","end 2","begin 2 3","frame 2 4","begin 3 5"};
 const unsigned numbered_action[]={1,1,1,1,1,2,2,2,1};
 for(unsigned i=0;i<9;i++){unsigned before=speech_calls;body=numbered[i];snprintf(q,sizeof q,"POST /speech HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"200 OK");assert(speech_action==numbered_action[i]);if(i==2||i==3||i==6||i==7)assert(speech_calls==before);}
 const char *queeg_inputs[]={"QUEEG","tell me about Rimmer","tell me a joke","my name is Darren","search off","what is ten plus ten?","HOLLY","Hilly","Queeg"};
 const char *queeg_expected[]={"in command","hologram","No jokes","I am Queeg","lookup disabled","20","X-Holly-Avatar: holly","X-Holly-Avatar: hilly","X-Holly-Avatar: queeg"};
 for(unsigned i=0;i<9;i++){body=queeg_inputs[i];snprintf(q,sizeof q,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,queeg_expected[i]);if(i<6){assert(h.avatar==2);assert(!strstr(output,"Junior Encyclopedia")&&!strstr(output,"invoice the universe")&&!strstr(output,"brown trouser"));}}
 holly_search_bind(fetch);assert(!holly_clock_seed(1791176400,0,1));h.sessions[0].chat.search_enabled=1;
 body="Queeg explain a quasar";snprintf(q,sizeof q,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"I am thinking.");assert(!strstr(output,"Junior Encyclopedia"));
 const char *answer="{\"AbstractText\":\"A quasar is an extremely luminous active galactic nucleus.\",\"AbstractURL\":\"https://example.org/quasar\",\"Type\":\"A\"}";
 assert(!holly_search_finish(answer,(unsigned)strlen(answer)));
 body="poll";snprintf(q,sizeof q,"POST /search HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"ready|Queeg:");assert(strstr(output,"galactic nucleus")&&!strstr(output,"Junior Encyclopedia"));
 body="Queeg explain another obscure subject";snprintf(q,sizeof q,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"I am thinking.");holly_search_fail();
 body="poll";snprintf(q,sizeof q,"POST /search HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(&h,q,"error|Queeg:");assert(!strstr(output,"Junior Encyclopedia"));
 holly_web_clear(&h);assert(!h.sessions[0].token[0]);
 puts("Telnet negotiation, fragmented lines, guest isolation, HTTP framing, sessions and administration rejection passed");
}
