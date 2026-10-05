#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "net.h"
int main(void) {
 uint8_t frame[14+20+20]={0};
 frame[12]=0x08;frame[13]=0;
 frame[14]=0x45;frame[16]=0;frame[17]=40;frame[22]=64;frame[23]=6;
 frame[26]=192;frame[27]=168;frame[28]=1;frame[29]=2;
 frame[30]=192;frame[31]=168;frame[32]=1;frame[33]=3;
 frame[34]=0xC0;frame[35]=0x01;frame[36]=0;frame[37]=22;
 frame[46]=0x50;
 unsigned short check=viper_checksum(frame+14,20);
 frame[24]=(uint8_t)(check>>8);frame[25]=(uint8_t)check;
 struct viper_packet packet={0};
 assert(viper_parse_packet(frame,sizeof(frame),&packet)==1);
 assert(packet.target_port==22 && packet.source_port==49153 && packet.payload_len==0);
 assert(viper_parse_packet(frame,19,&packet)==-1);
 frame[24]^=1;
 assert(viper_parse_packet(frame,sizeof(frame),&packet)==-1);
 puts("Network packet parsing tests passed");
}
