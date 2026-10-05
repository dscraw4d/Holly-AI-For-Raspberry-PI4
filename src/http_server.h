#ifndef HOLLY_HTTP_SERVER_H
#define HOLLY_HTTP_SERVER_H
#include "ssh_server.h"
struct holly_http_session { char token[33];unsigned avatar,speech_generation,speech_closed;struct holly_session chat; };
typedef void (*holly_web_speech_fn)(unsigned action,unsigned frame,void *);
void holly_web_set_speech_observer(holly_web_speech_fn,void *);
struct holly_web_server {
 holly_ssh_write_fn write;void *transport;
 holly_ssh_random_fn random;void *random_context;
 char speech_owner[33];
 char request[3073];unsigned used,done,next,avatar;
 struct holly_http_session sessions[4];
};
void holly_web_status(unsigned lan,unsigned vault);
int holly_web_start(holly_ssh_write_fn,void *,void *);
int holly_web_feed(const uint8_t *,size_t,void *);
void holly_web_close(void *);
void holly_web_clear(struct holly_web_server *);
#endif
