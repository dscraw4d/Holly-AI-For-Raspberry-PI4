#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tcp.h"
#include "net.h"
#include "ssh_ident.h"
static void be16(uint8_t *p,unsigned n){p[0]=(uint8_t)(n>>8);p[1]=(uint8_t)n;}
static void be32(uint8_t *p,uint32_t n){p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n;}
static uint32_t words(uint32_t sum,const uint8_t *p,unsigned n){
 while(n>=2){sum+=((unsigned)p[0]<<8)|p[1];p+=2;n-=2;}if(n)sum+=(unsigned)p[0]<<8;return sum;
}
static uint16_t sum(uint32_t n){while(n>>16)n=(n&65535)+(n>>16);return (uint16_t)~n;}
static uint16_t tcp_checksum(const uint8_t *p){
 unsigned len=((unsigned)p[2]<<8|p[3])-20;
 uint32_t n=words(0,p+12,8)+6+len;return sum(words(n,p+20,len));
}
static void incoming(uint8_t *f,uint8_t flags,uint32_t seq,uint32_t ack){
 memset(f,0,54);memset(f,0x55,6);memset(f+6,0xaa,6);be16(f+12,0x0800);
 uint8_t *p=f+14,*t=p+20;p[0]=0x45;be16(p+2,40);be16(p+6,0x4000);p[8]=64;p[9]=6;
 p[12]=192;p[13]=168;p[14]=1;p[15]=2;p[16]=192;p[17]=168;p[18]=1;p[19]=90;
 be16(p+10,viper_checksum(p,20));
 be16(t,45000);be16(t+2,2222);be32(t+4,seq);be32(t+8,ack);
 t[12]=0x50;t[13]=flags;be16(t+14,1024);be16(t+16,tcp_checksum(p));
}
static unsigned with_payload(uint8_t *f,const uint8_t *data,unsigned n){
 assert(n<=200);memcpy(f+54,data,n);be16(f+16,40+n);
 f[24]=f[25]=0;be16(f+24,viper_checksum(f+14,20));
 f[50]=f[51]=0;be16(f+50,tcp_checksum(f+14));return 54+n;
}
int main(void){
 struct holly_tcp c;holly_tcp_init(&c);
 const uint8_t mac[6]={2,0,0,0,0,1},ip[4]={192,168,1,90};
 uint8_t f[256],r[256];incoming(f,2,12345,0);
 assert(holly_tcp_receive(&c,f,54,mac,ip,r,53)==-1 && c.phase==HOLLY_TCP_CLOSED);
 assert(holly_tcp_receive(&c,f,54,mac,ip,r,54)==54);
 assert(c.phase==HOLLY_TCP_SYN_RECEIVED && c.next_remote==12346);
 assert(r[47]==0x12 && tcp_checksum(r+14)==0 && viper_checksum(r+14,20)==0);
 assert(holly_tcp_tick(&c,999,mac,ip,r,54)==0);
 assert(holly_tcp_tick(&c,1,mac,ip,r,53)==-1 && c.retries==0);
 assert(holly_tcp_tick(&c,0,mac,ip,r,54)==54 && c.retries==1);
 assert(r[47]==0x12 && tcp_checksum(r+14)==0);
 assert(holly_tcp_receive(&c,f,54,mac,ip,r,54)==54); /* lost SYN-ACK */
 assert(c.next_local==0x484f4c4du);
 incoming(f,16,12346,c.next_local-1);
 assert(holly_tcp_receive(&c,f,54,mac,ip,r,54)==0 && c.phase==HOLLY_TCP_SYN_RECEIVED);
 incoming(f,16,12346,c.next_local);
 f[50]^=1;assert(holly_tcp_receive(&c,f,54,mac,ip,r,54)==-1); /* corrupt checksum */
 incoming(f,16,12346,c.next_local);
 assert(holly_tcp_receive(&c,f,54,mac,ip,r,sizeof(r))==(int)(54+holly_ssh_server_ident_length));
 assert(c.phase==HOLLY_TCP_ESTABLISHED && r[47]==0x18);
 assert(memcmp(r+54,holly_ssh_server_ident,holly_ssh_server_ident_length)==0);
 assert(tcp_checksum(r+14)==0);
 const uint8_t client[]="SSH-2.0-Example_1\r\n";
 incoming(f,0x18,12346,c.next_local);
 unsigned in_len=with_payload(f,client,sizeof(client)-1);
 assert(holly_tcp_receive(&c,f,in_len,mac,ip,r,sizeof(r))==54);
 assert(c.client_ident.complete && c.next_remote==12346+sizeof(client)-1);
 assert(r[47]==0x10 && tcp_checksum(r+14)==0);
 assert(holly_tcp_tick(&c,29999,mac,ip,r,54)==0 && c.phase==HOLLY_TCP_ESTABLISHED);
 assert(holly_tcp_tick(&c,1,mac,ip,r,54)==0 && c.phase==HOLLY_TCP_CLOSED);
 incoming(f,2,12345,0);
 assert(holly_tcp_receive(&c,f,54,mac,ip,r,54)==54);
 incoming(f,16,12346,c.next_local);
 assert(holly_tcp_receive(&c,f,54,mac,ip,r,sizeof(r))==(int)(54+holly_ssh_server_ident_length) && c.phase==HOLLY_TCP_ESTABLISHED);
 incoming(f,17,12346,c.next_local);
 assert(holly_tcp_receive(&c,f,54,mac,ip,r,54)==54);
 assert(r[47]==16 && c.phase==HOLLY_TCP_CLOSED && tcp_checksum(r+14)==0);
 incoming(f,2,12345,0);
 assert(holly_tcp_receive(&c,f,54,mac,ip,r,54)==54);
 assert(holly_tcp_tick(&c,1000,mac,ip,r,54)==54 && c.retries==1);
 assert(holly_tcp_tick(&c,2000,mac,ip,r,54)==54 && c.retries==2);
 assert(holly_tcp_tick(&c,4000,mac,ip,r,54)==54 && c.retries==3);
 assert(holly_tcp_tick(&c,8000,mac,ip,r,54)==0 && c.phase==HOLLY_TCP_CLOSED);
 puts("Bounded TCP diagnostic handshake and timer tests passed");
}
