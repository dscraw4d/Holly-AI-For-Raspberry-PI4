#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "documents.h"
#include "mind.h"
#include "sha256.h"
#include "http_server.h"
#include "telnet.h"
#define SECTORS 160000u
static uint8_t media[SECTORS][512];
static struct holly_documents docs,reboot;
static char output[524288],cmd[8256];static unsigned used,reads,writes,serial;static int fail_lba=-1;
static int rd(uint32_t l,uint8_t*b,void*c){(void)c;assert(l<SECTORS);reads++;memcpy(b,media[l],512);return 0;}
static int wr(uint32_t l,const uint8_t*b,void*c){(void)c;assert(l<SECTORS);writes++;if((int)l==fail_lba)return -1;memcpy(media[l],b,512);return 0;}
static void emit(const char*s,void*c){(void)c;assert(used+strlen(s)<sizeof output);strcpy(output+used,s);used+=(unsigned)strlen(s);}
static void clear(void){used=0;output[0]=0;}
static void command(const char*q,const char*expected){clear();holly_documents_command(&docs,q,emit,0);if(!strstr(output,expected)){fprintf(stderr,"%.80s: %s\n",q,output);assert(0);}}
static void upload(unsigned id,const char*title,const char*text,unsigned n){
 uint8_t hash[32];char hex[65];holly_sha256_hash(text,n,hash);for(unsigned i=0;i<32;i++)sprintf(hex+i*2,"%02x",hash[i]);
 snprintf(cmd,sizeof cmd,"doc begin %u %s %s",n,hex,title);command(cmd,"DOC READY");
 for(unsigned off=0;off<n;off+=4096){unsigned count=n-off;if(count>4096)count=4096;
  unsigned at=(unsigned)sprintf(cmd,"doc put %u %u ",id,off);for(unsigned i=0;i<count;i++)sprintf(cmd+at+i*2,"%02x",(unsigned char)text[off+i]);command(cmd,"DOC ACK");}
 snprintf(cmd,sizeof cmd,"doc commit %u",id);command(cmd,"DOC COMMITTED");
}
static int reader(struct holly_session*s,const char*q,unsigned flags,holly_emit_fn fn,void*c){return holly_documents_chat(&docs,s,q,flags,fn,c);}
static void ask(struct holly_session*s,const char*q,const char*expected,int guest){
 clear();int r=guest?holly_conversation_only(s,q,emit,0):holly_turn(s,q,emit,0);
 if(r||!strstr(output,expected)){fprintf(stderr,"Question %s: expected %s; got %s\n",q,expected,output);assert(0);}
}
static int web_write(const uint8_t*b,size_t n,void*c){(void)c;assert(used+n<sizeof output);memcpy(output+used,b,n);used+=(unsigned)n;output[used]=0;return 0;}
static int random_bytes(uint8_t*b,size_t n,void*c){(void)c;for(size_t i=0;i<n;i++)b[i]=(uint8_t)++serial;return 0;}
static void http(struct holly_web_server*h,const char*q,const char*expected){clear();assert(!holly_web_start(web_write,0,h));assert(holly_web_feed((const uint8_t*)q,(unsigned)strlen(q),h)==1);assert(strstr(output,expected));}
static void post(struct holly_web_server*h,const char*token,const char*body,const char*expected){char request[1024];snprintf(request,sizeof request,"POST /chat HTTP/1.1\r\nHost: 192.168.1.2\r\nContent-Type: text/plain\r\nX-Holly-Session: %s\r\nContent-Length: %zu\r\n\r\n%s",token,strlen(body),body);http(h,request,expected);}
int main(void){
 struct holly_block_ops io={rd,wr,0};assert(!holly_documents_format(&docs,&io,0,SECTORS));assert(docs.slots>=2);
 unsigned n=70u*4096u;char *book=malloc(n);assert(book);memset(book,' ',n);
 const char *intro="Test fixture, not Red Dwarf canon. Lister was stranded on Mimas with a purple suitcase. Rimmer waited beside a silver taxi.\n\n";
 memcpy(book,intro,strlen(intro));
 const char *later="Lister found his purple suitcase again on Mimas after a long search. This is another invented fixture passage.\n";
 memcpy(book+4096,later,strlen(later));
 const char *boundary="QUESTION: What is the fixture checkpoint?\nANSWER: A marker split across two pages.\n";memcpy(book+64u*4096u-4u,boundary,strlen(boundary));
 upload(1,"Infinity Welcomes Carefull drivers",book,n);free(book);
 const char *qa="QUESTION: What colour is Rimmer's notebook?\nANSWER: Rimmer's notebook is silver in this invented fixture.\nQUESTION: What colour is Lister's notebook?\nANSWER: Lister's notebook is purple in this invented fixture.\n";
 upload(2,"Crew Test Reference",qa,(unsigned)strlen(qa));
 struct holly_session owner,guest;holly_session_init(&owner);holly_session_init(&guest);lesson_count=0;holly_set_memory(0,0);holly_set_document_chat(reader);
 ask(&owner,"reading status","queued 2",0);
 ask(&owner,"reading pause","paused",0);unsigned before=reads;holly_documents_read_step(&docs);assert(before==reads);
 ask(&guest,"reading resume","requires SSH",1);assert(docs.reading_paused);
 ask(&owner,"reading resume","automatic",0);
 for(unsigned i=0;i<64;i++)holly_documents_read_step(&docs);
 assert(docs.doc[0].reading_page==64);assert(!holly_documents_mount_span(&reboot,&io,0,SECTORS));assert(reboot.doc[0].reading_page==64&&reboot.doc[0].reading_match==4);
 docs=reboot;for(unsigned i=0;i<7;i++)holly_documents_read_step(&docs);
 assert(docs.doc[0].reading_page==70&&docs.doc[1].reading_qa==2&&docs.doc[1].reading_kind==4);
 assert(!holly_documents_mount_span(&reboot,&io,0,SECTORS));assert(reboot.doc[1].reading_qa==2);
 ask(&owner,"reading status","ready documents 2",0);
 ask(&guest,"What colour is Rimmer's notebook?","notebook is silver",1);assert(guest.document_focus==2);
 ask(&guest,"reading clear","general Red Dwarf",1);
 ask(&owner,"tell me about the novel infinity welcomes careful drivers","I'll keep our questions within this file",0);assert(owner.document_focus==1);
 ask(&owner,"What happened to Lister on Mimas?","purple suitcase",0);assert(!strstr(output,"uploaded document"));
 ask(&owner,"source","uploaded document 1",0);
 ask(&owner,"tell me more","purple suitcase",0);assert(owner.document_next>1);
 ask(&owner,"What is Rimmer's programming language?","haven't found a passage",0);
 ask(&guest,"reading select 2","Crew Test Reference",1);
 ask(&guest,"What colour is Rimmer's notebook?","notebook is silver",1);assert(!strstr(output,"QUESTION:")&&!strstr(output,"notebook is purple"));
 ask(&guest,"reading clear","general Red Dwarf",1);assert(!guest.document_focus);
 ask(&guest,"reading select 1","Infinity Welcomes",1);
 assert(mind_teach("tell me about the novel infinity welcomes carefull drivers","My taught overview of the novel.")>=0);
 ask(&guest,"tell me about the novel infinity welcomes carefull drivers","My taught overview",1);assert(guest.document_focus==1);
 ask(&guest,"What happened to Lister on Mimas?","purple suitcase",1);
 ask(&guest,"chat reset","context cleared",1);assert(!guest.document_focus);
 struct holly_web_server web={0};web.random=random_bytes;http(&web,"GET /session HTTP/1.1\r\nHost: 192.168.1.2\r\n\r\n","200 OK");char token[33];strcpy(token,web.sessions[0].token);
 post(&web,token,"reading select 2","Crew Test Reference");post(&web,token,"What colour is Rimmer's notebook?","notebook is silver");post(&web,token,"reading pause","requires SSH");
 http(&web,"GET /session HTTP/1.1\r\nHost: 192.168.1.2\r\n\r\n","200 OK");assert(!web.sessions[1].chat.document_focus);
 struct holly_telnet telnet;clear();assert(!holly_telnet_start(web_write,0,&telnet));clear();const char*line="reading select 2\n";assert(!holly_telnet_feed((const uint8_t*)line,(unsigned)strlen(line),&telnet));assert(strstr(output,"Crew Test Reference"));
 command("doc delete 2","DELETED");ask(&guest,"reading select 2","ready document",1);
 post(&web,token,"What colour is Rimmer's notebook?","changed or was removed");
 /* Older format-15 catalog entries have zeroed spare bytes: queue automatically. */
 memset(media[1]+410,0,70);holly_sha256_hash(media[1],480,media[1]+480);assert(!holly_documents_mount_span(&reboot,&io,0,SECTORS));assert(reboot.doc[0].reading_page==0);
 holly_documents_read_step(&reboot);assert(reboot.doc[0].reading_page==1);
 for(unsigned i=0;i<63;i++){if(i==62)fail_lba=1;holly_documents_read_step(&reboot);}
 assert(!reboot.ready&&reboot.reading_error==1);fail_lba=-1;
 assert(!holly_documents_mount_span(&reboot,&io,0,SECTORS));assert(reboot.doc[0].reading_page==0);
 const char *conflict="QUESTION: What colour is Rimmer's notebook?\nANSWER: Rimmer's notebook is silver.\nQUESTION: What colour is Rimmer's notebook?\nANSWER: Rimmer's notebook is red.\n";
 upload(2,"Contradictory Test Notes",conflict,(unsigned)strlen(conflict));holly_documents_read_step(&docs);
 holly_session_init(&guest);ask(&guest,"What colour is Rimmer's notebook?","differing answers",1);assert(!guest.document_focus);
 holly_set_document_chat(0);
 printf("Reading queue/checkpoints, legacy migration, title spelling, document scope, Q&A extraction, next passage, unknowns, taught overview, reset/isolation, deletion, HTTP and Telnet passed; %u SD reads/%u writes\n",reads,writes);
}
