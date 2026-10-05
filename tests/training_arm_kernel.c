/* Test-only UART result, production integer training code. */
#include "../src/selftrain.c"
#define REG(b,o) (*(volatile uint32_t *)(uintptr_t)((b)+(o)))
#define UART 0xfe201000ul
static void out(char c){while(REG(UART,0x18)&32u){}REG(UART,0)=c;}
void kernel_main(void){
 REG(UART,0x30)=0;REG(UART,0x44)=0x7ff;REG(UART,0x24)=26;REG(UART,0x28)=3;REG(UART,0x2c)=0x70;REG(UART,0x30)=0x301;
 holly_training_init();start_round(100);for(unsigned i=0;i<25;i++)train_token();
 uint32_t crc=model_crc(state.candidate,MODEL_BYTES);
 const char *s="CANDIDATE25 ";while(*s)out(*s++);
 for(int i=7;i>=0;i--)out("0123456789abcdef"[(crc>>(i*4))&15]);out('\n');
 for(;;)__asm__ volatile("wfe");
}
