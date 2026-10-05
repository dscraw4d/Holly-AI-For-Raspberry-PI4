#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lore.h"
static char out[257];
static void reply(struct holly_lore *s,const char *q,const char *part){
 assert(holly_lore_reply(s,q,out,sizeof out)==1);assert(strstr(out,part));
}
int main(void){
 struct holly_lore s,t;holly_lore_init(&s);holly_lore_init(&t);
 assert(holly_lore_count()==42&&s.enabled);
 reply(&s,"Who is Lister?","stasis");reply(&s,"who plays him?","Craig Charles");
 reply(&s,"dwarf source","theend");
 reply(&s,"dwarf Holly","6000");reply(&s,"tell me more","Hilly");
 reply(&s,"who plays her?","Hattie Hayridge");reply(&s,"more","all I have verified");
 reply(&s,"Tell me about Ace Rimmer","test pilot");
 reply(&s,"what is the Holly Hop Drive?","invention");
 reply(&s,"Who are Lister and Kochanski?","more than one");
 reply(&s,"dwarf source","no current");
 reply(&s,"dwarf Quarantine","twelve weeks");
 reply(&s,"dwarf nonexistent space walrus","don't have a verified");
 assert(!holly_lore_reply(&s,"my cat is asleep",out,sizeof out));
 assert(!holly_lore_reply(&s,"how do I quarantine a file",out,sizeof out));
 assert(!holly_lore_reply(&s,"what is a red dwarf star",out,sizeof out));
 assert(!holly_lore_reply(&s,"i enjoy red dwarf",out,sizeof out));
 assert(!holly_lore_reply(&s,"myhollyword",out,sizeof out));
 reply(&s,"dwarf off","off");assert(!holly_lore_reply(&s,"Who is Lister?",out,sizeof out));
 reply(&s,"dwarf Lister","stasis");assert(!s.enabled);
 assert(!holly_lore_reply(&t,"more",out,sizeof out));
 reply(&s,"dwarf on","on");reply(&s,"dwarf topics 6","Smeg");
 reply(&s,"dwarf topics 9999999999999999999","Use dwarf topics");
 reply(&s,"dwarf topics 2 extra","Use dwarf topics");
 reply(&s,"dwarf reset","cleared");reply(&s,"dwarf more","Choose a topic");
 char longq[321];memset(longq,'a',320);longq[320]=0;
 assert(holly_lore_reply(&s,longq,out,sizeof out)==-1);
 assert(holly_lore_reply(&s,"dwarf",out,1)==-1);
 struct {char buf[257];char guard[8];} protected;memset(&protected,0x5a,sizeof protected);
 assert(holly_lore_reply(&s,"dwarf topics 4",protected.buf,sizeof protected.buf)==1);
 for(unsigned i=0;i<8;i++)assert(protected.guard[i]==0x5a);
 puts("Lore matching, follow-ups, cast, sources, ambiguity, bounds and session isolation passed");
}
