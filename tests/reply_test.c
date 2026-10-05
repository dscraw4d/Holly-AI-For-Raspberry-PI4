#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "reply.h"
#include "net.h"
int main(void) {
 uint8_t mymac[6]={2,0,0,0,0,1},myip[4]={192,168,1,90};
 uint8_t request[64]={0},answer[128]={0};
 memset(request,255,6);request[6]=0x12;request[12]=8;request[13]=6;
 request[15]=1;request[16]=8;request[18]=6;request[19]=4;request[21]=1;
 request[22]=0x12;request[28]=192;request[29]=168;request[30]=1;request[31]=2;
 memcpy(request+38,myip,4);
 assert(viper_network_reply(request,42,mymac,myip,answer,sizeof(answer))==42);
 assert(answer[21]==2 && memcmp(answer+22,mymac,6)==0 && answer[32]==0x12);
 memset(request,0,sizeof(request));
 memcpy(request,mymac,6);request[6]=0x12;request[12]=8;request[13]=0;
 uint8_t *p=request+14;p[0]=0x45;p[2]=0;p[3]=28;p[8]=64;p[9]=1;
 p[12]=192;p[13]=168;p[14]=1;p[15]=2;memcpy(p+16,myip,4);
 unsigned short sum=viper_checksum(p,20);p[10]=sum>>8;p[11]=sum;
 p[20]=8;p[24]=0xBE;p[25]=0xEF;
 sum=viper_checksum(p+20,8);p[22]=sum>>8;p[23]=sum;
 assert(viper_network_reply(request,42,mymac,myip,answer,sizeof(answer))==42);
 assert(answer[34]==0 && memcmp(answer+26,myip,4)==0);
 assert(viper_checksum(answer+14,20)==0 && viper_checksum(answer+34,8)==0);
 request[36]^=1;assert(viper_network_reply(request,42,mymac,myip,answer,sizeof(answer))==0);
 puts("ARP and ICMP reply tests passed");
}
