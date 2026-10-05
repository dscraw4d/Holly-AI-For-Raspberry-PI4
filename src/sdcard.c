#include "sdcard.h"

#define SD_REG32(c,o) (*(volatile uint32_t *)((c)->base+(o)))
#define SD_REG16(c,o) (*(volatile uint16_t *)((c)->base+(o)))

enum {
    SD_ARG=0x08, SD_CMD=0x0e, SD_TRANSFER=0x0c, SD_RESPONSE=0x10,
    SD_DATA=0x20, SD_STATE=0x24, SD_CONTROL=0x28,
    SD_CONTROL1=0x2c, SD_STATUS=0x30, SD_ERROR=0x32, SD_STATUS_MASK=0x34,
    SD_ERROR_MASK=0x36, SD_STATUS_ENABLE=0x38, SD_ERROR_ENABLE=0x3a,
    SD_CAPABILITIES=0x40, SD_VERSION=0xfe
};
enum {
    SD_INT_COMMAND=1u<<0, SD_INT_TRANSFER=1u<<1, SD_INT_READ_READY=1u<<5,
    SD_INT_WRITE_READY=1u<<4
};
enum { SD_ERR_MASK=0xffffu, SD_STATE_CMD_BUSY=1u, SD_STATE_DATA_BUSY=2u,
       SD_STATE_CARD=1u<<16 };

static inline void barrier(void) {
#ifdef HOST_TEST
    __asm__ volatile("":::"memory");
#else
    __asm__ volatile("dmb sy":::"memory");
#endif
}
static void pause(unsigned loops) { volatile unsigned n=loops; while(n--)__asm__ volatile("nop"); }
#ifdef HOLLY_SD_TEST
extern uint32_t holly_sd_test_read(unsigned offset,unsigned width);
extern void holly_sd_test_write(unsigned offset,uint32_t value,unsigned width);
#endif
static uint32_t r32(struct holly_sdcard *c,unsigned offset) {
#ifdef HOLLY_SD_TEST
    (void)c;return holly_sd_test_read(offset,4);
#else
    barrier();uint32_t v=SD_REG32(c,offset);barrier();return v;
#endif
}
static uint16_t r16(struct holly_sdcard *c,unsigned offset) {
#ifdef HOLLY_SD_TEST
    (void)c;return (uint16_t)holly_sd_test_read(offset,2);
#else
    barrier();uint16_t v=SD_REG16(c,offset);barrier();return v;
#endif
}
static void w32(struct holly_sdcard *c,unsigned offset,uint32_t v) {
#ifdef HOLLY_SD_TEST
    (void)c;holly_sd_test_write(offset,v,4);
#else
    barrier();SD_REG32(c,offset)=v;barrier();
#endif
}
static void w16(struct holly_sdcard *c,unsigned offset,uint16_t v) {
#ifdef HOLLY_SD_TEST
    (void)c;holly_sd_test_write(offset,v,2);
#else
    barrier();SD_REG16(c,offset)=v;barrier();
#endif
}
static void snapshot(struct holly_sdcard *c) {
    c->state=r32(c,SD_STATE);c->control1=r32(c,SD_CONTROL1);
    c->interrupt=r32(c,SD_STATUS);
}
static int failure(struct holly_sdcard *c,int error) {
    snapshot(c);c->last_error=error;return error;
}

