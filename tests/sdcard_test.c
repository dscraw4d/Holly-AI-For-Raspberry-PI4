#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdcard.h"
#include "vault.h"

static void set_bits(uint32_t csd[4],unsigned start,unsigned size,uint32_t value) {
    for(unsigned i=0;i<size;i++) {
        unsigned bit=start+i,word=3u-bit/32u,offset=bit%32u;
        if(value&(1u<<i))csd[word]|=1u<<offset;
    }
}
static void partition(uint8_t mbr[512],uint32_t data_size) {
    memset(mbr,0,512);memcpy(mbr+440,"HLY2",4);mbr[510]=0x55;mbr[511]=0xaa;
    mbr[446+4]=0x0c;memcpy(mbr+446+8,(uint32_t[]){2048},4);
    mbr[462+4]=HOLLY_VAULT_PARTITION_TYPE;
    memcpy(mbr+462+8,(uint32_t[]){458752},4);memcpy(mbr+462+12,&data_size,4);
}
int main(void) {
    uint32_t csd[4]={0},sectors=0;
    set_bits(csd,126,2,1);set_bits(csd,48,22,244140);
    assert(holly_sdcard_capacity_sectors(csd,&sectors)==0);
    assert(sectors==250000384u); /* 128 GB class SDXC card */
    memset(csd,0,sizeof(csd));set_bits(csd,126,2,0);
    set_bits(csd,80,4,9);set_bits(csd,62,12,1023);set_bits(csd,47,3,3);
    assert(holly_sdcard_capacity_sectors(csd,&sectors)==0&&sectors==32768u);
    memset(csd,0,sizeof(csd));set_bits(csd,126,2,2);
    assert(holly_sdcard_capacity_sectors(csd,&sectors)<0);

    uint8_t mbr[512];uint32_t first=0,length=0;
    partition(mbr,65536);
    assert(holly_vault_expand_image_partition(mbr,250000384u,&first,&length)==1);
    assert(first==458752u&&length==250000384u-458752u);
    assert(holly_vault_expand_image_partition(mbr,250000384u,&first,&length)==0);
    partition(mbr,65536);mbr[478+4]=0x83;
    assert(holly_vault_expand_image_partition(mbr,250000384u,&first,&length)<0);
    puts("SD CSD capacity decoding and safe Holly partition expansion passed");
}
