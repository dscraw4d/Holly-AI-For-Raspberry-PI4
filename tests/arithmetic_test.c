#include "arithmetic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static char out[512];static unsigned used;
static void emit(const char*p,void*ctx){(void)ctx;while(*p&&used+1<sizeof out)out[used++]=*p++;out[used]=0;}
static void check(const char*q,const char*expected){struct holly_session s={0};used=0;out[0]=0;int result=holly_arithmetic(&s,q,emit,0);if(expected){if(!result||!strstr(out,expected)){fprintf(stderr,"%s -> %s; expected %s\n",q,out,expected);assert(0);}}else assert(!result);}
int main(void){check("what's 10 plus 10?","20.");check("what is ten plus ten?","20.");check("calculate twenty divided by four","5.");check("Holly what is 10 minus 3?","7.");check("what is 2 + 3 * 4?","14.");check("calculate (2 + 3) * 4","20.");check("math -5 * -3","15.");check("math 1.25 plus .75","2.");check("math 7 divided by 2","3.5.");check("math 1 / 3","0.333333.");check("math 10 / 2 / 5","1.");check("math 0 times 1000000","0.");check("math 1 / 0","divide by zero");check("math (1 + 2","parse");check("math 1 + 2 garbage","parse");check("math 99999999999999999999 + 1","limits");check("math -9223372036855 * 2","limits");check("math 0.0000001 + 1","limits");check("math (((((((((((((((((((1)))))))))))))))))))","parse");check("tell me about season 2",0);check("what is Red Dwarf?",0);check("1988 was a good year",0);check("what is 10?",0);
 unsigned seed=17;const char alphabet[]="0123456789 +-*/().abcdefghijklmnopqrstuvwxyz";for(unsigned i=0;i<20000;i++){char q[320]="math ";unsigned length=5+(i%300);for(unsigned j=5;j<length;j++){seed=seed*1664525u+1013904223u;q[j]=alphabet[seed%(sizeof alphabet-1)];}q[length]=0;struct holly_session s={0};used=0;holly_arithmetic(&s,q,emit,0);}
 puts("Arithmetic: precedence, decimals, zero division, overflow, depth, routing and 20000 malformed inputs passed.");}