static int wait_clear(struct holly_sdcard *c,unsigned mask) {
    for(unsigned i=0;i<3000000u;i++)if(!(r32(c,SD_STATE)&mask))return 0;
    return -1;
}
static int wait_status(struct holly_sdcard *c,uint16_t wanted) {
    for(unsigned i=0;i<5000000u;i++) {
        uint16_t error=r16(c,SD_ERROR),status=r16(c,SD_STATUS);
        if(error){snapshot(c);return -1;}
        if(status&wanted)return 0;
    }
    return -2;
}
static int set_clock(struct holly_sdcard *c,unsigned divisor) {
    uint32_t control=r32(c,SD_CONTROL1);
    control&=~(1u|4u);w32(c,SD_CONTROL1,control);
    control&=~((0xffu<<8)|(3u<<6));
    control|=((divisor&0xffu)<<8)|(((divisor>>8)&3u)<<6)|1u|(0xeu<<16);
    w32(c,SD_CONTROL1,control);
    unsigned stable=0;
    for(unsigned i=0;i<1000000u;i++)if(r32(c,SD_CONTROL1)&2u){stable=1;break;}
    if(!stable)return -1;
    control=r32(c,SD_CONTROL1)|4u;w32(c,SD_CONTROL1,control);return 0;
}
static void clear_interrupts(struct holly_sdcard *c) {
    w16(c,SD_STATUS,0xffff);w16(c,SD_ERROR,0xffff);
}
/* response: 0 none, 1 R2, 2 R1/R6/R7, 3 R1b, 4 R3. */
static uint16_t command_word(unsigned index,unsigned response,int data) {
    uint16_t word=(uint16_t)(index<<8);
    if(response==1)word|=(uint16_t)(1u|(1u<<3)); /* R2: long response + CRC */
    else if(response==2)word|=(uint16_t)(2u|(1u<<3)|(1u<<4));
    else if(response==3)word|=(uint16_t)(3u|(1u<<3)|(1u<<4));
    else if(response==4)word|=2u; /* R3: short response, no CRC/index */
    if(data)word|=(uint16_t)(1u<<5);
    return word;
}
static uint32_t csd_bits(const uint32_t csd[4],unsigned start,unsigned size) {
    unsigned word=3u-start/32u,shift=start%32u;
    uint32_t value=csd[word]>>shift;
    if(size+shift>32u&&word)value|=csd[word-1u]<<(32u-shift);
    return size==32u?value:value&((1u<<size)-1u);
}
int holly_sdcard_capacity_sectors(const uint32_t csd[4],uint32_t *sectors) {
    if(!csd||!sectors)return -1;
    uint64_t count;
    uint32_t structure=csd_bits(csd,126,2);
    if(structure==1u) {
        uint32_t c_size=csd_bits(csd,48,22);
        count=((uint64_t)c_size+1u)*1024u;
    } else if(structure==0u) {
        uint32_t read_bl_len=csd_bits(csd,80,4);
        uint32_t c_size=csd_bits(csd,62,12);
        uint32_t c_size_mult=csd_bits(csd,47,3);
        if(read_bl_len>11u)return -2;
        uint64_t blocks=((uint64_t)c_size+1u)<<(c_size_mult+2u);
        count=(blocks<<read_bl_len)/512u;
    } else return -3;
    if(!count)return -4;
    if(count>UINT32_MAX)count=UINT32_MAX;
    *sectors=(uint32_t)count;
    return 0;
}
static void response_136(struct holly_sdcard *c,uint32_t response[4]) {
    uint32_t raw[4];
    for(unsigned i=0;i<4;i++)raw[i]=r32(c,0x1cu-i*4u);
    for(unsigned i=0;i<4;i++) {
        response[i]=raw[i]<<8;
        if(i<3)response[i]|=raw[i+1u]>>24;
    }
}
static int command(struct holly_sdcard *c,unsigned index,uint32_t argument,unsigned response,
                   int data,int write,uint32_t *response0) {
    c->last_command=index;c->response=0;
    (void)write;
    if(data)return -6; /* data commands use the PIO path below */
    if(wait_clear(c,SD_STATE_CMD_BUSY))return -1;
    uint16_t command_code=command_word(index,response,0);
    w16(c,SD_TRANSFER,0);clear_interrupts(c);w32(c,SD_ARG,argument);w16(c,SD_CMD,command_code);
    if(wait_status(c,SD_INT_COMMAND))return -2;
    if(response){c->response=r32(c,SD_RESPONSE);if(response0)*response0=c->response;}
    /* R2/R3/R6/R7 have different layouts; check only R1/R1b commands. */
    if((response==3 || (response==2 && index!=3 && index!=8)) &&
       (c->response&0xfdffe008u))return -7;
    w16(c,SD_STATUS,SD_INT_COMMAND);
    if(response==3&&wait_clear(c,SD_STATE_DATA_BUSY))return -5;
    w16(c,SD_STATUS,0xffff);w16(c,SD_ERROR,0xffff);return 0;
}

/* The data-port operation is kept separate from command() so the compiler
 * never has to represent a device pointer as a normal RAM buffer. */
static int data_command(struct holly_sdcard *c,unsigned index,uint32_t argument,
                        int write,uint8_t *sector) {
    c->last_command=index;c->response=0;
    if(wait_clear(c,SD_STATE_CMD_BUSY|SD_STATE_DATA_BUSY))return -1;
    uint16_t word=command_word(index,2,1);
    w16(c,0x04,512);w16(c,0x06,1);w16(c,SD_TRANSFER,(uint16_t)(2u|(write?0u:1u<<4)));
    clear_interrupts(c);w32(c,SD_ARG,argument);w16(c,SD_CMD,word);
    if(wait_status(c,SD_INT_COMMAND))return -2;
    c->response=r32(c,SD_RESPONSE);
    if(c->response&0xfdffe008u)return -7;
    w16(c,SD_STATUS,SD_INT_COMMAND);
    if(wait_status(c,write?SD_INT_WRITE_READY:SD_INT_READ_READY))return -3;
    if(write) {
        for(unsigned i=0;i<128;i++) {
            uint32_t v=(uint32_t)sector[i*4]|((uint32_t)sector[i*4+1]<<8)|
                       ((uint32_t)sector[i*4+2]<<16)|((uint32_t)sector[i*4+3]<<24);
            w32(c,SD_DATA,v);
        }
    } else {
        for(unsigned i=0;i<128;i++) {
            uint32_t v=r32(c,SD_DATA);sector[i*4]=(uint8_t)v;sector[i*4+1]=(uint8_t)(v>>8);
            sector[i*4+2]=(uint8_t)(v>>16);sector[i*4+3]=(uint8_t)(v>>24);
        }
    }
    if(wait_status(c,SD_INT_TRANSFER))return -4;
    w16(c,SD_STATUS,0xffff);w16(c,SD_ERROR,0xffff);
    return wait_clear(c,SD_STATE_DATA_BUSY);
}

