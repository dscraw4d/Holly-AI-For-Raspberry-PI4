#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "reference.h"
#include "documents.h"
#include "sha256.h"
static struct holly_documents docs;
static char out[HOLLY_REFERENCE_REPLY];
static unsigned visits;
static uint8_t media[20000][512];
static int rd(uint32_t l,uint8_t*b,void*c){(void)c;if(l>=20000)return -1;memcpy(b,media[l],512);return 0;}
static int wr(uint32_t l,const uint8_t*b,void*c){(void)c;if(l>=20000)return -1;memcpy(media[l],b,512);return 0;}
static void ignore(const char*s,void*c){(void)s;(void)c;}

static void counted_visit(holly_reference_offer_fn offer,void *visitor,void *storage,const char *q){visits++;holly_documents_visit_query(offer,visitor,storage,q);}
static void ask(struct holly_reference *s,const char *q,const char *expect){
 int r=holly_reference_reply(s,q,out,sizeof out);
 if((r!=1&&r!=2)||!strstr(out,expect)){fprintf(stderr,"Question: %s\nExpected: %s\nGot (%d): %s\n",q,expect,r,out);assert(0);}
}
int main(void){
 struct holly_reference s;holly_reference_init(&s);
 /* Plain replies, explicit provenance, occasional remarks and exact repeat. */
 struct holly_reference natural;holly_reference_init(&natural);
 unsigned jokes=0;
 for(unsigned i=0;i<48;i++){
  holly_reference_clear(&natural);
  unsigned last=natural.banter_last;
  ask(&natural,"How did Lister survive the radiation leak?","stasis");
  assert(!strstr(out,"http")&&!strstr(out,"[1]")&&!strstr(out,"records say"));
  if(last!=natural.banter_last)jokes++;
  char saved[HOLLY_REFERENCE_REPLY];strcpy(saved,out);
  ask(&natural,"repeat that","stasis");assert(!strcmp(saved,out));
  ask(&natural,"source","reddwarf.co.uk");
 }
 assert(jokes>0&&jokes<48);
 natural.dry_voice=0;unsigned before=natural.banter_state;
 holly_reference_clear(&natural);ask(&natural,"Who is Cat?","Cat");assert(before==natural.banter_state);

 ask(&s,"How did Lister survive the radiation leak?","stasis");
 char voiced[HOLLY_REFERENCE_REPLY];strcpy(voiced,out);
 struct holly_reference plain;holly_reference_init(&plain);plain.dry_voice=0;
 ask(&plain,"How did Lister survive the radiation leak?","stasis");
 assert(!strstr(out,"http")&&!strstr(voiced,"http"));
 ask(&s,"Who played him?","Craig Charles");
 ask(&s,"Where was he born?","Liverpool");
 ask(&s,"source","reddwarf.co.uk");
 ask(&s,"What is Lister's favourite programming language?","don't have a passage");
 ask(&s,"What is Holly's IQ?","6000");
 ask(&s,"Who invented the Holly Hop Drive?","invents");
 ask(&s,"Compare Lister and Rimmer","Rimmer:");
 ask(&s,"Who plays him?","Which Red Dwarf");
 ask(&s,"Who is Ace Rimmer?","Ace Rimmer");
 ask(&s,"brain reset","retrieval is on");
 ask(&s,"Who is Lister?","stasis");
 ask(&s,"more","Liverpool");
 ask(&s,"more","Craig Charles");
 ask(&s,"more","don't have a passage");
 /* New uploaded facts are answered from text without a bespoke topic/reply. */
 struct holly_block_ops io={rd,wr,0};assert(!holly_documents_format(&docs,&io,0,20000));
 const char*text="This is invented test material, not canon. Lister keeps a purple notebook in locker seven. The notebook contains navigation puzzles. https://example.test/notes [S01]";
 uint8_t hash[32];char hex[65],cmd[1024];holly_sha256_hash(text,strlen(text),hash);for(unsigned i=0;i<32;i++)sprintf(hex+2*i,"%02x",hash[i]);
 sprintf(cmd,"doc begin %u %s Red Dwarf test notes",(unsigned)strlen(text),hex);holly_documents_command(&docs,cmd,ignore,0);
 unsigned at=sprintf(cmd,"doc put 1 0 ");for(unsigned i=0;i<strlen(text);i++)sprintf(cmd+at+i*2,"%02x",(unsigned char)text[i]);holly_documents_command(&docs,cmd,ignore,0);holly_documents_command(&docs,"doc commit 1",ignore,0);
 holly_reference_set_documents(counted_visit,&docs);
 visits=0;ask(&s,"Who is Cat?","Cat");assert(visits==0);
 ask(&s,"What notebook does Lister have?","purple notebook");assert(!strstr(out,"uploaded document")&&!strstr(out,"Red Dwarf test notes"));
 ask(&s,"source","uploaded document 1; byte 0");
 ask(&s,"What does the notebook contain in Red Dwarf?","navigation puzzles");assert(!strstr(out,"https://")&&!strstr(out,"[S01]"));
 docs.doc[0].state=1;ask(&s,"What notebook does Lister have?","don't have a passage");
 docs.doc[0].state=2;docs.ready=0;ask(&s,"What notebook does Lister have?","don't have a passage");
 struct holly_reference other;holly_reference_init(&other);ask(&other,"Who played him?","Which Red Dwarf");
 assert(holly_reference_reply(&other,"Who is the president?",out,sizeof out)==0);
 ask(&other,"brain off","retrieval is off");assert(holly_reference_reply(&other,"Who is Lister?",out,sizeof out)==0);
 /* Curated answers must bypass the SD index entirely. */
 docs.ready=1;holly_reference_init(&other);visits=0;
 ask(&other,"Who is Cat?","Cat");assert(visits==0);
 ask(&other,"What is Cat's favourite programming language?","don't have a passage");assert(visits==1);
 puts("Reference QA: paraphrases, context, sources, comparisons, compound entities, no-repeat more, uploads, unavailable data and unknowns passed");
}
