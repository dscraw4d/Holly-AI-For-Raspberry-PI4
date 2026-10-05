#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ssh_packet.h"
static struct holly_ssh_plain_packet packet;
static uint8_t payload[HOLLY_SSH_PAYLOAD_MAX],wire[HOLLY_SSH_PACKET_MAX+16];
int main(void){
    /* Hand-assembled initial SSH packet: length 12, padding 10, message 20. */
    const uint8_t golden[]={0,0,0,12,10,20,1,2,3,4,5,6,7,8,9,10};
    uint8_t message=20;size_t golden_bytes;
    assert(holly_ssh_plain_encode(&message,1,golden+6,10,wire,sizeof(wire),&golden_bytes)==0);
    assert(golden_bytes==16 && memcmp(wire,golden,16)==0);
    uint8_t padding[255];for(unsigned i=0;i<255;i++)padding[i]=(uint8_t)(i*17u);
    for(unsigned i=0;i<sizeof(payload);i++)payload[i]=(uint8_t)(i*29u);
    const unsigned lengths[]={1,2,7,8,55,56,63,64,65,32768};
    for(unsigned n=0;n<sizeof(lengths)/sizeof(lengths[0]);n++){
        size_t total,written;unsigned pad;
        assert(holly_ssh_plain_size(lengths[n],&total,&pad)==0);
        assert(total>=16 && (total&7u)==0 && pad>=4 && pad<=255);
        assert(holly_ssh_plain_encode(payload,lengths[n],padding,sizeof(padding),wire,sizeof(wire),&written)==0);
        assert(written==total);
        /* Every split boundary for small messages, 1-byte fragmentation for large. */
        holly_ssh_plain_init(&packet);
        for(size_t i=0;i<total;i++){
            size_t consumed;
            assert(holly_ssh_plain_feed(&packet,wire+i,1,&consumed)==(i+1==total));
            assert(consumed==1);
        }
        size_t bytes;const uint8_t *p=holly_ssh_plain_payload(&packet,&bytes);
        assert(bytes==lengths[n]&&memcmp(p,payload,bytes)==0);
        holly_ssh_plain_init(&packet);
        size_t consumed;
        assert(holly_ssh_plain_feed(&packet,wire,total+16,&consumed)==1&&consumed==total);
        assert(holly_ssh_plain_feed(&packet,wire,16,&consumed)==1&&consumed==0);
        if(total<128)for(size_t split=0;split<total;split++){
            holly_ssh_plain_init(&packet);
            assert(holly_ssh_plain_feed(&packet,wire,split,&consumed)==0&&consumed==split);
            assert(holly_ssh_plain_feed(&packet,wire+split,total-split,&consumed)==1);
        }
    }
    size_t written=99,total;unsigned pad;
    memset(wire,0xa5,sizeof(wire));
    assert(holly_ssh_plain_encode(payload,1,padding,1,wire,sizeof(wire),&written)==-1&&written==0&&wire[0]==0xa5);
    assert(holly_ssh_plain_encode(payload,1,padding,sizeof(padding),wire,15,&written)==-1&&written==0&&wire[0]==0xa5);
    assert(holly_ssh_plain_size(0,&total,&pad)==-1);
    assert(holly_ssh_plain_size(32769,&total,&pad)==-1);
    const uint8_t bad[][5]={{0xff,0xff,0xff,0xff,4},{0,0,0,4,4},
                           {0,0,0,13,4},{0,0,0,12,3},{0,0,0,12,11}};
    for(unsigned i=0;i<5;i++){
        holly_ssh_plain_init(&packet);size_t consumed;
        assert(holly_ssh_plain_feed(&packet,bad[i],5,&consumed)==-1);
        assert(consumed<=5&&packet.failed);
        assert(holly_ssh_plain_feed(&packet,wire,16,&consumed)==-1&&consumed==0);
        assert(holly_ssh_plain_payload(&packet,&total)==0&&total==0);
    }
    /* Accept legal overpadding at the byte maximum. */
    memset(wire,0,264);wire[2]=1;wire[3]=4;wire[4]=255;
    holly_ssh_plain_init(&packet);size_t consumed;
    assert(holly_ssh_plain_feed(&packet,wire,264,&consumed)==1);
    assert(holly_ssh_plain_payload(&packet,&total)!=0&&total==4);
    puts("SSH plaintext packet fragmentation, coalescing, bounds and padding tests passed");
}
