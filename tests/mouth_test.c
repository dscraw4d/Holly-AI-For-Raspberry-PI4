#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "animation.h"
int main(void){
 struct holly_mouth s={0};assert(!holly_mouth_step(&s,0));
 holly_mouth_reply(&s,"aoiembpt");assert(s.count==8);
 assert(s.frames[0]==3&&s.frames[1]==4&&s.frames[2]==2&&s.frames[3]==5);
 assert(s.frames[4]==0&&s.frames[5]==0&&s.frames[6]==0&&s.frames[7]==6);
 assert(holly_mouth_step(&s,1000)==1&&s.frame==3);
 assert(holly_mouth_step(&s,1020)==0);
 assert(holly_mouth_step(&s,1065)==1&&s.frame==4);
 assert(holly_mouth_step(&s,10000)==1&&!s.count&&!s.frame);
 assert(holly_mouth_step(&s,11000)==0);
 holly_mouth_reply(&s,"a");assert(holly_mouth_step(&s,12000)==1);
 assert(holly_mouth_step(&s,10)==1&&!s.count&&!s.frame); /* clock rollback */
 assert(!holly_mouth_frame(&s,6));assert(!holly_mouth_step(&s,20000)&&s.frame==6);
 assert(holly_mouth_frame(&s,7)==-1&&s.frame==6);
 char huge[4096];memset(huge,'a',sizeof huge);huge[4095]=0;
 holly_mouth_reply(&s,huge);assert(s.count==HOLLY_MOUTH_EVENTS&&!s.manual);
 assert(holly_mouth_step(&s,30000)==1);(void)holly_mouth_step(&s,100000);assert(!s.frame&&!s.count);
 holly_mouth_reply(&s,"[citation metadata] A.");assert(s.count==3&&s.frames[1]==3&&s.durations[2]==150);
 puts("Text mouth shapes, closed bilabials, punctuation pauses, bounded queue, restart, manual frames, clock rollback and idle stop passed");
}
