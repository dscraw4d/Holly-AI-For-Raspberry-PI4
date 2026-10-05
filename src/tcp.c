#include "tcp.h"
#include "net.h"
static unsigned get16(const uint8_t *p) {return ((unsigned)p[0]<<8)|p[1];}
static uint32_t get32(const uint8_t *p) {return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void put16(uint8_t *p,unsigned v) {p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void put32(uint8_t *p,uint32_t v) {p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static void copy(uint8_t *d,const uint8_t *s,unsigned n) {for(unsigned i=0;i<n;i++)d[i]=s[i];}
static int equal(const uint8_t *a,const uint8_t *b,unsigned n) {for(unsigned i=0;i<n;i++)if(a[i]!=b[i])return 0;return 1;}
static uint32_t add_words(uint32_t sum,const uint8_t *p,unsigned n) {
    while(n>=2) {sum+=get16(p);p+=2;n-=2;}
    if(n)sum+=(unsigned)p[0]<<8;
    return sum;
}
static uint16_t folded(uint32_t sum) {
    while(sum>>16)sum=(sum&0xffffu)+(sum>>16);
    return (uint16_t)~sum;
}
static uint16_t tcp_sum(const uint8_t *src,const uint8_t *dst,const uint8_t *tcp,unsigned len) {
    uint32_t sum=add_words(0,src,4);
    sum=add_words(sum,dst,4);
    sum+=6u+len;
    return folded(add_words(sum,tcp,len));
}
void holly_tcp_init(struct holly_tcp *c) {
    if(c){c->phase=HOLLY_TCP_CLOSED;c->elapsed_ms=0;c->retries=0;holly_ssh_ident_init(&c->client_ident);}
}
static int response(const struct holly_tcp *c,const uint8_t mac[6],const uint8_t ip[4],
    uint8_t *out,unsigned cap,uint8_t flags,uint32_t seq,uint32_t ack,
    const uint8_t *data,unsigned data_len) {
    if(data_len>255||cap<54+data_len)return -1;
    copy(out,c->peer_mac,6);copy(out+6,mac,6);put16(out+12,0x0800);
    uint8_t *q=out+14,*r=q+20;
    for(unsigned i=0;i<40;i++)q[i]=0;
    q[0]=0x45;put16(q+2,40+data_len);put16(q+6,0x4000);q[8]=64;q[9]=6;
    copy(q+12,ip,4);copy(q+16,c->peer_ip,4);
    put16(q+10,viper_checksum(q,20));
    put16(r,2222);put16(r+2,c->peer_port);put32(r+4,seq);put32(r+8,ack);
    r[12]=0x50;r[13]=flags;put16(r+14,1024);
    if(data_len)copy(r+20,data,data_len);
    put16(r+16,tcp_sum(q+12,q+16,r,20+data_len));
    return (int)(54+data_len);
}
int holly_tcp_tick(struct holly_tcp *c,uint32_t elapsed_ms,
    const uint8_t mac[6],const uint8_t ip[4],uint8_t *out,unsigned cap) {
    if(!c||!mac||!ip||!out)return -1;
    if(c->phase==HOLLY_TCP_CLOSED)return 0;
    if(elapsed_ms>30000u-c->elapsed_ms)c->elapsed_ms=30000u;
    else c->elapsed_ms+=elapsed_ms;
    if(c->phase==HOLLY_TCP_ESTABLISHED) {
        if(c->elapsed_ms>=30000u)holly_tcp_init(c);
        return 0;
    }
    uint32_t delay=1000u<<c->retries;
    if(c->elapsed_ms<delay)return 0;
    if(c->retries>=3u){holly_tcp_init(c);return 0;}
    if(cap<54)return -1; /* Preserve the pending retry for the next call. */
    c->elapsed_ms=0;c->retries++;
    return response(c,mac,ip,out,cap,0x12,c->next_local-1,c->next_remote,0,0);
}
int holly_tcp_receive(struct holly_tcp *c,const uint8_t *f,unsigned n,
    const uint8_t mac[6],const uint8_t ip[4],uint8_t *out,unsigned cap) {
    if(!c||!f||!mac||!ip||!out)return -1;
    if(n<14||get16(f+12)!=0x0800)return 0;
    if(n<54)return -1;
    const uint8_t *p=f+14;
    unsigned ihl=(unsigned)(p[0]&15u)*4u,total=get16(p+2);
    if((p[0]>>4)!=4||ihl<20||ihl>60||n<14+ihl||total<ihl+20||total>n-14)return -1;
    if(p[9]!=6||!equal(p+16,ip,4)||(get16(p+6)&0x3fffu))return 0;
    if(viper_checksum(p,ihl)!=0)return -1;
    const uint8_t *t=p+ihl;
    unsigned tlen=total-ihl,thl=(unsigned)(t[12]>>4)*4u;
    if(thl<20||thl>tlen||tcp_sum(p+12,p+16,t,tlen)!=0)return -1;
    if(get16(t+2)!=2222)return 0;
    if(cap<54)return -1;
    uint8_t flags=t[13];
    uint32_t seq=get32(t+4),ack=get32(t+8);
    unsigned payload=tlen-thl;
    uint8_t answer_flags=0;
    uint32_t answer_seq=0,answer_ack=0;
    const uint8_t *answer_data=0;
    unsigned answer_length=0;
    if(c->phase==HOLLY_TCP_CLOSED) {
        if(!(flags&2u)||(flags&4u)||(flags&16u)||payload)return 0;
        copy(c->peer_mac,f+6,6);copy(c->peer_ip,p+12,4);
        c->peer_port=(uint16_t)get16(t);
        c->next_remote=seq+1;
        holly_ssh_ident_init(&c->client_ident);
        /* Fixed sequence number is for offline protocol development only. */
        c->next_local=0x484f4c4cu;
        c->phase=HOLLY_TCP_SYN_RECEIVED;
        c->elapsed_ms=0;c->retries=0;
        answer_flags=0x12;answer_seq=c->next_local++;
        answer_ack=c->next_remote;
    } else {
        if(!equal(c->peer_mac,f+6,6)||!equal(c->peer_ip,p+12,4)||get16(t)!=c->peer_port)return 0;
        if(flags&4u){c->phase=HOLLY_TCP_CLOSED;return 0;}
        if(c->phase==HOLLY_TCP_SYN_RECEIVED) {
            if((flags&2u) && !(flags&16u) && seq+1==c->next_remote) {
                c->elapsed_ms=0;
                answer_flags=0x12;answer_seq=c->next_local-1;answer_ack=c->next_remote;
            } else if((flags&16u) && !(flags&2u) && ack==c->next_local && seq==c->next_remote) {
                c->phase=HOLLY_TCP_ESTABLISHED;c->elapsed_ms=0;
                if(payload) {
                    if(holly_ssh_ident_feed(&c->client_ident,t+thl,payload)<0){holly_tcp_init(c);return 0;}
                    c->next_remote+=payload;
                }
                answer_flags=0x18;answer_seq=c->next_local;answer_ack=c->next_remote;
                answer_data=holly_ssh_server_ident;answer_length=holly_ssh_server_ident_length;
                if(cap<54+answer_length){holly_tcp_init(c);return -1;}
                c->next_local+=answer_length;
            } else return 0;
        } else {
            if(!(flags&16u)||ack!=c->next_local||seq!=c->next_remote)return 0;
            if(flags&1u) {
                /* Acknowledge FIN, then forget the diagnostic connection. */
                answer_flags=0x10;answer_seq=c->next_local;answer_ack=seq+1;
                c->phase=HOLLY_TCP_CLOSED;
            } else if(payload) {
                if(holly_ssh_ident_feed(&c->client_ident,t+thl,payload)<0){holly_tcp_init(c);return 0;}
                c->next_remote+=payload;c->elapsed_ms=0;
                answer_flags=0x10;answer_seq=c->next_local;answer_ack=c->next_remote;
            } else {c->elapsed_ms=0;return 0;}
        }
    }
    return response(c,mac,ip,out,cap,answer_flags,answer_seq,answer_ack,answer_data,answer_length);
}
