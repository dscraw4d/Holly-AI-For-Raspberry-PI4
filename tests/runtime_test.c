#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 char a[16],b[16];
 assert(memset(a,0x141,16)==a&&a[0]=='A'&&a[15]=='A');
 assert(memcpy(b,a,16)==b&&memcmp(a,b,16)==0);
 b[15]='B';assert(memcmp(a,b,16)<0&&memcmp(b,a,16)>0);
 memcpy(a,"abcdefgh",9);memmove(a+2,a,6);assert(memcmp(a,"ababcdef",8)==0);
 memmove(a,a+2,6);assert(memcmp(a,"abcdef",6)==0);
 memmove(a,a,6);assert(memcmp(a,"abcdef",6)==0);
 assert(memcmp(a,b,0)==0);assert(memset(a,0,0)==a);
 puts("Freestanding memory runtime overlap and boundary checks passed");
}
