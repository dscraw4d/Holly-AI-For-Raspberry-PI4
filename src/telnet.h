#ifndef HOLLY_TELNET_H
#define HOLLY_TELNET_H
#include "ssh_server.h"
struct holly_telnet {
 struct holly_session chat;
 holly_ssh_write_fn write;void *context;
 char line[320];unsigned used,overflow,state,verb,skip,failed;
};
int holly_telnet_start(holly_ssh_write_fn,void *transport,void *app);
int holly_telnet_feed(const uint8_t *,size_t,void *app);
void holly_telnet_close(void *app);
#endif
