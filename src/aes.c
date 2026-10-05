#include "aes.h"
void holly_secret_wipe(void *p,size_t n){volatile uint8_t *q=p;while(n--)*q++=0;}
static uint8_t xt(uint8_t v){return (uint8_t)((v<<1)^(0x1bu&(0u-(v>>7))));}
static uint8_t mul(uint8_t a,uint8_t b){
    uint8_t r=0;
    for(unsigned i=0;i<8;i++){r^=(uint8_t)(a&(0u-(b&1u)));a=xt(a);b>>=1;}
    return r;
}
static uint8_t rot(uint8_t x,unsigned n){return (uint8_t)((x<<n)|(x>>(8u-n)));}
static uint8_t sub(uint8_t x){
    /* x^254, including the convention 0^-1 = 0, followed by affine map. */
    uint8_t y=1;
    for(unsigned i=0;i<8;i++){
        uint8_t z=mul(y,x),mask=(uint8_t)(0u-((254u>>i)&1u));
        y=(uint8_t)((y&~mask)|(z&mask));x=mul(x,x);
    }
    return (uint8_t)(y^rot(y,1)^rot(y,2)^rot(y,3)^rot(y,4)^0x63u);
}
void holly_aes128_keys(const uint8_t key[16],uint8_t k[176]){
    for(unsigned i=0;i<16;i++)k[i]=key[i];
    uint8_t rcon=1;
    for(unsigned i=16;i<176;i+=4){
        uint8_t t[4];for(unsigned j=0;j<4;j++)t[j]=k[i-4+j];
        if((i&15u)==0){uint8_t a=t[0];t[0]=(uint8_t)(sub(t[1])^rcon);
            t[1]=sub(t[2]);t[2]=sub(t[3]);t[3]=sub(a);rcon=xt(rcon);}
        for(unsigned j=0;j<4;j++)k[i+j]=(uint8_t)(k[i-16+j]^t[j]);
        holly_secret_wipe(t,sizeof(t));
    }
}
void holly_aes128_block(const uint8_t k[176],const uint8_t in[16],uint8_t out[16]){
    uint8_t s[16],t[16];for(unsigned i=0;i<16;i++)s[i]=(uint8_t)(in[i]^k[i]);
    for(unsigned round=1;round<=10;round++){
        for(unsigned c=0;c<4;c++)for(unsigned r=0;r<4;r++)t[4*c+r]=sub(s[4*((c+r)&3u)+r]);
        if(round<10)for(unsigned c=0;c<4;c++){
            unsigned i=4*c;uint8_t a=t[i],b=t[i+1],d=t[i+2],e=t[i+3],v=(uint8_t)(a^b^d^e);
            t[i]=(uint8_t)(a^v^xt((uint8_t)(a^b)));
            t[i+1]=(uint8_t)(b^v^xt((uint8_t)(b^d)));
            t[i+2]=(uint8_t)(d^v^xt((uint8_t)(d^e)));
            t[i+3]=(uint8_t)(e^v^xt((uint8_t)(e^a)));
        }
        for(unsigned i=0;i<16;i++)s[i]=(uint8_t)(t[i]^k[round*16+i]);
    }
    for(unsigned i=0;i<16;i++)out[i]=s[i];
    holly_secret_wipe(s,sizeof(s));holly_secret_wipe(t,sizeof(t));
}
void holly_aes_ctr_init(struct holly_aes_ctr *s,const uint8_t k[16],const uint8_t iv[16]){
    holly_aes128_keys(k,s->keys);for(unsigned i=0;i<16;i++)s->counter[i]=iv[i];
    s->used=16;s->exhausted=0;
}
int holly_aes_ctr_xor(struct holly_aes_ctr *s,uint8_t *p,size_t n){
    if(!s||(!p&&n)||s->used>16)return -1;
    for(size_t i=0;i<n;i++){
        if(s->used==16){
            if(s->exhausted)return -1;
            holly_aes128_block(s->keys,s->counter,s->stream);s->used=0;
            unsigned carry=1;
            for(unsigned j=16;j>0;j--){unsigned v=s->counter[j-1]+carry;s->counter[j-1]=(uint8_t)v;carry=v>>8;}
            s->exhausted=carry;
        }
        p[i]^=s->stream[s->used++];
    }
    return 0;
}
