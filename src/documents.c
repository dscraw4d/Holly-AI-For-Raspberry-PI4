/* SD-backed format 15. Catalog in RAM;
 text, checksummed pages and indexes on SD. */
#include "documents.h"
#include "episodes.h"
#include "sha256.h"
#include <string.h>
#define INTERNAL_WEB_TITLE "!Holly Internal Web References v1"
static int internal(const struct holly_document*x){return !strcmp(x->title,INTERNAL_WEB_TITLE);}
static int eq(const char*a,const char*b){return !strcmp(a,b);
}
static unsigned len(const char*s){return (unsigned)strlen(s);
}
static int starts(const char*a,const char*b){while(*b)if(*a++!=*b++)return 0;return 1;
}
static char low(char c){return c>='A'&&c<='Z'?(char)(c+32):c;
}
static int contains(const char*h,unsigned n,const char*q){unsigned m=len(q);
for(unsigned i=0;
m&&i+m<=n;
i++){unsigned j=0;
while(j<m&&low(h[i+j])==low(q[j]))j++;
if(j==m)return 1;
}return 0;
}
static uint32_t get(const uint8_t*p){return p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static void put(uint8_t*p,uint32_t n){for(unsigned i=0;
i<4;
i++)p[i]=(uint8_t)(n>>(8*i));
}
static int ascii(const uint8_t*p,unsigned n,int title){for(unsigned i=0;
i<n;
i++)if((p[i]<32||p[i]>126)&&(title||(p[i]!=9&&p[i]!=10)))return 0;
return 1;
}
static int hex(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
}
static int unhex(const char*s,uint8_t*p,unsigned n){for(unsigned i=0;
i<n;
i++){int a=hex(s[i*2]),b=hex(s[i*2+1]);
if(a<0||b<0)return -1;
p[i]=(uint8_t)(16*a+b);
}return 0;
}
static int num(const char**s,unsigned*n){const char*p=*s;
uint64_t v=0;
if(*p<'0'||*p>'9')return -1;
while(*p>='0'&&*p<='9'){v=v*10+(unsigned)(*p++-'0');
if(v>UINT32_MAX)return -1;
}*n=(unsigned)v;
*s=p;
return 0;
}
static void number(holly_emit_fn emit,void*ctx,unsigned n){char b[12];
unsigned i=0;
do{b[i++]=(char)('0'+n%10);
n/=10;
}while(n);
char r[12];
unsigned j=0;
while(i)r[j++]=b[--i];
r[j]=0;
emit(r,ctx);
}
static int checked(const uint8_t*b){uint8_t h[32];
holly_sha256_hash(b,480,h);
return holly_tag_equal(h,b+480,32);
}
static int write_checked(struct holly_documents*d,uint32_t lba,uint8_t*b){uint8_t r[512];
holly_sha256_hash(b,480,b+480);
if(d->io.write(lba,b,d->io.context)||d->io.read(lba,r,d->io.context)||memcmp(b,r,512)){d->ready=0;
return -1;
}return 0;
}
static uint32_t page_lba(struct holly_documents*d,unsigned id,unsigned page){return d->first+HOLLY_DOC_CATALOG+(id*d->pages+page)*9u;
}
static void bloom_add(uint8_t*b,unsigned bytes,const char*s,unsigned n){for(unsigned i=0;
i+2<n;
i++){uint32_t h=2166136261u;
for(unsigned j=0;
j<3;
j++)h=(h^(uint8_t)low(s[i+j]))*16777619u;
unsigned a=h%(bytes*8),c=(h>>16)%(bytes*8);
b[a/8]|=(uint8_t)(1u<<(a%8));
b[c/8]|=(uint8_t)(1u<<(c%8));
}}
static int bloom_has(const uint8_t*b,unsigned bytes,const char*s,unsigned n){for(unsigned i=0;
i+2<n;
i++){uint32_t h=2166136261u;
for(unsigned j=0;
j<3;
j++)h=(h^(uint8_t)low(s[i+j]))*16777619u;
unsigned a=h%(bytes*8),c=(h>>16)%(bytes*8);
if(!(b[a/8]&(1u<<(a%8)))||!(b[c/8]&(1u<<(c%8))))return 0;
}return 1;
}
static int meta(struct holly_documents*d,unsigned id){uint8_t b[512]={0};
struct holly_document*x=&d->doc[id];
memcpy(b,"HLYDOC15",8);
put(b+8,x->state);
put(b+12,x->size);
put(b+16,x->received);
memcpy(b+20,x->hash,32);
memcpy(b+52,x->title,81);
memcpy(b+133,x->episode,81);
memcpy(b+214,x->bloom,64);
put(b+278,x->tail_size);
memcpy(b+282,x->tail,128);
memcpy(b+410,"RDR1",4);
put(b+414,x->reading_page);put(b+418,x->reading_lines);
put(b+422,x->reading_qa);put(b+426,x->reading_kind);put(b+430,x->reading_match);
return write_checked(d,d->first+1+id,b);
}
int holly_documents_mount_span(struct holly_documents*d,const struct holly_block_ops*io,uint32_t first,uint32_t sectors){
 if(!d||!io||!io->read||!io->write||sectors<=HOLLY_DOC_CATALOG||first>UINT32_MAX-sectors)return -1;

 memset(d,0,sizeof *d);
d->io=*io;
d->first=first;
d->sectors=sectors;
uint8_t b[512];
if(io->read(first,b,io->context)||memcmp(b,"HLYDBK15",8)||!checked(b)||get(b+8)!=sectors)return -1;

 d->pages=get(b+12);
d->slots=get(b+16);
if(!d->pages||d->pages>UINT32_MAX/4096u||!d->slots||d->slots>HOLLY_DOC_MAX||(uint64_t)d->pages*d->slots*9+HOLLY_DOC_CATALOG>sectors)return -1;

 for(unsigned id=0;
id<d->slots;
id++){if(io->read(first+1+id,b,io->context))return -1;
if(memcmp(b,"HLYDOC15",8)){unsigned empty=1;for(unsigned j=0;j<480;j++)if(b[j])empty=0;if(!empty)d->doc[id].state=3;continue;}
struct holly_document*x=&d->doc[id];
x->state=3;
if(!checked(b))continue;
unsigned state=get(b+8),size=get(b+12),received=get(b+16);
if(!state){x->state=0;
continue;
}if((state!=1&&state!=2)||!size||size>d->pages*4096u||received>size||(state==2&&received!=size)||(received%4096u&&received!=size)||b[132]||b[213]||get(b+278)>128)continue;
x->state=state;
x->size=size;
x->received=received;
memcpy(x->hash,b+20,32);
memcpy(x->title,b+52,81);
memcpy(x->episode,b+133,81);
memcpy(x->bloom,b+214,64);
x->tail_size=get(b+278);
memcpy(x->tail,b+282,128);
if(!memcmp(b+410,"RDR1",4)&&get(b+414)<=(size+4095u)/4096u&&get(b+426)<=4&&get(b+430)<9){
 x->reading_page=get(b+414);x->reading_lines=get(b+418);x->reading_qa=get(b+422);
 x->reading_kind=get(b+426);x->reading_match=get(b+430);
}
if(!ascii((uint8_t*)x->title,len(x->title),1)||!ascii((uint8_t*)x->episode,len(x->episode),1))x->state=3;
}
 d->ready=1;
return 0;

}
int holly_documents_mount(struct holly_documents*d,const struct holly_block_ops*io,uint32_t first){uint8_t b[512];
if(!io||!io->read||io->read(first,b,io->context)||memcmp(b,"HLYDBK15",8))return -1;
return holly_documents_mount_span(d,io,first,get(b+8));
}
int holly_documents_format(struct holly_documents*d,const struct holly_block_ops*io,uint32_t first,uint32_t sectors){
 if(!d||!io||!io->read||!io->write||sectors<=HOLLY_DOC_CATALOG+9*32u||first>UINT32_MAX-sectors)return -1;
memset(d,0,sizeof *d);
d->io=*io;
d->first=first;
d->sectors=sectors;
unsigned remaining=sectors-HOLLY_DOC_CATALOG;
d->pages=8192;
if(remaining/9<d->pages)d->pages=remaining/9;
d->slots=remaining/(d->pages*9);
if(d->slots>HOLLY_DOC_MAX){d->slots=HOLLY_DOC_MAX;
d->pages=remaining/(9*HOLLY_DOC_MAX);
}d->ready=1;
uint8_t b[512]={0};

 /* Invalidate superblock before destructive catalog reset;
 never mount a partial format. */
 if(write_checked(d,first,b))return -1;
for(unsigned id=0;
id<d->slots;
id++)if(write_checked(d,first+1+id,b))return -1;
memcpy(b,"HLYDBK15",8);
put(b+8,sectors);
put(b+12,d->pages);
put(b+16,d->slots);
return write_checked(d,first,b);

}
static int load(struct holly_documents*d,unsigned id,unsigned page){
 if(id>=d->slots||internal(&d->doc[id]))return -1;
 if(d->cache_valid&&d->cache_id==id&&d->cache_page==page)return 0;
struct holly_doc_text_cache*cached=&d->text_cache[(page+id*3u)%8u];
if(cached->valid&&cached->id==id&&cached->page==page){memcpy(d->cache,cached->text,cached->n+1);memcpy(d->episode,cached->episode,81);d->cache_valid=1;d->cache_id=id;d->cache_page=page;d->text_hits++;return 0;}
struct holly_document*x=&d->doc[id];
unsigned off=page*4096u,n=x->received>off?x->received-off:0;
if(n>4096)n=4096;
if(!n)return -1;
uint32_t lba=page_lba(d,id,page);
uint8_t b[512],hash[32];
d->cache_valid=0;

 if(d->io.read(lba,b,d->io.context)||memcmp(b,"HLYCHK15",8)||!checked(b)||get(b+8)!=off||get(b+12)!=n||memcmp(b+16,x->hash,32)||b[416])goto bad;
memcpy(hash,b+48,32);
memcpy(d->episode,b+336,81);

 for(unsigned i=0;
i<(n+511)/512;
i++){uint8_t s[512];
if(d->io.read(lba+1+i,s,d->io.context))goto bad;
unsigned count=n-i*512;
if(count>512)count=512;
memcpy(d->cache+i*512,s,count);
d->data_reads++;
}
 uint8_t actual[32];
holly_sha256_hash(d->cache,n,actual);
if(memcmp(hash,actual,32))goto bad;
d->cache[n]=0;
d->cache_id=id;
d->cache_page=page;
d->cache_valid=1;
cached->valid=1;cached->id=id;cached->page=page;cached->n=n;memcpy(cached->text,d->cache,n+1);memcpy(cached->episode,d->episode,81);
return 0;

bad:d->ready=0;
return -1;

}
static int read_text(struct holly_documents*d,unsigned id,unsigned off,char*out,unsigned n){struct holly_document*x=&d->doc[id];
if(off>x->received||n>x->received-off)return -1;
while(n){unsigned page=off/4096,at=off%4096,count=4096-at;
if(count>n)count=n;
if(load(d,id,page))return -1;
memcpy(out,d->cache+at,count);
off+=count;
out+=count;
n-=count;
}return 0;
}
static int possible(struct holly_documents*d,unsigned id,unsigned page,const char*q){unsigned n=len(q);
if(n<3)return 1;
uint32_t lba=page_lba(d,id,page);
struct holly_doc_index*c=&d->index[lba%1024];
if(c->lba!=lba){uint8_t b[512];
if(d->io.read(lba,b,d->io.context)||memcmp(b,"HLYCHK15",8)||!checked(b)||get(b+8)!=page*4096u||memcmp(b+16,d->doc[id].hash,32)){d->ready=0;
return -1;
}memcpy(c->bloom,b+80,256);
c->lba=lba;
d->index_reads++;
}else d->index_hits++;
int yes=bloom_has(c->bloom,256,q,n);
if(!yes)d->skipped++;
return yes;
}
static unsigned marker(const char*s,unsigned n,unsigned at,char*episode){const char*p="@@EPISODE ";
if(at&&s[at-1]!='\n')return 0;
unsigned k=10;
if(at+k>=n||memcmp(s+at,p,k))return 0;
unsigned end=at+k;
while(end<n&&s[end]!='\n'&&end-at<=90)end++;
if(end>=n||s[end]!='\n'||end-at-k>80||end==at+k)return 0;
memcpy(episode,s+at+k,end-at-k);
episode[end-at-k]=0;
return end+1;
}
static unsigned context(struct holly_documents*d,unsigned id,unsigned page){unsigned off=page*4096,n=d->doc[id].size-off;
if(n>4096)n=4096;
unsigned before=off>128?128:off;
if(before&&read_text(d,id,off-before,d->work,before))return 0;
if(read_text(d,id,off,d->work+before,n))return 0;
d->work[before+n]=0;
return before+n;
}
int holly_documents_search(struct holly_documents*d,const char*q,holly_emit_fn emit,void*ctx){if(!d||!d->ready){emit("Holly: Document bank unavailable; use doc format over SSH.\n",ctx);
return 0;
}unsigned start_id=0,end_id=d->slots;
const char*probe=q;unsigned selected;
if(!num(&probe,&selected)&&starts(probe," | ")){if(!selected||selected>d->slots){emit("ERR invalid document ID.\n",ctx);return 0;}start_id=selected-1;end_id=selected;q=probe+3;}
unsigned qn=len(q);
if(qn<2||qn>96){emit("Use find <phrase of 2-96 characters>.\n",ctx);
return 0;
}unsigned found=0,budget=8192;
d->limited=0;

 for(unsigned id=start_id;
id<end_id&&found<3;
id++){struct holly_document*x=&d->doc[id];
if(internal(x)||x->state!=2||!bloom_has(x->bloom,64,q,qn))continue;
for(unsigned page=0;
page*4096u<x->size&&found<3;
page++){if(!budget--){d->limited=1;
goto finish;
}int ok=possible(d,id,page,q);
if(ok<0)goto finish;
if(!ok)continue;
unsigned n=context(d,id,page),off=page*4096u,before=off>128?128:off;

 for(unsigned at=0;
at+qn<=n&&found<3;
at++)if(contains(d->work+at,qn,q)){unsigned absolute=off-before+at;
if(absolute+qn<=off&&page)continue;
unsigned start=absolute>64?absolute-64:0,count=x->size-start;
if(count>224)count=224;
char passage[225];
if(read_text(d,id,start,passage,count))goto finish;
passage[count]=0;
emit("[Document ",ctx);
number(emit,ctx,id+1);
emit(": ",ctx);
emit(x->title,ctx);
emit("; byte ",ctx);
number(emit,ctx,absolute);
emit("]\n",ctx);
emit(passage,ctx);
emit("\n",ctx);
found++;
at+=qn-1;
}
 }}
finish:if(!d->ready)emit("ERR document read/checksum failed; bank disabled until reboot.\n",ctx);
else if(d->limited)emit("Search budget reached; use doc search ID | phrase to select a document.\n",ctx);
else if(!found)emit("Holly: No document passage matches that phrase.\n",ctx);
return (int)found;
}
int holly_documents_command(struct holly_documents*d,const char*line,holly_emit_fn emit,void*ctx){if(!d||!d->ready){emit("ERR document bank unavailable; use doc format over SSH.\n",ctx);
return 0;
}const char*q=line;
if(starts(q,"doc "))q+=4;

 if(eq(q,"status")){emit("DOC bank online: format=15 slots=",ctx);
number(emit,ctx,d->slots);
emit(" bytes=",ctx);
number(emit,ctx,d->pages*4096u);
emit(" chunk=4096 sectors=",ctx);
number(emit,ctx,d->sectors);
emit(". SD-backed; indexed; persistent uploads.\n",ctx);
return 0;
}
 if(eq(q,"stats")){emit("Index reads/hits/skipped, text sector reads: ",ctx);
number(emit,ctx,d->index_reads);
emit("/",ctx);
number(emit,ctx,d->index_hits);
emit("/",ctx);
number(emit,ctx,d->skipped);
emit("/",ctx);
number(emit,ctx,d->data_reads);
emit("; text cache hits: ",ctx);number(emit,ctx,d->text_hits);emit("\n",ctx);
return 0;
}
 if(eq(q,"list")||starts(q,"list ")){
 unsigned page=1;const char*arg=q+4;
 if(*arg&&(*arg++!=' '||num(&arg,&page)||*arg))goto invalid;
 if(!page||page>(d->slots+31u)/32u)goto invalid;
 unsigned end=page*32u;if(end>d->slots)end=d->slots;
 for(unsigned id=(page-1)*32u;id<end;id++){struct holly_document*x=&d->doc[id];if(!x->state||internal(x))continue;number(emit,ctx,id+1);emit(x->state==2?" ready ":x->state==1?" uploading ":" damaged ",ctx);number(emit,ctx,x->received);emit("/",ctx);number(emit,ctx,x->size);emit(" ",ctx);emit(x->title,ctx);emit("\n",ctx);}
 emit("Catalog page ",ctx);number(emit,ctx,page);emit("/",ctx);number(emit,ctx,(d->slots+31u)/32u);emit("; use doc list N for another page.\n",ctx);return 0;
 }
 if(starts(q,"search "))return holly_documents_search(d,q+7,emit,ctx),0;

 if(starts(q,"begin ")){q+=6;
unsigned size;
uint8_t h[32];
if(num(&q,&size)||*q++!=' '||!size||size>d->pages*4096u||len(q)<66||unhex(q,h,32)||q[64]!=' ')goto invalid;
const char*title=q+65;
unsigned tn=len(title);
if(!strcmp(title,INTERNAL_WEB_TITLE)||!tn||tn>80||!ascii((const uint8_t*)title,tn,1))goto invalid;
unsigned id=d->slots;
for(unsigned i=0;
i<d->slots;
i++)if((d->doc[i].state==1||d->doc[i].state==2)&&d->doc[i].size==size&&!memcmp(d->doc[i].hash,h,32)&&eq(d->doc[i].title,title)){id=i;
break;
}if(id==d->slots){for(unsigned i=0;
i<d->slots;
i++)if(!d->doc[i].state){id=i;
break;
}if(id==d->slots){emit("ERR document slots full.\n",ctx);
return 0;
}struct holly_document*x=&d->doc[id];
memset(x,0,sizeof *x);
x->state=1;
x->size=size;
memcpy(x->hash,h,32);
memcpy(x->title,title,tn);
if(meta(d,id))goto io_error;
}emit("DOC READY ",ctx);
number(emit,ctx,id+1);
emit(" ",ctx);
number(emit,ctx,d->doc[id].received);
emit("\n",ctx);
return 0;
}
 if(starts(q,"put ")){q+=4;
unsigned id,off;
if(num(&q,&id)||*q++!=' '||!id||id>d->slots||num(&q,&off)||*q++!=' ')goto invalid;
struct holly_document*x=&d->doc[--id];
if(internal(x))goto invalid;
unsigned hn=len(q),n=x->size-x->received;
if(n>4096)n=4096;
if(x->state!=1||off!=x->received||off%4096||hn!=n*2||!n||unhex(q,(uint8_t*)d->work+x->tail_size,n)||!ascii((uint8_t*)d->work+x->tail_size,n,0))goto invalid;
unsigned before=x->tail_size;
memcpy(d->work,x->tail,before);
uint8_t b[512]={0};
memcpy(b,"HLYCHK15",8);
put(b+8,off);
put(b+12,n);
memcpy(b+16,x->hash,32);
holly_sha256_hash(d->work+before,n,b+48);
bloom_add(b+80,256,d->work,before+n);
memcpy(b+336,x->episode,81);
uint32_t lba=page_lba(d,id,off/4096u);

 for(unsigned i=0;
i<(n+511)/512;
i++){uint8_t sector[512]={0},check[512];
unsigned count=n-i*512;
if(count>512)count=512;
memcpy(sector,d->work+before+i*512,count);
if(d->io.write(lba+1+i,sector,d->io.context)||d->io.read(lba+1+i,check,d->io.context)||memcmp(sector,check,512))goto io_error;
}
 if(write_checked(d,lba,b))goto io_error;
d->index[lba%1024].lba=0;
d->cache_valid=0;
memset(d->text_cache,0,sizeof d->text_cache);
bloom_add(x->bloom,64,d->work,before+n);
for(unsigned at=off?1u:0u;
at<before+n;
at++){unsigned next=marker(d->work,before+n,at,x->episode);
if(next)at=next-1;
}x->tail_size=n<128?n:128;
memcpy(x->tail,d->work+before+n-x->tail_size,x->tail_size);
x->received+=n;
if(meta(d,id))goto io_error;
emit("DOC ACK ",ctx);
number(emit,ctx,x->received);
emit("\n",ctx);
return 0;
}
 if(starts(q,"commit ")||starts(q,"delete ")||starts(q,"show ")){unsigned commit=starts(q,"commit "),del=starts(q,"delete ");
q+=commit||del?7:5;
unsigned id;
if(num(&q,&id)||!id||id>d->slots)goto invalid;
struct holly_document*x=&d->doc[--id];
if(internal(x))goto invalid;
if(del){if(*q)goto invalid;
memset(x,0,sizeof *x);
if(meta(d,id))goto io_error;
d->cache_valid=0;
memset(d->index,0,sizeof d->index);
memset(d->text_cache,0,sizeof d->text_cache);
emit("DOC DELETED (logical deletion).\n",ctx);
return 0;
}if(commit){if(*q||(x->state!=1&&x->state!=2)||x->received!=x->size)goto invalid;
struct holly_sha256 hash;
holly_sha256_init(&hash);
for(unsigned page=0;
page*4096u<x->size;
page++){if(load(d,id,page))goto io_error;
unsigned n=x->size-page*4096u;
if(n>4096)n=4096;
holly_sha256_update(&hash,d->cache,n);
}uint8_t result[32];
holly_sha256_finish(&hash,result);
if(memcmp(result,x->hash,32)){emit("ERR document checksum mismatch; delete and retry.\n",ctx);
return 0;
}x->state=2;
if(meta(d,id))goto io_error;
emit("DOC COMMITTED\n",ctx);
return 0;
}unsigned off=0;
if(*q&&(*q++!=' '||num(&q,&off)||*q))goto invalid;
if(internal(x)||x->state!=2||off>=x->size)goto invalid;
unsigned n=x->size-off;
if(n>512)n=512;
char part[513];
if(read_text(d,id,off,part,n))goto io_error;
part[n]=0;
emit("[Document excerpt]\n",ctx);
emit(part,ctx);
emit("\n",ctx);
return 0;
}
invalid:emit("ERR use doc status/stats/list/search/begin/put/commit/show/delete. Bulk chunks are 4096 bytes.\n",ctx);
return 0;

io_error:d->ready=0;
emit("ERR document write/readback/checksum failed; bank disabled until reboot.\n",ctx);
return 0;
}

static int word(char c){c=low(c);
return (c>='a'&&c<='z')||(c>='0'&&c<='9');
}
static unsigned terms(const char*q,char out[16][24]){
 const char*stops=" what who which when where why how did does do is was were the this that about with from into episode scene script and for are you they them happened happen said say tell me know everything ";
unsigned nt=0;

 for(unsigned i=0;
q[i]&&nt<16;
){while(q[i]&&!word(q[i]))i++;
unsigned n=0;
char t[24];
while(q[i]&&word(q[i])){if(n+1<sizeof t)t[n++]=low(q[i]);
i++;
}t[n]=0;
if(n<3)continue;
char padded[27]=" ";
memcpy(padded+1,t,n);
padded[n+1]=' ';
padded[n+2]=0;
if(contains(stops,len(stops),padded))continue;
unsigned seen=0;
for(unsigned j=0;
j<nt;
j++)if(eq(out[j],t))seen=1;
if(!seen)memcpy(out[nt++],t,n+1);
}
 return nt;

}
/* Page selection uses OR over keywords: a relevant passage need not contain every word. */
static int candidates(struct holly_documents*d,unsigned id,unsigned page,char ts[16][24],unsigned nt){if(!nt)return 1;
for(unsigned i=0;
i<nt;
i++){int r=possible(d,id,page,ts[i]);
if(r<0)return -1;
if(r)return 1;
}return 0;
}
struct walk {holly_reference_offer_fn offer;
void*visitor;
holly_emit_fn emit;
void*ctx;
unsigned seen[HOLLY_EPISODE_COUNT],count,best,best_id,best_at,best_n;
char selected[81],best_title[81],best_text[361],ts[16][24];
unsigned only_id,qa_only,nt,mode,list_start,list_end;
};

static void walk_bank(struct holly_documents*d,struct walk*w){
 unsigned budget=8192;
d->limited=0;

for(unsigned id=0;
id<d->slots&&d->ready;
id++){struct holly_document*x=&d->doc[id];
if(internal(x)||(w->only_id&&id+1!=w->only_id)||(w->qa_only&&x->reading_kind!=4)||x->state!=2||(w->mode&& !starts(x->title,"RD Scripts ")))continue;
 if(w->nt){unsigned candidate=0;for(unsigned i=0;i<w->nt;i++)if(bloom_has(x->bloom,64,w->ts[i],len(w->ts[i])))candidate=1;if(!candidate)continue;}

 for(unsigned page=0;
page*4096u<x->size;
page++){
 if(!budget--){d->limited=1;
return;
}int ok=candidates(d,id,page,w->ts,w->nt);
if(ok<0)return;
if(!ok)continue;

 unsigned n=context(d,id,page),off=page*4096u,before=off>128?128:off;
if(!n)return;
char episode[81];
memcpy(episode,d->episode,81);

 /* Include a page overlap so a short phrase split across pages remains searchable. */
 for(unsigned at=page?1u:0u;
at<n;
){unsigned next=marker(d->work,n,at,episode);
if(next){

 if(w->mode==1&&(!page||next>before)){if(w->count>=w->list_end){d->limited=1;return;}if(w->count>=w->list_start){w->emit("[",w->ctx);
w->emit(episode,w->ctx);
w->emit("; document ",w->ctx);
number(w->emit,w->ctx,id+1);
w->emit("]\n",w->ctx);
}
w->count++;
}
 if(w->mode==2)for(unsigned i=0;
i<HOLLY_EPISODE_COUNT;
i++)if(len(episode)==len(holly_episodes[i].title)&&contains(episode,len(episode),holly_episodes[i].title))w->seen[i]=1;

 at=next;
continue;
}
 unsigned end=at;
while(end<n&&end-at<360){
if(end>at&&d->work[end-1]=='\n'&&(starts(d->work+end,"QUESTION:")||starts(d->work+end,"Question:")))break;
if(marker(d->work,n,end,(char[81]){0}))break;
end++;
if(end-at>=120&&d->work[end-1]=='\n')break;
}if(end==at){at++;
continue;
}
 if(w->mode<1||w->mode==3){unsigned score=0;
for(unsigned i=0;
i<w->nt;
i++)if(contains(d->work+at,end-at,w->ts[i]))score++;

 if(w->mode==3){if(episode[0]&&contains(episode,len(episode),w->selected)&&score>w->best){w->best=score;
w->best_id=id;
w->best_at=off-before+at;
w->best_n=end-at;
memcpy(w->best_title,episode,81);
memcpy(w->best_text,d->work+at,end-at);
w->best_text[end-at]=0;
}}
 else if(!w->nt||score){char source[64]="uploaded document ";
unsigned used=18,v=id+1,k=0;
char digits[12];
do{digits[k++]=(char)('0'+v%10);
v/=10;
}while(v);
while(k)source[used++]=digits[--k];
memcpy(source+used,"; byte ",7);
used+=7;
v=off-before+at;
do{digits[k++]=(char)('0'+v%10);
v/=10;
}while(v);
while(k)source[used++]=digits[--k];
source[used]=0;
w->offer(d->work+at,end-at,episode[0]?episode:x->title,source,(1000u+(id*2654435761u))^(off-before+at),w->visitor);
}
 }
 at=end;

 }
 }
 }
}
void holly_documents_visit_query(holly_reference_offer_fn offer,void*visitor,void*storage,const char*q){struct holly_documents*d=storage;
if(!d||!d->ready||!offer)return;
struct walk w={0};
w.offer=offer;
w.visitor=visitor;
if(q)w.nt=terms(q,w.ts);
walk_bank(d,&w);
}
void holly_documents_visit(holly_reference_offer_fn offer,void*visitor,void*storage){holly_documents_visit_query(offer,visitor,storage,0);
}
int holly_documents_script_ask(struct holly_documents*d,const char*q,holly_emit_fn emit,void*ctx){
 if(!d||!d->ready){emit("Holly: Document bank unavailable; use doc format over SSH.\n",ctx);
return 0;
}struct walk w={0};
w.emit=emit;
w.ctx=ctx;

 if(eq(q,"list")||starts(q,"list ")||eq(q,"coverage")){w.mode=eq(q,"coverage")?2:1;
if(w.mode==1){unsigned page=1;const char*arg=q+4;if(*arg&&(*arg++!=' '||num(&arg,&page)||*arg||!page||page>1000000u))goto usage;w.list_start=(page-1)*64u;w.list_end=page*64u;}
walk_bank(d,&w);
if(w.mode==1){if(!w.count)emit("Holly: No episode scripts uploaded yet.\n",ctx);
}else{for(unsigned series=1;
series<=8;
series++){unsigned have=0,expected=0;
for(unsigned i=0;
i<HOLLY_EPISODE_COUNT;
i++)if(holly_episodes[i].series==series){expected++;
have+=w.seen[i];
}w.count+=have;
emit("Series ",ctx);
number(emit,ctx,series);
emit(": ",ctx);
number(emit,ctx,have);
emit("/",ctx);
number(emit,ctx,expected);
emit(" episode titles indexed\n",ctx);
for(unsigned i=0;
i<HOLLY_EPISODE_COUNT;
i++)if(holly_episodes[i].series==series&&!w.seen[i]){emit("  Missing: ",ctx);
emit(holly_episodes[i].title,ctx);
emit("\n",ctx);
}}emit("A marker confirms indexing, not transcript completeness.\n",ctx);
}if(d->limited)emit("Scan/list budget reached; use script list N for later pages (64 titles/page).\n",ctx);
return (int)w.count;
}
 if(!starts(q,"ask "))goto usage;
q+=4;
const char*bar=q;
while(*bar&&!(bar[0]==' '&&bar[1]=='|'&&bar[2]==' '))bar++;
unsigned n=(unsigned)(bar-q);
if(!*bar||!n||n>80||!bar[3])goto usage;
memcpy(w.selected,q,n);
w.selected[n]=0;
w.nt=terms(bar+3,w.ts);
if(!w.nt){emit("Ask with at least one distinctive word from the scene.\n",ctx);
return 0;
}w.mode=3;
walk_bank(d,&w);

 if(!d->ready){emit("ERR document read/checksum failed.\n",ctx);
return 0;
}if(!w.best){emit(d->limited?"Holly: Search budget reached; narrow the script collection.\n":"Holly: No matching passage for that title and question. Try script list.\n",ctx);
return 0;
}
 emit("Holly: Closest script passage; check the context before treating it as an answer.\n[",ctx);
emit(w.best_title,ctx);
emit("; document ",ctx);
number(emit,ctx,w.best_id+1);
emit("; byte ",ctx);
number(emit,ctx,w.best_at);
emit("]\n",ctx);
emit(w.best_text,ctx);
emit("\n",ctx);
return 1;

usage:emit("Use script coverage, script list or script ask <episode title> | <question>.\n",ctx);
return 0;

}

/* Format-15 spare catalog bytes hold reading checkpoints. Text/index geometry
 * stays unchanged; old ready files enter the queue with a zero checkpoint. */
static void reader_copy(char *out,const char *text,unsigned cap){
 unsigned n=0;if(!cap)return;
 while(text[n]&&n+1<cap){out[n]=text[n];n++;}out[n]=0;
}
static void reader_add(char *out,const char *text,unsigned cap){
 unsigned n=len(out);if(n<cap)reader_copy(out+n,text,cap-n);
}
static const char *reader_kind(unsigned kind){
 return kind==2?"script":kind==3?"book":kind==4?"question-and-answer reference":"text document";
}
void holly_documents_read_step(struct holly_documents*d){
 if(!d||!d->ready||d->reading_paused||d->reading_error||!d->slots)return;
 unsigned chosen=d->slots;
 for(unsigned i=0;i<d->slots;i++){
  unsigned id=(d->reading_cursor+i)%d->slots;struct holly_document*x=&d->doc[id];
  if(!internal(x)&&x->state==2&&x->reading_page<(x->size+4095u)/4096u){chosen=id;break;}
 }
 if(chosen==d->slots)return;
 struct holly_document*x=&d->doc[chosen];unsigned page=x->reading_page;
 if(load(d,chosen,page)){d->reading_error=chosen+1;return;}
 unsigned n=x->size-page*4096u;if(n>4096)n=4096;
 if(!page){
  x->reading_kind=1;
  if(starts(x->title,"RD Scripts ")||contains(x->title,len(x->title),"script"))x->reading_kind=2;
  else if(contains(x->title,len(x->title),"novel")||contains(x->title,len(x->title),"book")||contains(x->title,len(x->title),"Infinity Welcomes")||contains(x->title,len(x->title),"Better Than Life")||contains(x->title,len(x->title),"Last Human")||contains(x->title,len(x->title),"Backwards"))x->reading_kind=3;
 }
 const char *mark="question:";
 for(unsigned i=0;i<n;i++){
  if(d->cache[i]=='\n')x->reading_lines++;
  char c=low(d->cache[i]);
  if(c==mark[x->reading_match])x->reading_match++;
  else x->reading_match=c==mark[0]?1u:0u;
  if(x->reading_match==9){x->reading_qa++;x->reading_match=0;x->reading_kind=4;}
 }
 x->reading_page++;
 if(!(x->reading_page%64u)||x->reading_page==(x->size+4095u)/4096u){
  if(meta(d,chosen)){d->reading_error=chosen+1;return;}
 }
 /* Finish the current document before advancing, keeping page reads sequential. */
 d->reading_cursor=x->reading_page==(x->size+4095u)/4096u?(chosen+1)%d->slots:chosen;
}
static unsigned reader_words(const char*q,char out[16][24]){
 char all[16][24];unsigned n=terms(q,all),used=0;
 const char *ignore=" novel book text txt pdf red dwarf chapter volume part please read reading summary overview synopsis it its him her them more next explain describe can could would should of on at to has have had a an some give long ";
 for(unsigned i=0;i<n;i++){
  char padded[27]=" ";memcpy(padded+1,all[i],len(all[i]));padded[len(all[i])+1]=' ';padded[len(all[i])+2]=0;
  if(contains(ignore,len(ignore),padded))continue;
  if(eq(all[i],"carefull"))reader_copy(all[i],"careful",24);
  memcpy(out[used++],all[i],24);
 }
 return used;
}
static int reader_question(const char*q){
 const char *prefixes[]={"tell ","what ","who ","why ","how ","where ","when ","describe ","explain ","read ","in ","more","next"};
 for(unsigned i=0;i<sizeof prefixes/sizeof prefixes[0];i++)if(starts(q,prefixes[i]))return 1;
 return 0;
}
static void reader_normalize(const char*input,char*out,unsigned cap){
 unsigned n=0;for(unsigned i=0;input[i]&&n+1<cap;i++)out[n++]=low(input[i]);out[n]=0;
}
static void reader_clean(char*out,const char*text,unsigned cap){
 out[0]=0;
 while(*text==' '||*text=='\n'||*text=='\t')text++;
 if(starts(text,"ANSWER:"))text+=7;
 if(starts(text,"Answer:"))text+=7;
 while(*text){
  if(starts(text,"http://")||starts(text,"https://")||starts(text,"www.")){
   while(*text&&*text!=' '&&*text!='\n'&&*text!='\t')text++;
   continue;
  }
  if(*text=='['){const char *at=text+1;if(*at=='S')at++;
   const char *first=at;while(*at>='0'&&*at<='9')at++;
   if(at>first&&*at==']'){text=at+1;continue;}
  }
  char c=*text++;if(c=='\n'||c=='\t')c=' ';
  if(c==' '&&(!out[0]||out[len(out)-1]==' '))continue;
  char one[2]={c,0};reader_add(out,one,cap);
 }
 unsigned n=len(out);while(n&&out[n-1]==' ')out[--n]=0;
}
struct reader_rank {char words[16][24],text[401],source[81];unsigned n,score,offset,after,found,doc_id,qa_only,ambiguous;};
static void reader_offer(const char*text,unsigned size,const char*title,const char*source,unsigned key,void*ctx){
 (void)title;(void)key;struct reader_rank*r=ctx;
 const char *pos=source;while(*pos&&!starts(pos,"; byte "))pos++;
 unsigned off=0;if(*pos){pos+=7;if(num(&pos,&off))return;}
 if(r->after&&off<r->after)return;
 char body[401];if(size>400)size=400;memcpy(body,text,size);body[size]=0;
 unsigned score=0;
 for(unsigned i=0;i<r->n;i++)if(contains(body,size,r->words[i]))score++;
 if(score<r->n||!score)return;
 /* Only a pair with an explicit answer is treated as a Q&A lesson. */
 const char *answer=body;
 if(r->qa_only&&!starts(body,"QUESTION:")&&!starts(body,"Question:"))return;
 if(starts(body,"QUESTION:")||starts(body,"Question:")){
  const char *at=body;while(*at&&!starts(at,"ANSWER:")&&!starts(at,"Answer:"))at++;
  if(!*at)return;
  answer=at+7;score+=16;
 }
 if(r->found&&score==r->score){
  char clean[401];reader_clean(clean,answer,sizeof clean);
  if(!eq(clean,r->text))r->ambiguous=1;
 }
 if(r->found&&score<=r->score)return;
 r->ambiguous=0;
 const char *doc=source+18;if(num(&doc,&r->doc_id))return;
 r->score=score;r->offset=off;r->found=1;
 reader_clean(r->text,answer,sizeof r->text);reader_copy(r->source,source,sizeof r->source);
}
static void reader_status(struct holly_documents*d,holly_emit_fn emit,void*ctx){
 unsigned queued=0,complete=0,active=0;
 for(unsigned i=0;i<d->slots;i++)if(d->doc[i].state==2&&!internal(&d->doc[i])){
  if(d->doc[i].reading_page==(d->doc[i].size+4095u)/4096u)complete++;
  else {queued++;if(!active)active=i+1;}
 }
 emit("Reading: ",ctx);emit(d->reading_error?"error":d->reading_paused?"paused":"automatic",ctx);
 emit("; ready documents ",ctx);number(emit,ctx,complete);emit("; queued ",ctx);number(emit,ctx,queued);emit(".\n",ctx);
 if(active){struct holly_document*x=&d->doc[active-1];emit("Document ",ctx);number(emit,ctx,active);emit(": ",ctx);emit(x->title,ctx);
  emit("; pages ",ctx);number(emit,ctx,x->reading_page);emit("/",ctx);number(emit,ctx,(x->size+4095u)/4096u);emit(".\n",ctx);}
 if(d->reading_error){emit("Reading failed for document ",ctx);number(emit,ctx,d->reading_error);emit("; preserve originals and report diagnostics.\n",ctx);}
 emit("Passage indexes already persist; reading checkpoints resume on reboot. This is document processing, not neural training.\n",ctx);
}
static void reader_emit(struct holly_session*s,const char*reply,const char*source,holly_emit_fn emit,void*ctx){
 holly_reference_clear(&s->reference);holly_lore_clear(&s->lore);
 s->series_focus=0;s->episode_focus[0]=0;s->discussion_profile=0;s->last_memory_id=0;
 reader_copy(s->last_answer,reply,sizeof s->last_answer);
 s->reference.dry_voice=s->personality_enabled;
 if(starts(source,"uploaded document "))holly_reference_banter(&s->reference,reply,s->last_answer,sizeof s->last_answer);
 reader_copy(s->reference.answer,s->last_answer,sizeof s->reference.answer);
 reader_copy(s->reference.sources,source,sizeof s->reference.sources);
 s->expression=HOLLY_SPEAKING;emit("Holly: ",ctx);emit(s->last_answer,ctx);emit("\n",ctx);
}
int holly_documents_chat(struct holly_documents*d,struct holly_session*s,const char*input,unsigned flags,holly_emit_fn emit,void*ctx){
 if(!d||!s||!input||!emit)return 0;
 char q[320];reader_normalize(input,q,sizeof q);
 int control=starts(q,"reading ")||eq(q,"reading");
 if(control&&!d->ready){emit("Reading unavailable: document bank is not mounted. Use storage and doc status over SSH; do not format an existing bank.\n",ctx);return 1;}
 if(!d->ready)return 0;
 if(eq(q,"reading")||eq(q,"reading status")){reader_status(d,emit,ctx);return 1;}
 if(eq(q,"reading pause")||eq(q,"reading resume")){
  if(!(flags&1u)){emit("Holly: Pausing background reading requires SSH.\n",ctx);return 1;}
  d->reading_paused=eq(q,"reading pause");reader_status(d,emit,ctx);return 1;
 }
 if(eq(q,"reading clear")){
  s->document_focus=s->document_next=0;s->document_keywords[0]=0;
  holly_reference_clear(&s->reference);emit("Holly: We're back to general Red Dwarf conversation.\n",ctx);return 1;
 }
 if(eq(q,"reading list")){
  unsigned shown=0;
  for(unsigned i=0;i<d->slots&&shown<32;i++)if(d->doc[i].state==2&&!internal(&d->doc[i])){
   number(emit,ctx,i+1);emit(" ",ctx);emit(d->doc[i].title,ctx);emit(" - ",ctx);emit(reader_kind(d->doc[i].reading_kind),ctx);
   emit("; pages ",ctx);number(emit,ctx,d->doc[i].reading_page);emit("/",ctx);number(emit,ctx,(d->doc[i].size+4095u)/4096u);
   emit("; Q&A markers ",ctx);number(emit,ctx,d->doc[i].reading_qa);emit("\n",ctx);shown++;
  }
  emit("First 32 ready entries; use doc list N over SSH for the full catalog, or reading select ID.\n",ctx);return 1;
 }
 unsigned selected=0;int explicit_select=starts(q,"reading select "),named=0;
 if(explicit_select){const char *at=q+15;if(num(&at,&selected)||*at||!selected||selected>d->slots||d->doc[selected-1].state!=2||internal(&d->doc[selected-1])){emit("Holly: Use reading select ID from a ready document in reading list.\n",ctx);return 1;}}
 else if(control){emit("Use reading status/list/select ID/clear/pause/resume.\n",ctx);return 1;}
 else if(eq(q,"what is my name")||eq(q,"what is my name?")||eq(q,"what's my name?")||eq(q,"do you remember my name?"))return 0;
 else if(!reader_question(q))return 0;
 else {
  char qw[16][24];unsigned nq=reader_words(q,qw),best=0,ties=0;
  for(unsigned id=0;id<d->slots;id++)if(d->doc[id].state==2&&!internal(&d->doc[id])){
   char tw[16][24];unsigned nt=reader_words(d->doc[id].title,tw),hit=0;
   for(unsigned i=0;i<nt;i++)for(unsigned j=0;j<nq;j++)if(eq(tw[i],qw[j])){hit++;break;}
   if(!nt||hit!=nt||(hit<2&&(nt!=1||len(tw[0])<5)))continue;
   if(hit>best){selected=id+1;best=hit;ties=0;}else if(hit==best)ties++;
  }
  if(ties){emit("Holly: More than one uploaded title fits. Choose one with reading select ID.\n",ctx);return 1;}
  named=selected!=0;
 }
 if(selected){
  s->document_focus=selected;s->document_next=0;s->document_keywords[0]=0;
  memcpy(s->document_hash,d->doc[selected-1].hash,32);
 }
 if(!s->document_focus){
  if(flags&2u)return 0;
  struct reader_rank global={0};global.qa_only=1;global.n=reader_words(q,global.words);
  if(global.n<2)return 0;
  struct walk scan={0};scan.qa_only=1;scan.offer=reader_offer;scan.visitor=&global;scan.nt=global.n;
  for(unsigned i=0;i<global.n;i++)memcpy(scan.ts[i],global.words[i],24);
  walk_bank(d,&scan);
  if(!global.found)return 0;
  if(global.ambiguous){reader_emit(s,"I've found differing answers in the uploaded Q&A references. Choose a file with reading select ID.","",emit,ctx);return 1;}
  if(!global.doc_id||global.doc_id>d->slots)return 0;
  s->document_focus=global.doc_id;s->document_next=global.offset+1;
  memcpy(s->document_hash,d->doc[global.doc_id-1].hash,32);
  s->document_keywords[0]=0;
  for(unsigned i=0;i<global.n;i++){if(i)reader_add(s->document_keywords," ",sizeof s->document_keywords);reader_add(s->document_keywords,global.words[i],sizeof s->document_keywords);}
  reader_emit(s,global.text,global.source,emit,ctx);return 1;
 }
 unsigned id=s->document_focus-1;
 if(id>=d->slots||internal(&d->doc[id])||d->doc[id].state!=2||memcmp(s->document_hash,d->doc[id].hash,32)){
  s->document_focus=0;emit("Holly: That document changed or was removed. Select its current entry with reading select ID.\n",ctx);return 1;
 }
 struct holly_document*x=&d->doc[id];
 if(flags&2u)return 0; /* Preserve a user's explicitly taught overview, but keep book context. */
 int more=eq(q,"more")||eq(q,"tell me more")||eq(q,"what else")||eq(q,"what else?")||eq(q,"next")||eq(q,"what happened next?");
 struct reader_rank rank={0};rank.n=explicit_select?0:reader_words(q,rank.words);
 if(named||explicit_select){
  char title_words[16][24];unsigned ntitle=reader_words(x->title,title_words),kept=0;
  for(unsigned i=0;i<rank.n;i++){
   unsigned drop=0;for(unsigned j=0;j<ntitle;j++)if(eq(rank.words[i],title_words[j]))drop=1;
   if(!drop){if(kept!=i)memcpy(rank.words[kept],rank.words[i],24);kept++;}
  }
  rank.n=kept;
 }
 if(more){rank.n=reader_words(s->document_keywords,rank.words);rank.after=s->document_next;}
 if((named||explicit_select)&&!rank.n&&!more){
  char reply[1000]="I've got ";reader_add(reply,x->title,sizeof reply);reader_add(reply,". I'll keep our questions within this file. A passage from the beginning: ",sizeof reply);
  char opening[1025];unsigned count=x->size>1024?1024:x->size;
  if(read_text(d,id,0,opening,count)){emit("Holly: I couldn't read that document safely.\n",ctx);return 1;}opening[count]=0;
  char clean[401];reader_clean(clean,opening,sizeof clean);reader_add(reply,clean,sizeof reply);
  reader_add(reply," Which character, event or phrase shall we explore?",sizeof reply);
  char source[160]="Uploaded document: ";reader_add(source,x->title,sizeof source);reader_emit(s,reply,source,emit,ctx);return 1;
 }
 if(!rank.n){reader_emit(s,"Which character, event or phrase in this file do you mean?","",emit,ctx);return 1;}
 char filter[320]="";for(unsigned i=0;i<rank.n;i++){if(i)reader_add(filter," ",sizeof filter);reader_add(filter,rank.words[i],sizeof filter);}
 struct walk w={0};w.only_id=id+1;w.offer=reader_offer;w.visitor=&rank;w.nt=rank.n;
 for(unsigned i=0;i<rank.n;i++)memcpy(w.ts[i],rank.words[i],24);
 walk_bank(d,&w);
 if(!rank.found){
  reader_emit(s,more?"I haven't found another matching passage in this file. Shall we try a different detail?":"I haven't found a passage in this file that supports that answer. Try a character name or a distinctive phrase.","",emit,ctx);return 1;
 }
 s->document_next=rank.offset+1;reader_copy(s->document_keywords,filter,sizeof s->document_keywords);
 reader_copy(s->topic,x->title,sizeof s->topic);reader_emit(s,rank.text,rank.source,emit,ctx);return 1;
}

int holly_documents_web_area(struct holly_documents*d,struct holly_block_ops*out,uint32_t*first,uint32_t*span){
 if(!d||!d->ready||d->pages*9u<2050u)return -1;
 unsigned slot=d->slots;
 for(unsigned i=0;i<d->slots;i++)if(internal(&d->doc[i])){if(d->doc[i].state!=2||d->doc[i].size!=1||memcmp(d->doc[i].hash,"HOLLY-WEB-AREA-26",17))return -1;slot=i;break;}
 if(slot==d->slots){for(unsigned i=0;i<d->slots;i++)if(!d->doc[i].state){slot=i;break;}if(slot==d->slots)return -1;
  struct holly_document previous=d->doc[slot];struct holly_document*x=&d->doc[slot];memset(x,0,sizeof *x);x->state=2;x->size=x->received=x->reading_page=1;x->reading_kind=1;memcpy(x->hash,"HOLLY-WEB-AREA-26",17);memcpy(x->title,INTERNAL_WEB_TITLE,sizeof INTERNAL_WEB_TITLE);
  if(meta(d,slot)){d->doc[slot]=previous;return -1;}
  uint8_t zero[512]={0};uint32_t lba=d->first+HOLLY_DOC_CATALOG+slot*d->pages*9u;
  if(d->io.write(lba,zero,d->io.context)||d->io.write(lba+1,zero,d->io.context))return -1;
 }
 *out=d->io;*first=d->first+HOLLY_DOC_CATALOG+slot*d->pages*9u;*span=d->pages*9u;return 0;
}
