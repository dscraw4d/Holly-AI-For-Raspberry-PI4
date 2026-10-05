#include "net.h"
static uint16_t u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0]<<8|p[1]); }
uint16_t viper_checksum(const uint8_t *p, unsigned n) {
    uint32_t sum=0;
    while(n>=2) {sum+=u16(p);p+=2;n-=2;}
    if(n) sum+=(uint16_t)p[0]<<8;
    while(sum>>16) sum=(sum&0xFFFFu)+(sum>>16);
    return (uint16_t)~sum;
}
int viper_parse_packet(const uint8_t *data,unsigned size,struct viper_packet *out) {
    if(!data || !out || size<14) return -1;
    for(unsigned i=0;i<6;i++) {out->target_mac[i]=data[i];out->source_mac[i]=data[i+6];}
    if(u16(data+12)!=0x0800) return 0;
    if(size<34) return -1;
    const uint8_t *ip=data+14;
    unsigned ihl=(unsigned)(ip[0]&15u)*4u;
    if((ip[0]>>4)!=4 || ihl<20 || size<14+ihl) return -1;
    unsigned total=u16(ip+2);
    if(total<ihl || total>size-14 || viper_checksum(ip,ihl)!=0) return -1;
    /* Fragment reassembly is deliberately not implemented. */
    if(u16(ip+6)&0x3FFFu) return 0;
    out->protocol=ip[9];
    for(unsigned i=0;i<4;i++) {out->source_ip[i]=ip[12+i];out->target_ip[i]=ip[16+i];}
    const uint8_t *transport=ip+ihl;
    unsigned available=total-ihl;
    if(out->protocol==6) {
        if(available<20) return -1;
        unsigned header=(unsigned)(transport[12]>>4)*4u;
        if(header<20 || header>available) return -1;
        out->source_port=u16(transport);
        out->target_port=u16(transport+2);
        out->payload=transport+header;
        out->payload_len=(uint16_t)(available-header);
        return 1;
    }
    if(out->protocol==17) {
        if(available<8 || u16(transport+4)<8 || u16(transport+4)>available) return -1;
        out->source_port=u16(transport);
        out->target_port=u16(transport+2);
        out->payload=transport+8;
        out->payload_len=(uint16_t)(u16(transport+4)-8);
        return 1;
    }
    return 0;
}
