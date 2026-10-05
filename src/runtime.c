#include <stddef.h>
#include <stdint.h>
/* Original freestanding byte operations required by compiler-generated code. */
void *memset(void *destination,int value,size_t bytes){
    uint8_t *out=destination;
    for(size_t i=0;i<bytes;i++)out[i]=(uint8_t)value;
    return destination;
}
void *memcpy(void *destination,const void *source,size_t bytes){
    uint8_t *out=destination;const uint8_t *in=source;
    for(size_t i=0;i<bytes;i++)out[i]=in[i];
    return destination;
}
void *memmove(void *destination,const void *source,size_t bytes){
    uint8_t *out=destination;const uint8_t *in=source;
    if((uintptr_t)out<(uintptr_t)in){for(size_t i=0;i<bytes;i++)out[i]=in[i];}
    else {while(bytes){bytes--;out[bytes]=in[bytes];}}
    return destination;
}
int memcmp(const void *left,const void *right,size_t bytes){
    const uint8_t *a=left,*b=right;
    for(size_t i=0;i<bytes;i++)if(a[i]!=b[i])return a[i]<b[i]?-1:1;
    return 0;
}
size_t strlen(const char *text){size_t n=0;while(text[n])n++;return n;}
int strcmp(const char *a,const char *b){
    while(*a&&*a==*b){a++;b++;}
    return (unsigned char)*a-(unsigned char)*b;
}
