#include <assert.h>
#include <stdio.h>
#include "face.h"
int main(void){
 uint32_t pixels[66*64];
 for(unsigned i=0;i<66u*64u;i++)pixels[i]=0xdeadbeefu;
 assert(holly_face_render(pixels,64,64,66,66u*64u,HOLLY_IDLE,0)==0);
 assert(pixels[0]!=0xdeadbeefu && pixels[65]==0xdeadbeefu);
 assert(pixels[66u*64u-1]==0xdeadbeefu);
 uint32_t idle=pixels[43u*66u+32u];
 uint32_t eye=pixels[29u*66u+25u];
 assert(holly_face_render(pixels,64,64,66,66u*64u,HOLLY_BLINKING,0)==0);
 assert(pixels[29u*66u+25u]!=eye && pixels[65]==0xdeadbeefu);
 assert(holly_face_render(pixels,64,64,66,66u*64u,HOLLY_SPEAKING,255)==0);
 assert(pixels[43u*66u+32u]!=idle);
 assert(holly_face_render(pixels,64,64,66,66u*64u-1,HOLLY_IDLE,0)==-1);
 assert(holly_face_render(pixels,63,64,66,66u*64u,HOLLY_IDLE,0)==-1);
 assert(holly_face_render(pixels,64,64,63,66u*64u,HOLLY_IDLE,0)==-1);
 assert(holly_face_render(pixels,64,64,66,66u*64u,HOLLY_IDLE,256)==-1);
 puts("Holly face renderer bounds and animation tests passed");
}
