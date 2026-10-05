#include "search_disk.h"
#include "search_cache.h"
#include <string.h>
/* Full summaries use private zero-confidence Vault records, followed by a
 * commit manifest. The existing journal verifies every record on SD. */
#define CHUNKS 7u
struct entry {uint32_t id,epoch,hash;};
struct manifest {uint32_t epoch,crc,ids[CHUNKS];unsigned text,url;};
static struct entry entries[HOLLY_SEARCH_CACHE_LIMIT];static unsigned count;
static const struct holly_memory_ops*ops;static void*context;
static uint32_t crc32(const char*p,unsigned n){uint32_t x=0xffffffffu;for(unsigned i=0;i<n;i++){x^=(unsigned char)p[i];for(unsigned b=0;b<8;b++)x=(x>>1)^((0u-(x&1u))&0xedb88320u);}return ~x;}
static void hex(char*out,uint32_t x,unsigned n){const char*h="0123456789abcdef";while(n){out[--n]=h[x&15];x>>=4;}}
static int unhex(const char*p,unsigned n,uint32_t*x){*x=0;for(unsigned i=0;i<n;i++){unsigned v=p[i]>='0'&&p[i]<='9'?(unsigned)(p[i]-'0'):p[i]>='a'&&p[i]<='f'?(unsigned)(p[i]-'a'+10):16;if(v>15)return -1;*x=(*x<<4)|v;}return 0;}
static int decode(const struct holly_memory_item*i,struct manifest*m){
 if(i->confidence||strlen(i->source)!=85||memcmp(i->source,"ddg1 ",5)||!i->text[0]||strlen(i->text)>192)return -1;
 uint32_t tl,ul;if(unhex(i->source+5,8,&m->epoch)||unhex(i->source+13,4,&tl)||unhex(i->source+17,4,&ul)||unhex(i->source+21,8,&m->crc))return -1;
 if(m->epoch<1577836800u||m->epoch>4102444800u||!tl||tl>1200||ul<9||ul>512)return -1;
 m->text=tl;m->url=ul;
 unsigned chunks=(tl+ul+255)/256;
 for(unsigned n=0;n<CHUNKS;n++){if(unhex(i->source+29+n*8,8,&m->ids[n]))return -1;if((n<chunks&&!m->ids[n])||(n>=chunks&&m->ids[n]))return -1;for(unsigned j=0;j<n;j++)if(m->ids[n]&&m->ids[n]==m->ids[j])return -1;}
 return 0;
}
static int payload(const struct manifest*m,char data[1713]){
 unsigned size=m->text+m->url,at=0;
 for(unsigned n=0;n<CHUNKS&&at<size;n++){struct holly_memory_item i;if(ops->get(m->ids[n],&i,context)||i.confidence||strcmp(i.source,"ddg chunk"))return -1;unsigned length=size-at>256?256:size-at;if(strlen(i.text)!=length)return -1;memcpy(data+at,i.text,length);at+=length;}
 data[at]=0;if(at!=size||crc32(data,size)!=m->crc||memcmp(data+m->text,"https://",8))return -1;return 0;
}
int holly_search_cache_available(void){return holly_search_disk_ready()||(ops&&ops->add&&ops->count&&ops->get&&ops->get_at&&ops->forget);}
unsigned holly_search_cache_count(void){return holly_search_disk_ready()?holly_search_disk_count():count;}
static uint32_t hash_topic(const char*q){uint32_t h=2166136261u;while(*q)h=(h^(uint8_t)*q++)*16777619u;return h;}
static int locate(const char*q){uint32_t h=hash_topic(q);for(unsigned n=0;n<count;n++)if(entries[n].hash==h&&ops){struct holly_memory_item i;if(!ops->get(entries[n].id,&i,context)&&!strcmp(i.text,q))return (int)n;}return -1;}
static unsigned oldest(void){unsigned at=0;for(unsigned n=1;n<count;n++)if(entries[n].epoch<entries[at].epoch)at=n;return at;}
void holly_search_cache_bind(const struct holly_memory_ops*memory,void*c){
 (void)holly_search_disk_bind(0,0,0);
 ops=memory;context=c;count=0;memset(entries,0,sizeof entries);if(!holly_search_cache_available())return;
 unsigned total=ops->count(context);for(unsigned n=0;n<total;n++){struct holly_memory_item i;struct manifest m;char data[1713];if(ops->get_at(n,&i,context)||decode(&i,&m)||payload(&m,data))continue;
  int existing=locate(i.text);unsigned slot=existing>=0?(unsigned)existing:count<HOLLY_SEARCH_CACHE_LIMIT?count++:oldest();
  if(existing>=0&&entries[slot].epoch>m.epoch)continue;
  if(existing<0&&entries[slot].id&&entries[slot].epoch>m.epoch)continue;
  entries[slot].id=i.id;entries[slot].epoch=m.epoch;entries[slot].hash=hash_topic(i.text);
 }
}
int holly_search_cache_get(const char*q,char text[1201],char source[513],uint32_t*epoch){
 if(holly_search_disk_ready()){if(holly_search_disk_get(q,text,source,epoch))return 1;if(holly_search_disk_known(q))return 0;}
 int slot=locate(q);if(slot<0||!holly_search_cache_available())return 0;
 struct holly_memory_item i;struct manifest m;char data[1713];if(ops->get(entries[slot].id,&i,context)||strcmp(i.text,q)||decode(&i,&m)||payload(&m,data))return 0;
 memcpy(text,data,m.text);text[m.text]=0;memcpy(source,data+m.text,m.url);source[m.url]=0;*epoch=m.epoch;return 1;
}
static int remove_id(uint32_t id){
 struct holly_memory_item i;struct manifest m;char data[1713];if(ops->get(id,&i,context)||decode(&i,&m)||payload(&m,data))return -1;
 /* Delete the manifest first: chunks can never form a partly recalled entry. */
 if(ops->forget(id,context))return -1;
 for(unsigned n=0;n<CHUNKS&&m.ids[n];n++)(void)ops->forget(m.ids[n],context);
 return 0;
}
int holly_search_cache_forget(const char*q){if(holly_search_disk_ready()){int old=locate(q);if(old>=0&&ops){struct holly_memory_item item;if(!ops->get(entries[old].id,&item,context)&&remove_id(entries[old].id))return -1;}return holly_search_disk_forget(q);}int slot=locate(q);if(slot<0)return 1;if(remove_id(entries[slot].id))return -1;entries[slot]=entries[--count];memset(entries+count,0,sizeof entries[0]);return 0;}
int holly_search_cache_save(const char*q,const char*text,const char*source,uint32_t epoch){
 if(holly_search_disk_ready())return holly_search_disk_save(q,text,source,epoch);
 if(!holly_search_cache_available()||!q||!*q||strlen(q)>192||!text||!*text||strlen(text)>1200||!source||strlen(source)<9||strlen(source)>512||memcmp(source,"https://",8)||epoch<1577836800u||epoch>4102444800u)return -1;
 if(locate(q)<0&&count==HOLLY_SEARCH_CACHE_LIMIT)return -1;
 unsigned tl=(unsigned)strlen(text),ul=(unsigned)strlen(source),size=tl+ul,at=0,written=0;char data[1713],chunk[257],tag[97]="ddg1 ";uint32_t ids[CHUNKS]={0},commit=0;
 memcpy(data,text,tl);memcpy(data+tl,source,ul);data[size]=0;
 for(unsigned n=0;at<size;n++){unsigned length=size-at>256?256:size-at;memcpy(chunk,data+at,length);chunk[length]=0;if(ops->add(chunk,"ddg chunk",0,&ids[n],context))goto fail;written++;at+=length;}
 hex(tag+5,epoch,8);hex(tag+13,tl,4);hex(tag+17,ul,4);hex(tag+21,crc32(data,size),8);for(unsigned n=0;n<CHUNKS;n++)hex(tag+29+n*8,ids[n],8);tag[85]=0;
 if(ops->add(q,tag,0,&commit,context))goto fail;
 {struct holly_memory_item check;struct manifest m;char verify[1713];if(ops->get(commit,&check,context)||decode(&check,&m)||payload(&m,verify)||memcmp(verify,data,size))goto fail;}
 {int previous=locate(q);unsigned slot=previous>=0?(unsigned)previous:count<HOLLY_SEARCH_CACHE_LIMIT?count++:oldest();uint32_t old=entries[slot].id;
  entries[slot].id=commit;entries[slot].epoch=epoch;entries[slot].hash=hash_topic(q);if(old)(void)remove_id(old);}
 return 0;
fail:
 if(commit)(void)ops->forget(commit,context);
 for(unsigned n=0;n<written;n++)(void)ops->forget(ids[n],context);
 return -1;
}
void holly_search_cache_date(uint32_t stamp,char out[21]){
 unsigned days=stamp/86400,y=1970;for(;;){unsigned span=365+(y%4==0&&(y%100!=0||y%400==0));if(days<span)break;days-=span;y++;}
 const unsigned lens[]={31,28,31,30,31,30,31,31,30,31,30,31};unsigned mo=0;while(mo<11){unsigned span=lens[mo]+(mo==1&&y%4==0&&(y%100!=0||y%400==0));if(days<span)break;days-=span;mo++;}
 unsigned v[]={y,mo+1,days+1,(stamp%86400)/3600,(stamp%3600)/60};unsigned at=0;for(unsigned n=0;n<5;n++){unsigned width=n==0?4:2;for(unsigned i=0;i<width;i++){out[at+width-1-i]=(char)('0'+v[n]%10);v[n]/=10;}at+=width;if(n<4)out[at++]=n<2?'-':n==2?' ':':';}memcpy(out+at," UTC",5);
}

unsigned holly_search_cache_capacity(void){return holly_search_disk_ready()?256:HOLLY_SEARCH_CACHE_LIMIT;}
int holly_search_cache_bind_disk(const struct holly_block_ops *io,uint32_t first,uint32_t span){
 if(holly_search_disk_bind(io,first,span))return -1;
 /* Legacy commits are retired only after verified new disk commits. */
 for(unsigned n=0;n<count;n++){struct holly_memory_item i;struct manifest m;char data[1713],text[1201],url[513];
  if(!ops||ops->get(entries[n].id,&i,context)||decode(&i,&m)||payload(&m,data))continue;
  int known=holly_search_disk_known(i.text);if(known){uint32_t stamp;if(known==2||holly_search_disk_get(i.text,text,url,&stamp))(void)remove_id(entries[n].id);continue;}
  memcpy(text,data,m.text);text[m.text]=0;memcpy(url,data+m.text,m.url);url[m.url]=0;
  if(holly_search_disk_save(i.text,text,url,m.epoch))return -1;
  (void)remove_id(entries[n].id);
 }return 0;
}
