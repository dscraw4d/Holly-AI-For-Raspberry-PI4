/* Behavioral SDHCI fixture: validates sequencing and failures, not real hardware. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sdcard.h"
static uint32_t regs[64];
static unsigned mode,commands,data_words,last_divisor,first_divisor;
static void set_csd(void){
    uint32_t n[4]={0x40000000,244140u>>16,244140u<<16,0};
    for(unsigned i=0;i<4;i++)regs[(0x1c-i*4)/4]=(n[i]>>8)|(i?n[i-1]<<24:0);
}
uint32_t holly_sd_test_read(unsigned offset,unsigned width){
    if(offset==0x20){data_words++;return 0x12345678;}
    uint32_t v=regs[offset/4];
    if(offset==0x2c){
        if(mode!=2)v&=~(1u<<24);
        if(mode!=3 && (v&1))v|=2;
    }
    return width==2?(v>>((offset&2)*8))&0xffffu:v;
}
void holly_sd_test_write(unsigned offset,uint32_t value,unsigned width){
    if(offset==0x20){data_words++;return;}
    unsigned shift=(offset&2)*8;
    if(offset==0x30 || offset==0x32){regs[0x30/4]&=~(value<<shift);return;}
    if(width==2)regs[offset/4]=(regs[offset/4]&~(0xffffu<<shift))|(value<<shift);
    else regs[offset/4]=value;
    if(offset==0x2c && (value&1)){
        last_divisor=((value>>8)&255)|((value&0xc0)<<2);
        if(!first_divisor)first_divisor=last_divisor;
    }
    if(offset!=0x0e)return;
    unsigned cmd=value>>8;commands++;
    assert((regs[0x28/4]&0xff00u)==0x0f00u);
    assert((regs[0x2c/4]&5)==5);
    assert(!(regs[0x28/4]&2)); /* conservative one-bit mode */
    assert(cmd!=6 && cmd!=16); /* SDHC does not need either */
    regs[0x30/4]=0x33;
    regs[0x10/4]=0;
    if(cmd==8)regs[0x10/4]=mode==5?0:0x1aa;
    if(cmd==41)regs[0x10/4]=0xc0ff8000;
    if(cmd==3)regs[0x10/4]=0x12340000;
    if(cmd==9)set_csd();
    if(cmd==17 && mode==6)regs[0x10/4]=1u<<31;
    if(cmd==17 && mode==7)regs[0x30/4]=1u<<16;
}
static struct holly_sdcard setup(unsigned failure){
    memset(regs,0,sizeof(regs));mode=failure;commands=data_words=last_divisor=first_divisor=0;
    regs[0x40/4]=failure==1?0:0x01000000;
    regs[0xfc/4]=2u<<16;
    regs[0x24/4]=failure==4?0:1u<<16;
    return holly_pi4_sdcard();
}
int main(void){
    struct holly_sdcard c=setup(0);uint8_t data[512]={0};
    assert(holly_sdcard_start(&c)==0 && c.ready && c.stage==16);
    assert(c.sectors==250000384u && first_divisor==512 && last_divisor==16);
    assert(holly_sdcard_read_sector(&c,0,data)==0 && data_words==128 && data[0]==0x78);
    assert(holly_sdcard_write_sector(&c,1,data)==0 && data_words==256);
    unsigned before=commands;
    assert(holly_sdcard_write_sector(&c,c.sectors,data)==-8 && commands==before);
    const unsigned stage[]={0,1,2,4,5,7};
    for(unsigned i=1;i<=5;i++){
        c=setup(i);assert(holly_sdcard_start(&c)<0 && !c.ready && c.stage==stage[i]);
        if(i==3)assert(!(regs[0x2c/4]&4)); /* no card clock without stable source */
    }
    c=setup(0);assert(!holly_sdcard_start(&c));mode=6;
    assert(holly_sdcard_read_sector(&c,0,data)==-7 && !data_words && c.response==(1u<<31));
    c=setup(0);assert(!holly_sdcard_start(&c));mode=7;
    assert(holly_sdcard_read_sector(&c,0,data)==-2 && !data_words && c.interrupt==(1u<<16));
    puts("SD startup sequencing, voltage, clocks, error snapshots, R1 rejection and LBA bounds passed");
}
