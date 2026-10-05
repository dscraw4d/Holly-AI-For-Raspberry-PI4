#include "smp.h"
#include <stdint.h>
/* Pi 4 firmware/QEMU AArch64 spin table at e0,e8,f0. Primary owns all I/O.
   Control data lies in BSS (normal RAM), not in the MMIO mapping. */
static uint32_t started[4],complete[4],generation;
static void (*work)(unsigned,void *);
static void *argument;
unsigned holly_smp_workers(void){unsigned n=0;for(unsigned i=1;i<4;i++)if(__atomic_load_n(&started[i],__ATOMIC_ACQUIRE))n++;return n;}
void holly_smp_worker(unsigned core){
 if(!core||core>3)for(;;)__asm__ volatile("wfe");
 __atomic_store_n(&started[core],1,__ATOMIC_RELEASE);
 unsigned seen=0;
 for(;;){
  unsigned ticket=__atomic_load_n(&generation,__ATOMIC_ACQUIRE);
  if(ticket!=seen){
   void (*f)(unsigned,void *)=work;void *arg=argument;
   if(f)f(core,arg);
   __atomic_store_n(&complete[core],ticket,__ATOMIC_RELEASE);
   seen=ticket;__asm__ volatile("sev");
  }else __asm__ volatile("wfe");
 }
}
extern void holly_secondary_entry(void);
unsigned holly_smp_start(void){
 uintptr_t entry=(uintptr_t)&holly_secondary_entry;
 for(unsigned i=1;i<4;i++){uintptr_t slot=0xd8u+i*8u;__asm__ volatile("str %0,[%1]"::"r"((uint64_t)entry),"r"(slot):"memory");}
 __asm__ volatile("dsb sy\nsev" ::: "memory");
 /* Bounded handshake: if firmware leaves any core parked, serial inference works. */
 for(unsigned tries=0;tries<400000;tries++)if(holly_smp_workers()==3)break;
 return holly_smp_workers();
}
void holly_smp_parallel(void (*fn)(unsigned,void *),void *ctx){
 if(holly_smp_workers()!=3){for(unsigned i=0;i<4;i++)fn(i,ctx);return;}
 work=fn;argument=ctx;__asm__ volatile("dmb sy" ::: "memory");
 unsigned ticket=__atomic_load_n(&generation,__ATOMIC_RELAXED)+1;
 __atomic_store_n(&generation,ticket,__ATOMIC_RELEASE);
 __asm__ volatile("sev" ::: "memory");
 fn(0,ctx);
 for(unsigned core=1;core<4;core++)while(__atomic_load_n(&complete[core],__ATOMIC_ACQUIRE)!=ticket)__asm__ volatile("wfe");
}
