/* Test-only AArch64 kernel for SSH CPU verification. The UART framing and
 * host-provided entropy below are not part of the production Pi image. */
#include "ssh_server.h"
#include "ssh_credentials_generated.h"
#define REG(b,o) (*(volatile uint32_t *)(uintptr_t)((b)+(o)))
#define UART 0xfe201000ul
#define GPIO 0xfe200000ul
static struct holly_ssh_server server;
static uint8_t input[4096];
static uint8_t get(void){while(REG(UART,0x18)&16u){}return (uint8_t)REG(UART,0);}
static void put(uint8_t b){while(REG(UART,0x18)&32u){}REG(UART,0)=b;}
static void header(uint8_t type,uint32_t n){put(type);for(unsigned i=0;i<4;i++)put((uint8_t)(n>>(24-i*8)));}
static uint32_t length(void){uint32_t n=0;for(unsigned i=0;i<4;i++)n=(n<<8)|get();return n;}
static int output(const uint8_t *p,size_t n,void *context){(void)context;header('O',(uint32_t)n);for(size_t i=0;i<n;i++)put(p[i]);return 0;}
static int random_bytes(uint8_t *p,size_t n,void *context){(void)context;header('R',(uint32_t)n);for(size_t i=0;i<n;i++)p[i]=get();return 0;}
void kernel_main(void){
    REG(UART,0x30)=0;uint32_t select=REG(GPIO,4);select&=~((7u<<12)|(7u<<15));select|=(4u<<12)|(4u<<15);REG(GPIO,4)=select;
    REG(UART,0x44)=0x7ff;REG(UART,0x24)=26;REG(UART,0x28)=3;REG(UART,0x2c)=0x70;REG(UART,0x30)=0x301;
    static const char marker[]="HLSSH23\n";for(unsigned i=0;i<sizeof(marker)-1;i++)put((uint8_t)marker[i]);
    for(;;){
        uint8_t kind=get();uint32_t n=length();
        if(kind=='C'&&!n){holly_ssh_server_destroy(&server);
            int result=holly_ssh_server_start(&server,&holly_credentials,output,random_bytes,0);header('S',result?1:0);}
        else if(kind=='I'&&n<=sizeof(input)){
            for(unsigned i=0;i<n;i++)input[i]=get();
            int result=holly_ssh_server_feed(&server,input,n);holly_secret_wipe(input,sizeof(input));header('S',result?1:0);
        }else{header('S',1);for(;;)__asm__ volatile("wfe");}
    }
}
