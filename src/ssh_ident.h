#ifndef HOLLY_SSH_IDENT_H
#define HOLLY_SSH_IDENT_H
#include <stdint.h>
#define HOLLY_SSH_ID_MAX 255u
struct holly_ssh_ident {
    char line[HOLLY_SSH_ID_MAX+1u];
    unsigned length;
    int complete;
};
extern const uint8_t holly_ssh_server_ident[];
extern const unsigned holly_ssh_server_ident_length;
void holly_ssh_ident_init(struct holly_ssh_ident *state);
/* 0 incomplete, 1 complete, -1 invalid. Only accepts one SSH-2.0 line. */
int holly_ssh_ident_feed(struct holly_ssh_ident *state,const uint8_t *data,unsigned length);
#endif
