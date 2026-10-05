#include "ssh_packet.h"
void holly_ssh_plain_init(struct holly_ssh_plain_packet *p){
    if(!p)return;
    p->used=0;p->total=0;p->payload_bytes=0;p->complete=0;p->failed=0;
}
static int reject(struct holly_ssh_plain_packet *p){p->failed=1;return -1;}
int holly_ssh_plain_feed(struct holly_ssh_plain_packet *p,const uint8_t *data,
                         size_t bytes,size_t *consumed){
    if(!consumed)return -1;
    *consumed=0;
    if(!p||(!data&&bytes))return -1;
    if(p->failed)return -1;
    if(p->complete)return 1;
    if(p->used>=HOLLY_SSH_PACKET_MAX)return reject(p);
    while(*consumed<bytes){
        p->bytes[p->used++]=data[(*consumed)++];
        if(p->used==4){
            uint32_t length=((uint32_t)p->bytes[0]<<24)|((uint32_t)p->bytes[1]<<16)|
                            ((uint32_t)p->bytes[2]<<8)|p->bytes[3];
            if(length>HOLLY_SSH_PACKET_MAX-4u||length<12u||((length+4u)&7u))return reject(p);
            p->total=length+4u;
        }
        if(p->used==5){
            unsigned pad=p->bytes[4];
            if(pad<4u||pad>=p->total-5u)return reject(p);
            p->payload_bytes=p->total-5u-pad;
            if(p->payload_bytes>HOLLY_SSH_PAYLOAD_MAX)return reject(p);
        }
        if(p->total&&p->used==p->total){p->complete=1;return 1;}
    }
    return 0;
}
const uint8_t *holly_ssh_plain_payload(const struct holly_ssh_plain_packet *p,size_t *bytes){
    if(bytes)*bytes=0;
    if(!p||!bytes||!p->complete||p->failed)return 0;
    *bytes=p->payload_bytes;return p->bytes+5;
}
int holly_ssh_plain_size(size_t bytes,size_t *total,unsigned *padding){
    if(!bytes||bytes>HOLLY_SSH_PAYLOAD_MAX||!total||!padding)return -1;
    unsigned pad=8u-(unsigned)((5u+bytes)%8u);
    if(pad<4u)pad+=8u;
    *total=5u+bytes+pad;*padding=pad;return 0;
}
int holly_ssh_plain_encode(const uint8_t *payload,size_t bytes,
                           const uint8_t *padding,size_t padding_bytes,
                           uint8_t *output,size_t capacity,size_t *written){
    if(!written)return -1;
    *written=0;
    size_t total;unsigned pad;
    if(!payload||!padding||!output||holly_ssh_plain_size(bytes,&total,&pad)||
       capacity<total||padding_bytes<pad)return -1;
    uint32_t length=(uint32_t)total-4u;
    for(unsigned i=0;i<4;i++)output[i]=(uint8_t)(length>>(24u-i*8u));
    output[4]=(uint8_t)pad;
    for(size_t i=0;i<bytes;i++)output[5+i]=payload[i];
    for(unsigned i=0;i<pad;i++)output[5+bytes+i]=padding[i];
    *written=total;return 0;
}
