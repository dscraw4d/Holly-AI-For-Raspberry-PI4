/* Run the actual embedded HTTP/chat server on localhost for UI checks. */
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "http_server.h"
static int write_out(const uint8_t *b,size_t n,void *ctx){int fd=*(int *)ctx;while(n){ssize_t k=send(fd,b,n,0);if(k<=0)return -1;b+=k;n-=k;}return 0;}
static int random_bytes(uint8_t *b,size_t n,void *ctx){(void)ctx;int fd=open("/dev/urandom",O_RDONLY);if(fd<0)return -1;ssize_t k=read(fd,b,n);close(fd);return k==(ssize_t)n?0:-1;}
int main(void){int fd=socket(AF_INET,SOCK_STREAM,0),yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof yes);struct sockaddr_in a={0};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(8087);if(bind(fd,(struct sockaddr *)&a,sizeof a)||listen(fd,8))return 1;static struct holly_web_server h;h.random=random_bytes;holly_web_status(1,0);for(;;){int c=accept(fd,0,0);if(c<0)continue;holly_web_start(write_out,&c,&h);uint8_t b[1024];ssize_t n;while((n=recv(c,b,sizeof b,0))>0)if(holly_web_feed(b,n,&h))break;holly_web_close(&h);close(c);}}
