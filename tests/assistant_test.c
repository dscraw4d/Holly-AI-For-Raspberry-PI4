#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "holly.h"
#include "mind.h"
static struct holly_memory_item items[64];static unsigned count;static int fail;
static unsigned count_items(void*c){(void)c;return count;}
static int get_at(unsigned i,struct holly_memory_item*x,void*c){(void)c;if(i>=count)return -1;*x=items[i];return 0;}
static int get(unsigned id,struct holly_memory_item*x,void*c){return id?get_at(id-1,x,c):-1;}
static int update(unsigned id,const char*t,const char*s,unsigned conf,void*c){(void)c;if(fail||!id||id>count)return -1;struct holly_memory_item*x=&items[id-1];x->id=id;x->confidence=conf;snprintf(x->text,sizeof x->text,"%s",t);snprintf(x->source,sizeof x->source,"%s",s);return 0;}
static int add(const char*t,const char*s,unsigned conf,uint32_t*id,void*c){if(fail||count==64)return -1;*id=++count;return update(*id,t,s,conf,c);}
static char output[4096];static unsigned used;
static void emit(const char*t,void*c){(void)c;while(*t&&used+1<sizeof output)output[used++]=*t++;output[used]=0;}
static void ask(struct holly_session*s,const char*q,const char*expected,int guest){used=0;output[0]=0;int r=guest?holly_conversation_only(s,q,emit,0):holly_turn(s,q,emit,0);if(r||!strstr(output,expected)){fprintf(stderr,"%s expected %s got %s\n",q,expected,output);assert(0);}}
int main(void){
 static const struct holly_memory_ops ops={add,count_items,get,get_at,update,0};
 struct holly_session a,b;holly_session_init(&a);holly_session_init(&b);holly_set_memory(&ops,0);lesson_count=0;
 ask(&a,"Holly what time is it?","haven't synchronised",1);
 ask(&a,"set an alarm in 5 minutes","synchronised clock",0);
 assert(holly_clock_seed(1,0,0)==-1);assert(holly_clock_seed(1709164800,841,0)==-1);
 assert(!holly_clock_seed(1709164800,0,1)); // 2024-02-29 00:00 UTC
 ask(&a,"what is todays date?","Thursday, 29 February 2024",1);
 assert(!holly_clock_seed(1709164800,-60,0));ask(&a,"what time is it","23:00 on Wednesday, 28 February 2024",0);
 holly_assistant_tick(3600000000ULL);ask(&a,"what time is it","00:00 on Thursday, 29 February 2024",0);
 ask(&a,"clock set 1709164800 0","require SSH",1);
 ask(&a,"clock set 1709164800 0","Clock set",0);
 ask(&a,"Holly take a note Repair Starbug","Saved note 1",1);
 ask(&b,"holly recall my notes","Repair Starbug",1);
 ask(&a,"take a note","What shall I note",0);ask(&b,"notes","Repair Starbug",1);
 ask(&a,"Buy a Vindaloo","Saved note 2",0);assert(!strcmp(items[1].text,"Buy a Vindaloo"));
 holly_session_init(&b);ask(&b,"recall note 2","Buy a Vindaloo",1);
 ask(&a,"take a note","What shall I note",1);ask(&a,"cancel","cancelled",1);
 uint32_t private_id;assert(!add("private fact","user",100,&private_id,0));ask(&a,"recall note 3","haven't found",1);
 fail=1;ask(&a,"take a note Failed","couldn't save",1);fail=0;
 ask(&a,"set an alarm in five seconds","Alarm 4 saved",1);
 char poll[400];assert(!holly_alarm_poll(poll,sizeof poll));assert(holly_alarm_ack(4)==-1);
 holly_assistant_tick(3605000000ULL);assert(holly_alarm_poll(poll,sizeof poll));assert(strstr(poll,"4|Emergency! There is an emergency going on... it\'s still going on. Alarm:"));
 holly_set_memory(&ops,0);assert(!holly_alarm_poll(poll,sizeof poll));holly_assistant_tick(3605020000ULL);assert(holly_alarm_poll(poll,sizeof poll)); // rebuild the RAM index from persisted records
 fail=1;assert(holly_alarm_ack(4)==-1);fail=0;assert(!holly_alarm_ack_text("4"));assert(!holly_alarm_poll(poll,sizeof poll));assert(holly_alarm_ack(1)==-1);
 ask(&a,"set an alarm for 07:30","Alarm 4 saved",1);assert(strstr(items[3].text,"1709191800"));
 ask(&a,"set an alarm for 7 PM","Alarm 5 saved",0);assert(strstr(items[4].text,"1709233200"));
 ask(&b,"alarm status","is set",1);ask(&a,"set an alarm for 25:90","Use set an alarm",1);
 assert(holly_browser_clock("bad")==-1);assert(holly_browser_clock("1709164800 -841")==-1);assert(!holly_browser_clock("1709164800 0"));
 const char*aliases[]={"Crichton","CRIGHTON","kryton","kriton","cryten","cryton","criten","krytan","krytten","krayton","cry ten","kry ten","cry ton","kry ton","cr eye ten","kriten","crayten","crayton","krytonne","krypton"};
 for(unsigned i=0;i<sizeof aliases/sizeof aliases[0];i++){char q[100];snprintf(q,sizeof q,"who is %s",aliases[i]);holly_session_init(&b);ask(&b,q,"Kryten",i%2);}
 puts("Clock/date offsets and leap day, persistent shared notes, alarm delivery retry and Kryten speech aliases passed");return 0;
}
