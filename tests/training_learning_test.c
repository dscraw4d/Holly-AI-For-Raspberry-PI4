#include <stdio.h>
#include "../src/selftrain.c"
int main(void){
 holly_training_init();
 for(unsigned round=0;round<12;round++){
  start_round(64);while(state.phase!=IDLE){if(state.phase==TRAIN)train_token();else evaluate_token();}
  printf("round=%u accepted=%u rejected=%u validation=%llu/%llu retention=%llu/%llu anchor=%llu/%llu\n",round+1,state.accepted,state.rejected,
   (unsigned long long)state.losses[0],(unsigned long long)state.losses[1],(unsigned long long)state.losses[2],(unsigned long long)state.losses[3],(unsigned long long)state.losses[4],(unsigned long long)state.losses[5]);
 }
 printf("Generation %u; this is character-level adaptation, not evidence of conversational ability.\n",state.generation);
 return 0;
}
