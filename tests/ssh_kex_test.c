#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ssh_kex.h"
#include "ssh_packet.h"
static struct holly_ssh_names names(const char *s){
    struct holly_ssh_names n={(const uint8_t *)s,strlen(s)};return n;
}
static struct holly_ssh_plain_packet packet;
int main(void){
    struct holly_ssh_kexinit proposal={0},parsed={0};
    for(unsigned i=0;i<16;i++)proposal.cookie[i]=(uint8_t)i;
    for(unsigned i=0;i<8;i++)proposal.lists[i]=names("test-algorithm");
    proposal.lists[0]=names("second,first");proposal.first_packet_follows=1;
    uint8_t payload[2048],wire[2100],padding[16]={0};size_t bytes,consumed;
    assert(holly_ssh_kex_encode(&proposal,payload,sizeof(payload),&bytes)==0);
    assert(payload[0]==20&&payload[17]==0&&payload[20]==12&&payload[21]=='s');
    assert(holly_ssh_kex_parse(payload,bytes,&parsed)==0);
    assert(parsed.first_packet_follows==1&&memcmp(parsed.cookie,proposal.cookie,16)==0);
    assert(parsed.lists[0].bytes==12&&memcmp(parsed.lists[0].data,"second,first",12)==0);
    /* Entire packet-to-KEX path with fragmented reads. */
    size_t written;assert(holly_ssh_plain_encode(payload,bytes,padding,16,wire,sizeof(wire),&written)==0);
    holly_ssh_plain_init(&packet);
    assert(holly_ssh_plain_feed(&packet,wire,9,&consumed)==0&&consumed==9);
    assert(holly_ssh_plain_feed(&packet,wire+9,written-9,&consumed)==1);
    size_t length;const uint8_t *p=holly_ssh_plain_payload(&packet,&length);
    assert(holly_ssh_kex_parse(p,length,&parsed)==0);
    struct holly_ssh_names selected;
    assert(holly_ssh_name_select(names("second,first"),names("first,second"),&selected)==1);
    assert(selected.bytes==6&&memcmp(selected.data,"second",6)==0);
    assert(holly_ssh_name_select(names("firstly,first"),names("first"),&selected)==1&&selected.bytes==5);
    assert(holly_ssh_name_select(names("FIRST"),names("first"),&selected)==0);
    assert(holly_ssh_name_select(names("first,"),names("first"),&selected)==-1);
    assert(holly_ssh_name_select(names("a,,b"),names("a"),&selected)==-1);
    assert(holly_ssh_name_select(names("a b"),names("a"),&selected)==-1);
    char long_name[66];memset(long_name,'a',65);long_name[65]=0;
    assert(holly_ssh_name_select(names(long_name),names("a"),&selected)==-1);
    long_name[64]=0;
    assert(holly_ssh_name_select(names(long_name),names(long_name),&selected)==1&&selected.bytes==64);
    struct holly_ssh_names too_big={(const uint8_t *)long_name,1025};
    assert(holly_ssh_name_select(too_big,names("a"),&selected)==-1);
    for(size_t i=0;i<bytes;i++)assert(holly_ssh_kex_parse(payload,i,&parsed)==-1);
    payload[bytes]=0;assert(holly_ssh_kex_parse(payload,bytes+1,&parsed)==-1);
    payload[bytes-1]=1;assert(holly_ssh_kex_parse(payload,bytes,&parsed)==-1);payload[bytes-1]=0;
    payload[17]=255;assert(holly_ssh_kex_parse(payload,bytes,&parsed)==-1);payload[17]=0;
    payload[21]=',';assert(holly_ssh_kex_parse(payload,bytes,&parsed)==-1);payload[21]='s';
    payload[bytes-5]=255;assert(holly_ssh_kex_parse(payload,bytes,&parsed)==0&&parsed.first_packet_follows==1);
    uint8_t canary[1]={0xa5};
    assert(holly_ssh_kex_encode(&proposal,canary,1,&written)==-1&&written==0&&canary[0]==0xa5);
    proposal.lists[0]=names("");assert(holly_ssh_kex_encode(&proposal,payload,sizeof(payload),&written)==-1);
    puts("SSH KEXINIT framing integration, truncation and client-preference matching passed");
}
