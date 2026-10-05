#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ssh_ident.h"
static int feed(struct holly_ssh_ident *s,const char *p){return holly_ssh_ident_feed(s,(const uint8_t *)p,(unsigned)strlen(p));}
int main(void){
 struct holly_ssh_ident s;holly_ssh_ident_init(&s);
 assert(feed(&s,"SSH-2.0-Client_")==0);
 assert(feed(&s,"1\r")==0);
 assert(feed(&s,"\n")==1 && s.complete);
 assert(strcmp(s.line,"SSH-2.0-Client_1\r\n")==0);
 assert(feed(&s,"x")==-1);
 holly_ssh_ident_init(&s);assert(feed(&s,"SSH-1.5-Old\r\n")==-1);
 holly_ssh_ident_init(&s);assert(feed(&s,"SSH-2.0-\r\n")==-1);
 holly_ssh_ident_init(&s);assert(feed(&s,"SSH-2.0-X\rX")==-1);
 holly_ssh_ident_init(&s);assert(feed(&s,"SSH-2.0-X\n")==-1);
 holly_ssh_ident_init(&s);
 for(unsigned i=0;i<254;i++) {
  uint8_t c=(uint8_t)(i<8 ? "SSH-2.0-"[i] : 'a');
  assert(holly_ssh_ident_feed(&s,&c,1)==0);
 }
 assert(feed(&s,"\r")==0);
 assert(feed(&s,"\n")==-1); /* 256 bytes including CRLF */
 puts("SSH identification parsing tests passed");
}
