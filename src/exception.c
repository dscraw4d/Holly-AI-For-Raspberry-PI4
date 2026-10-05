#include "exception.h"
extern char holly_probe_load[],holly_probe_fail[];
void holly_exception_dispatch(uint64_t f[36]){
    unsigned ec=(unsigned)(f[33]>>26),fault=(unsigned)(f[33]&63u);
    /* Frame: x0..x30, ELR, SPSR, ESR, FAR, CurrentEL. Synchronous
     * external abort and translation faults are valid 'device absent' cases.
     * Alignment/permission faults and arbitrary faulting PCs are not hidden. */
    if(ec==0x25u&&f[31]==(uintptr_t)holly_probe_load&&
       (fault==0x10u||(fault>=4u&&fault<=7u))){
        f[31]=(uintptr_t)holly_probe_fail;return;
    }
    for(;;)__asm__ volatile("wfe");
}
