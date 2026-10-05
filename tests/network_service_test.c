#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "network_service.h"
#include "net.h"
#include "ssh_ident.h"
static void b16(uint8_t *p,unsigned n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void b32(uint8_t *p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static uint16_t tcp_sum(const uint8_t *p){
 unsigned sum=6+20;
 for(unsigned i=12;i<20;i+=2)sum+=((unsigned)p[i]<<8)|p[i+1];
 for(unsigned i=20;i<40;i+=2)sum+=((unsigned)p[i]<<8)|p[i+1];
 while(sum>>16)sum=(sum&65535)+(sum>>16);
 return (uint16_t)~sum;
}
int main(void) {
 static struct holly_network net;
 const uint8_t mac[6]={2,0,0,0,0,1},ip[4]={192,168,1,90};
 holly_network_init(&net,mac,ip);
 uint8_t arp[42]={0},out[64]={0};
 memset(arp,255,6);arp[6]=0x12;arp[12]=8;arp[13]=6;
 arp[15]=1;arp[16]=8;arp[18]=6;arp[19]=4;arp[21]=1;
 arp[22]=0x12;arp[28]=192;arp[29]=168;arp[30]=1;arp[31]=2;
 memcpy(arp+38,ip,4);
 assert(holly_network_receive(&net,arp,sizeof(arp))==0);
 assert(holly_network_process(&net,1)==1);
 assert(net.received==1 && net.answered==1 && net.dropped==0);
 unsigned len=0;
 assert(holly_network_transmit(&net,out,sizeof(out),&len)==0);
 assert(len==42 && out[21]==2 && memcmp(out+22,mac,6)==0);
 assert(holly_network_transmit(&net,out,sizeof(out),&len)==-2);
 uint8_t syn[54]={0};memcpy(syn,mac,6);memset(syn+6,0x12,6);b16(syn+12,0x0800);
 uint8_t *pkt=syn+14,*tcp=pkt+20;
 pkt[0]=0x45;b16(pkt+2,40);b16(pkt+6,0x4000);pkt[8]=64;pkt[9]=6;
 pkt[12]=192;pkt[13]=168;pkt[14]=1;pkt[15]=2;memcpy(pkt+16,ip,4);
 b16(pkt+10,viper_checksum(pkt,20));
 b16(tcp,45000);b16(tcp+2,2222);tcp[7]=1;tcp[12]=0x50;tcp[13]=2;
 b16(tcp+14,1024);b16(tcp+16,tcp_sum(pkt));
 assert(holly_network_receive(&net,syn,sizeof(syn))==0);
 assert(holly_network_process(&net,1)==1);
 assert(holly_network_transmit(&net,out,sizeof(out),&len)==0);
 assert(len==54 && out[47]==0x12 && net.diagnostic_tcp.phase==HOLLY_TCP_SYN_RECEIVED);
 assert(net.answered==2 && net.dropped==0);
 assert(holly_network_tick(&net,1000)==54);
 assert(holly_network_transmit(&net,out,sizeof(out),&len)==0);
 assert(len==54 && out[47]==0x12 && net.answered==3);
 uint32_t server_seq=((uint32_t)out[38]<<24)|((uint32_t)out[39]<<16)|((uint32_t)out[40]<<8)|out[41];
 tcp[13]=0x10;b32(tcp+4,2);b32(tcp+8,server_seq+1);
 tcp[16]=tcp[17]=0;b16(tcp+16,tcp_sum(pkt));
 assert(holly_network_receive(&net,syn,sizeof(syn))==0);
 assert(holly_network_process(&net,1)==1);
 assert(holly_network_transmit(&net,out,sizeof(out),&len)==-3); /* reply exceeds this buffer */
 uint8_t greeting[128];
 assert(holly_network_transmit(&net,greeting,sizeof(greeting),&len)==0);
 assert(len==54+holly_ssh_server_ident_length);
 assert(memcmp(greeting+54,holly_ssh_server_ident,holly_ssh_server_ident_length)==0);
 assert(net.diagnostic_tcp.phase==HOLLY_TCP_ESTABLISHED);
 puts("End-to-end software ARP and SSH greeting paths passed");
}
