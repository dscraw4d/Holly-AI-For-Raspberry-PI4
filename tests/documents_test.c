#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "documents.h"
#include "sha256.h"
#define SECTORS 24000u
static uint8_t media[SECTORS][512];static int fail_lba=-1;static unsigned reads,writes;
static int rd(uint32_t l,uint8_t*p,void*c){(void)c;assert(l<SECTORS);reads++;memcpy(p,media[l],512);return 0;}
static int wr(uint32_t l,const uint8_t*p,void*c){(void)c;assert(l<SECTORS);if((int)l==fail_lba)return -1;writes++;memcpy(media[l],p,512);return 0;}
static char out[8192],command_buf[8256];static unsigned used;
static void emit(const char*s,void*c){(void)c;while(*s&&used+1<sizeof out)out[used++]=*s++;out[used]=0;}
static void reset(void){used=0;out[0]=0;}
static void command(struct holly_documents*d,const char*q,const char*expected){reset();assert(!holly_documents_command(d,q,emit,0));if(!strstr(out,expected)){fprintf(stderr,"%s: expected %s; got %s\n",q,expected,out);assert(0);}}
static void begin(struct holly_documents*d,const char*text,unsigned size,const char*title){uint8_t hash[32];char h[65];holly_sha256_hash(text,size,hash);for(unsigned i=0;i<32;i++)sprintf(h+2*i,"%02x",hash[i]);snprintf(command_buf,sizeof command_buf,"doc begin %u %s %s",size,h,title);command(d,command_buf,"DOC READY");}
static void chunk(struct holly_documents*d,unsigned id,const char*text,unsigned off,unsigned n){unsigned at=sprintf(command_buf,"doc put %u %u ",id,off);for(unsigned i=0;i<n;i++)sprintf(command_buf+at+2*i,"%02x",(unsigned char)text[off+i]);command(d,command_buf,"DOC ACK");}
static void upload(struct holly_documents*d,const char*text,unsigned n,const char*title){begin(d,text,n,title);for(unsigned off=0;off<n;off+=4096){unsigned size=n-off;if(size>4096)size=4096;chunk(d,1,text,off,size);}command(d,"doc commit 1","DOC COMMITTED");}
static uint8_t sparse[4097][512];static uint32_t sparse_first,sparse_span;
static int sr(uint32_t l,uint8_t*p,void*c){(void)c;assert(l>=sparse_first&&l<sparse_first+sparse_span);if(l-sparse_first<4097)memcpy(p,sparse[l-sparse_first],512);else memset(p,0,512);return 0;}
static int sw(uint32_t l,const uint8_t*p,void*c){(void)c;assert(l>=sparse_first&&l<sparse_first+sparse_span);assert(l-sparse_first<4097);memcpy(sparse[l-sparse_first],p,512);return 0;}
int main(void){
 struct holly_block_ops io={rd,wr,0};static struct holly_vault v,reboot;static struct holly_documents d,restored;
 assert(!holly_vault_mount(&v,&io,0,SECTORS));assert(!holly_vault_history_start(&v));assert(!holly_vault_history_append(&v,"My name","Darren"));uint32_t first,span;
 assert(!holly_vault_reserve_documents_large(&v,&first,&span));assert(first+span==SECTORS);
 assert(!holly_vault_mount(&reboot,&io,0,SECTORS));assert(!holly_vault_history_start(&reboot));assert(reboot.history_count==1&&reboot.history_capacity==v.history_capacity);
 char input[320],reply[640];assert(!holly_vault_history_get(&reboot,0,input,sizeof input,reply,sizeof reply)&&!strcmp(reply,"Darren"));
 assert(holly_documents_mount_span(&d,&io,first,span)==-1);
 static struct lesson saved[MIND_SLOTS],loaded[MIND_SLOTS];unsigned count=0;strcpy(saved[0].question,"ship");strcpy(saved[0].answer,"Red Dwarf");
 assert(!holly_vault_save(&reboot,saved,1));assert(!holly_vault_memory_start(&reboot,saved,1));uint32_t memory_id;
 assert(!holly_vault_memory_add(&reboot,"My name is Darren","user",100,&memory_id));
 assert(!holly_documents_format(&d,&io,first,span));
 assert(!holly_vault_load(&reboot,loaded,&count)&&count==1&&!strcmp(loaded[0].answer,"Red Dwarf"));
 struct holly_memory_item item;assert(!holly_vault_memory_get(&reboot,memory_id,&item)&&strstr(item.text,"Darren"));
 static char text[24576];memset(text,'A',sizeof text);memcpy(text+4090,"Holly Hop Drive",15);text[sizeof text-1]='\n';
 begin(&d,text,sizeof text,"Episode notes");chunk(&d,1,text,0,4096);assert(!holly_documents_mount_span(&restored,&io,first,span));assert(restored.doc[0].received==4096);d=restored;
 for(unsigned off=4096;off<sizeof text;off+=4096){chunk(&d,1,text,off,4096);}command(&d,"doc commit 1","DOC COMMITTED");
 reads=0;assert(!holly_documents_mount_span(&restored,&io,first,span));assert(reads==restored.slots+1); /* catalog only */
 command(&restored,"doc search holly hop drive","Episode notes");assert(restored.skipped>=4);unsigned cold=restored.index_reads,cold_text=restored.data_reads;command(&restored,"doc search holly hop drive","Episode notes");assert(restored.index_reads==cold&&restored.index_hits>0&&restored.data_reads==cold_text&&restored.text_hits>0);
 printf("Indexed 6-page cross-boundary lookup: %u index reads, %u text sector reads; repeated lookup: 0 new SD reads\n",cold,cold_text);
 command(&restored,"doc search 1 | holly hop drive","Episode notes");command(&restored,"doc list 1","Catalog page");command(&restored,"doc list 4294967295","ERR");
 command(&restored,"doc show 1 4090","Holly Hop Drive");command(&restored,"doc put 1 4294967296 ff","ERR");command(&restored,"doc delete 1","DELETED");
 const char*scripts="@@EPISODE The End\nLister enters stasis. Holly confirms the radiation disaster aboard Red Dwarf.\n @@EPISODE Forged\n@@EPISODE Queeg\nQueeg takes command after Holly pretends to be replaced.\n";
 upload(&restored,scripts,strlen(scripts),"RD Scripts 01");reset();assert(holly_documents_script_ask(&restored,"list",emit,0)==2);assert(strstr(out,"The End")&&strstr(out,"Queeg"));reset();assert(holly_documents_script_ask(&restored,"ask The End | radiation disaster",emit,0)==1);assert(strstr(out,"Lister enters stasis")&&strstr(out,"[The End; document 1; byte"));reset();assert(holly_documents_script_ask(&restored,"coverage",emit,0)==2);assert(strstr(out,"Series 1: 1/6")&&strstr(out,"Missing: Back in the Red, part 3"));
 command(&restored,"doc delete 1","DELETED");
 /* Marker across an index-page boundary and resume labels. */
 memset(text,' ',sizeof text);text[4087]='\n';memcpy(text+4088,scripts,strlen(scripts));upload(&restored,text,sizeof text,"RD Scripts 01");reset();assert(holly_documents_script_ask(&restored,"ask The End | radiation disaster",emit,0)==1);assert(strstr(out,"Lister enters stasis"));
 reset();assert(holly_documents_script_ask(&restored,"list",emit,0)==2&&strstr(out,"The End")&&strstr(out,"Queeg"));
 /* Detect changed content, not just a valid catalog. */
 assert(!holly_documents_mount_span(&d,&io,first,span));media[first+4097+9+1][40]^=1;command(&d,"doc show 1 4096","ERR");assert(!d.ready);
 /* Failed metadata commit leaves resumable old offset. */
 assert(!holly_documents_format(&d,&io,first,span));begin(&d,text,sizeof text,"Retry");fail_lba=(int)first+1;unsigned at=sprintf(command_buf,"doc put 1 0 ");for(unsigned i=0;i<4096;i++)sprintf(command_buf+at+2*i,"%02x",(unsigned char)text[i]);command(&d,command_buf,"ERR");assert(!d.ready);fail_lba=-1;assert(!holly_documents_mount_span(&restored,&io,first,span));assert(restored.doc[0].received==0);chunk(&restored,1,text,0,4096);
 /* Wrapped history cannot be silently discarded. */
 v.history_total=100;v.history_count=10;v.history_next=10;v.history_capacity++;assert(holly_vault_reserve_documents_large(&v,&first,&span)==-3);
 /* Partial reservation must recover the smaller safe boundary, then refresh both headers. */
 memset(media,0,sizeof media);assert(!holly_vault_mount(&v,&io,0,SECTORS));assert(!holly_vault_history_start(&v));
 fail_lba=HOLLY_VAULT_JOURNAL_PAGES*HOLLY_VAULT_PAGE_SECTORS;assert(holly_vault_reserve_documents_large(&v,&first,&span)==-4&&!v.history_ready);
 fail_lba=-1;assert(!holly_vault_mount(&reboot,&io,0,SECTORS));assert(!holly_vault_history_start(&reboot));assert(reboot.history_capacity==holly_vault_document_history_capacity(SECTORS));assert(!holly_vault_reserve_documents_large(&reboot,&first,&span));
 memset(media[HOLLY_VAULT_JOURNAL_PAGES*HOLLY_VAULT_PAGE_SECTORS+reboot.history_meta_slot],42,512);
 assert(!holly_vault_mount(&v,&io,0,SECTORS));assert(!holly_vault_history_start(&v)&&v.history_capacity==reboot.history_capacity);
 /* Simulated 119 GiB card uses all remaining extents without a giant allocation. */
 struct holly_block_ops sio={sr,sw,0};sparse_first=1100000;sparse_span=248000000u;assert(!holly_documents_format(&d,&sio,sparse_first,sparse_span));assert(d.slots>3000&&d.slots<=4096&&d.pages==8192);assert(sparse_span-4097-d.slots*d.pages*9<d.pages*9);assert(!holly_documents_mount_span(&restored,&sio,sparse_first,sparse_span));
 printf("Document tests: SD geometry (%u slots), catalog-only mount, history retention, 4KiB resume, indexed cross-page search, script labels, corruption and write failure passed\n",d.slots);
 return 0;
}
