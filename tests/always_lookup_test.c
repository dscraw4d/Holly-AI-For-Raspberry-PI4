#include "search.h"
#include "holly.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char output[4096],query[577];static unsigned used,calls;
static int fetch(uint32_t t,const char*q,unsigned provider){assert(t&&!provider);calls++;strcpy(query,q);return 0;}
static void emit(const char*p,void*c){(void)c;while(*p&&used+1<sizeof output)output[used++]=*p++;output[used]=0;}
static const char*article="{\"Type\":\"A\",\"AbstractText\":\"Jupiter has many moons.\",\"AbstractURL\":\"https://example.org/jupiter\"}";
static void ask(struct holly_session*s,const char*q,int guest){used=0;output[0]=0;assert(!(guest?holly_conversation_only(s,q,emit,0):holly_turn(s,q,emit,0)));assert(!strstr(output,"I don't know")&&!strstr(output,"add a reference through SSH")&&!strstr(output,"unverified"));}
static void check(const char*q,int guest){struct holly_session s;holly_session_init(&s);holly_search_bind(fetch);unsigned before=calls;ask(&s,q,guest);if(calls!=before+1||!strstr(output,"searching the Junior Encyclopedia of Space")){fprintf(stderr,"%s -> %s\n",q,output);assert(0);}assert(!holly_search_finish(article,strlen(article)));char ready[1500];holly_search_poll(&s,ready,sizeof ready);assert(!strcmp(ready,"ready|Jupiter has many moons."));ask(&s,"search status",guest);assert(!strcmp(output,"Holly: Jupiter has many moons.\n"));}
int main(void){holly_set_memory(0,0);assert(!holly_clock_seed(1791176400,0,1));
 check("Jupiter's moons",0);check("Jupiter's moons",1);check("Give me information on a quasar",0);check("give me information on a quasar",1);check("What is Lister's favourite programming language?",0);check("What is Lister's favourite programming language?",1);check("ask quasars",0);
 struct holly_session s;holly_session_init(&s);holly_search_bind(fetch);unsigned before=calls;ask(&s,"what is my unknown password",1);assert(calls==before);ask(&s,"train status",1);assert(calls==before&&strstr(output,"requires SSH"));ask(&s,"search off",0);ask(&s,"give me information on a quasar",0);assert(calls==before);ask(&s,"search on",0);ask(&s,"what is a password manager",0);assert(calls==before+1);holly_search_fail();ask(&s,"search status",0);assert(strstr(output,"couldn't retrieve"));holly_search_bind(0);ask(&s,"quasars",1);assert(strstr(output,"reader is unavailable"));
 puts("Always lookup: SSH and guests, bare topics, new phrasing, reference misses, ask, direct fresh speech, private/admin isolation, opt-out and honest service failure passed.");}
