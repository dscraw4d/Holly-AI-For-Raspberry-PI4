#include "animation.h"
int holly_animation_step(struct holly_animation *s,uint64_t now,
                         enum holly_expression requested){
    if(!s||requested>HOLLY_THINKING)return -1;
    if(!s->initialized||now<s->last_ms)s->origin_ms=now;
    uint64_t elapsed=now-s->origin_ms;
    enum holly_expression expression=requested;
    unsigned level=0;
    if(requested==HOLLY_SPEAKING){
        unsigned phase=(unsigned)((elapsed/80u)%6u);
        level=40u+((phase<=3u)?phase:6u-phase)*60u;
    }else if((requested==HOLLY_IDLE||requested==HOLLY_LISTENING)&&
             elapsed%5000u>=4850u)expression=HOLLY_BLINKING;
    if(s->initialized&&expression==s->expression&&level==s->level){
        s->last_ms=now;return 0;
    }
    s->initialized=1;s->last_ms=now;s->expression=expression;s->level=level;
    return 1;
}

static char mouth_lower(char c){return c>='A'&&c<='Z'?(char)(c+32):c;}
void holly_mouth_reply(struct holly_mouth *s,const char *text){
 if(!s||!text)return;
 s->count=s->index=s->started=s->manual=0;
 unsigned scanned=0;
 while(*text&&s->count<HOLLY_MOUTH_EVENTS&&scanned++<1024){
  char c=mouth_lower(*text++);unsigned frame=1,duration=65;
  if(c=='['){while(*text&&*text!=']'&&scanned++<1024)text++;if(*text)text++;continue;}
  if(c=='a')frame=3;
  else if(c=='e'||c=='f'||c=='v')frame=5;
  else if(c=='i'||c=='y')frame=2;
  else if(c=='o'||c=='u'||c=='w'||c=='q')frame=4;
  else if(c=='m'||c=='b'||c=='p')frame=0;
  else if(c=='t'||c=='d'||c=='n'||c=='l'||c=='s'||c=='z')frame=6;
  else if(c==' '||c=='\n'||c=='\r'){frame=0;duration=70;}
  else if(c=='.'||c==','||c=='!'||c=='?'||c==';'||c==':'){frame=0;duration=150;}
  else if(c<'a'||c>'z')continue;
  unsigned at=s->count++;s->frames[at]=(uint8_t)frame;s->durations[at]=(uint8_t)duration;
 }
}
int holly_mouth_frame(struct holly_mouth *s,unsigned frame){
 if(!s||frame>6)return -1;
 s->count=s->index=s->started=0;s->manual=1;s->frame=frame;return 0;
}
int holly_mouth_step(struct holly_mouth *s,uint64_t now){
 if(!s)return -1;
 unsigned old=s->frame;
 if(s->manual)return 0;
 if(s->started&&now<s->last_ms){s->index=s->count=0;s->started=0;}
 s->last_ms=now;
 if(!s->count){s->frame=0;return old!=s->frame;}
 if(!s->started){s->started=1;s->index=0;s->frame=s->frames[0];s->next_ms=now+s->durations[0];}
 else {
  /* Skip expired events rather than replaying a burst after a stalled poll. */
  while(s->index<s->count&&now>=s->next_ms){
   if(++s->index<s->count){s->frame=s->frames[s->index];s->next_ms+=s->durations[s->index];}
   else {s->frame=0;s->count=s->index=s->started=0;break;}
  }
 }
 return old!=s->frame;
}
