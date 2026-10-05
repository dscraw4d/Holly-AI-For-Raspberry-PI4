#ifndef HOLLY_SMP_H
#define HOLLY_SMP_H
#ifdef __aarch64__
/* One primary, up to three workers; inference only. Bare metal, cache-off. */
unsigned holly_smp_start(void);
unsigned holly_smp_workers(void);
void holly_smp_parallel(void (*run)(unsigned,void *),void *context);
void holly_smp_worker(unsigned core);
#else
static inline unsigned holly_smp_workers(void){return 0;}
static inline void holly_smp_parallel(void (*run)(unsigned,void *),void *context){for(unsigned i=0;i<4;i++)run(i,context);}
#endif
#endif
