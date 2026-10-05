#include "reply.h"
#include "net.h"
static unsigned u16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static void put16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static int equal(const uint8_t *a,const uint8_t *b,unsigned n){for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static void copy(uint8_t *a,const uint8_t *b,unsigned n){for(unsigned i=0;i<n;i++)a[i]=b[i];}
int viper_network_reply(const uint8_t *in,unsigned n,const uint8_t mac[6],const uint8_t ip[4],uint8_t *out,unsigned cap) {
 if(!in||!mac||!ip||!out||n<14) return -1;
 if(u16(in+12)==0x0806) {
  if(n<42||cap<42||u16(in+14)!=1||u16(in+16)!=0x0800||in[18]!=6||in[19]!=4||u16(in+20)!=1||!equal(in+38,ip,4))return 0;
  copy(out,in+22,6);copy(out+6,mac,6);put16(out+12,0x0806);
  copy(out+14,in+14,8);put16(out+20,2);
  copy(out+22,mac,6);copy(out+28,ip,4);
  copy(out+32,in+22,6);copy(out+38,in+28,4);
  return 42;
 }
 if(u16(in+12)!=0x0800||n<42)return 0;
 const uint8_t *pkt=in+14;
 unsigned header=(pkt[0]&15u)*4u,total=u16(pkt+2);
 if((pkt[0]>>4)!=4||header<20||n<14+header||total<header+8||total>n-14||pkt[9]!=1||
    (u16(pkt+6)&0x3FFFu)||!equal(pkt+16,ip,4)||viper_checksum(pkt,header)!=0||cap<14+total)return 0;
 const uint8_t *icmp=pkt+header;
 if(icmp[0]!=8||icmp[1]!=0||viper_checksum(icmp,total-header)!=0)return 0;
 copy(out,in+6,6);copy(out+6,mac,6);put16(out+12,0x0800);
 copy(out+14,pkt,total);
 uint8_t *reply=out+14;
 copy(reply+12,ip,4);copy(reply+16,pkt+12,4);
 reply[8]=64;reply[10]=reply[11]=0;
 put16(reply+10,viper_checksum(reply,header));
 uint8_t *echo=reply+header;echo[0]=0;echo[2]=echo[3]=0;
 put16(echo+2,viper_checksum(echo,total-header));
 return (int)(14+total);
}
