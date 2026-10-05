#define _GNU_SOURCE
#include "tcp_stream.h"
#include <errno.h>
#include <sys/random.h>
#include <string.h>
static struct holly_tcp_stream stream;
static struct holly_ssh_credentials credentials;
static uint8_t frames[64][1514];static unsigned lengths[64],read_index,write_index;
static int random_bytes(uint8_t *p,size_t n,void *ctx){(void)ctx;
    while(n){ssize_t r=getrandom(p,n,0);if(r<0&&errno==EINTR)continue;if(r<=0)return -1;p+=r;n-=(size_t)r;}return 0;}
static int write_frame(const uint8_t *p,unsigned n,void *ctx){(void)ctx;
    if(write_index-read_index==64||n>1514)return -1;
    memcpy(frames[write_index&63u],p,n);lengths[write_index++&63u]=n;return 0;}
int test_tcp_start(const uint8_t *data,unsigned n){
    if(n!=8+sizeof(credentials)||memcmp(data,"HLYSSH23",8))return -1;
    memcpy(&credentials,data+8,sizeof(credentials));read_index=0;write_index=0;
    uint8_t mac[6]={2,1,2,3,4,5},ip[4]={169,254,77,1};
    holly_tcp_stream_init(&stream,mac,ip,&credentials,random_bytes,write_frame,0);return 0;
}
int test_tcp_feed(const uint8_t *data,unsigned n){return holly_tcp_stream_receive(&stream,data,n);}
int test_tcp_tick(unsigned ms){return holly_tcp_stream_tick(&stream,ms);}
int test_tcp_pop(uint8_t *data){if(read_index==write_index)return 0;
    unsigned n=lengths[read_index&63u];memcpy(data,frames[read_index++&63u],n);return (int)n;}
unsigned test_tcp_phase(void){return stream.phase;}
void test_tcp_link_lost(void){holly_tcp_stream_link_lost(&stream);}
/* Ordering check: app processing may be expensive. Its TCP ACK must already
 * have been transmitted when the application callback begins. */
static unsigned ack_order_seen;
static int order_start(holly_ssh_write_fn write,void *ctx,void *app){(void)write;(void)ctx;(void)app;return 0;}
static int order_feed(const uint8_t *data,size_t size,void *app){
 (void)data;(void)app;
 if(size&&write_index>read_index&&frames[(write_index-1)&63u][47]==0x10u)ack_order_seen=1;
 return 0;
}
void test_ack_order_start(void){
 holly_tcp_stream_link_lost(&stream);read_index=write_index=ack_order_seen=0;
 stream.app_start=order_start;stream.app_feed=order_feed;
}
unsigned test_ack_order_seen(void){return ack_order_seen;}
#ifdef HOLLY_GUEST_TEST
#include "telnet.h"
#include "http_server.h"
static struct holly_telnet guest;
static struct holly_web_server web;
void test_guest_start(unsigned port){
 uint8_t mac[6]={2,1,2,3,4,5},ip[4]={169,254,77,1};
 read_index=write_index=0;holly_tcp_stream_init(&stream,mac,ip,&credentials,random_bytes,write_frame,0);
 stream.local_port=(uint16_t)port;
 if(port==23){stream.app_start=holly_telnet_start;stream.app_feed=holly_telnet_feed;stream.app_close=holly_telnet_close;stream.app_context=&guest;}
 else {memset(&web,0,sizeof web);web.random=random_bytes;stream.app_start=holly_web_start;stream.app_feed=holly_web_feed;stream.app_close=holly_web_close;stream.app_context=&web;}
}
#endif
