#ifndef HOLLY_EXCEPTION_H
#define HOLLY_EXCEPTION_H
#include <stdint.h>
/* Recover only a synchronous translation/external data abort at one explicitly
 * guarded MMIO load instruction. All unrelated exceptions halt. */
int holly_cpu_probe32(uintptr_t address,uint32_t *value);
void holly_exception_dispatch(uint64_t frame[36]);
#endif
