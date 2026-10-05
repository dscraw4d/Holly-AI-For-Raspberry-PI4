#ifndef HOLLY_PI_NETWORK_H
#define HOLLY_PI_NETWORK_H
#include <stdint.h>
#include "holly.h"
/* Production Pi integration: RNG200 -> SSH/TCP -> coherent GENET DMA.
 * Initialization is enabled only in a provisioned build. */
int holly_pi_network_start(void);
void holly_pi_network_poll(uint64_t microseconds);
int holly_pi_network_expression(uint64_t microseconds,enum holly_expression *expression);
extern int holly_pi_network_status;
extern unsigned holly_pi_network_restarts;
/* 0 waiting, 1 direct-cable fallback, 2 DHCP lease. */
extern int holly_pi_network_dhcp;
extern uint8_t holly_pi_network_ip[4];
#endif
