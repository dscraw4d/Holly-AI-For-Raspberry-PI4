/* Test-only UART byte transport and host entropy; absent from production. */
#include "https.h"
#define REG(b,o) (*(volatile uint32_t *)(uintptr_t)((b)+(o)))
#define UART 0xfe201000ul
#define GPIO 0xfe200000ul
static struct holly_https https;
static uint8_t get(void){while(REG(UART,0x18)&16u){}return (uint8_t)REG(UART,0);}
static void put(uint8_t b){while(REG(UART,0x18)&32u){}REG(UART,0)=b;}
static uint32_t length(void){uint32_t n=0;for(unsigned i=0;i<4;i++)n=(n<<8)|get();return n;}
static void header(uint8_t kind,uint32_t n){put(kind);for(unsigned i=0;i<4;i++)put((uint8_t)(n>>(24-i*8)));}
static int random_bytes(uint8_t *p,size_t n,void *context){(void)context;header('R',(uint32_t)n);for(size_t i=0;i<n;i++)p[i]=get();return 0;}
static int write_bytes(uint8_t *p,unsigned n,void *context){(void)context;if(n>4096)n=4096;header('O',n);for(unsigned i=0;i<n;i++)put(p[i]);return (int)n;}
static int read_bytes(uint8_t *p,unsigned n,void *context){(void)context;if(n>4096)n=4096;header('Q',n);unsigned got=length();if(got==UINT32_MAX)return -1;if(got>n)return -1;for(unsigned i=0;i<got;i++)p[i]=get();return (int)got;}
void kernel_main(void){
 REG(UART,0x30)=0;uint32_t select=REG(GPIO,4);select&=~((7u<<12)|(7u<<15));select|=(4u<<12)|(4u<<15);REG(GPIO,4)=select;
 REG(UART,0x44)=0x7ff;REG(UART,0x24)=26;REG(UART,0x28)=3;REG(UART,0x2c)=0x70;REG(UART,0x30)=0x301;
 static const char marker[]="HLHTTPS\n";for(unsigned i=0;i<sizeof(marker)-1;i++)put((uint8_t)marker[i]);
 uint64_t epoch=(uint64_t)length()<<32;epoch|=length();
 int result=holly_https_start(&https,"en.wikipedia.org","GET /test HTTP/1.1\r\nHost: en.wikipedia.org\r\n\r\n",epoch,random_bytes,write_bytes,read_bytes,0);
 if(!result)do{result=holly_https_poll(&https);}while(!result);
 if(result==1){header('B',https.http.body_size);for(unsigned i=0;i<https.http.body_size;i++)put((uint8_t)https.http.body[i]);}
 header('E',https.error);header('S',result==1?1:2);holly_https_destroy(&https);
 for(;;)__asm__ volatile("wfe");
}
