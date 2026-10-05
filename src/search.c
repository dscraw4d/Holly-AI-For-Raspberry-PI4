#include "search.h"
#include "search_cache.h"
#include <string.h>
static holly_search_fetch_fn fetch;
static unsigned state,provider;static uint32_t job,serial;static uint64_t ticks,started;
static char answer[1400],source[513],active_topic[193];static uint32_t fetched_at;
static void copy(char*d,const char*s,unsigned cap){unsigned i=0;while(s[i]&&i+1<cap){d[i]=s[i];i++;}d[i]=0;}
static void add(char*d,const char*s,unsigned cap){unsigned n=(unsigned)strlen(d);if(n<cap)copy(d+n,s,cap-n);}
static int prefix(const char*p,const char*s){while(*s){if(!*p||*p++!=*s++)return 0;}return 1;}
static int word(const char*p,const char*s){unsigned n=(unsigned)strlen(s);for(unsigned i=0;p[i];i++)if((!i||p[i-1]==' ')&&prefix(p+i,s)&&(!p[i+n]||p[i+n]==' '))return 1;return 0;}
static const char*normal(const char*in,char out[320]){unsigned n=0;while(*in&&n+1<320){char c=*in++;out[n++]=c>='A'&&c<='Z'?(char)(c+32):c;}out[n]=0;while(n&&(out[n-1]=='?'||out[n-1]=='.'||out[n-1]=='!'||out[n-1]==' '))out[--n]=0;const char*q=out;while(*q==' ')q++;const char*names[]={"holly, ","hilly, ","hillary, ","hilary, ","holly ","hilly ","hillary ","hilary ","queeg, ","queeg "};for(unsigned i=0;i<sizeof names/sizeof names[0];i++)if(prefix(q,names[i])){q+=strlen(names[i]);break;}return q;}
void holly_search_bind(holly_search_fetch_fn fn){fetch=fn;state=0;job=0;answer[0]=source[0]=0;}
int holly_search_pending(void){return state==1;}
void holly_search_fail(void){state=3;source[0]=0;copy(answer,"I couldn't retrieve a topic summary for the Junior Encyclopedia of Space. Check my clock and internet connection, then try again. I haven't learned an answer from this request.",sizeof answer);}
void holly_search_tick(uint64_t us){ticks=us;if(state==1&&us-started>=60000000ull)holly_search_fail();}
int holly_search_finish(const char*body,unsigned n){if(state!=1)return -1;char text[1201];int r=provider?holly_search_wiki_parse(body,n,text,sizeof text,source,sizeof source):holly_search_parse(body,n,text,sizeof text,source,sizeof source);if(!r&&!provider){provider=1;return 1;}if(r<0){holly_search_fail();return -1;}state=2;answer[0]=0;if(!r){source[0]=0;copy(answer,"The Junior Encyclopedia of Space didn't turn up a usable topic summary. Try a specific person, place or subject. Please try a more specific description.",sizeof answer);return 0;}(void)holly_search_cache_save(active_topic,text,source,fetched_at);add(answer,text,sizeof answer);return 0;}
void holly_search_poll(struct holly_session*s,char*out,unsigned cap){out[0]=0;if(!s||!s->search_job||s->search_job!=job){add(out,"idle|",cap);return;}add(out,state==1?"pending|":state==2?"ready|":state==3?"error|":"idle|",cap);if(state==2||state==3){const char *spoken=s->persona==2&&(state==3||!source[0])?"No usable answer was retrieved. Check the connection or give me a more specific subject.":answer;add(out,spoken,cap);copy(s->last_answer,spoken,sizeof s->last_answer);s->last_memory_id=0;}}
static const char*topic_query(const char*q){const char*lead[]={"what do you know about ","what can you tell me about ","can you tell me about ","could you tell me about ","tell me everything about ","tell me more about ","what is ","what are ","who is ","who was ","tell me about ","do you know about ","describe ","explain "};for(unsigned i=0;i<sizeof lead/sizeof lead[0];i++)if(prefix(q,lead[i])&&q[strlen(lead[i])])return q+strlen(lead[i]);return q;}
static int recall(struct holly_session*s,const char*q,holly_emit_fn emit,void*ctx){
 char text[1201],url[513];uint32_t date;if(!holly_search_cache_get(q,text,url,&date))return 0;
 s->search_cached=1;s->search_job=0;copy(s->search_topic,q,sizeof s->search_topic);s->last_answer[0]=0;const char*spoken=prefix(text,"Article overview: ")?text+18:text;add(s->last_answer,spoken,sizeof s->last_answer);s->last_memory_id=0;emit("Holly: ",ctx);emit(s->last_answer,ctx);emit("\n",ctx);return 1;
}
static int start(struct holly_session*s,const char*q,unsigned refresh,unsigned network,holly_emit_fn emit,void*ctx){
 if(!*q||strlen(q)>192){emit("Holly: Use a search topic between 1 and 192 characters.\n",ctx);return 1;}
 q=topic_query(q);if(prefix(q,"a "))q+=2;else if(prefix(q,"an "))q+=3;
 if(!refresh&&recall(s,q,emit,ctx))return 1;
 if(!network)return 0;
 s->last_memory_id=0;s->expression=HOLLY_THINKING;
 if(!fetch){emit(s->persona==2?"Queeg: The online reader is unavailable. Check the connection.\n":"Holly: The Junior Encyclopedia of Space's online reader is unavailable on this build.\n",ctx);return 1;}
 if(state==1){if(s->search_job==job)emit(s->persona==2?"Queeg: I am thinking. Stand by; use search status over SSH or Telnet.\n":"Holly: I'm searching the Junior Encyclopedia of Space. Please wait; use search status over SSH or Telnet.\n",ctx);else emit("Holly: The outbound reader is busy. Please try your question again shortly.\n",ctx);return 1;}
 uint32_t now=holly_clock_utc();if(!now){emit(s->persona==2?"Queeg: Synchronise the clock through the web page or clock set over SSH. Then repeat your request.\n":"Holly: Open my web page or use clock set over SSH to synchronise the clock before I can consult the Junior Encyclopedia of Space.\n",ctx);return 1;}
 if(!*q||strlen(q)>192){emit("Holly: Use a search topic between 1 and 192 characters.\n",ctx);return 1;}

 char encoded[577];unsigned at=0;const char*hex="0123456789ABCDEF";for(unsigned i=0;q[i];i++){unsigned char c=(unsigned char)q[i];if((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')encoded[at++]=(char)c;else{encoded[at++]='%';encoded[at++]=hex[c>>4];encoded[at++]=hex[c&15];}}encoded[at]=0;
 int r=fetch(now,encoded,0);if(r){emit(r==-2?"Holly: The outbound reader is busy. Please try again shortly.\n":r==-3?"Holly: Please wait 10 seconds between online reference requests.\n":"Holly: I need a DHCP internet connection with DNS and a gateway to reach the online reference service.\n",ctx);return 1;}
 if(!++serial)serial++;
 job=serial;s->search_job=job;s->search_cached=0;copy(s->search_topic,q,sizeof s->search_topic);copy(active_topic,q,sizeof active_topic);fetched_at=now;provider=0;state=1;started=ticks;answer[0]=source[0]=0;
 emit(s->persona==2?"Queeg: I am thinking. Stand by for the answer. SSH and Telnet users can ask search status.\n":"Holly: I'm searching the Junior Encyclopedia of Space. I'll read the topic summary when it arrives. SSH and Telnet users can ask search status.\n",ctx);return 1;
}
int holly_search_command(struct holly_session*s,const char*input,unsigned trusted,holly_emit_fn emit,void*ctx){
 char b[320];const char*q=normal(input,b);
 if(!strcmp(q,"search on")||!strcmp(q,"search off")){s->search_enabled=!strcmp(q,"search on");emit(s->persona==2?(s->search_enabled?"Queeg: Automatic lookup enabled. Stand by for unknown public questions.\n":"Queeg: Automatic lookup disabled. Saved references remain available.\n"):s->search_enabled?"Holly: Junior Encyclopedia of Space lookup on for this session. Unknown public questions can use the online reference service.\n":"Holly: Junior Encyclopedia of Space automatic network lookup off for this session. Saved references and explicit search still work.\n",ctx);return 1;}
 if(!strcmp(q,"search cache")){char number[12];unsigned v=holly_search_cache_count(),n=0;do{number[n++]=(char)('0'+v%10);v/=10;}while(v);emit(holly_search_cache_available()?"Holly: Persistent web reference bank online. Saved topics: ":"Holly: Persistent web reference bank unavailable. Saved topics: ",ctx);while(n){char c[2]={number[--n],0};emit(c,ctx);}emit(holly_search_cache_capacity()==256?" / 256. Use search refresh followed by a topic to fetch again.\n":" / 64 (legacy bank; dedicated bank unavailable). Use search refresh followed by a topic to fetch again.\n",ctx);return 1;}
 if(prefix(q,"search forget ")){if(!trusted){emit("Holly: Forgetting saved web references requires SSH.\n",ctx);return 1;}int r=holly_search_cache_forget(topic_query(q+14));emit(r==0?"Holly: Saved web reference forgotten.\n":r==1?"Holly: No saved web reference for that topic.\n":"Holly: I couldn't remove the saved reference safely.\n",ctx);return 1;}
 if(!strcmp(q,"search status")){if(s->search_cached){if(!recall(s,s->search_topic,emit,ctx))emit("Holly: That saved reference is no longer available. Search the topic again.\n",ctx);return 1;}char result[1450];holly_search_poll(s,result,sizeof result);emit("Holly: ",ctx);emit(prefix(result,"pending|")?(s->persona==2?"I am thinking. Stand by.":"I'm searching the Junior Encyclopedia of Space. Please wait."):prefix(result,"idle|")?"No retained search for this session. Use search followed by a topic.":result+6,ctx);emit("\n",ctx);return 1;}
 if(!strcmp(q,"search source")){char text[1201],url[513],date[21];uint32_t stamp=0;int found=s->search_cached?holly_search_cache_get(s->search_topic,text,url,&stamp):0;const char*ref=found?url:s->search_job==job&&source[0]?source:0;if(ref&&!found)stamp=fetched_at;emit("Holly: ",ctx);emit(ref?ref:"No source for this session's search.",ctx);if(ref){holly_search_cache_date(stamp,date);emit("; fetched ",ctx);emit(date,ctx);}emit("\n",ctx);return 1;}
 unsigned refresh=prefix(q,"search refresh ");const char*topic=refresh?q+15:prefix(q,"web search ")?q+11:prefix(q,"search ")?q+7:0;if(!topic)return 0;
 return start(s,topic,refresh,1,emit,ctx);
}
int holly_search_fallback(struct holly_session*s,const char*input,holly_emit_fn emit,void*ctx){
 if(!s->search_enabled&&!holly_search_cache_count())return 0;
 char b[320];const char*q=normal(input,b);
 /* Personal queries and administrative inputs stay local; only unhandled
  * public information questions trigger the automatic network request. */
 const char*social[]={"how are you","how are you holly","how's it going","can we talk","can we chat","hello","hi","hey","thanks","thanks holly","thank you","thank you holly","what would you like to know"};for(unsigned i=0;i<sizeof social/sizeof social[0];i++)if(!strcmp(q,social[i]))return 0;
 if(prefix(q,"i enjoy ")||prefix(q,"i like ")||prefix(q,"i feel ")||prefix(q,"i am ")||prefix(q,"i'm "))return 0;
 const char*private[]={"my","our"};for(unsigned i=0;i<sizeof private/sizeof private[0];i++)if(word(q,private[i]))return 0;
 if(prefix(q,"where do i live")||prefix(q,"where am i")||prefix(q,"what is saved in")||prefix(q,"what did i "))return 0;
 /* Every remaining unhandled chat request is a lookup candidate, including
  * terse topics and question forms absent from the old prefix whitelist. */
 return start(s,q,0,s->search_enabled,emit,ctx);
}

int holly_search_continue(void){if(state!=1||provider!=1||!fetch)return -1;char encoded[577];unsigned at=0;const char*hex="0123456789ABCDEF";for(unsigned i=0;active_topic[i];i++){unsigned char c=(unsigned char)active_topic[i];if((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')encoded[at++]=(char)c;else{encoded[at++]='%';encoded[at++]=hex[c>>4];encoded[at++]=hex[c&15];}}encoded[at]=0;int r=fetch(fetched_at,encoded,1);if(r)holly_search_fail();return r;}
