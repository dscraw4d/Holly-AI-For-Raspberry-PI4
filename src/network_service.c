#include "network_service.h"
#include "reply.h"
void holly_network_init(struct holly_network *n,const uint8_t mac[6],const uint8_t ip[4]) {
    if(!n || !mac || !ip) return;
    viper_ring_init(&n->rx);viper_ring_init(&n->tx);
    holly_tcp_init(&n->diagnostic_tcp);
    for(unsigned i=0;i<6;i++)n->mac[i]=mac[i];
    for(unsigned i=0;i<4;i++)n->ip[i]=ip[i];
    n->received=n->answered=n->dropped=0;
}
int holly_network_receive(struct holly_network *n,const uint8_t *frame,unsigned length) {
    if(!n) return -1;
    int result=viper_ring_push(&n->rx,frame,length);
    if(result==0)n->received++;
    else n->dropped++;
    return result;
}
unsigned holly_network_process(struct holly_network *n,unsigned budget) {
    if(!n)return 0;
    unsigned processed=0;
    uint8_t frame[VIPER_FRAME_CAP],answer[VIPER_FRAME_CAP];
    while(processed<budget) {
        unsigned length=0;
        if(viper_ring_pop(&n->rx,frame,sizeof(frame),&length))break;
        processed++;
        int out=viper_network_reply(frame,length,n->mac,n->ip,answer,sizeof(answer));
        if(out==0)out=holly_tcp_receive(&n->diagnostic_tcp,frame,length,n->mac,n->ip,answer,sizeof(answer));
        if(out>0) {
            if(viper_ring_push(&n->tx,answer,(unsigned)out)==0)n->answered++;
            else n->dropped++;
        } else if(out<0)n->dropped++;
    }
    return processed;
}
int holly_network_tick(struct holly_network *n,uint32_t elapsed_ms) {
    if(!n)return -1;
    uint8_t answer[54];
    int out=holly_tcp_tick(&n->diagnostic_tcp,elapsed_ms,n->mac,n->ip,answer,sizeof(answer));
    if(out>0) {
        if(viper_ring_push(&n->tx,answer,(unsigned)out)) {n->dropped++;return -2;}
        n->answered++;
    }
    return out;
}
int holly_network_transmit(struct holly_network *n,uint8_t *frame,unsigned capacity,unsigned *length) {
    if(!n)return -1;
    return viper_ring_pop(&n->tx,frame,capacity,length);
}
