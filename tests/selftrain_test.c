#include <assert.h>
#include <stdio.h>
/* White-box state assertions exercise real production training and checkpoint code. */
#include "../src/selftrain.c"
static uint8_t media[2048][512];static int fail_write;static char reply[4096];static unsigned used;
static int read_sector(uint32_t l,uint8_t *p,void *c){(void)c;assert(l<2048);memcpy(p,media[l],512);return 0;}
static int write_sector(uint32_t l,const uint8_t *p,void *c){(void)c;assert(l>=8&&l<=2046);if(fail_write)return -1;memcpy(media[l],p,512);return 0;}
static void put32(uint8_t *p,unsigned n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static void capture(const char *s,void *c){(void)c;unsigned n=strlen(s);assert(used+n<sizeof(reply));memcpy(reply+used,s,n+1);used+=n;}
static void command(const char *s){char b[320];copy_text(b,s);used=0;reply[0]=0;holly_training_command(b,capture,0,0,0);}
static int memory_get(uint32_t id,struct holly_memory_item *item,void *c){(void)c;if(id!=17)return -1;memset(item,0,sizeof(*item));item->id=17;item->revision=3;copy_text(item->text,"Holly has a training store.");copy_text(item->source,"user reviewed correction");return 0;}
static void flush(void){while(disk.busy)holly_training_tick(last_tick+20000);}
int main(void){
 struct holly_block_ops io={read_sector,write_sector,0};memcpy(media[0]+440,"HLY2",4);media[0][510]=0x55;media[0][511]=0xaa;media[0][450]=0x0c;put32(media[0]+454,2048);put32(media[0]+458,456704);media[0][466]=0xda;put32(media[0]+470,458752);
 assert(holly_training_mount(&io)==0);flush();
 struct holly_memory_ops mem={0};mem.get=memory_get;char approve[]="train approve 17 train";
 used=0;holly_training_command(approve,capture,0,&mem,0);assert(state.count==55&&state.records[54].revision==3);flush();
 assert(holly_training_mount(&io)==1&&state.count==55&&state.records[54].id==17&&state.records[54].revision==3);
 char duplicate[]="train approve 17 valid";used=0;holly_training_command(duplicate,capture,0,&mem,0);assert(strstr(reply,"ERR duplicate")&&state.count==55);
 command("train export 54");assert(strstr(reply,"EXAMPLE 0 17 3"));command("train remove 17");flush();assert(state.count==54);
 command("train start 100");assert(strstr(reply,"OK"));flush();
 for(unsigned i=0;i<25;i++)train_token();
 save();flush();
 static int16_t checkpoint[HOLLY_LM_PARAMETERS],expected[HOLLY_LM_PARAMETERS];memcpy(checkpoint,state.candidate,MODEL_BYTES);
 printf("CANDIDATE25 %08x\n",model_crc(state.candidate,MODEL_BYTES));
 assert(memcmp(state.active,state.candidate,MODEL_BYTES));for(unsigned i=0;i<20;i++)train_token();memcpy(expected,state.candidate,MODEL_BYTES);
 assert(holly_training_mount(&io)==1&&state.steps==25&&state.remaining==75);assert(!memcmp(checkpoint,state.candidate,MODEL_BYTES));
 for(unsigned i=0;i<20;i++)train_token();
 assert(!memcmp(expected,state.candidate,MODEL_BYTES));
 command("train pause");flush();unsigned steps=state.steps;holly_training_tick(last_tick+3000000);assert(state.steps==steps);
 command("train resume");flush();holly_training_tick(last_tick+3000000);assert(state.steps==steps+1);
 command("train cancel");flush();assert(state.phase==IDLE&&!state.automatic);
 command("model import begin");command("model import 1 0000");assert(strstr(reply,"ERR"));command("model import 0 0");assert(strstr(reply,"ERR"));command("model import finish 0");assert(strstr(reply,"ERR"));command("train cancel");flush();
 /* Exercise both gate outcomes and durable rollback. */
 memcpy(state.candidate,state.active,MODEL_BYTES);state.candidate[HOLLY_LM_BIAS_OFFSET]++;
 state.phase=EVAL;state.eval_phase=5;state.eval_record=state.count;
 for(unsigned i=0;i<6;i++){state.losses[i]=10;state.tokens[i]=10;state.correct[i]=5;}
 state.losses[1]=9;evaluate_token();flush();assert(state.accepted==1&&state.previous_valid&&state.generation==1);
 assert(state.active[HOLLY_LM_BIAS_OFFSET]==state.previous[HOLLY_LM_BIAS_OFFSET]+1);
 command("model rollback");flush();assert(state.generation==2&&!state.automatic);
 assert(holly_training_mount(&io)==1&&state.generation==2);
 state.phase=EVAL;state.eval_phase=5;state.eval_record=state.count;state.losses[1]=11;evaluate_token();flush();assert(state.rejected==1);
 /* End-to-end real candidate: complete training and all six evaluation passes. */
 command("train start 16");flush();unsigned bound=30000;
 while(state.phase!=IDLE&&bound--){if(state.phase==TRAIN)train_token();else evaluate_token();flush();}
 assert(bound&&state.tokens[0]&&state.tokens[2]&&state.tokens[4]);
 command("train status");puts(reply);
 /* Failed promotion save must restore the old active weights and stop auto. */
 command("train status");
 memcpy(state.candidate,state.active,MODEL_BYTES);state.candidate[HOLLY_LM_BIAS_OFFSET]++;
 state.phase=EVAL;state.eval_phase=5;state.eval_record=state.count;state.automatic=1;
 for(unsigned i=0;i<6;i++){state.losses[i]=10;state.tokens[i]=10;state.correct[i]=5;}state.losses[1]=9;
 static int16_t old_active[HOLLY_LM_PARAMETERS],old_previous[HOLLY_LM_PARAMETERS];
 memcpy(old_active,state.active,MODEL_BYTES);memcpy(old_previous,state.previous,MODEL_BYTES);unsigned old_generation=state.generation;
 evaluate_token();uint8_t ctx[64]={0};int32_t actual[98],expected_logits[98],hidden[64];holly_lm_logits(ctx,actual);holly_lm_forward(old_active,ctx,hidden,expected_logits);assert(!memcmp(actual,expected_logits,sizeof(actual)));fail_write=1;flush();assert(disk.error&&state.paused&&!state.automatic&&!memcmp(state.active,old_active,MODEL_BYTES)&&!memcmp(state.previous,old_previous,MODEL_BYTES)&&state.generation==old_generation);fail_write=0;
 assert(holly_training_mount(&io)==1);
 state.phase=TRAIN;state.remaining=0;assert(!valid_state());state.phase=IDLE;
 state.records[0].split=2;assert(!valid_state());state.records[0].split=0;assert(valid_state());
 puts("Self-training: weight updates, exact resume, pause, upload errors, gates, rollback, failed-save recovery and full evaluation passed");
}
