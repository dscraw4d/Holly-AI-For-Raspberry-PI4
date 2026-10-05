#include "dhcp.h"
#include "net.h"
static void cp(uint8_t *d,const uint8_t *s,unsigned n){while(n--)*d++=*s++;}
static void zero(void *p,unsigned n){uint8_t *d=p;while(n--)*d++=0;}
static int eq(const uint8_t *a,const uint8_t *b,unsigned n){while(n--)if(*a++!=*b++)return 0;return 1;}
static unsigned r16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t r32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void w16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void w32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(24-8*i));}
static const uint8_t broadcast[6]={255,255,255,255,255,255},nil[4]={0};
static int ip_ok(const uint8_t *p){return p[0]&&p[0]!=127&&p[0]<224&&!eq(p,nil,4);}
static uint16_t udp_sum(const uint8_t *ip,const uint8_t *u,unsigned n){
 uint32_t s=17+n;for(unsigned i=12;i<20;i+=2)s+=r16(ip+i);
 while(n>1){s+=r16(u);u+=2;n-=2;}if(n)s+=(unsigned)*u<<8;
 while(s>>16)s=(s&65535)+(s>>16);
 return (uint16_t)~s;
}
static void opt(uint8_t *p,unsigned *n,unsigned code,const uint8_t *v,unsigned len){p[(*n)++]=(uint8_t)code;p[(*n)++]=(uint8_t)len;cp(p+*n,v,len);*n+=len;}
static int message(struct holly_dhcp *s,unsigned type){
 uint8_t *f=s->frame,*ip=f+14,*u=ip+20,*b=u+8;zero(f,sizeof(s->frame));
 int renew=s->phase==HOLLY_DHCP_RENEW,assigned=renew||s->phase==HOLLY_DHCP_REBIND;
 cp(f,renew?s->server_mac:broadcast,6);cp(f+6,s->mac,6);w16(f+12,0x800);
 ip[0]=0x45;ip[8]=64;ip[9]=17;cp(ip+12,assigned?s->ip:nil,4);cp(ip+16,renew?s->server:broadcast,4);
 w16(u,68);w16(u+2,67);b[0]=1;b[1]=1;b[2]=6;w32(b+4,s->xid);
 uint64_t elapsed=(s->now-s->started)/1000;w16(b+8,elapsed>65535?65535:(unsigned)elapsed);
 if(!renew)w16(b+10,0x8000);
 if(assigned)cp(b+12,s->ip,4);
 cp(b+28,s->mac,6);w32(b+236,0x63825363);unsigned n=240;uint8_t t=(uint8_t)type;
 opt(b,&n,53,&t,1);uint8_t id[7]={1};cp(id+1,s->mac,6);opt(b,&n,61,id,7);
 opt(b,&n,12,(const uint8_t *)"holly",5);
 if((type==3&&s->phase==HOLLY_DHCP_REQUEST)||type==4){opt(b,&n,50,s->offered,4);opt(b,&n,54,s->server,4);}
 if(type!=4){const uint8_t requested[]={1,3,6,51,58,59};opt(b,&n,55,requested,sizeof(requested));const uint8_t max[]={2,64};opt(b,&n,57,max,2);}
 b[n++]=255;if(n<300)n=300;
 w16(u+4,n+8);w16(ip+2,n+28);w16(ip+10,viper_checksum(ip,20));
 uint16_t sum=udp_sum(ip,u,n+8);w16(u+6,sum?sum:65535);
 return s->send(f,n+42,s->context);
}
static int arp(struct holly_dhcp *s,int announce){
 uint8_t *f=s->frame;zero(f,60);cp(f,broadcast,6);cp(f+6,s->mac,6);w16(f+12,0x806);
 w16(f+14,1);w16(f+16,0x800);f[18]=6;f[19]=4;w16(f+20,1);cp(f+22,s->mac,6);
 if(announce)cp(f+28,s->offered,4);
 cp(f+38,s->offered,4);return s->send(f,60,s->context);
}
static int begin(struct holly_dhcp *s){
 uint8_t id[4];if(s->random(id,4,s->context)){s->phase=HOLLY_DHCP_OFF;return -1;}
 s->xid=r32(id);s->phase=HOLLY_DHCP_SELECT;s->started=s->now;s->retries=0;s->due=s->now;
 return 0;
}
void holly_dhcp_init(struct holly_dhcp *s,const uint8_t mac[6],const uint8_t fallback[4],holly_dhcp_send_fn send,holly_dhcp_random_fn random,void *ctx){
 zero(s,sizeof(*s));cp(s->mac,mac,6);cp(s->fallback,fallback,4);s->send=send;s->random=random;s->context=ctx;
}
int holly_dhcp_link(struct holly_dhcp *s,int up,uint64_t ms){
 s->now=ms;s->configured=0;s->leased=0;zero(s->ip,4);s->phase=HOLLY_DHCP_OFF;
 return up?begin(s):0;
}
static void revoke(struct holly_dhcp *s){s->configured=0;s->leased=0;zero(s->ip,4);}
int holly_dhcp_tick(struct holly_dhcp *s,uint64_t ms){
 s->now=ms;if(s->phase==HOLLY_DHCP_OFF)return 0;
 if(s->leased&&ms>=s->expires){revoke(s);if(begin(s))return -1;}
 if(s->leased&&ms>=s->rebind_at&&s->phase!=HOLLY_DHCP_REBIND){s->phase=HOLLY_DHCP_REBIND;s->due=ms;s->retries=0;}
 else if(s->phase==HOLLY_DHCP_BOUND&&ms>=s->renew_at){
  uint8_t id[4];if(s->random(id,4,s->context)){revoke(s);s->phase=HOLLY_DHCP_OFF;return -1;}
  s->xid=r32(id);s->phase=HOLLY_DHCP_RENEW;s->started=ms;s->due=ms;s->retries=0;
 }
 if(s->phase==HOLLY_DHCP_SELECT&&!s->configured&&ms-s->started>=30000){cp(s->ip,s->fallback,4);s->configured=1;}
 if(ms<s->due||s->phase==HOLLY_DHCP_BOUND)return 0;
 if(s->phase==HOLLY_DHCP_WAIT)return begin(s);
 if(s->phase==HOLLY_DHCP_PROBE){
  if(ms>=s->expires){revoke(s);return begin(s);}
  if(s->probes<3){if(arp(s,0))return -1;s->probes++;s->due=ms+(s->probes==3?2000:1000);return 0;}
  cp(s->ip,s->offered,4);s->configured=1;s->leased=1;s->phase=HOLLY_DHCP_BOUND;return arp(s,1);
 }
 if(s->phase==HOLLY_DHCP_REQUEST&&s->retries>=4)return begin(s);
 if(!s->retries&&s->phase!=HOLLY_DHCP_SELECT)s->request_time=ms;
 int result=message(s,s->phase==HOLLY_DHCP_SELECT?1:3);
 unsigned shift=s->retries<4?s->retries:4;uint8_t jitter[2];
 if(s->random(jitter,2,s->context)){revoke(s);s->phase=HOLLY_DHCP_OFF;return -1;}
 s->due=ms+(4000u<<shift)+(r16(jitter)%1000);if(s->retries<5)s->retries++;
 return result;
}
struct options {uint8_t type,server[4],mask[4],router[4],dns[4],overload;uint32_t lease,t1,t2;unsigned seen;};
static int options(struct options *o,const uint8_t *p,unsigned n,int main){
 unsigned i=0;while(i<n){unsigned code=p[i++];if(code==255)return 0;if(!code)continue;if(i>=n)return -1;
  unsigned len=p[i++];if(len>n-i)return -1;const uint8_t *v=p+i;i+=len;
  unsigned bit=0,need=4;switch(code){case 53:bit=1;need=1;break;case 54:bit=2;break;case 51:bit=4;break;
   case 1:bit=8;break;case 3:bit=16;break;case 6:bit=32;break;case 58:bit=64;break;case 59:bit=128;break;
   case 52:bit=256;need=1;if(!main)return -1;break;default:continue;}
  if(o->seen&bit)return -1;
  if((code==3||code==6)?(!len||len%4):(len!=need))return -1;
  o->seen|=bit;switch(code){case 53:o->type=v[0];break;case 54:cp(o->server,v,4);break;
   case 51:o->lease=r32(v);break;case 58:o->t1=r32(v);break;case 59:o->t2=r32(v);break;
   case 1:cp(o->mask,v,4);break;case 3:cp(o->router,v,4);break;case 6:cp(o->dns,v,4);break;
   case 52:if(!v[0]||v[0]>3)return -1;o->overload=v[0];break;}
 }return -1;
}
int holly_dhcp_receive(struct holly_dhcp *s,const uint8_t *f,unsigned n,uint64_t ms){
 s->now=ms;if(s->phase==HOLLY_DHCP_OFF||n<14)return 0;
 if(r16(f+12)==0x806&&n>=42&&(s->phase==HOLLY_DHCP_PROBE||s->leased)){
  if(r16(f+14)!=1||r16(f+16)!=0x800||f[18]!=6||f[19]!=4||(r16(f+20)!=1&&r16(f+20)!=2))return 0;
  if(!eq(f+22,s->mac,6)&&(eq(f+28,s->offered,4)||(s->phase==HOLLY_DHCP_PROBE&&eq(f+28,nil,4)&&eq(f+38,s->offered,4)))){
   (void)message(s,4);revoke(s);s->phase=HOLLY_DHCP_WAIT;s->due=ms+10000;return 1;
  }return 0;
 }
 struct viper_packet p;if(viper_parse_packet(f,n,&p)!=1||p.protocol!=17||p.source_port!=67||p.target_port!=68)return 0;
 if(!eq(f,s->mac,6)&&!eq(f,broadcast,6))return 0;
 unsigned ihl=(f[14]&15u)*4;const uint8_t *u=f+14+ihl,*b=p.payload;unsigned len=p.payload_len;
 if(r16(u+6)&&udp_sum(f+14,u,len+8))return -1;
 if(len<240||b[0]!=2||b[1]!=1||b[2]!=6||r32(b+4)!=s->xid||!eq(b+28,s->mac,6)||r32(b+236)!=0x63825363)return -1;
 /* Direct LAN DHCP only: no BOOTP relay routing. */
 if(!eq(b+24,nil,4))return 0;
 if(!eq(p.target_ip,broadcast,4)&&!eq(p.target_ip,b+16,4)&&!eq(p.target_ip,s->ip,4))return 0;
 struct options o;zero(&o,sizeof(o));
 if(options(&o,b+240,len-240,1)||(o.overload&1&&options(&o,b+108,128,0))||(o.overload&2&&options(&o,b+44,64,0)))return -1;
 if((o.seen&3)!=3||!ip_ok(o.server)||!eq(o.server,p.source_ip,4))return -1;
 if(s->phase==HOLLY_DHCP_SELECT&&o.type==2){
  if(!ip_ok(b+16))return -1;
  cp(s->offered,b+16,4);cp(s->server,o.server,4);cp(s->server_mac,p.source_mac,6);
  s->phase=HOLLY_DHCP_REQUEST;s->retries=0;s->due=ms;return 1;
 }
 if(s->phase!=HOLLY_DHCP_REQUEST&&s->phase!=HOLLY_DHCP_RENEW&&s->phase!=HOLLY_DHCP_REBIND)return 0;
 if(s->phase!=HOLLY_DHCP_REBIND&&!eq(o.server,s->server,4))return 0;
 if(o.type==6){revoke(s);s->phase=HOLLY_DHCP_WAIT;s->due=ms+10000;return 1;}
 if(o.type!=5)return 0;
 const uint8_t *ack_ip=b+16;
 if(!ip_ok(ack_ip)){
  /* In RENEWING/REBINDING, DHCPACK normally carries the address in
     ciaddr and leaves yiaddr zero. Keep the current lease address. */
  if(!s->leased||!ip_ok(s->ip))return -1;
  ack_ip=s->ip;
 }
 if((s->phase==HOLLY_DHCP_REQUEST&&!eq(ack_ip,s->offered,4))||
    (s->phase!=HOLLY_DHCP_REQUEST&&!eq(ack_ip,s->ip,4)&&!eq(ack_ip,s->offered,4))||
    (o.seen&12)!=12||o.lease<10)return -1;
 uint32_t mask=r32(o.mask),inverse=~mask,addr=r32(ack_ip);
 if(!mask||(inverse&(inverse+1))||!(addr&inverse)||(addr&inverse)==inverse)return -1;
 uint32_t t1=o.t1,t2=o.t2;if(!t1||t1>=t2||t2>=o.lease){t1=o.lease/2;t2=(uint32_t)((uint64_t)o.lease*7/8);}
 s->lease=o.lease;s->t1=t1;s->t2=t2;
 s->lease_started=ms;
 s->renew_at=ms+(uint64_t)t1*1000;s->rebind_at=ms+(uint64_t)t2*1000;s->expires=ms+(uint64_t)o.lease*1000;
 if(ms>=s->expires)return -1;
 cp(s->server,o.server,4);cp(s->server_mac,p.source_mac,6);cp(s->mask,o.mask,4);cp(s->router,o.router,4);cp(s->dns,o.dns,4);
 if(s->leased)s->phase=HOLLY_DHCP_BOUND;
 else {s->phase=HOLLY_DHCP_PROBE;s->probes=0;s->due=ms;}
 return 1;
}
