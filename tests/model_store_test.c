#include "model_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t media[2048][512],backup[2048][512];
static int reads(unsigned l,uint8_t *p,void *c){(void)c;assert(l<2048);memcpy(p,media[l],512);return 0;}
static int writes(unsigned l,const uint8_t *p,void *c){(void)c;assert(l>=8&&l<=2046);memcpy(media[l],p,512);return 0;}
static void put(uint8_t *p,unsigned n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
int main(void){
 struct holly_block_ops io={reads,writes,0};struct model_store s,t;uint8_t a[1024],b[1024],out[1024];
 memset(a,1,sizeof(a));memset(b,2,sizeof(b));memcpy(media[0]+440,"HLY2",4);media[0][510]=0x55;media[0][511]=0xaa;
 media[0][450]=0x0c;put(media[0]+454,2048);put(media[0]+458,456704);media[0][466]=0xda;put(media[0]+470,458752);
 media[0][478]=0x80;assert(model_store_open(&s,&io,out,sizeof(out))==-1);media[0][478]=0;
 media[25][0]=1;assert(model_store_open(&s,&io,out,sizeof(out))==-1);assert(media[25][0]==1);media[25][0]=0;
 assert(model_store_open(&s,&io,out,sizeof(out))==0);assert(!model_store_save(&s,a));while(s.busy)model_store_tick(&s);
 memcpy(backup,media,sizeof(media));
 for(unsigned writes_done=0;writes_done<=3;writes_done++){
  memcpy(media,backup,sizeof(media));assert(model_store_open(&s,&io,out,sizeof(out))==1);assert(!memcmp(out,a,1024));
  assert(!model_store_save(&s,b));for(unsigned i=0;i<writes_done;i++)model_store_tick(&s);
  assert(model_store_open(&t,&io,out,sizeof(out))==1);assert(!memcmp(out,writes_done==3?b:a,1024));
 }
 /* Corrupt newest body; old generation remains recoverable. */
 media[1029][0]^=1;assert(model_store_open(&t,&io,out,sizeof(out))==1);assert(!memcmp(out,a,1024));
 assert(model_crc("123456789",9)==0xcbf43926);
 puts("Model store: foreign-gap refusal, all commit boundaries, corruption fallback and CRC passed");
}
