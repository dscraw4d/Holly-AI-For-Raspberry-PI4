#include "ssh_ident.h"
const uint8_t holly_ssh_server_ident[]="SSH-2.0-HollyAI_0.49.6\r\n";
const unsigned holly_ssh_server_ident_length=sizeof(holly_ssh_server_ident)-1u;
void holly_ssh_ident_init(struct holly_ssh_ident *s) {
    if(s){s->length=0;s->complete=0;s->line[0]=0;}
}
int holly_ssh_ident_feed(struct holly_ssh_ident *s,const uint8_t *data,unsigned n) {
    static const char prefix[]="SSH-2.0-";
    if(!s||(!data&&n)||s->complete)return -1;
    for(unsigned i=0;i<n;i++) {
        unsigned k=s->length;
        if(k>=HOLLY_SSH_ID_MAX)return -1;
        uint8_t c=data[i];
        if(k && s->line[k-1]=='\r' && c!='\n')return -1;
        if(c==0||c>127||c<32) {
            if(c!='\r'&&c!='\n')return -1;
            if(c=='\r' && (k<9||(i+1<n&&data[i+1]!='\n')))return -1;
            if(c=='\n' && (k<10||s->line[k-1]!='\r'||i+1<n))return -1;
        }
        if(k<8 && c!=(uint8_t)prefix[k])return -1;
        s->line[k]=(char)c;s->length=k+1;s->line[k+1]=0;
        if(c=='\n') {s->complete=1;return 1;}
    }
    return 0;
}
