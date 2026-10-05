#include <assert.h>
#include <stdio.h>
#include "mind.h"
int main(void) {
 assert(mind_ask("Where is the garden?")==-1);
 assert(mind_teach("Where is the garden?","Behind the house.")==0);
 assert(mind_ask("Where is the garden?")==0);
 assert(mind_ask("Where garden") == 0);
 assert(mind_ask("garden engine") == -1);
 assert(mind_teach("WHERE IS THE GARDEN?","Outside.")==0);
 assert(lesson_count==1);
 assert(mind_teach("Where is the garage?","Near the house.")==1);
 assert(mind_ask("Where is the garage?")==1);
 puts("Memory engine tests passed");
}
