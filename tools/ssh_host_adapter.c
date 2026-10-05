#define _POSIX_C_SOURCE 200809L
/* Test-only POSIX byte transport. OS/SSH/crypto/chat logic is in src/.
 * Loopback only. Never included in the freestanding kernel. */
#include "ssh_server.h"
#include "selftrain.h"
#include "documents.h"
#include <string.h>
#include <poll.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
static struct holly_ssh_server server;
static uint8_t model_media[2048][512];
static int model_read(uint32_t l,uint8_t *p,void *ctx){(void)ctx;if(l>=2048)return -1;memcpy(p,model_media[l],512);return 0;}
static int model_write(uint32_t l,const uint8_t *p,void *ctx){(void)ctx;if(l<8||l>2046)return -1;memcpy(model_media[l],p,512);return 0;}
static void model_put(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}

static struct holly_documents docs;
static uint8_t document_media[160000u][512];
static int doc_read(uint32_t l,uint8_t *p,void *c){(void)c;if(l>=160000u)return -1;memcpy(p,document_media[l],512);return 0;}
static int doc_write(uint32_t l,const uint8_t *p,void *c){(void)c;if(l>=160000u)return -1;memcpy(document_media[l],p,512);return 0;}
static int doc_command(const char *q,holly_emit_fn emit,void *ctx){return holly_documents_command(&docs,q,emit,ctx);}
static int doc_search(const char *q,holly_emit_fn emit,void *ctx){holly_documents_search(&docs,q,emit,ctx);return 0;}
static int doc_script(const char *q,holly_emit_fn emit,void *ctx){holly_documents_script_ask(&docs,q,emit,ctx);return 0;}
static struct holly_ssh_credentials credentials;
static int send_all(const uint8_t *p,size_t n,void *context){
    int fd=*(int *)context;
    while(n){ssize_t r=send(fd,p,n,MSG_NOSIGNAL);if(r<0&&errno==EINTR)continue;if(r<=0)return -1;p+=r;n-=(size_t)r;}
    return 0;
}
static int random_bytes(uint8_t *p,size_t n,void *context){
    (void)context;while(n){ssize_t r=getrandom(p,n,0);if(r<0&&errno==EINTR)continue;if(r<=0)return -1;p+=r;n-=(size_t)r;}return 0;
}
int main(int argc,char **argv){
    if(argc!=3&&argc!=4){fprintf(stderr,"Usage: ssh_host_adapter credentials.bin fragment-size\n");return 1;}
    FILE *file=fopen(argv[1],"rb");uint8_t magic[8];
    if(!file||fread(magic,1,8,file)!=8||!holly_tag_equal(magic,(const uint8_t *)"HLYSSH23",8)||
       fread(&credentials,1,sizeof(credentials),file)!=sizeof(credentials)||fgetc(file)!=EOF){fprintf(stderr,"Invalid credentials\n");return 1;}
    fclose(file);unsigned fragment=(unsigned)strtoul(argv[2],0,10);if(!fragment||fragment>4096)return 1;
    int training=argc==4&&!strcmp(argv[3],"training");uint64_t model_ticks=0;
    if(training){
        memcpy(model_media[0]+440,"HLY2",4);model_media[0][510]=0x55;model_media[0][511]=0xaa;model_media[0][450]=0x0c;
        model_put(model_media[0]+454,2048);model_put(model_media[0]+458,456704);model_media[0][466]=0xda;model_put(model_media[0]+470,458752);
        struct holly_block_ops io={model_read,model_write,0};if(holly_training_mount(&io)<0)return 1;
    }
    if(argc==4&&!strcmp(argv[3],"documents")){
        struct holly_block_ops io={doc_read,doc_write,0};if(holly_documents_format(&docs,&io,0,160000u))return 1;
        holly_set_document_commands(doc_command,doc_search,doc_script);holly_reference_set_documents(holly_documents_visit_query,&docs);
    }
    signal(SIGPIPE,SIG_IGN);int listener=socket(AF_INET,SOCK_STREAM,0);if(listener<0)return 1;
    struct sockaddr_in address={0};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(listener,(struct sockaddr *)&address,sizeof(address))||listen(listener,4))return 1;
    socklen_t length=sizeof(address);if(getsockname(listener,(struct sockaddr *)&address,&length))return 1;
    printf("PORT %u\n",ntohs(address.sin_port));fflush(stdout);
    for(;;){
        int fd=accept(listener,0,0);if(fd<0){if(errno==EINTR)continue;return 1;}
        struct timeval timeout={15,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
        setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
        if(!holly_ssh_server_start(&server,&credentials,send_all,random_bytes,&fd)){
            uint8_t buffer[4096];ssize_t n;
            for(;;){
                if(training){struct pollfd pfd={fd,POLLIN,0};int ready=poll(&pfd,1,5);model_ticks+=20000;holly_training_tick(model_ticks);if(ready==0)continue;if(ready<0)break;}
                n=recv(fd,buffer,fragment,0);if(n<=0||holly_ssh_server_feed(&server,buffer,(size_t)n))break;
            }
        }
        holly_ssh_server_destroy(&server);close(fd);
    }
}
