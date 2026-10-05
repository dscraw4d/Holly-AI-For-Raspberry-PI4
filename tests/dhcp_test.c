#include "dhcp.h"
#include "net.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct holly_dhcp s;
static uint8_t sent[600];static unsigned sent_n,count;
static const uint8_t mac[]={2,1,2,3,4,5},fallback[]={169,254,77,1},server[]={192,168,1,1},address[]={192,168,1,75};
static int send_frame(const uint8_t *p,unsigned n,void *c){(void)c;memcpy(sent,p,n);sent_n=n;count++;return 0;}
static int random_bytes(uint8_t *p,size_t n,void *c){(void)c;for(size_t i=0;i<n;i++)p[i]=(uint8_t)(i+count);return 0;}
static void w16(uint8_t *p,unsigned v){p[0]=v>>8;p[1]=v;}
static void w32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=v>>(24-i*8);}
static void start(void){holly_dhcp_init(&s,mac,fallback,send_frame,random_bytes,0);assert(!holly_dhcp_link(&s,1,100));assert(!holly_dhcp_tick(&s,100));assert(sent_n>=342&&sent[282]==53&&sent[284]==1);}
static unsigned reply(uint8_t *f,unsigned type){
 memset(f,0,600);memset(f,255,6);memcpy(f+6,"ROUTER",6);w16(f+12,0x800);
 uint8_t *ip=f+14,*u=ip+20,*b=u+8;ip[0]=0x45;ip[8]=64;ip[9]=17;memcpy(ip+12,server,4);memset(ip+16,255,4);
 w16(u,67);w16(u+2,68);b[0]=2;b[1]=1;b[2]=6;w32(b+4,s.xid);memcpy(b+16,address,4);memcpy(b+28,mac,6);w32(b+236,0x63825363);
 unsigned n=240;b[n++]=53;b[n++]=1;b[n++]=type;b[n++]=54;b[n++]=4;memcpy(b+n,server,4);n+=4;
 b[n++]=51;b[n++]=4;w32(b+n,120);n+=4;b[n++]=1;b[n++]=4;memcpy(b+n,"\xff\xff\xff\0",4);n+=4;
 b[n++]=255;w16(u+4,n+8);w16(ip+2,n+28);w16(ip+10,viper_checksum(ip,20));return n+42;
}
static void bind(void){
 uint8_t f[600];start();unsigned n=reply(f,2);assert(holly_dhcp_receive(&s,f,n,200)==1&&s.phase==HOLLY_DHCP_REQUEST);
 assert(!holly_dhcp_tick(&s,200));assert(sent[284]==3);n=reply(f,5);
 assert(holly_dhcp_receive(&s,f,n,300)==1&&s.phase==HOLLY_DHCP_PROBE&&!s.configured);
 assert(!holly_dhcp_tick(&s,300));assert(sent[13]==6&&sent[28]==0);
 assert(!holly_dhcp_tick(&s,1300));assert(!holly_dhcp_tick(&s,2300));assert(!holly_dhcp_tick(&s,4300));
 assert(s.leased&&s.configured&&s.phase==HOLLY_DHCP_BOUND&&!memcmp(s.ip,address,4));
}
int main(void){
 uint8_t f[600];start();unsigned n=reply(f,2);
 f[46]^=1;assert(holly_dhcp_receive(&s,f,n,150)==-1);f[46]^=1;
 f[70]^=1;assert(holly_dhcp_receive(&s,f,n,150)==-1);f[70]^=1;
 f[40]=1;assert(holly_dhcp_receive(&s,f,n,150)==-1);f[40]=0;
 f[284]=5;assert(!holly_dhcp_receive(&s,f,n,150));f[284]=2;
 /* Every truncated frame must leave the client unconfigured. */
 for(unsigned i=0;i<n;i++){(void)holly_dhcp_receive(&s,f,i,150);assert(!s.configured);}
 assert(holly_dhcp_receive(&s,f,n,200)==1);assert(!holly_dhcp_tick(&s,200));n=reply(f,5);
 f[n-1]=51;assert(holly_dhcp_receive(&s,f,n,300)==-1);f[n-1]=255;
 assert(holly_dhcp_receive(&s,f,n,300)==1);assert(!holly_dhcp_tick(&s,300));
 memcpy(f,sent,60);memcpy(f+22,"OTHER!",6);memcpy(f+28,address,4);
 assert(holly_dhcp_receive(&s,f,60,400)==1&&!s.configured&&s.phase==HOLLY_DHCP_WAIT&&sent[284]==4);
 bind();uint64_t renewal=s.renew_at;assert(!holly_dhcp_tick(&s,renewal)&&s.phase==HOLLY_DHCP_RENEW);
 assert(!memcmp(sent,"ROUTER",6)&&!memcmp(sent+26,address,4)&&!memcmp(sent+54,address,4));
 n=reply(f,5);/* Renewal ACKs commonly leave yiaddr zero and echo ciaddr. */
 memset(f+14+20+8+16,0,4); /* yiaddr is zero in a renewal ACK. */
 memcpy(f+14+20+8+12,s.ip,4); /* ciaddr carries the current lease. */
 /* The helper's broadcast target is still valid for parser coverage. */
 assert(holly_dhcp_receive(&s,f,n,renewal+100)==1&&s.phase==HOLLY_DHCP_BOUND&&s.leased);
 assert(s.renew_at>renewal);
 assert(!holly_dhcp_tick(&s,s.rebind_at)&&s.phase==HOLLY_DHCP_REBIND&&sent[0]==255);
 uint64_t expiry=s.expires;assert(!holly_dhcp_tick(&s,expiry)&&!s.configured&&s.phase==HOLLY_DHCP_SELECT);
 assert(!holly_dhcp_tick(&s,expiry+30000)&&s.configured&&!s.leased&&!memcmp(s.ip,fallback,4));
 assert(!holly_dhcp_link(&s,0,expiry+31000)&&!s.configured);
 bind();assert(!holly_dhcp_tick(&s,s.renew_at));n=reply(f,6);assert(holly_dhcp_receive(&s,f,n,s.now+10)==1&&!s.configured);
 puts("DHCP: discovery/request/ACK, ARP probing/conflict decline, renewal/rebinding/expiry, NAK, fallback, link reset and malformed input passed");
}
