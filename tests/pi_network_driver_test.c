#include "genet_live.h"
#include "rng200.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t regs[0x6000/4];static uint16_t phy[32];
static uint8_t rx[256*2048],tx[256*2048];static unsigned barriers;
static uint32_t read_reg(uint32_t at,void *ctx){(void)ctx;assert(at<sizeof(regs));return regs[at/4];}
static void write_reg(uint32_t at,uint32_t v,void *ctx){
    (void)ctx;assert(at<sizeof(regs));
    /* Hardware-owned indices must never be zeroed, even at startup. */
    assert(at!=0x3008&&at!=0x5008);
    if(at==0xe14){unsigned addr=(v>>21)&31u,reg=(v>>16)&31u;
        if(addr!=1)regs[at/4]=(1u<<28);
        else if((v>>26&3u)==2)regs[at/4]=phy[reg];
        else {phy[reg]=(uint16_t)v;regs[at/4]=0;}
    }else regs[at/4]=v;
}
static void delay(unsigned n,void *ctx){(void)n;(void)ctx;}
static void barrier(void *ctx){(void)ctx;barriers++;}
static uint32_t rng_value,rng_status,rng_count,rng_same;
static uint32_t rng_read(unsigned at,void *ctx){(void)ctx;
    if(at==0x18)return rng_status;
    if(at==0x24)return rng_count;
    if(at==0x20)return rng_same?rng_value:++rng_value;
    return 0;}
static void rng_write(unsigned at,uint32_t v,void *ctx){(void)ctx;assert(at==0);assert(v==1);}
int main(void){
    regs[0]=6u<<24;regs[0x3008/4]=65535;regs[0x5008/4]=65535;phy[2]=0x600d;phy[3]=0x84a1;
    struct holly_genet s={.device={read_reg,write_reg,0},.delay_us=delay,.barrier=barrier,.rx=rx,.tx=tx,.rx_physical=0x200000,.tx_physical=0x300000};
    const uint8_t mac[6]={2,1,2,3,4,5};assert(!holly_genet_start(&s,mac));
    assert(s.consumer==65535&&s.producer==65535);assert(regs[0x300c/4]==65535);
    assert(regs[0x3044/4]==0x20001&&regs[0x5044/4]==0x20001);
    assert(holly_genet_link(&s)==0);phy[1]=0x24;phy[10]=0x800;assert(holly_genet_link(&s)==1&&s.speed==2);
    uint8_t data[100];for(unsigned i=0;i<sizeof(data);i++)data[i]=(uint8_t)i;
    assert(!holly_genet_transmit(&s,data,40));assert(s.producer==0&&regs[0x500c/4]==0);
    assert(!memcmp(tx+255*2048,data,40));for(unsigned i=40;i<60;i++)assert(!tx[255*2048+i]);
    assert((regs[(0x4000+255*12)/4]>>16)==60);
    s.producer=255;regs[0x5008/4]=65535;assert(holly_genet_transmit(&s,data,100)==-1);s.producer=0;
    memcpy(rx+255*2048+2,data,100);regs[(0x2000+255*12)/4]=(102u<<16)|0x6000;regs[0x3008/4]=0;
    uint8_t out[110];memset(out,0xa5,sizeof(out));assert(holly_genet_receive(&s,out,100)==100);
    assert(s.consumer==0&&regs[0x300c/4]==0&&!memcmp(out,data,100)&&out[100]==0xa5);
    /* Invalid CRC/error flags discard without copying; consume once. */
    regs[0x3008/4]=1;regs[0x2000/4]=(102u<<16)|0x6002;
    assert(!holly_genet_receive(&s,out,100)&&s.consumer==1);
    regs[0x3008/4]=300;assert(holly_genet_receive(&s,out,100)==-1&&!s.active);
    assert(!regs[0x3044/4]&&!regs[0x5044/4]&&barriers);
    struct holly_rng200 random={rng_read,rng_write,0,0,0,0,0};rng_count=1;
    assert(!holly_rng200_start(&random)&&rng_value==32);
    uint8_t bytes[7];assert(!holly_rng200_bytes(&random,bytes,7));assert(bytes[0]==33&&bytes[4]==34);
    rng_same=1;memset(bytes,0xa5,7);assert(holly_rng200_bytes(&random,bytes,7)==-1&&random.failed);
    for(unsigned i=0;i<7;i++)assert(!bytes[i]);
    assert(holly_rng200_start(&random)==-1);
    random=(struct holly_rng200){rng_read,rng_write,0,0,0,0,0};rng_same=0;rng_status=0x20;
    assert(holly_rng200_start(&random)==-1);
    random=(struct holly_rng200){rng_read,rng_write,0,0,0,0,0};rng_status=0;rng_count=0;
    assert(holly_rng200_start(&random)==-1);
    puts("Pi driver model: DMA ownership, wrap, bounds, errors, link and RNG fail-closed cases passed");
}
