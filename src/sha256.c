#include "sha256.h"
/* Independent implementation of FIPS 180-4 SHA-256 and RFC 2104 HMAC.
 * Constants below are the fractional cube roots specified by the standard. */
static const uint32_t round_constant[64]={
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
 0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
 0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
 0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
 0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static uint32_t rotate(uint32_t word,unsigned bits){return (word>>bits)|(word<<(32u-bits));}
static void wipe(void *buffer,size_t bytes){
    volatile uint8_t *p=buffer;while(bytes--)*p++=0;
}
static void compress(struct holly_sha256 *c){
    uint32_t words[64];
    for(unsigned i=0;i<16;i++){
        const uint8_t *p=c->block+i*4u;
        words[i]=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
    }
    for(unsigned i=16;i<64;i++){
        uint32_t x=words[i-15],y=words[i-2];
        words[i]=words[i-16]+(rotate(x,7)^rotate(x,18)^(x>>3))+
                 words[i-7]+(rotate(y,17)^rotate(y,19)^(y>>10));
    }
    uint32_t a=c->state[0],b=c->state[1],d=c->state[3],e=c->state[4];
    uint32_t f=c->state[5],g=c->state[6],h=c->state[7],v=c->state[2];
    for(unsigned i=0;i<64;i++){
        uint32_t t1=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+
                    ((e&f)^(~e&g))+round_constant[i]+words[i];
        uint32_t t2=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&v)^(b&v));
        h=g;g=f;f=e;e=d+t1;d=v;v=b;b=a;a=t1+t2;
    }
    c->state[0]+=a;c->state[1]+=b;c->state[2]+=v;c->state[3]+=d;
    c->state[4]+=e;c->state[5]+=f;c->state[6]+=g;c->state[7]+=h;
    wipe(words,sizeof(words));
}
void holly_sha256_init(struct holly_sha256 *c){
    if(!c)return;
    static const uint32_t initial[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                    0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    for(unsigned i=0;i<8;i++)c->state[i]=initial[i];
    c->bytes=0;c->used=0;c->finalized=0;
    for(unsigned i=0;i<64;i++)c->block[i]=0;
}
int holly_sha256_update(struct holly_sha256 *c,const void *data,size_t bytes){
    const uint64_t maximum=UINT64_MAX/8u;
    if(!c||(!data&&bytes)||c->finalized||c->used>=64||
       c->bytes>maximum||bytes>maximum-c->bytes)return -1;
    const uint8_t *p=data;c->bytes+=bytes;
    while(bytes){
        size_t take=64u-c->used;if(take>bytes)take=bytes;
        for(size_t i=0;i<take;i++)c->block[c->used++]=*p++;
        bytes-=take;
        if(c->used==64){compress(c);c->used=0;}
    }
    return 0;
}
int holly_sha256_finish(struct holly_sha256 *c,uint8_t digest[32]){
    if(!c||!digest||c->finalized||c->used>=64||c->bytes>UINT64_MAX/8u)return -1;
    uint64_t bits=c->bytes*8u;
    c->block[c->used++]=0x80;
    if(c->used>56){
        while(c->used<64)c->block[c->used++]=0;
        compress(c);c->used=0;
    }
    while(c->used<56)c->block[c->used++]=0;
    for(unsigned i=0;i<8;i++)c->block[56+i]=(uint8_t)(bits>>(56u-i*8u));
    compress(c);
    for(unsigned i=0;i<8;i++)for(unsigned j=0;j<4;j++)
        digest[i*4+j]=(uint8_t)(c->state[i]>>(24u-j*8u));
    wipe(c,sizeof(*c));c->finalized=1;
    return 0;
}
int holly_sha256_hash(const void *data,size_t bytes,uint8_t digest[32]){
    if(!digest||(!data&&bytes))return -1;
    struct holly_sha256 c;holly_sha256_init(&c);
    int result=holly_sha256_update(&c,data,bytes);
    if(!result)result=holly_sha256_finish(&c,digest);
    wipe(&c,sizeof(c));return result;
}
int holly_hmac_sha256(const void *key,size_t key_bytes,const void *data,
                      size_t bytes,uint8_t digest[32]){
    if(!digest||(!key&&key_bytes)||(!data&&bytes)||
       bytes>UINT64_MAX/8u-64u||key_bytes>UINT64_MAX/8u)return -1;
    uint8_t pad[64]={0},inner[32];struct holly_sha256 c;
    if(key_bytes>64){
        if(holly_sha256_hash(key,key_bytes,pad))return -1;
    }else{
        const uint8_t *p=key;for(size_t i=0;i<key_bytes;i++)pad[i]=p[i];
    }
    for(unsigned i=0;i<64;i++)pad[i]^=0x36;
    holly_sha256_init(&c);
    int result=holly_sha256_update(&c,pad,64);
    if(!result)result=holly_sha256_update(&c,data,bytes);
    if(!result)result=holly_sha256_finish(&c,inner);
    for(unsigned i=0;i<64;i++)pad[i]^=0x36^0x5c;
    if(!result){
        holly_sha256_init(&c);
        result=holly_sha256_update(&c,pad,64);
        if(!result)result=holly_sha256_update(&c,inner,32);
        if(!result)result=holly_sha256_finish(&c,digest);
    }
    wipe(pad,sizeof(pad));wipe(inner,sizeof(inner));wipe(&c,sizeof(c));
    return result;
}
int holly_tag_equal(const uint8_t *a,const uint8_t *b,size_t bytes){
    if(!a||!b||!bytes)return 0;
    volatile unsigned difference=0;
    for(size_t i=0;i<bytes;i++)difference|=(unsigned)(a[i]^b[i]);
    return difference==0;
}
