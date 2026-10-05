#include "ssh_server.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct holly_ssh_server server;static struct holly_ssh_credentials credentials;
static unsigned writes,fail_random;
static int output(const uint8_t *p,size_t n,void *ctx){(void)p;(void)n;(void)ctx;writes++;return 0;}
static int random_bytes(uint8_t *p,size_t n,void *ctx){(void)ctx;if(fail_random)return -1;for(size_t i=0;i<n;i++)p[i]=(uint8_t)i;return 0;}
static void start(void){writes=0;fail_random=0;assert(!holly_ssh_server_start(&server,&credentials,output,random_bytes,0));}
static void word(uint8_t *p,unsigned v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(24-i*8));}
static unsigned envelope(const uint8_t *p,unsigned n,uint8_t *out,unsigned block){
    unsigned pad=block-(n+5)%block;if(pad<4)pad+=block;word(out,n+1+pad);out[4]=(uint8_t)pad;
    memcpy(out+5,p,n);memset(out+5+n,0xa5,pad);return n+5+pad;
}
static uint8_t key[16],iv[16],mac[32];
static unsigned packet_sequence;
static unsigned encrypted(const uint8_t *payload,unsigned n,uint8_t *out){
    unsigned size=envelope(payload,n,out,16);uint8_t auth[1024];word(auth,packet_sequence);memcpy(auth+4,out,size);
    assert(!holly_hmac_sha256(mac,32,auth,size+4,out+size));
    struct holly_aes_ctr c;holly_aes_ctr_init(&c,key,iv);assert(!holly_aes_ctr_xor(&c,out,size));return size+32;
}
static void service(void){start();server.stage=HOLLY_SSH_SERVICE;server.encrypted_in=1;
    holly_aes_ctr_init(&server.rx_cipher,key,iv);memcpy(server.rx_mac,mac,32);}
int main(void){
    credentials.rsa_n[0]=0x80;credentials.rsa_n[255]=1;memcpy(credentials.username,"holly",6);
    uint8_t bytes[1024];
    start();assert(holly_ssh_server_feed(&server,(const uint8_t *)"SSH-1.0-bad\r\n",13)==-1);
    start();fail_random=1;assert(holly_ssh_server_feed(&server,(const uint8_t *)"SSH-2.0-test\r\n",14)==-1);
    assert(writes==1); /* greeting only; no deterministic cookie fallback */
    start();server.stage=HOLLY_SSH_KEX;word(bytes,35001);assert(holly_ssh_server_feed(&server,bytes,4)==0);
    memset(bytes+4,0,4);assert(holly_ssh_server_feed(&server,bytes+4,4)==-1);
    assert(holly_ssh_server_feed(&server,bytes,8)==-1);
    uint8_t request[]={5,0,0,0,12,'s','s','h','-','u','s','e','r','a','u','t','h'};
    service();unsigned n=encrypted(request,sizeof(request),bytes);
    for(unsigned i=0;i<n;i++)assert(!holly_ssh_server_feed(&server,bytes+i,1));
    assert(server.stage==HOLLY_SSH_AUTH&&writes==2);
    service();n=encrypted(request,sizeof(request),bytes);bytes[n-1]^=1;
    assert(holly_ssh_server_feed(&server,bytes,n)==-1&&writes==1); /* Reject before service dispatch. */
    service();n=envelope(request,sizeof(request),bytes,16);assert(holly_ssh_server_feed(&server,bytes,n)==-1);
    /* NEWKEYS must authenticate under the old epoch, preserve packet sequence,
       install the pending receive key, and erase its staging copy. */
    service();server.stage=HOLLY_SSH_NEWKEYS;server.resume_stage=HOLLY_SSH_RUNNING;
    server.rx_bytes=1000;server.tx_sequence=9;server.have_session_id=1;
    memset(server.session_id,0x71,32);
    uint8_t next_key[16],next_iv[16],next_mac[32];
    memset(next_key,0x21,16);memset(next_iv,0x43,16);memset(next_mac,0x65,32);
    holly_aes_ctr_init(&server.pending_rx_cipher,next_key,next_iv);
    memcpy(server.pending_rx_mac,next_mac,32);
    uint8_t newkeys[]={21};n=encrypted(newkeys,1,bytes);
    assert(!holly_ssh_server_feed(&server,bytes,n));
    assert(server.stage==HOLLY_SSH_RUNNING&&server.rx_sequence==1&&server.tx_sequence==9);
    assert(server.rx_bytes==0&&server.key_exchanges==1&&server.session_id[0]==0x71);
    const uint8_t *pending=(const uint8_t *)&server.pending_rx_cipher;
    for(size_t i=0;i<sizeof(server.pending_rx_cipher);i++)assert(!pending[i]);
    for(unsigned i=0;i<32;i++)assert(!server.pending_rx_mac[i]);
    memcpy(key,next_key,16);memcpy(iv,next_iv,16);memcpy(mac,next_mac,32);packet_sequence=1;
    uint8_t ignore[]={2,0,0,0,0};n=encrypted(ignore,sizeof(ignore),bytes);
    assert(!holly_ssh_server_feed(&server,bytes,n)&&server.rx_sequence==2);
    memset(key,0,16);memset(iv,0,16);memset(mac,0,32);packet_sequence=0;
    start();server.stage=HOLLY_SSH_AUTH;
    /* none authentication is never success; fifth attempt closes. */
    uint8_t none[]={50,0,0,0,5,'h','o','l','l','y',0,0,0,14,'s','s','h','-','c','o','n','n','e','c','t','i','o','n',0,0,0,4,'n','o','n','e'};
    n=envelope(none,sizeof(none),bytes,8);
    for(unsigned i=0;i<4;i++)assert(!holly_ssh_server_feed(&server,bytes,n)&&server.stage==HOLLY_SSH_AUTH);
    assert(holly_ssh_server_feed(&server,bytes,n)==-1&&server.stage==HOLLY_SSH_DEAD);
    holly_ssh_server_destroy(&server);const uint8_t *memory=(const uint8_t *)&server;
    /* stage enum occupies first field; all remaining bytes must be erased. */
    for(size_t i=sizeof(server.stage);i<sizeof(server);i++)assert(!memory[i]);
    puts("SSH security boundaries: MAC tamper, encryption required, auth limit, RNG failure, oversize, NEWKEYS handover and secret teardown passed");
}
