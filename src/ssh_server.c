#include "ssh_server.h"
#include "modexp.h"
#include "ssh_ident.h"
static void cp(void *a,const void *b,size_t n){uint8_t *d=a;const uint8_t *s=b;for(size_t i=0;i<n;i++)d[i]=s[i];}
static size_t sl(const char *p){size_t n=0;while(p[n])n++;return n;}
static uint32_t u32(const uint8_t *p){return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
static void p32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(24-8*i));}
struct reader {const uint8_t *p;size_t n;int failed;};
struct writer {uint8_t *p;size_t n,cap;int failed;};
static uint8_t byte(struct reader *r){if(!r->n){r->failed=1;return 0;}r->n--;return *r->p++;}
static uint32_t word(struct reader *r){if(r->n<4){r->failed=1;return 0;}uint32_t v=u32(r->p);r->p+=4;r->n-=4;return v;}
static struct holly_ssh_names string(struct reader *r){
    struct holly_ssh_names v={0,0};uint32_t n=word(r);
    if(r->failed||n>r->n){r->failed=1;return v;}v.data=r->p;v.bytes=n;r->p+=n;r->n-=n;return v;
}
static int eq(struct holly_ssh_names n,const char *p){size_t b=sl(p);return n.bytes==b&&holly_tag_equal(n.data,(const uint8_t *)p,b);}
static void raw(struct writer *w,const void *p,size_t n){if(w->failed||n>w->cap-w->n){w->failed=1;return;}cp(w->p+w->n,p,n);w->n+=n;}
static void wb(struct writer *w,uint8_t b){raw(w,&b,1);}
static void ww(struct writer *w,uint32_t n){uint8_t b[4];p32(b,n);raw(w,b,4);}
static void ws(struct writer *w,const void *p,size_t n){ww(w,(uint32_t)n);raw(w,p,n);}
static void text(struct writer *w,const char *p){ws(w,p,sl(p));}
static void mpint(struct writer *w,const uint8_t a[256]){
    unsigned start=0;while(start<256&&!a[start])start++;
    unsigned pad=start<256&&(a[start]&128u);ww(w,256-start+pad);if(pad)wb(w,0);raw(w,a+start,256-start);
}
static int dead(struct holly_ssh_server *s){s->stage=HOLLY_SSH_DEAD;return -1;}
static int packet(struct holly_ssh_server *s,const uint8_t *data,size_t n){
    if(s->stage==HOLLY_SSH_DEAD||!n||n>HOLLY_SSH_PAYLOAD_MAX)return dead(s);
    unsigned block=s->encrypted_out?16:8;
    unsigned pad=block-(unsigned)((n+5)%block);if(pad<4)pad+=block;
    size_t total=n+5+pad;if(total>HOLLY_SSH_PACKET_MAX||s->tx_bytes>16777216u-total)return dead(s);
    uint8_t *p=s->outgoing;p32(p,(uint32_t)total-4);p[4]=(uint8_t)pad;cp(p+5,data,n);
    if(s->random(p+5+n,pad,s->context))return dead(s);
    if(s->encrypted_out){
        p32(s->mac_input,s->tx_sequence);cp(s->mac_input+4,p,total);
        if(holly_hmac_sha256(s->tx_mac,32,s->mac_input,total+4,p+total)||holly_aes_ctr_xor(&s->tx_cipher,p,total))return dead(s);
    }
    s->tx_sequence++;s->tx_bytes+=(uint32_t)total;
    if(s->write(p,total+(s->encrypted_out?32:0),s->context))return dead(s);
    return 0;
}
static int send_writer(struct holly_ssh_server *s,struct writer *w){return w->failed?dead(s):packet(s,w->p,w->n);}
static int single(struct holly_ssh_server *s,uint8_t type){return packet(s,&type,1);}
static int disconnect(struct holly_ssh_server *s,const char *why){
    struct writer w={s->scratch,0,sizeof(s->scratch),0};wb(&w,1);ww(&w,2);text(&w,why);text(&w,"");
    (void)send_writer(s,&w);return dead(s);
}
static void hash_string(struct holly_sha256 *h,const void *p,size_t n){uint8_t b[4];p32(b,(uint32_t)n);(void)holly_sha256_update(h,b,4);(void)holly_sha256_update(h,p,n);}
static int proposal(struct holly_ssh_server *s){
    static const char *names[10]={"diffie-hellman-group14-sha256","rsa-sha2-256","aes128-ctr","aes128-ctr",
        "hmac-sha2-256","hmac-sha2-256","none","none","",""};
    struct holly_ssh_kexinit k={0};if(s->random(k.cookie,16,s->context))return dead(s);
    for(unsigned i=0;i<10;i++){k.lists[i].data=(const uint8_t *)names[i];k.lists[i].bytes=sl(names[i]);}
    if(holly_ssh_kex_encode(&k,s->server_kex,sizeof(s->server_kex),&s->server_kex_bytes))return dead(s);
    return packet(s,s->server_kex,s->server_kex_bytes);
}
static int agree(struct holly_ssh_server *s,const uint8_t *p,size_t n){
    struct holly_ssh_kexinit client,server;
    if(n>sizeof(s->client_kex)||holly_ssh_kex_parse(p,n,&client)||
       holly_ssh_kex_parse(s->server_kex,s->server_kex_bytes,&server))return disconnect(s,"Invalid KEXINIT");
    for(unsigned i=0;i<8;i++){
        struct holly_ssh_names chosen;
        if(holly_ssh_name_select(client.lists[i],server.lists[i],&chosen)!=1)return disconnect(s,"Unsupported SSH algorithm");
        if(client.first_packet_follows&&i<2){
            size_t first=0;while(first<client.lists[i].bytes&&client.lists[i].data[first]!=',')first++;
            if(first!=chosen.bytes||!holly_tag_equal(client.lists[i].data,chosen.data,first))s->discard_guess=1;
        }
    }
    cp(s->client_kex,p,n);s->client_kex_bytes=n;s->stage=HOLLY_SSH_DH;return 0;
}
static int read_mpint(struct holly_ssh_names v,uint8_t out[256]){
    if(!v.bytes||v.bytes>257||(v.data[0]&128u))return -1;
    if(v.data[0]==0){if(v.bytes==1||!(v.data[1]&128u))return -1;v.data++;v.bytes--;}
    if(v.bytes>256)return -1;
    for(unsigned i=0;i<256;i++)out[i]=0;
    cp(out+256-v.bytes,v.data,v.bytes);return 0;
}
static int dh_value_ok(const uint8_t e[256]){
    unsigned nonzero=0;for(unsigned i=0;i<255;i++)nonzero|=e[i];
    if(!nonzero&&e[255]<2)return 0;
    /* Compare with p-1, whose final byte is FE. */
    for(unsigned i=0;i<256;i++){unsigned limit=holly_dh14_prime[i]-(i==255);
        if(e[i]<limit)return 1;
        if(e[i]>limit)return 0;}
    return 0;
}
static int key_exchange(struct holly_ssh_server *s,struct reader *r){
    uint8_t e[256],f[256],k[256],x[32],g[256]={0},encoded_k[261],host[300],signature[256],em[256],check[256];
    struct holly_ssh_names incoming=string(r);
    if(r->failed||r->n||read_mpint(incoming,e)||!dh_value_ok(e))return disconnect(s,"Invalid DH public value");
    uint8_t exchange_hash[32],derived[6][32];
    int result=-1;
    if(s->random(x,sizeof(x),s->context))goto cleanup;
    x[0]|=128;g[255]=2;
    if(holly_modexp2048(g,x,32,holly_dh14_prime,f)||holly_modexp2048(e,x,32,holly_dh14_prime,k)||!dh_value_ok(k))goto cleanup;
    struct writer host_w={host,0,sizeof(host),0};text(&host_w,"ssh-rsa");const uint8_t exponent[]={1,0,1};ws(&host_w,exponent,3);mpint(&host_w,s->credentials->rsa_n);
    struct writer k_w={encoded_k,0,sizeof(encoded_k),0};mpint(&k_w,k);
    if(host_w.failed||k_w.failed)goto cleanup;
    struct holly_sha256 hash;holly_sha256_init(&hash);
    hash_string(&hash,s->client_ident,s->ident_bytes);
    hash_string(&hash,holly_ssh_server_ident,holly_ssh_server_ident_length-2);
    hash_string(&hash,s->client_kex,s->client_kex_bytes);hash_string(&hash,s->server_kex,s->server_kex_bytes);
    hash_string(&hash,host,host_w.n);
    struct writer temp={s->scratch,0,sizeof(s->scratch),0};mpint(&temp,e);mpint(&temp,f);
    (void)holly_sha256_update(&hash,temp.p,temp.n);(void)holly_sha256_update(&hash,encoded_k,k_w.n);
    if(holly_sha256_finish(&hash,exchange_hash))goto cleanup;
    if(!s->have_session_id){cp(s->session_id,exchange_hash,32);s->have_session_id=1;}
    static const uint8_t prefix[]={0x30,0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01,0x05,0x00,0x04,0x20};
    for(unsigned i=0;i<256;i++)em[i]=0xff;
    em[0]=0;em[1]=1;em[204]=0;cp(em+205,prefix,sizeof(prefix));
    if(holly_sha256_hash(exchange_hash,32,em+224)||holly_modexp2048(em,s->credentials->rsa_d,256,s->credentials->rsa_n,signature)||
       holly_modexp2048(signature,exponent,3,s->credentials->rsa_n,check)||!holly_tag_equal(em,check,256))goto cleanup;
    for(unsigned i=0;i<6;i++){
        uint8_t label=(uint8_t)('A'+i);holly_sha256_init(&hash);
        (void)holly_sha256_update(&hash,encoded_k,k_w.n);(void)holly_sha256_update(&hash,exchange_hash,32);
        (void)holly_sha256_update(&hash,&label,1);(void)holly_sha256_update(&hash,s->session_id,32);
        if(holly_sha256_finish(&hash,derived[i]))goto cleanup;
    }
    holly_aes_ctr_init(&s->pending_rx_cipher,derived[2],derived[0]);
    cp(s->pending_rx_mac,derived[4],32);
    uint8_t sig_blob[280];struct writer sig_w={sig_blob,0,sizeof(sig_blob),0};text(&sig_w,"rsa-sha2-256");ws(&sig_w,signature,256);
    struct writer w={s->scratch,0,sizeof(s->scratch),0};wb(&w,31);ws(&w,host,host_w.n);mpint(&w,f);ws(&w,sig_blob,sig_w.n);
    if(send_writer(s,&w)||single(s,21))goto cleanup;
    /* Our NEWKEYS was sent using the old outbound epoch. */
    holly_aes_ctr_init(&s->tx_cipher,derived[3],derived[1]);cp(s->tx_mac,derived[5],32);
    s->tx_bytes=0;s->encrypted_out=1;s->stage=HOLLY_SSH_NEWKEYS;result=0;
cleanup:
    holly_secret_wipe(derived,sizeof(derived));holly_secret_wipe(exchange_hash,sizeof(exchange_hash));
    holly_secret_wipe(x,sizeof(x));holly_secret_wipe(k,sizeof(k));holly_secret_wipe(encoded_k,sizeof(encoded_k));
    holly_secret_wipe(em,sizeof(em));holly_secret_wipe(signature,sizeof(signature));holly_secret_wipe(check,sizeof(check));
    return result?dead(s):0;
}
static int auth_failure(struct holly_ssh_server *s){
    if(++s->auth_failures>=5)return disconnect(s,"Authentication attempts exhausted");
    struct writer w={s->scratch,0,sizeof(s->scratch),0};wb(&w,51);text(&w,"password");wb(&w,0);return send_writer(s,&w);
}
static int authenticate(struct holly_ssh_server *s,struct reader *r){
    struct holly_ssh_names user=string(r),service=string(r),method=string(r);
    if(r->failed)return dead(s);
    if(eq(method,"none")){if(r->n)return dead(s);return auth_failure(s);}
    if(!eq(method,"password"))return auth_failure(s);
    unsigned change=byte(r);struct holly_ssh_names password=string(r);
    if(r->failed||r->n||password.bytes>256||change)return auth_failure(s);
    uint8_t digest[32];struct holly_sha256 h;holly_sha256_init(&h);
    (void)holly_sha256_update(&h,s->credentials->salt,32);(void)holly_sha256_update(&h,password.data,password.bytes);
    (void)holly_sha256_finish(&h,digest);
    int matches=holly_tag_equal(digest,s->credentials->password_hash,32)&eq(user,s->credentials->username)&eq(service,"ssh-connection");
    holly_secret_wipe(digest,sizeof(digest));
    holly_secret_wipe(&h,sizeof(h));
    if(!matches)return auth_failure(s);
    s->stage=HOLLY_SSH_CHANNEL;return single(s,52);
}
static int channel_message(struct holly_ssh_server *s,uint8_t type){
    struct writer w={s->scratch,0,sizeof(s->scratch),0};wb(&w,type);ww(&w,s->peer_channel);return send_writer(s,&w);
}
static int flush_chat(struct holly_ssh_server *s){
    while(s->chat_sent<s->chat_used&&s->peer_window){
        unsigned n=s->chat_used-s->chat_sent;if(n>1024)n=1024;if(n>s->peer_max)n=s->peer_max;if(n>s->peer_window)n=s->peer_window;
        struct writer w={s->scratch,0,sizeof(s->scratch),0};wb(&w,94);ww(&w,s->peer_channel);ws(&w,s->chat_output+s->chat_sent,n);
        if(send_writer(s,&w))return -1;
        s->chat_sent+=n;s->peer_window-=n;
    }
    if(s->chat_sent==s->chat_used){
        s->chat_used=0;s->chat_sent=0;
        if(s->close_pending&&!s->channel_closed){
            struct writer w={s->scratch,0,sizeof(s->scratch),0};wb(&w,98);ww(&w,s->peer_channel);text(&w,"exit-status");wb(&w,0);ww(&w,0);
            if(send_writer(s,&w)||channel_message(s,96)||channel_message(s,97))return -1;
            s->channel_closed=1;
        }
    }
    return 0;
}
static void emit(const char *p,void *context){
    struct holly_ssh_server *s=context;
    while(*p){
        unsigned need=(*p=='\n'&&s->pty)?2:1;
        if(need>sizeof(s->chat_output)-s->chat_used){s->chat_failed=1;return;}
        if(need==2)s->chat_output[s->chat_used++]='\r';
        s->chat_output[s->chat_used++]=(uint8_t)*p++;
    }
}
static int channel_open(struct holly_ssh_server *s,struct reader *r){
    struct holly_ssh_names type=string(r);uint32_t id=word(r),window=word(r),max=word(r);
    if(r->failed)return dead(s);
    if(!eq(type,"session")||r->n||s->channel_open||max<1024){
        struct writer w={s->scratch,0,sizeof(s->scratch),0};wb(&w,92);ww(&w,id);ww(&w,1);text(&w,"Only one Holly session is supported");text(&w,"");return send_writer(s,&w);
    }
    s->peer_channel=id;s->peer_window=window;s->peer_max=max;s->channel_open=1;s->stage=HOLLY_SSH_RUNNING;
    struct writer w={s->scratch,0,sizeof(s->scratch),0};wb(&w,91);ww(&w,id);ww(&w,0);ww(&w,32768);ww(&w,HOLLY_SSH_PAYLOAD_MAX-9u);return send_writer(s,&w);
}
static int request(struct holly_ssh_server *s,struct reader *r){
    uint32_t channel=word(r);struct holly_ssh_names kind=string(r);unsigned want=byte(r),accepted=0,execute=0;
    struct holly_ssh_names command={0,0};
    if(r->failed||channel!=0||!s->channel_open||s->channel_closed)return dead(s);
    if(eq(kind,"pty-req")&&!s->started){
        (void)string(r);for(unsigned i=0;i<4;i++)(void)word(r);(void)string(r);
        if(!r->failed&&!r->n){s->pty=1;accepted=1;}
    }else if(eq(kind,"window-change")&&s->pty){
        for(unsigned i=0;i<4;i++)(void)word(r);
        accepted=!r->failed&&!r->n;
    }else if(eq(kind,"shell")&&!s->started&&!r->n){accepted=1;s->started=1;execute=1;}
    else if(eq(kind,"exec")&&!s->started){
        command=string(r);
        if(!r->failed&&!r->n&&command.bytes<sizeof(s->line)){
            accepted=1;
            for(size_t i=0;i<command.bytes;i++)if(command.data[i]<32||command.data[i]>126)accepted=0;
            if(accepted){s->started=1;execute=2;}
        }
    }
    if(r->failed)return dead(s);
    if(want&&channel_message(s,accepted?99:100))return -1;
    if(execute==1)emit("Holly AI Learning OS: encrypted chat. Type help or exit.\nholly> ",s);
    else if(execute==2){cp(s->line,command.data,command.bytes);s->line[command.bytes]=0;
        (void)holly_turn(&s->chat,s->line,emit,s);s->close_pending=1;}
    return s->chat_failed?disconnect(s,"Holly output queue full"):flush_chat(s);
}
static int input_chat(struct holly_ssh_server *s,struct reader *r){
    uint32_t channel=word(r);struct holly_ssh_names data=string(r);
    if(r->failed||r->n||channel!=0||!s->started||s->close_pending||data.bytes>HOLLY_SSH_PAYLOAD_MAX-9u)return dead(s);
    for(size_t i=0;i<data.bytes;i++){
        uint8_t c=data.data[i];
        if(s->discard_lf){s->discard_lf=0;if(c=='\n')continue;}
        if(c=='\r'||c=='\n'){
            s->discard_lf=c=='\r';s->line[s->line_bytes]=0;if(s->pty)emit("\n",s);
            if(s->line_overflow)emit("Holly: That line is too long. Limit: 8255 transport characters (chat: 319).\n",s);
            else if(eq((struct holly_ssh_names){(const uint8_t *)s->line,s->line_bytes},"exit")){emit("Holly: See you later.\n",s);s->close_pending=1;}
            else (void)holly_turn(&s->chat,s->line,emit,s);
            s->line_bytes=0;s->line_overflow=0;if(!s->close_pending)emit("holly> ",s);
        }else if(c==8||c==127){if(s->line_bytes){s->line_bytes--;if(s->pty)emit("\b \b",s);}}
        else if(c==3){s->line_bytes=0;s->line_overflow=0;emit("^C\nholly> ",s);}
        else if(c>=32&&c<=126){
            if(s->line_bytes+1<sizeof(s->line)){s->line[s->line_bytes++]=(char)c;
                if(s->pty){char b[2]={(char)c,0};emit(b,s);}}
            else s->line_overflow=1;
        }
        if(s->chat_failed)return disconnect(s,"Holly output queue full");
        if(s->close_pending)break;
    }
    struct writer w={s->scratch,0,sizeof(s->scratch),0};wb(&w,93);ww(&w,s->peer_channel);ww(&w,(uint32_t)data.bytes);
    if(send_writer(s,&w))return -1;
    return flush_chat(s);
}
static int dispatch(struct holly_ssh_server *s,const uint8_t *p,size_t n){
    if(!n)return dead(s);
    struct reader r={p+1,n-1,0};unsigned type=p[0];
    if(s->discard_guess){s->discard_guess=0;return 0;}
    if(type==1)return dead(s);
    if(type==2){(void)string(&r);return (r.failed||r.n)?dead(s):0;}
    if(type==4){(void)byte(&r);(void)string(&r);(void)string(&r);return (r.failed||r.n)?dead(s):0;}
    if(type==20&&s->encrypted_in&&s->stage>=HOLLY_SSH_SERVICE&&s->stage<=HOLLY_SSH_RUNNING){
        s->resume_stage=s->stage;s->stage=HOLLY_SSH_KEX;
        if(proposal(s))return -1;
        return agree(s,p,n);
    }
    if(s->stage==HOLLY_SSH_KEX&&type==20)return agree(s,p,n);
    if(s->stage==HOLLY_SSH_DH&&type==30)return key_exchange(s,&r);
    if(s->stage==HOLLY_SSH_NEWKEYS&&type==21&&!r.n){
        /* This packet has already been verified/decrypted with the old key. */
        s->rx_cipher=s->pending_rx_cipher;cp(s->rx_mac,s->pending_rx_mac,32);
        holly_secret_wipe(&s->pending_rx_cipher,sizeof(s->pending_rx_cipher));
        holly_secret_wipe(s->pending_rx_mac,sizeof(s->pending_rx_mac));
        s->rx_bytes=0;s->encrypted_in=1;s->key_exchanges++;s->stage=s->resume_stage;
        return s->stage==HOLLY_SSH_RUNNING?flush_chat(s):0;
    }
    if(s->stage==HOLLY_SSH_SERVICE&&type==5){
        struct holly_ssh_names service=string(&r);if(r.failed||r.n||!eq(service,"ssh-userauth"))return dead(s);
        struct writer w={s->scratch,0,sizeof(s->scratch),0};wb(&w,6);text(&w,"ssh-userauth");s->stage=HOLLY_SSH_AUTH;return send_writer(s,&w);
    }
    if(s->stage==HOLLY_SSH_AUTH&&type==50)return authenticate(s,&r);
    if((s->stage==HOLLY_SSH_CHANNEL||s->stage==HOLLY_SSH_RUNNING)&&type==90)return channel_open(s,&r);
    if(s->stage==HOLLY_SSH_RUNNING){
        if(type==98)return request(s,&r);
        if(type==94)return input_chat(s,&r);
        if(type==93){uint32_t ch=word(&r),add=word(&r);if(r.failed||r.n||ch!=0||add>UINT32_MAX-s->peer_window)return dead(s);
            s->peer_window+=add;return flush_chat(s);}
        if(type==96){uint32_t ch=word(&r);if(r.failed||r.n||ch!=0)return dead(s);s->close_pending=1;return flush_chat(s);}
        if(type==97){uint32_t ch=word(&r);if(r.failed||r.n||ch!=0)return dead(s);if(!s->channel_closed)(void)channel_message(s,97);return dead(s);}
        if(type==80){(void)string(&r);unsigned want=byte(&r);if(r.failed)return dead(s);return want?single(s,82):0;}
    }
    return disconnect(s,"Unexpected SSH message");
}
int holly_ssh_server_start(struct holly_ssh_server *s,const struct holly_ssh_credentials *c,
    holly_ssh_write_fn write,holly_ssh_random_fn random,void *context){
    if(!s||!c||!write||!random)return -1;
    holly_secret_wipe(s,sizeof(*s));s->credentials=c;s->write=write;s->random=random;s->context=context;
    if(!(c->rsa_n[0]&128u)||!(c->rsa_n[255]&1u)||!c->username[0]||c->username[32])return dead(s);
    s->stage=HOLLY_SSH_IDENTIFICATION;s->resume_stage=HOLLY_SSH_SERVICE;holly_session_init(&s->chat);
    return write(holly_ssh_server_ident,holly_ssh_server_ident_length,context)?dead(s):0;
}
void holly_ssh_server_destroy(struct holly_ssh_server *s){if(s){holly_secret_wipe(s,sizeof(*s));s->stage=HOLLY_SSH_DEAD;}}
int holly_ssh_server_feed(struct holly_ssh_server *s,const uint8_t *p,size_t n){
    if(!s||(!p&&n)||s->stage==HOLLY_SSH_DEAD)return -1;
    while(n){
        if(s->stage==HOLLY_SSH_IDENTIFICATION){
            uint8_t b=*p++;n--;
            if(s->ident_bytes>=255)return dead(s);
            if(b=='\n'){
                if(s->ident_bytes<10||s->client_ident[s->ident_bytes-1]!='\r')return dead(s);
                s->client_ident[--s->ident_bytes]=0;
                static const uint8_t prefix[]="SSH-2.0-";
                if(!holly_tag_equal((const uint8_t *)s->client_ident,prefix,8))return dead(s);
                s->stage=HOLLY_SSH_KEX;if(proposal(s))return -1;
            }else{
                if((b<32&&b!='\r')||b>126||(s->ident_bytes&&s->client_ident[s->ident_bytes-1]=='\r'))return dead(s);
                s->client_ident[s->ident_bytes++]=(char)b;
            }
            continue;
        }
        unsigned block=s->encrypted_in?16:8;
        unsigned target=s->header_ready?s->total+(s->encrypted_in?32:0):block;
        unsigned take=target-s->used;if(take>n)take=(unsigned)n;
        cp(s->incoming+s->used,p,take);s->used+=take;p+=take;n-=take;
        if(!s->header_ready&&s->used==block){
            if(s->encrypted_in&&holly_aes_ctr_xor(&s->rx_cipher,s->incoming,block))return dead(s);
            uint32_t len=u32(s->incoming);
            if(len<12||len>HOLLY_SSH_PACKET_MAX-4||((len+4)%block))return dead(s);
            s->total=len+4;s->header_ready=1;
        }
        if(s->header_ready&&s->used==s->total+(s->encrypted_in?32:0)){
            if(s->rx_bytes>16777216u-s->total)return disconnect(s,"Receive key byte limit reached");
            if(s->encrypted_in){
                if(holly_aes_ctr_xor(&s->rx_cipher,s->incoming+block,s->total-block))return dead(s);
                uint8_t tag[32];p32(s->mac_input,s->rx_sequence);cp(s->mac_input+4,s->incoming,s->total);
                if(holly_hmac_sha256(s->rx_mac,32,s->mac_input,s->total+4,tag)||!holly_tag_equal(tag,s->incoming+s->total,32))return dead(s);
            }
            unsigned pad=s->incoming[4];
            if(pad<4||pad>=s->total-5)return dead(s);
            unsigned payload=s->total-5-pad;if(payload>HOLLY_SSH_PAYLOAD_MAX)return dead(s);
            s->rx_sequence++;s->rx_bytes+=s->total;
            int result=dispatch(s,s->incoming+5,payload);
            holly_secret_wipe(s->incoming,s->used);s->used=0;s->total=0;s->header_ready=0;
            if(result)return -1;
        }
    }
    return 0;
}
