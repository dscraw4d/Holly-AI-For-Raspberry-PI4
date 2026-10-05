#ifndef HOLLY_DHCP_H
#define HOLLY_DHCP_H
#include <stdint.h>
#include <stddef.h>
typedef int (*holly_dhcp_send_fn)(const uint8_t *,unsigned,void *);
typedef int (*holly_dhcp_random_fn)(uint8_t *,size_t,void *);
enum holly_dhcp_phase { HOLLY_DHCP_OFF,HOLLY_DHCP_SELECT,HOLLY_DHCP_REQUEST,HOLLY_DHCP_PROBE,HOLLY_DHCP_BOUND,HOLLY_DHCP_RENEW,HOLLY_DHCP_REBIND,HOLLY_DHCP_WAIT };
struct holly_dhcp {
 enum holly_dhcp_phase phase;
 uint8_t mac[6],ip[4],offered[4],server[4],server_mac[6],mask[4],router[4],dns[4],fallback[4];
 uint32_t xid,lease,t1,t2; unsigned configured,leased,probes,retries;
 uint64_t now,started,due,request_time,lease_started,renew_at,rebind_at,expires;
 holly_dhcp_send_fn send; holly_dhcp_random_fn random; void *context;
 uint8_t frame[600];
};
void holly_dhcp_init(struct holly_dhcp *,const uint8_t[6],const uint8_t[4],holly_dhcp_send_fn,holly_dhcp_random_fn,void *);
int holly_dhcp_link(struct holly_dhcp *,int up,uint64_t ms);
int holly_dhcp_tick(struct holly_dhcp *,uint64_t ms);
/* Returns 1 for a handled DHCP/ARP conflict, 0 unrelated, -1 malformed. */
int holly_dhcp_receive(struct holly_dhcp *,const uint8_t *,unsigned,uint64_t ms);
#endif
