#include "telnet.h"
/* NVT ASCII line mode: refuse options; client performs local echo. RFC 854/855. */
static void emit(const char *s,void *ctx){
 struct holly_telnet *t=ctx;uint8_t b[128];unsigned n=0;
 while(*s&&!t->failed){
  unsigned char c=(unsigned char)*s++;
  if(c=='\n')b[n++]='\r';
  b[n++]=c;if(c==255)b[n++]=255;
  if(n>124||!*s){if(t->write(b,n,t->context))t->failed=1;n=0;}
 }
}
int holly_telnet_start(holly_ssh_write_fn write,void *transport,void *app){
 struct holly_telnet *t=app;if(!t||!write)return -1;
 holly_secret_wipe(t,sizeof(*t));t->write=write;t->context=transport;
 holly_session_init(&t->chat);t->chat.learning_enabled=0;
 emit("Holly: LAN Telnet conversation. Unencrypted guest access; no training or administration.\nUse local echo. Type help or exit.\nholly> ",t);
 return t->failed?-1:0;
}
void holly_telnet_close(void *app){if(app)holly_secret_wipe(app,sizeof(struct holly_telnet));}
int holly_telnet_feed(const uint8_t *data,size_t n,void *app){
 struct holly_telnet *t=app;if(!t||!t->write||(!data&&n)||t->failed)return -1;
 for(size_t i=0;i<n;i++){
  unsigned c=data[i];
  if(t->state==1){
   if(c>=251&&c<=254){t->verb=c;t->state=2;}
   else if(c==250)t->state=3;
   else{t->state=0;if(c==247&&t->used)t->used--;if(c==248)t->used=t->overflow=0;}
   continue;
  }
  if(t->state==2){
   if(t->verb==251||t->verb==253){uint8_t no[3]={255,t->verb==251?254:252,(uint8_t)c};if(t->write(no,3,t->context))return -1;}
   t->state=0;continue;
  }
  if(t->state==3){if(c==255)t->state=4;continue;}
  if(t->state==4){t->state=c==240?0:3;continue;}
  if(c==255){t->state=1;continue;}
  if(t->skip){t->skip=0;if(c=='\n'||c==0)continue;}
  if(c=='\r'||c=='\n'){
   if(c=='\r')t->skip=1;
   t->line[t->used]=0;
   if(t->overflow)emit("Holly: Line too long; discarded.\n",t);
   else if(holly_conversation_only(&t->chat,t->line,emit,t)<0)return -1;
   t->used=t->overflow=0;emit("holly> ",t);if(t->failed)return -1;
  }else if(c==8||c==127){if(t->used&&!t->overflow)t->used--;}
  else if(c>=32&&c<127){if(t->used+1<sizeof t->line&&!t->overflow)t->line[t->used++]=(char)c;else t->overflow=1;}
 }
 return 0;
}
