#include "search_disk.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SECTORS 2100
static uint8_t disk[SECTORS*512],snapshot[SECTORS*512];static int fail=-1,writes;
static int rd(uint32_t n,uint8_t*b,void*c){(void)c;assert(n>=10&&n<10+HOLLY_SEARCH_DISK_SECTORS);memcpy(b,disk+n*512,512);return 0;}
static int wr(uint32_t n,const uint8_t*b,void*c){(void)c;assert(n>=10&&n<10+HOLLY_SEARCH_DISK_SECTORS);if(fail>=0&&writes++>=fail)return -1;memcpy(disk+n*512,b,512);return 0;}
static struct holly_block_ops io={rd,wr,0};
int main(void){memset(disk,0,sizeof disk);memset(disk,0xa5,10*512);memset(disk+(10+HOLLY_SEARCH_DISK_SECTORS)*512,0x5a,(SECTORS-10-HOLLY_SEARCH_DISK_SECTORS)*512);assert(!holly_search_disk_bind(&io,10,HOLLY_SEARCH_DISK_SECTORS));char text[1201],url[513],out[1201],source[513],topic[193];uint32_t epoch;memset(text,'x',1200);text[1200]=0;memset(url,'y',512);memcpy(url,"https://",8);url[512]=0;
 for(unsigned n=0;n<256;n++){snprintf(topic,sizeof topic,"topic %u",n);assert(!holly_search_disk_save(topic,text,url,1791176400));}assert(holly_search_disk_count()==256);assert(holly_search_disk_save("extra",text,url,1791176400)<0);assert(!holly_search_disk_bind(&io,10,HOLLY_SEARCH_DISK_SECTORS));assert(holly_search_disk_count()==256);
 for(unsigned n=0;n<256;n++){snprintf(topic,sizeof topic,"topic %u",n);assert(holly_search_disk_get(topic,out,source,&epoch)==1);assert(!strcmp(text,out)&&!strcmp(url,source)&&epoch==1791176400);}
 memcpy(snapshot,disk,sizeof disk);for(unsigned f=0;f<4;f++){memcpy(disk,snapshot,sizeof disk);assert(!holly_search_disk_bind(&io,10,HOLLY_SEARCH_DISK_SECTORS));fail=(int)f;writes=0;assert(holly_search_disk_save("topic 0","replacement","https://example.org",1791176401)<0);fail=-1;assert(!holly_search_disk_bind(&io,10,HOLLY_SEARCH_DISK_SECTORS));assert(holly_search_disk_get("topic 0",out,source,&epoch)==1&&!strcmp(out,text));}
 assert(!holly_search_disk_forget("topic 0"));assert(!holly_search_disk_bind(&io,10,HOLLY_SEARCH_DISK_SECTORS));assert(holly_search_disk_count()==255&&!holly_search_disk_get("topic 0",out,source,&epoch)&&holly_search_disk_known("topic 0"));assert(!holly_search_disk_save("new topic",text,url,1791176402));assert(holly_search_disk_count()==256);assert(!holly_search_disk_save("topic 1","replacement","https://example.org",1791176403));assert(!holly_search_disk_bind(&io,10,HOLLY_SEARCH_DISK_SECTORS));assert(holly_search_disk_get("topic 1",out,source,&epoch)&&!strcmp(out,"replacement"));
 for(unsigned n=0;n<10*512;n++)assert(disk[n]==0xa5);
 for(unsigned n=(10+HOLLY_SEARCH_DISK_SECTORS)*512;n<sizeof disk;n++)assert(disk[n]==0x5a);
 puts("Disk cache: 256 maximum summaries/sources, reboot, quota, interrupted writes, refresh, durable forget, reuse and bounds passed.");}
