#include "model_store.h"
#include <string.h>
static uint32_t get(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void put(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static uint32_t crc_more(uint32_t c,const uint8_t *p,unsigned n){while(n--){c^=*p++;for(unsigned b=0;b<8;b++)c=(c>>1)^(0xedb88320u& (0u-(c&1)));}return c;}
uint32_t model_crc(const void *p,unsigned n){return ~crc_more(~0u,p,n);}
static uint32_t base(unsigned bank){return 9u+bank*1019u;}
static int read_bank(struct model_store *s,unsigned bank,uint8_t *out,uint32_t *seq){
 uint8_t h[512],b[512];
 if(s->io.read(base(bank),h,s->io.context)||memcmp(h,"HLYBNK01",8)||get(h+8)!=s->size||get(h+20)!=model_crc(h,20)||!get(h+12))return -1;
 uint32_t c=~0u;
 for(unsigned off=0;off<s->size;off+=512){unsigned n=s->size-off;if(n>512)n=512;
  if(s->io.read(base(bank)+1+off/512,b,s->io.context))return -1;
  c=crc_more(c,b,n);if(out)memcpy(out+off,b,n);
 }
 if(~c!=get(h+16))return -1;
 *seq=get(h+12);return 0;
}
int model_store_open(struct model_store *s,const struct holly_block_ops *io,void *out,unsigned size){
 memset(s,0,sizeof(*s));if(!io||!io->read||!io->write||!out||!size||size>MODEL_STORE_MAX)return -1;
 s->io=*io;s->size=size;uint8_t b[512];
 if(io->read(0,b,io->context)||memcmp(b+440,"HLY2",4)||b[510]!=0x55||b[511]!=0xaa||
 b[450]!=0x0c||get(b+454)!=2048||get(b+458)!=456704||b[466]!=0xda||get(b+470)!=458752)return -1;
 for(unsigned i=478;i<510;i++)if(b[i])return -1;
 if(io->read(8,b,io->context))return -1;
 if(memcmp(b,"HLYMOD01",8)||get(b+8)!=model_crc(b,8)){
  /* Claim only a completely empty, known-layout gap. A torn first marker is
     deliberately refused rather than guessed to be ours. */
  for(unsigned lba=8;lba<=2046;lba++){
   if(io->read(lba,b,io->context))return -1;
   for(unsigned i=0;i<512;i++)if(b[i])return -1;
  }
  memset(b,0,512);memcpy(b,"HLYMOD01",8);put(b+8,model_crc(b,8));
  if(io->write(8,b,io->context))return -1;
  uint8_t check[512];if(io->read(8,check,io->context)||memcmp(b,check,512))return -1;
 }
 uint32_t seq[2]={0,0};int a=read_bank(s,0,0,&seq[0]),c=read_bank(s,1,0,&seq[1]);
 s->ready=1;
 if(a&&c){s->bank=1;return 0;}
 s->bank=(!c&&(a||seq[1]>seq[0]))?1:0;
 if(read_bank(s,s->bank,out,&s->sequence)){s->ready=0;return -1;}return 1;
}
int model_store_save(struct model_store *s,const void *p){
 if(!s->ready||s->busy||s->sequence==UINT32_MAX)return -1;
 s->pending=p;s->sector=0;s->crc=~0u;s->busy=1;s->error=0;return 0;
}
void model_store_tick(struct model_store *s){
 if(!s->busy)return;
 uint8_t b[512]={0},check[512];uint32_t lba;
 unsigned count=(s->size+511)/512;
 if(s->sector<count){
  unsigned off=s->sector*512,n=s->size-off;if(n>512)n=512;
  memcpy(b,s->pending+off,n);s->crc=crc_more(s->crc,b,n);lba=base(1-s->bank)+1+s->sector;
 }else{
  memcpy(b,"HLYBNK01",8);put(b+8,s->size);put(b+12,s->sequence+1);put(b+16,~s->crc);put(b+20,model_crc(b,20));lba=base(1-s->bank);
 }
 if(s->io.write(lba,b,s->io.context)||s->io.read(lba,check,s->io.context)||memcmp(b,check,512)){
  s->error=-1;s->busy=0;s->pending=0;return;
 }
 if(s->sector++==count){s->bank=1-s->bank;s->sequence++;s->busy=0;s->pending=0;}
}
