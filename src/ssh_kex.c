#include "ssh_kex.h"
static int valid(struct holly_ssh_names list,int required){
    if(list.bytes>HOLLY_SSH_NAME_LIST_MAX||(!list.data&&list.bytes)||(!list.bytes&&required))return 0;
    size_t name=0;
    for(size_t i=0;i<list.bytes;i++){
        unsigned c=list.data[i];
        if(c==','){if(!name)return 0;name=0;}
        else {if(c<=32||c>=127||++name>64)return 0;}
    }
    return !list.bytes||name!=0;
}
static uint32_t read32(const uint8_t *p){
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
int holly_ssh_kex_parse(const uint8_t *data,size_t bytes,struct holly_ssh_kexinit *out){
    if(!data||!out||bytes<62||data[0]!=20)return -1;
    struct holly_ssh_kexinit parsed;
    for(unsigned i=0;i<16;i++)parsed.cookie[i]=data[1+i];
    size_t at=17;
    for(unsigned i=0;i<HOLLY_SSH_KEX_LISTS;i++){
        if(bytes-at<4)return -1;
        size_t length=read32(data+at);at+=4;
        if(length>bytes-at)return -1;
        parsed.lists[i].data=data+at;parsed.lists[i].bytes=length;
        if(!valid(parsed.lists[i],i<8))return -1;
        at+=length;
    }
    if(bytes-at!=5||read32(data+at+1)!=0)return -1;
    parsed.first_packet_follows=data[at]!=0;
    *out=parsed;return 0;
}
int holly_ssh_kex_encode(const struct holly_ssh_kexinit *p,uint8_t *out,
                         size_t capacity,size_t *written){
    if(!written)return -1;
    *written=0;if(!p||!out)return -1;
    size_t total=62;
    for(unsigned i=0;i<HOLLY_SSH_KEX_LISTS;i++){
        if(!valid(p->lists[i],i<8))return -1;
        total+=p->lists[i].bytes;
    }
    if(total>capacity)return -1;
    out[0]=20;for(unsigned i=0;i<16;i++)out[i+1]=p->cookie[i];
    size_t at=17;
    for(unsigned i=0;i<HOLLY_SSH_KEX_LISTS;i++){
        uint32_t length=(uint32_t)p->lists[i].bytes;
        for(unsigned j=0;j<4;j++)out[at++]=(uint8_t)(length>>(24u-j*8u));
        for(size_t j=0;j<length;j++)out[at++]=p->lists[i].data[j];
    }
    out[at++]=p->first_packet_follows!=0;
    for(unsigned i=0;i<4;i++)out[at++]=0;
    *written=at;return 0;
}
static size_t end(struct holly_ssh_names list,size_t start){
    size_t at=start;while(at<list.bytes&&list.data[at]!=',')at++;return at;
}
int holly_ssh_name_select(struct holly_ssh_names client,struct holly_ssh_names server,
                          struct holly_ssh_names *selected){
    if(!selected)return -1;
    selected->data=0;selected->bytes=0;
    if(!valid(client,1)||!valid(server,1))return -1;
    for(size_t c=0;c<client.bytes;){
        size_t ce=end(client,c);
        for(size_t s=0;s<server.bytes;){
            size_t se=end(server,s),n=ce-c;
            if(n==se-s){
                size_t i=0;while(i<n&&client.data[c+i]==server.data[s+i])i++;
                if(i==n){selected->data=client.data+c;selected->bytes=n;return 1;}
            }
            s=se+1;
        }
        c=ce+1;
    }
    return 0;
}
