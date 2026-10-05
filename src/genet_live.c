#include "genet_live.h"
#define RX 0x2000u
#define TX 0x4000u
#define RING 0x1000u
#define COMMON 0x1040u
static uint32_t rd(struct holly_genet *s,unsigned at){return s->device.read(at,s->device.context);}
static void wr(struct holly_genet *s,unsigned at,uint32_t v){s->device.write(at,v,s->device.context);}
static int phy_write(struct holly_genet *s,unsigned reg,uint16_t value){
    unsigned at=0xe14;
    for(unsigned i=0;i<100000;i++)if(!(rd(s,at)&(1u<<29))){
        wr(s,at,(1u<<29)|(1u<<26)|(s->phy<<21)|(reg<<16)|value);
        for(unsigned j=0;j<100000;j++){uint32_t v=rd(s,at);if(!(v&(1u<<29)))return (v&(1u<<28))?-1:0;}
        break;
    }
    return -1;
}
void holly_genet_stop(struct holly_genet *s){
    if(!s||!s->device.read||!s->device.write)return;
    wr(s,0x808,rd(s,0x808)&~3u);
    wr(s,RX+COMMON+4,0);wr(s,TX+COMMON+4,0);
    if(s->barrier)s->barrier(s->context);
    s->active=0;s->link=0;
}
int holly_genet_start(struct holly_genet *s,const uint8_t mac[6]){
    if(!s||!mac||!s->device.read||!s->device.write||!s->delay_us||!s->barrier||!s->rx||!s->tx||
       !s->rx_physical||!s->tx_physical||(s->rx_physical&63u)||(s->tx_physical&63u)||
       s->rx_physical>((1ull<<40)-524288u)||s->tx_physical>((1ull<<40)-524288u))return -1;
    unsigned version=(rd(s,0)>>24)&15u;if(version!=6&&version!=7)return -2;
    holly_genet_stop(s);
    wr(s,8,2);s->delay_us(10,s->context);wr(s,8,0);s->delay_us(10,s->context);
    wr(s,0x808,(1u<<13)|(1u<<15));s->delay_us(10,s->context);wr(s,0x808,0);
    wr(s,0xd80,7);wr(s,0xd80,0);wr(s,0x814,1536);
    wr(s,4,3); /* external RGMII PHY */
    wr(s,0x300,(rd(s,0x300)&~1u)|2u);wr(s,0x3b4,1);
    wr(s,0x80c,((uint32_t)mac[0]<<24)|((uint32_t)mac[1]<<16)|((uint32_t)mac[2]<<8)|mac[3]);
    wr(s,0x810,((uint32_t)mac[4]<<8)|mac[5]);wr(s,0xb34,1);s->delay_us(10,s->context);wr(s,0xb34,0);
    for(unsigned direction=0;direction<2;direction++){
        unsigned base=direction?TX:RX;
        wr(s,base+COMMON+12,8);wr(s,base+COMMON,1u<<16);
        wr(s,base+RING,0);wr(s,base+RING+4,0);wr(s,base+RING+0x2c,0);wr(s,base+RING+0x30,0);
        wr(s,base+RING+0x14,0);wr(s,base+RING+0x18,0);wr(s,base+RING+0x1c,767);wr(s,base+RING+0x20,0);
        wr(s,base+RING+0x10,(256u<<16)|2048u);
        wr(s,base+RING+0x24,1);wr(s,base+RING+0x28,direction?0:((5u<<16)|16u));
    }
    /* Hardware-owned indices are read, never overwritten. */
    s->consumer=(uint16_t)rd(s,RX+RING+8);wr(s,RX+RING+12,s->consumer);
    s->producer=(uint16_t)rd(s,TX+RING+8);wr(s,TX+RING+12,s->producer);
    for(unsigned i=0;i<256;i++){
        uint64_t address=s->rx_physical+(uint64_t)i*2048;
        wr(s,RX+i*12+4,(uint32_t)address);wr(s,RX+i*12+8,(uint32_t)(address>>32));wr(s,RX+i*12,(2048u<<16)|0x8000u);
    }
    uint32_t identifier;
    if(ether_phy_probe(&s->device,&s->phy,&identifier))return -3;
    /* Advertise full duplex 10/100/1000. Preserve board PHY delay
     * configuration supplied by firmware/PHY reset defaults. */
    if(phy_write(s,4,0x0141)||phy_write(s,9,0x0200)||phy_write(s,0,0x1200))return -3;
    s->barrier(s->context);wr(s,TX+COMMON+4,(1u<<17)|1u);wr(s,RX+COMMON+4,(1u<<17)|1u);
    s->active=1;s->link=0;return 0;
}
int holly_genet_link(struct holly_genet *s){
    if(!s||!s->active)return -1;
    uint16_t status,partner,advertised,gigabit;
    if(ether_phy_read(&s->device,s->phy,1,&status)||ether_phy_read(&s->device,s->phy,1,&status))return -1;
    if((status&0x24u)!=0x24u){s->link=0;wr(s,0x808,rd(s,0x808)&~3u);return 0;}
    if(ether_phy_read(&s->device,s->phy,10,&gigabit)||ether_phy_read(&s->device,s->phy,5,&partner)||
       ether_phy_read(&s->device,s->phy,4,&advertised))return -1;
    unsigned speed;
    if(gigabit&0x0800u)speed=2;
    else if(partner&advertised&0x0100u)speed=1;
    else if(partner&advertised&0x0040u)speed=0;
    else {s->link=0;return 0;} /* no supported full-duplex mode */
    /* RGMII control selection for the Pi 4's rgmii-rxid interface. */
    uint32_t oob=rd(s,0x8c);oob&=~(1u<<5);oob|=(1u<<4)|(1u<<6)|(1u<<16);wr(s,0x8c,oob);
    wr(s,0x808,(speed<<2)|3u);s->speed=speed;s->link=1;return 1;
}
int holly_genet_receive(struct holly_genet *s,uint8_t *out,unsigned cap){
    if(!s||!s->active||!out)return -1;
    uint16_t producer=(uint16_t)rd(s,RX+RING+8),pending=(uint16_t)(producer-s->consumer);
    if(!pending)return 0;
    if(pending>256){holly_genet_stop(s);return -1;}
    s->barrier(s->context);unsigned index=s->consumer&255u;uint32_t status=rd(s,RX+index*12);
    unsigned length=(status>>16)&4095u;uint64_t address=rd(s,RX+index*12+4)|((uint64_t)rd(s,RX+index*12+8)<<32);
    uint64_t expected=s->rx_physical+(uint64_t)index*2048;
    int result=0;
    if(address!=expected){holly_genet_stop(s);return -1;}
    if((status&0x6000u)==0x6000u&&!(status&0x001fu)&&length>=16&&length<=1516&&length-2<=cap){
        for(unsigned i=0;i<length-2;i++)out[i]=s->rx[index*2048+2+i];
        result=(int)length-2;
    }
    s->barrier(s->context);s->consumer++;wr(s,RX+RING+12,s->consumer);return result;
}
int holly_genet_transmit(struct holly_genet *s,const uint8_t *p,unsigned n){
    if(!s||!s->active||!s->link||!p||n<14||n>1514)return -1;
    uint16_t consumed=(uint16_t)rd(s,TX+RING+8);if((uint16_t)(s->producer-consumed)>=256)return -1;
    unsigned index=s->producer&255u;
    for(unsigned i=0;i<n;i++)s->tx[index*2048+i]=p[i];
    unsigned padded=n<60?60:n;for(unsigned i=n;i<padded;i++)s->tx[index*2048+i]=0;
    uint64_t address=s->tx_physical+(uint64_t)index*2048;
    wr(s,TX+index*12+4,(uint32_t)address);wr(s,TX+index*12+8,(uint32_t)(address>>32));
    s->barrier(s->context);wr(s,TX+index*12,(padded<<16)|0x7fc0u);
    s->barrier(s->context);s->producer++;wr(s,TX+RING+12,s->producer);return 0;
}
