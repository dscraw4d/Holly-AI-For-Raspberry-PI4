#include "tcp_stream.h"
#include "net.h"
#include "reply.h"
static void cp(uint8_t *a,const uint8_t *b,unsigned n){for(unsigned i=0;i<n;i++)a[i]=b[i];}
static unsigned r16(const uint8_t *p){return ((unsigned)p[0]<<8)|p[1];}
static uint32_t r32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void w16(uint8_t *p,unsigned v){p[0]=(uint8_t)(v>>8);p[1]=(uint8_t)v;}
static void w32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(24-i*8));}
static uint16_t sum(const uint8_t *a,const uint8_t *b,const uint8_t *p,unsigned n){
    uint32_t v=6+n;
    for(unsigned i=0;i<4;i+=2)v+=r16(a+i)+r16(b+i);
    while(n>1){v+=r16(p);p+=2;n-=2;}if(n)v+=(unsigned)*p<<8;
    while(v>>16)v=(v&65535u)+(v>>16);
    return (uint16_t)~v;
}
static int send(struct holly_tcp_stream *s,unsigned flags,uint32_t seq,unsigned bytes){
    uint8_t *f=s->frame,*ip=f+14,*t=ip+20;unsigned th=(flags&2u)?24:20;
    if(bytes>1460||bytes>s->queued)return -1;
    for(unsigned i=0;i<14+20+th+bytes;i++)f[i]=0;
    cp(f,s->peer_mac,6);cp(f+6,s->mac,6);w16(f+12,0x0800);
    ip[0]=0x45;w16(ip+2,20+th+bytes);w16(ip+6,0x4000);ip[8]=64;ip[9]=6;
    cp(ip+12,s->ip,4);cp(ip+16,s->peer_ip,4);w16(ip+10,viper_checksum(ip,20));
    w16(t,s->local_port);w16(t+2,s->peer_port);w32(t+4,seq);w32(t+8,s->remote);
    t[12]=(uint8_t)((th/4)<<4);t[13]=(uint8_t)flags;w16(t+14,32768);
    if(th==24){t[20]=2;t[21]=4;w16(t+22,1460);}
    for(unsigned i=0;i<bytes;i++)t[th+i]=s->queue[(s->head+i)&(HOLLY_TCP_QUEUE-1u)];
    w16(t+16,sum(ip+12,ip+16,t,th+bytes));
    return s->transmit(f,14+20+th+bytes,s->context);
}
static void reset(struct holly_tcp_stream *s){
    if(s->app_close)s->app_close(s->app_context);
    holly_ssh_server_destroy(&s->ssh);holly_secret_wipe(s->queue,sizeof(s->queue));
    s->phase=HOLLY_STREAM_LISTEN;s->queued=0;s->head=0;s->flight=0;s->fin_requested=0;s->fin_sent=0;s->peer_fin=0;
    s->idle_ms=0;s->retry_ms=0;s->retries=0;
}
void holly_tcp_stream_link_lost(struct holly_tcp_stream *s){if(s)reset(s);}
static int random_bytes(uint8_t *p,size_t n,void *context){struct holly_tcp_stream *s=context;return s->random(p,n,s->context);}
static int enqueue(const uint8_t *p,size_t n,void *context){
    struct holly_tcp_stream *s=context;if(n>HOLLY_TCP_QUEUE-s->queued||s->fin_requested)return -1;
    for(size_t i=0;i<n;i++)s->queue[(s->head+s->queued+(unsigned)i)&(HOLLY_TCP_QUEUE-1u)]=p[i];
    s->queued+=(unsigned)n;return 0;
}
static int pump(struct holly_tcp_stream *s){
    if(s->phase!=HOLLY_STREAM_OPEN||s->flight)return 0;
    if(s->queued&&s->peer_window){
        unsigned n=s->queued;if(n>s->mss)n=s->mss;if(n>s->peer_window)n=s->peer_window;
        if(send(s,0x18,s->next,n))return -1;
        s->flight=n;s->next+=n;s->retry_ms=0;s->retries=0;
    }else if(!s->queued&&s->fin_requested){
        if(send(s,0x11,s->next,0))return -1;
        s->fin_sent=1;s->flight=1;s->next++;s->phase=HOLLY_STREAM_FIN;s->retry_ms=0;s->retries=0;
    }
    return 0;
}
void holly_tcp_stream_init(struct holly_tcp_stream *s,const uint8_t mac[6],const uint8_t ip[4],
    const struct holly_ssh_credentials *c,holly_ssh_random_fn random,holly_frame_write_fn transmit,void *context){
    holly_secret_wipe(s,sizeof(*s));cp(s->mac,mac,6);cp(s->ip,ip,4);s->credentials=c;
    s->random=random;s->transmit=transmit;s->context=context;s->phase=HOLLY_STREAM_LISTEN;s->local_port=22;
}
int holly_tcp_stream_receive(struct holly_tcp_stream *s,const uint8_t *f,unsigned n){
    if(!s||!f||!s->random||!s->transmit||n<14)return -1;
    const uint8_t broadcast[6]={255,255,255,255,255,255};
    if(!holly_tag_equal(f,s->mac,6)&&!holly_tag_equal(f,broadcast,6))return 0;
    int reply=s->local_port==22?viper_network_reply(f,n,s->mac,s->ip,s->frame,sizeof(s->frame)):0;
    if(reply>0)return s->transmit(s->frame,(unsigned)reply,s->context);
    if(r16(f+12)!=0x0800||n<54)return 0;
    const uint8_t *ip=f+14;unsigned ihl=(ip[0]&15u)*4u,total=r16(ip+2);
    if((ip[0]>>4)!=4||ihl<20||ihl>60||n<14+ihl||total<ihl+20||total>n-14)return -1;
    if(ip[9]!=6||!holly_tag_equal(ip+16,s->ip,4)||(r16(ip+6)&0x3fffu))return 0;
    if(viper_checksum(ip,ihl))return -1;
    const uint8_t *t=ip+ihl;unsigned len=total-ihl,th=(t[12]>>4)*4u;
    if(th<20||th>len||sum(ip+12,ip+16,t,len))return -1;
    if(r16(t+2)!=s->local_port)return 0;
    unsigned flags=t[13],payload=len-th;uint32_t seq=r32(t+4),ack=r32(t+8);
    if(s->phase==HOLLY_STREAM_LISTEN){
        if((flags&0x17u)!=2||payload)return 0;
        uint16_t mss=536;
        for(unsigned i=20;i<th;){
            unsigned kind=t[i++];if(!kind)break;if(kind==1)continue;
            if(i>=th||t[i]<2||t[i]>th-i+1)return -1;
            unsigned size=t[i++];if(kind==2&&size==4){mss=(uint16_t)r16(t+i);if(!mss)return -1;if(mss>1460)mss=1460;}
            i+=size-2;
        }
        uint8_t rnd[4];if(s->random(rnd,4,s->context))return -1;
        cp(s->peer_mac,f+6,6);cp(s->peer_ip,ip+12,4);s->peer_port=(uint16_t)r16(t);s->peer_window=(uint16_t)r16(t+14);s->mss=mss;
        s->una=r32(rnd);s->next=s->una+1;s->remote=seq+1;s->phase=HOLLY_STREAM_SYN;s->flight=1;
        s->idle_ms=0;s->retry_ms=0;s->retries=0;
        return send(s,0x12,s->una,0);
    }
    if(!holly_tag_equal(f+6,s->peer_mac,6)||!holly_tag_equal(ip+12,s->peer_ip,4)||r16(t)!=s->peer_port)return 0;
    if(flags&4u){if(seq==s->remote)reset(s);return 0;}
    if(s->phase==HOLLY_STREAM_SYN){
        if((flags&0x17u)==2&&seq+1==s->remote)return send(s,0x12,s->una,0);
        if(!(flags&16u)||(flags&2u)||ack!=s->next||seq!=s->remote)return 0;
        s->una=ack;s->flight=0;s->phase=HOLLY_STREAM_OPEN;s->peer_window=(uint16_t)r16(t+14);
        if(s->app_start?s->app_start(enqueue,s,s->app_context):holly_ssh_server_start(&s->ssh,s->credentials,enqueue,random_bytes,s))s->fin_requested=1;
    }else{
        if(!(flags&16u)||(flags&2u))return 0;
        uint32_t advance=ack-s->una;
        if(advance>s->flight)return send(s,0x10,s->next,0);
        if(advance){
            s->una=ack;s->flight-=advance;s->retry_ms=0;s->retries=0;
            if(!s->fin_sent){s->head=(s->head+advance)&(HOLLY_TCP_QUEUE-1u);s->queued-=advance;}
        }
        /* Only current receive sequence may update the advertised window. */
        if(seq==s->remote){
            uint16_t updated=(uint16_t)r16(t+14);
            if(!s->peer_window&&updated){s->retry_ms=0;s->retries=0;}
            s->peer_window=updated;
        }
    }
    if(s->phase==HOLLY_STREAM_FIN&&!s->flight&&s->peer_fin){reset(s);return 0;}
    if(seq!=s->remote)return send(s,0x10,s->next,0); /* ACK duplicates/out-of-order; do not deliver twice. */
    s->idle_ms=0;
    if(s->phase==HOLLY_STREAM_OPEN&&payload){
        s->remote+=payload;
        /* Acknowledge accepted TCP bytes before synchronous key computation.
         * Otherwise the client retransmits while this CPU is signing RSA. */
        if(send(s,0x10,s->next,0))return -1;
        if(!s->fin_requested&&(s->app_feed?s->app_feed(t+th,payload,s->app_context):holly_ssh_server_feed(&s->ssh,t+th,payload)))s->fin_requested=1;
    }
    if(flags&1u){s->remote++;s->peer_fin=1;s->fin_requested=1;if(send(s,0x10,s->next,0))return -1;
        /* Free a closing port probe before draining its unread SSH banner. */
        if(!s->app_feed&&!s->ssh.have_session_id){
            (void)send(s,0x14,s->next,0);reset(s);return 0;
        }
        if(s->phase==HOLLY_STREAM_FIN&&!s->flight){reset(s);return 0;}}
    return pump(s);
}
int holly_tcp_stream_tick(struct holly_tcp_stream *s,uint32_t ms){
    if(!s)return -1;
    if(s->phase==HOLLY_STREAM_LISTEN)return 0;
    unsigned limit=(s->local_port==80)?10000u:(s->phase==HOLLY_STREAM_OPEN&&(s->app_feed||s->ssh.stage==HOLLY_SSH_RUNNING))?3600000u:120000u;
    if(s->phase==HOLLY_STREAM_SYN)limit=10000u;
    else if(s->phase==HOLLY_STREAM_FIN)limit=5000u;
    else if(!s->app_feed&&s->ssh.stage==HOLLY_SSH_IDENTIFICATION)limit=15000u;
    if(s->idle_ms>=limit||ms>limit-s->idle_ms)s->idle_ms=limit;else s->idle_ms+=ms;
    if(s->idle_ms==limit){(void)send(s,0x14,s->next,0);reset(s);return 0;}
    if(s->flight){
        if(ms>8000u-s->retry_ms)s->retry_ms=8000;else s->retry_ms+=ms;
        unsigned delay=1000u<<(s->retries>3?3:s->retries);
        if(s->retry_ms>=delay){
            if(s->retries==6&&s->peer_window){(void)send(s,0x14,s->next,0);reset(s);return 0;}
            unsigned flags=s->phase==HOLLY_STREAM_SYN?0x12:s->fin_sent?0x11:0x18;
            if(send(s,flags,s->una,(flags==0x18)?s->flight:0))return -1;
            s->retry_ms=0;if(s->retries<6)s->retries++;
        }
        return 0;
    }
    /* Persist probe: one byte is retained as a real outstanding segment. A
     * zero-window receiver returns the same ACK; when its window reopens the
     * byte is acknowledged and normal sending resumes. Never send the rest
     * of the queue while its advertised window is zero. */
    if(s->phase==HOLLY_STREAM_OPEN&&s->queued&&!s->peer_window){
        if(ms>1000u-s->retry_ms)s->retry_ms=1000;else s->retry_ms+=ms;
        if(s->retry_ms==1000){
            if(send(s,0x18,s->next,1))return -1;
            s->flight=1;s->next++;s->retry_ms=0;s->retries=0;
        }
        return 0;
    }
    return pump(s);
}
