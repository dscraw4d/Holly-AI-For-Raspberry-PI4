#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "holly.h"
#include "mind.h"
static char output[4096];static unsigned used;
static void capture(const char *p,void *unused){(void)unused;while(*p&&used+1<sizeof output)output[used++]=*p++;output[used]=0;}
static void ask(struct holly_session *s,const char *q,const char *expected,int guest){
 used=0;output[0]=0;int result=guest?holly_conversation_only(s,q,capture,0):holly_turn(s,q,capture,0);
 if(result||!strstr(output,expected)){fprintf(stderr,"Question: %s\nExpected: %s\nGot: %s\n",q,expected,output);assert(0);}
}
static char script_request[420];
static int script_answer(const char *q,holly_emit_fn emit,void *ctx){strcpy(script_request,q);emit("Holly: Closest script passage; check the context before treating it as an answer.\n[The End; document 1; byte 100]\nLister survived in stasis.\n",ctx);return 0;}
static char observed_reply[640];static unsigned observed_count;
static void observe_reply(const char *text,void *ctx){(void)ctx;snprintf(observed_reply,sizeof observed_reply,"%s",text);observed_count++;}
static struct holly_memory_item name_memory;
static unsigned name_count;
static int add_name(const char *text,const char *source,unsigned confidence,uint32_t *id,void *ctx){
 (void)ctx;name_count=1;name_memory.id=1;name_memory.confidence=confidence;
 strcpy(name_memory.text,text);strcpy(name_memory.source,source);*id=1;return 0;
}
static unsigned count_names(void *ctx){(void)ctx;return name_count;}
static int get_name_at(unsigned index,struct holly_memory_item *item,void *ctx){
 (void)ctx;if(index||!name_count)return -1;*item=name_memory;return 0;
}
static int update_name(uint32_t id,const char *text,const char *source,unsigned confidence,void *ctx){
 uint32_t out;if(id!=1)return -1;return add_name(text,source,confidence,&out,ctx);
}
int main(void){
 lesson_count=0;holly_set_memory(0,0);holly_set_history(0,0,0,0);
 struct holly_session a,b;holly_session_init(&a);holly_session_init(&b);
 ask(&a,"what is 10 plus 10?","20.",0);
 ask(&b,"Holly whats (2 + 3) times 4?","20.",1);
 ask(&a,"calculate 1 / 0","divide by zero",0);
 ask(&b,"what is 7 divided by 2?","3.5.",1);
 holly_session_init(&a);holly_session_init(&b);
 const char *queries[]={"What do you think about Rimmer?","I dislike Lister","I like Cat","Let's talk about Holly","What do you think of Kryten?","I reckon Kochanski matters","I like Ace Rimmer"};
 const char *subjects[]={"Arnold Rimmer","Dave Lister","The Cat","Holly","Kryten","Kristine Kochanski","Ace Rimmer"};
 for(unsigned i=0;i<7;i++){
  ask(&a,queries[i],"interpretation",0);assert(!strcmp(a.topic,subjects[i]));
  ask(&a,"Why?","my reasoning",0);assert(strlen(a.last_answer)>100);
  ask(&a,"source","Basis for my interpretation",0);
  assert(!b.topic[0]&&!b.discussion_profile);
 }
 ask(&a,"What if Rimmer were captain?","Hypothetical, not canon",0);
 ask(&a,"Why?","not a prediction of canon",0);
 ask(&a,"What if Rimmer were a banana?","Arnold Rimmer",0);
 assert(!strstr(output,"procedure")&&!strstr(output,"captain"));
 ask(&a,"I think Rimmer is interesting","interpretation",0);
 ask(&a,"Who played him?","Chris Barrie",0);assert(!a.discussion_profile);
 ask(&a,"I think the sound is strange","Which Red Dwarf character",0);
 ask(&a,"source","Ask a Red Dwarf question first",0);
 ask(&a,"chat on","chat is on",0);
 ask(&a,"What do you think about Rimmer?","interpretation",0);
 ask(&a,"CHAT RESET","chat is on",0);
 assert(!a.topic[0]&&!a.last_answer[0]&&!a.discussion_profile&&!a.dialogue_previous[0]);
 ask(&a,"What were we talking about?","haven't picked a topic",0);
 ask(&b,"What do you think of Ace Rimmer?","interpretation",1);
 ask(&b,"Why?","Ace and Arnold aren't interchangeable",1);
 ask(&b,"Who plays him?","Chris Barrie",1);
 ask(&b,"chat reset","context cleared",1);
 assert(!b.topic[0]&&!b.last_answer[0]&&!b.discussion_profile);
 ask(&b,"Who plays him?","Which Red Dwarf",1);
 ask(&b,"What do you think about Holly?","interpretation",1);
 ask(&b,"brain reset","retrieval is on",1);
 assert(!b.topic[0]&&!b.last_answer[0]&&!b.discussion_profile);
 ask(&b,"remember secret","requires SSH",1);
 ask(&a,"Compare Lister and Rimmer","Rimmer:",0);
 assert(strlen(a.last_answer)>257);char previous[HOLLY_REPLY_SIZE];strcpy(previous,a.last_answer);
 ask(&a,"repeat that","Rimmer:",0);assert(!strcmp(previous,a.last_answer));
 ask(&a,"My Name is Darren","Hi Darren. I'm Holly",0);
 ask(&a,"hello","Hi Darren",0);
 ask(&a,"what is my name?","You're Darren",0);
 ask(&a,"What do you know about season 1?","six episodes from 1988",0);
 assert(strstr(output,"Confidence and Paranoia")&&strstr(output,"Me2"));
 ask(&a,"tell me more","first series",0);
 ask(&a,"source","official Red Dwarf",0);
 ask(&a,"which episodes?","Future Echoes",0);
 ask(&a,"Who is Cat?","Cat",0);assert(!a.series_focus);
 ask(&a,"chat reset","chat is on",0);assert(!a.series_focus);
 ask(&b,"what is my name?","What would you like",1);
 ask(&b,"Call me Viper","Hi Viper",1);
 ask(&a,"what is my name?","Darren",0);
 ask(&b,"What do you know about series one?","six episodes",1);
 ask(&b,"tell me more","first series",1);
 ask(&a,"Tell me about Season 2","Kryten",0);assert(a.series_focus==2);
 ask(&b,"Tell me about Season 2","Kryten",1);assert(b.series_focus==2);
 const char *ends[]={"","Me2","Parallel Universe","The Last Day","Meltdown","Back to Reality","Out of Time","Nanarchy","Only the Good"};
 for(unsigned i=2;i<=8;i++){
  char question[80];sprintf(question,"What do you know about season %u?",i);
  ask(&a,question,ends[i],0);assert(a.series_focus==i);
  ask(&a,"which episodes?",ends[i],0);
  ask(&a,"tell me more","particular episode",0);
  ask(&a,"source","official Red Dwarf",0);
  ask(&b,question,ends[i],1);assert(b.series_focus==i);
 }
 ask(&a,"What do you know about series eight?","Only the Good",0);
 holly_set_document_commands(0,0,script_answer);
 ask(&a,"In The End why was there a radiation leak?","Lister survived",0);
 assert(strstr(script_request,"ask The End |")&&strstr(script_request,"radiation"));
 assert(!strstr(output,"document 1")&&!strstr(output,"Closest script passage"));
 ask(&a,"source","document 1",0);
 ask(&a,"Why did Lister survive?","Lister survived",0);assert(strstr(script_request,"ask The End |"));
 ask(&a,"Who is Cat?","Cat",0);assert(!a.episode_focus[0]);
 ask(&b,"In Queeg what was Holly doing?","Lister survived",1);assert(strstr(script_request,"ask Queeg |"));
 ask(&b,"chat reset","context cleared",1);assert(!b.episode_focus[0]);
 holly_set_document_commands(0,0,0);
 static const struct holly_memory_ops names={add_name,count_names,0,get_name_at,update_name,0};
 holly_set_memory(&names,0);struct holly_session owner,reconnected,visitor;
 holly_session_init(&owner);ask(&owner,"My name is Darren","Hi Darren. I'm Holly",0);
 assert(name_count==1&&strstr(name_memory.text,"Darren"));
 holly_session_init(&reconnected);ask(&reconnected,"hello","Hi Darren",0);
 holly_session_init(&visitor);ask(&visitor,"what is my name?","What would you like",1);
 ask(&visitor,"call me Cat","Hi Cat",1);assert(strstr(name_memory.text,"Darren"));
 ask(&owner,"call me Viper","Hi Viper",0);assert(name_count==1);
 holly_session_init(&reconnected);ask(&reconnected,"hello","Hi Viper",0);
 holly_set_memory(0,0);
 holly_set_reply_observer(observe_reply,0);
 ask(&a,"My name is Darren","Hi Darren",0);assert(observed_count==1&&strstr(observed_reply,"Hi Darren")&&!strstr(observed_reply,"Holly:"));
 ask(&b,"hello","Hi Viper",1);assert(observed_count==2&&strstr(observed_reply,"Hi Viper"));
 ask(&a,"version","0.49.29",0);assert(observed_count==2);
 /* A lesson taught through authenticated chat is readable by web/Telnet guests. */
 ask(&a,"teach tell me about the novel infinity welcomes carefull drivers => The first Red Dwarf novel follows Lister from Mimas to Red Dwarf.","have that in memory",0);
 ask(&a,"tell me about the novel infinity welcomes carefull drivers","first Red Dwarf novel",0);
 ask(&b,"tell me about the novel infinity welcomes carefull drivers","first Red Dwarf novel",1);
 assert(strstr(observed_reply,"first Red Dwarf novel"));
 ask(&b,"repeat that","first Red Dwarf novel",1);
 ask(&b,"source","Ask a Red Dwarf question first",1);
 unsigned saved_lessons=lesson_count;
 ask(&b,"teach bad => overwritten","requires SSH",1);assert(lesson_count==saved_lessons);
 ask(&b,"memory list","requires SSH",1);
 holly_set_reply_observer(0,0);
 puts("Natural introductions, persistent name reload/update, guest name isolation and series-one follow-ups passed");
 puts("Grounded discussion: seven profiles, reason/source follow-ups, hypothetical separation, actor context, complete repeats, case-insensitive resets and guest isolation passed");
}
