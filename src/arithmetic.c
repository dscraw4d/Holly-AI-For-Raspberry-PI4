/* Original bounded six-place fixed-point calculator. No heap or network. */
#include "arithmetic.h"
#include <stdint.h>
#define SCALE INT64_C(1000000)
struct parser {const char*p;unsigned depth,operations;int error;};
static void space(struct parser*s){while(*s->p==' ')s->p++;}
static int match(struct parser*s,const char*w){space(s);const char*p=s->p;while(*w&&*p==*w){p++;w++;}if(*w)return 0;s->p=p;return 1;}
static int named_number(struct parser*s,int64_t*v){const char*names[]={"zero","one","two","three","four","five","six","seven","eight","nine","ten","eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen","twenty"};space(s);const char*begin=s->p;for(unsigned i=0;i<21;i++){s->p=begin;if(match(s,names[i])&&!((*s->p>='a'&&*s->p<='z')||(*s->p>='0'&&*s->p<='9'))){*v=(int64_t)i*SCALE;return 1;}}s->p=begin;return 0;}
static int64_t expression(struct parser*s);
static int64_t atom(struct parser*s){
 space(s);if(++s->depth>16){s->error=1;s->depth--;return 0;}
 int negative=0;if(*s->p=='+'||*s->p=='-')negative=*s->p++=='-';space(s);int64_t v=0;
 if(*s->p=='('){s->p++;v=expression(s);space(s);if(*s->p!=')'){if(!s->error)s->error=1;}else s->p++;}
 else if(named_number(s,&v)){}
 else {unsigned digits=0;while(*s->p>='0'&&*s->p<='9'){int64_t t;if(__builtin_mul_overflow(v,INT64_C(10),&t)||__builtin_add_overflow(t,(int64_t)(*s->p-'0'),&v)){s->error=3;break;}s->p++;digits++;}
 if(!s->error&&__builtin_mul_overflow(v,SCALE,&v))s->error=3;
 if(!s->error&&*s->p=='.'){s->p++;int64_t place=SCALE;unsigned decimals=0;while(*s->p>='0'&&*s->p<='9'){if(++decimals>6){s->error=3;break;}place/=10;int64_t t=(int64_t)(*s->p++-'0')*place;if(__builtin_add_overflow(v,t,&v)){s->error=3;break;}digits++;}}
 if(!digits&&!s->error)s->error=1;}
 if(negative&&!s->error){if(v==INT64_MIN)s->error=3;else v=-v;}
 s->depth--;return v;
}
static int64_t product(struct parser*s){int64_t v=atom(s);while(!s->error){int op=0;space(s);if(match(s,"multiplied by ")||match(s,"times ")||match(s,"*"))op=1;else if(match(s,"divided by ")||match(s,"over ")||match(s,"/"))op=2;else break;if(++s->operations>64){s->error=1;break;}int64_t b=atom(s),t;if(s->error)break;if(op==1){if(__builtin_mul_overflow(v,b,&t))s->error=3;else v=t/SCALE;}else if(!b)s->error=2;else if(__builtin_mul_overflow(v,SCALE,&t))s->error=3;else if(t==INT64_MIN&&b==-1)s->error=3;else v=t/b;}return v;}
static int64_t expression(struct parser*s){int64_t v=product(s);while(!s->error){int op=0;space(s);if(match(s,"plus ")||match(s,"+"))op=1;else if(match(s,"minus ")||match(s,"-"))op=2;else break;if(++s->operations>64){s->error=1;break;}int64_t b=product(s);if(s->error)break;if(op==1?__builtin_add_overflow(v,b,&v):__builtin_sub_overflow(v,b,&v))s->error=3;}return v;}
static void number(int64_t v,char*out){unsigned n=0;uint64_t u;if(v<0){out[n++]='-';u=(uint64_t)(-(v+1))+1;}else u=(uint64_t)v;uint64_t whole=u/SCALE;char reverse[24];unsigned k=0;do{reverse[k++]=(char)('0'+whole%10);whole/=10;}while(whole);while(k)out[n++]=reverse[--k];unsigned fraction=(unsigned)(u%SCALE);if(fraction){out[n++]='.';unsigned place=100000;for(unsigned i=0;i<6;i++){out[n++]=(char)('0'+fraction/place);fraction%=place;place/=10;}while(out[n-1]=='0')n--;}out[n]=0;}
int holly_arithmetic(struct holly_session*s,const char*input,holly_emit_fn emit,void*ctx){char line[320];unsigned n=0;while(input[n]&&n+1<sizeof line){char c=input[n];line[n]=c>='A'&&c<='Z'?(char)(c+32):c;n++;}if(input[n])return 0;line[n]=0;struct parser p={line,0,0,0};space(&p);(void)(match(&p,"holly, ")||match(&p,"holly ")||match(&p,"hilly ")||match(&p,"hillary "));
 int explicit=match(&p,"calculate ")||match(&p,"math ");if(!explicit)(void)(match(&p,"what is ")||match(&p,"what's ")||match(&p,"whats "));space(&p);if(!explicit&&!((*p.p>='0'&&*p.p<='9')||*p.p=='('||*p.p=='-'||*p.p=='.')){struct parser probe=p;int64_t unused;if(!named_number(&probe,&unused))return 0;}
 int64_t v=expression(&p);space(&p);(void)(match(&p,"equals")||match(&p,"equal"));space(&p);if(*p.p=='?'){p.p++;space(&p);}if(*p.p&&!p.error)p.error=1;if(!explicit&&!p.operations)return 0;
 if(p.error){emit("Holly: ",ctx);emit(p.error==2?"I can't divide by zero.":p.error==3?"That exceeds my calculator limits. Use smaller numbers and at most six decimal places.":"I couldn't parse that calculation. Try 10 plus 10, or (2 + 3) * 4.",ctx);emit("\n",ctx);return 1;}
 char reply[40];number(v,reply);unsigned i=0;while(reply[i]&&i+1<sizeof s->last_answer){s->last_answer[i]=reply[i];i++;}s->last_answer[i]=0;s->expression=HOLLY_SPEAKING;emit("Holly: ",ctx);emit(reply,ctx);emit(".\n",ctx);return 1;}
