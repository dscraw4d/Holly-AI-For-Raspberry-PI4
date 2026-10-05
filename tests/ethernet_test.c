#include <assert.h>
#include <stdio.h>
#include "ethernet.h"
static unsigned phy_seen,reg_seen;
static uint32_t last;
static unsigned prepare_writes,prepare_revision=6,prepare_active;
static uint32_t offsets[24],values[24];
static unsigned staged_writes,staged_busy;
static uint32_t first_offsets[3],first_values[3],last_offsets[3],last_values[3];
static uint16_t mock_rx_producer;
static uint32_t mock_rx_status=(110u<<16)|0x6000u;
static uint32_t mock_rx_hi=0x12u;
static uint32_t io_read(uint32_t offset,void *ctx) {
 (void)ctx;
 if(offset==0)return prepare_revision<<24;
 if(offset==0x5044)return prepare_active;
 if(offset==0x5048)return staged_busy;
 if(offset==0x3008)return mock_rx_producer;
 if(offset==0x2000+5u*12u)return mock_rx_status;
 if(offset==0x2004+5u*12u)return 0x12345000u;
 if(offset==0x2008+5u*12u)return mock_rx_hi;
 if(offset==0x2000+255u*12u)return mock_rx_status;
 if(offset==0x2004+255u*12u)return 0x12345000u;
 if(offset==0x2008+255u*12u)return 0x12u;
 assert(!"unexpected IO read");return 0;
}
static uint32_t stage_read(uint32_t offset,void *ctx) {
 (void)ctx;
 if(offset==0)return prepare_revision<<24;
 if(offset==0x3044)return prepare_active;
 if(offset==0x3048)return staged_busy;
 assert(!"unexpected staging read");return 0;
}
static void stage_write(uint32_t offset,uint32_t value,void *ctx) {
 (void)ctx;
 if(staged_writes<3) {first_offsets[staged_writes]=offset;first_values[staged_writes]=value;}
 if(staged_writes>=765) {unsigned k=staged_writes-765;last_offsets[k]=offset;last_values[k]=value;}
 staged_writes++;
}
static uint32_t prepare_read(uint32_t offset,void *ctx) {
 (void)ctx;
 if(offset==0)return prepare_revision<<24;
 if(offset==0x3044||offset==0x5044)return prepare_active;
 assert(!"unexpected preparation read");return 0;
}
static void prepare_write(uint32_t offset,uint32_t value,void *ctx) {
 (void)ctx;assert(prepare_writes<24);
 offsets[prepare_writes]=offset;values[prepare_writes]=value;prepare_writes++;
}
static uint32_t dma_read(uint32_t offset,void *ctx) {
 (void)ctx;
 switch(offset) {
 case 0x3040: return 0x10000; case 0x3044: return 0x20000;
 case 0x3048: return 0x30000; case 0x3008: return 7;
 case 0x300c: return 5; case 0x3010: return 0x1000800;
 case 0x3014: return 0x11223344; case 0x3018: return 0x12;
 case 0x301c: return 0x55667788; case 0x3020: return 0x34;
 case 0x5040: return 0x40000; case 0x5044: return 0x50000;
 case 0x5048: return 0x60000; case 0x5008: return 9;
 case 0x500c: return 11; case 0x5010: return 0x2000800;
 case 0x5014: return 0x01020304; case 0x5018: return 0x01;
 case 0x501c: return 0x05060708; case 0x5020: return 0x02;
 default: assert(!"unexpected DMA register offset");return 0;
 }
}
static uint32_t read_reg(uint32_t offset,void *ctx) {
 (void)ctx;
 if(offset==0) return 0x05010000;
 if(offset!=0xE14) return 0;
 if(!last) return 0;
 if(phy_seen!=1) return 1u<<28;
 return reg_seen==2 ? 0x1234u : 0x5678u;
}
static void write_reg(uint32_t offset,uint32_t value,void *ctx) {
 (void)ctx;assert(offset==0xE14);
 last=value;phy_seen=(value>>21)&31;reg_seen=(value>>16)&31;
}
int main(void) {
 struct ether_device d={read_reg,write_reg,0};
 assert(ether_revision(&d)==0x05010000);
 unsigned address=99;uint32_t id=0;
 assert(ether_phy_probe(&d,&address,&id)==0);
 assert(address==1 && id==0x12345678);
 uint16_t value=0;
 assert(ether_phy_read(&d,32,2,&value)==-1);
 struct ether_device dma={dma_read,0,0};
 struct ether_dma_snapshot snapshot;
 assert(ether_dma_snapshot(&dma,0,&snapshot)==0);
 assert(snapshot.configuration==0x10000 && snapshot.control==0x20000);
 assert(snapshot.status==0x30000 && snapshot.producer==7 && snapshot.consumer==5);
 assert(snapshot.buffer_size==0x1000800);
 assert(snapshot.descriptor_start==0x1211223344ULL);
 assert(snapshot.descriptor_end==0x3455667788ULL);
 assert(ether_dma_snapshot(&dma,1,&snapshot)==0);
 assert(snapshot.configuration==0x40000 && snapshot.control==0x50000);
 assert(snapshot.status==0x60000 && snapshot.producer==11 && snapshot.consumer==9);
 assert(snapshot.buffer_size==0x2000800);
 assert(snapshot.descriptor_start==0x101020304ULL);
 assert(snapshot.descriptor_end==0x205060708ULL);
 assert(ether_dma_snapshot(&dma,2,&snapshot)==-1);
 assert(ether_dma_snapshot(0,0,&snapshot)==-1);
 struct ether_device staged={prepare_read,prepare_write,0};
 prepare_revision=5;assert(ether_dma_prepare_ring(&staged,0)==-2 && prepare_writes==0);
 prepare_revision=6;prepare_active=1;
 assert(ether_dma_prepare_ring(&staged,0)==-3 && prepare_writes==0);
 prepare_active=0;
 assert(ether_dma_prepare_ring(&staged,0)==0 && prepare_writes==11);
 assert(offsets[0]==0x3008 && offsets[1]==0x300c);
 assert(offsets[6]==0x3014 && offsets[8]==0x301c && values[8]==767);
 assert(offsets[10]==0x3010 && values[10]==0x01000800);
 for(unsigned i=0;i<11;i++)if(i!=8&&i!=10)assert(values[i]==0);
 prepare_writes=0;prepare_revision=7;
 assert(ether_dma_prepare_ring(&staged,1)==0 && prepare_writes==11);
 assert(offsets[0]==0x5008 && offsets[8]==0x501c && offsets[10]==0x5010);
 assert(ether_dma_prepare_ring(&staged,2)==-1 && prepare_writes==11);
 struct ether_device rx_stage={stage_read,stage_write,0};
 prepare_revision=5;
 assert(ether_dma_stage_rx(&rx_stage,0x1200000000ULL)==-2 && staged_writes==0);
 prepare_revision=6;prepare_active=1;
 assert(ether_dma_stage_rx(&rx_stage,0x1200000000ULL)==-3 && staged_writes==0);
 prepare_active=0;staged_busy=2;
 assert(ether_dma_stage_rx(&rx_stage,0x1200000000ULL)==-4 && staged_writes==0);
 staged_busy=0;
 assert(ether_dma_stage_rx(&rx_stage,0)==-1);
 assert(ether_dma_stage_rx(&rx_stage,(1ULL<<40)-2048u)==-1);
 assert(ether_dma_stage_rx(&rx_stage,0x1200000001ULL)==-1);
 assert(ether_dma_stage_rx(&rx_stage,0x1200000000ULL)==0 && staged_writes==768);
 assert(first_offsets[0]==0x2004 && first_values[0]==0);
 assert(first_offsets[1]==0x2008 && first_values[1]==0x12);
 assert(first_offsets[2]==0x2000 && first_values[2]==0x08008000);
 assert(last_offsets[0]==0x2bf8 && last_values[0]==0x0007f800);
 assert(last_offsets[1]==0x2bfc && last_values[1]==0x12);
 assert(last_offsets[2]==0x2bf4 && last_values[2]==0x08008000);
 struct ether_device io={io_read,prepare_write,0};
 prepare_writes=0;prepare_revision=5;
 assert(ether_dma_stage_tx(&io,3,0x1200001000ULL,100)==-2 && prepare_writes==0);
 prepare_revision=6;prepare_active=1;
 assert(ether_dma_stage_tx(&io,3,0x1200001000ULL,100)==-3 && prepare_writes==0);
 prepare_active=0;staged_busy=2;
 assert(ether_dma_stage_tx(&io,3,0x1200001000ULL,100)==-4 && prepare_writes==0);
 staged_busy=0;
 assert(ether_dma_stage_tx(&io,256,0x1200001000ULL,100)==-1);
 assert(ether_dma_stage_tx(&io,3,0x1200001001ULL,100)==-1);
 assert(ether_dma_stage_tx(&io,3,(1ULL<<40)-64u,100)==-1);
 assert(ether_dma_stage_tx(&io,3,0x1200001000ULL,100)==0 && prepare_writes==3);
 assert(offsets[0]==0x4028 && values[0]==0x1000);
 assert(offsets[1]==0x402c && values[1]==0x12);
 assert(offsets[2]==0x4024 && values[2]==(100u<<16|0x7fc0u));
 struct ether_rx_descriptor desc={0};
 mock_rx_producer=5;
 assert(ether_dma_rx_peek(&io,5,&desc)==0);
 mock_rx_producer=6;
 assert(ether_dma_rx_peek(&io,5,&desc)==1);
 assert(desc.length==110 && desc.flags==0x6000 && desc.dma_address==0x1212345000ULL);
 mock_rx_status=(3000u<<16)|0x6000u;
 assert(ether_dma_rx_peek(&io,5,&desc)==-4);
 mock_rx_status=(110u<<16)|0x6000u;
 mock_rx_hi=0x100u;assert(ether_dma_rx_peek(&io,5,&desc)==-4);
 mock_rx_hi=0x12u;
 mock_rx_producer=258;
 assert(ether_dma_rx_peek(&io,0,&desc)==-3);
 mock_rx_producer=0;
 assert(ether_dma_rx_peek(&io,65535u,&desc)==1); /* 16-bit index wrap */
 puts("GENET MDIO diagnostic tests passed");
}