struct holly_sdcard holly_pi4_sdcard(void) {
    struct holly_sdcard c={0};c.base=HOLLY_PI4_EMMC2_BASE;return c;
}
int holly_sdcard_start(struct holly_sdcard *c) {
    if(!c||!c->base)return -1;
    c->ready=0;c->sectors=0;c->stage=1;c->last_command=0;
    c->capabilities=r32(c,SD_CAPABILITIES);c->version=r16(c,SD_VERSION);
    if(!c->capabilities||c->capabilities==0xffffffffu||c->version==0xffffu)
        return failure(c,-2);
    c->stage=2;
    w32(c,SD_CONTROL1,r32(c,SD_CONTROL1)|(1u<<24));
    for(unsigned i=0;i<1000000u&&r32(c,SD_CONTROL1)&(1u<<24);i++){}
    if(r32(c,SD_CONTROL1)&(1u<<24))return failure(c,-3);
    w32(c,SD_CONTROL1,r32(c,SD_CONTROL1)|(0xeu<<16));
    c->stage=3;
    /* SDHCI power byte: 3.3 V selection (111) plus bus power enable. */
    w32(c,SD_CONTROL,(r32(c,SD_CONTROL)&~0xff00u)|0x0f00u);
    pause(500000);
    w16(c,SD_STATUS_MASK,0xffff);w16(c,SD_ERROR_MASK,0xffff);
    /* Polling driver: mask CPU interrupt signalling. */
    w16(c,SD_STATUS_ENABLE,0);w16(c,SD_ERROR_ENABLE,0);
    c->stage=4;
    if(set_clock(c,512))return failure(c,-6);
    pause(500000);
    c->stage=5;
    if(!(r32(c,SD_STATE)&SD_STATE_CARD))return failure(c,-4);
    c->stage=6;
    uint32_t response=0;
    if(command(c,0,0,0,0,0,0))goto fail;
    c->stage=7;
    /* Pi 4 target: SDHC/SDXC. Refuse an invalid CMD8 echo rather than
     * continuing with an uncleared timed-out command engine. */
    if(command(c,8,0x1aau,2,0,0,&response)||(response&0xfffu)!=0x1aau)goto fail;
    c->stage=8;
    unsigned ready=0;
    for(unsigned attempt=0;attempt<100;attempt++) {
        if(command(c,55,0,2,0,0,&response))goto fail;
        if(command(c,41,0x40ff8000u,4,0,0,&response))goto fail;
        if(response&0x80000000u){ready=1;break;} pause(50000);
    }
    if(!ready)goto fail;
    c->high_capacity=(response>>30)&1u;
    c->stage=9;
    if(command(c,2,0,1,0,0,0))goto fail;
    c->stage=10;
    if(command(c,3,0,2,0,0,&response))goto fail;
    c->rca=(uint16_t)(response>>16);if(!c->rca)goto fail;
    c->stage=11;
    if(command(c,9,(uint32_t)c->rca<<16,1,0,0,0))goto fail;
    uint32_t csd[4];response_136(c,csd);
    c->stage=12;
    if(holly_sdcard_capacity_sectors(csd,&c->sectors))goto fail;
    c->stage=13;
    if(command(c,7,(uint32_t)c->rca<<16,3,0,0,0))goto fail;
    /* Keep the default 1-bit bus for hardware bring-up; no ACMD6 switch. */
    w32(c,SD_CONTROL,r32(c,SD_CONTROL)&~2u);
    c->stage=14;
    if(!c->high_capacity && command(c,16,512,2,0,0,&response))goto fail;
    c->stage=15;
    if(set_clock(c,16))return failure(c,-6);
    c->ready=1;c->last_error=0;c->stage=16;snapshot(c);return 0;
fail:
    c->ready=0;return failure(c,-5);
}
int holly_sdcard_read_sector(struct holly_sdcard *c,uint32_t lba,uint8_t *sector) {
    if(!c||!c->ready||!sector)return -1;
    if(lba>=c->sectors)return failure(c,-8);
    uint32_t address=c->high_capacity?lba:lba*512u;
    int result=data_command(c,17,address,0,sector);
    return result?failure(c,result):0;
}
int holly_sdcard_write_sector(struct holly_sdcard *c,uint32_t lba,const uint8_t *sector) {
    if(!c||!c->ready||!sector)return -1;
    if(lba>=c->sectors)return failure(c,-8);
    uint32_t address=c->high_capacity?lba:lba*512u;
    int result=data_command(c,24,address,1,(uint8_t *)(uintptr_t)sector);
    return result?failure(c,result):0;
}
const char *holly_sdcard_status(const struct holly_sdcard *c) {
    return c&&c->ready?"online":"offline";
}
